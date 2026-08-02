#include "hf_capture.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "app_timer.h"
#include "app_util_platform.h"
#include "nfc_14a.h"
#include "rc522.h"
#include "rfid_main.h"

#define HF_CAPTURE_STATE_EMPTY   0u
#define HF_CAPTURE_STATE_RUNNING 1u
#define HF_CAPTURE_STATE_STOPPED 2u

#define HF_CAPTURE_META_FLAG_OVERFLOW 0x01u

#define HF_CAPTURE_RECORD_FRAME      1u
#define HF_CAPTURE_RECORD_FIELD      2u
#define HF_CAPTURE_RECORD_HEADER_SIZE 22u
#define HF_CAPTURE_RECORD_BODY_SIZE   20u

static uint8_t m_ring[HF_CAPTURE_RING_CAPACITY];
static volatile uint16_t m_head;
static volatile uint16_t m_tail;
static volatile uint16_t m_used;
static volatile uint32_t m_stored_records;
static volatile uint32_t m_observed_records;
static volatile uint32_t m_dropped_records;
static volatile uint32_t m_next_sequence;
static volatile uint32_t m_session_id;
static volatile uint32_t m_start_token;
static uint32_t m_next_session_id = 1u;
static uint32_t m_boot_id;
static volatile uint8_t m_state;
static volatile hf_capture_mode_t m_mode;
static volatile data_frame_transport_t m_owner;
static volatile data_frame_transport_t m_recovery_transport;
static volatile uint8_t m_meta_flags;
static volatile bool m_notification_pending;
static volatile uint64_t m_last_notification_ticks;
static volatile uint32_t m_last_clock_tick;
static volatile uint64_t m_elapsed_ticks;
static volatile uint32_t m_delivered_records;
static volatile uint32_t m_last_acknowledged_sequence;
static volatile bool m_has_acknowledged_sequence;
static volatile uint64_t m_next_delivery_token;
static volatile uint64_t m_last_delivered_token;
static volatile uint64_t m_last_acknowledged_token;
static volatile uint32_t m_notification_generation;
static volatile uint32_t m_notification_sent_generation;

static void put_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value >> 8);
    out[1] = (uint8_t)value;
}

static void put_u32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static void put_u64(uint8_t *out, uint64_t value) {
    for (uint8_t i = 0; i < 8u; i++) {
        out[i] = (uint8_t)(value >> (56u - i * 8u));
    }
}

static uint32_t get_u32(const uint8_t *in) {
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) | in[3];
}

static uint32_t crc32(const uint8_t *data, uint16_t length) {
    uint32_t crc = UINT32_MAX;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return ~crc;
}

static void ring_write(const uint8_t *data, uint16_t length) {
    uint16_t first = (uint16_t)(HF_CAPTURE_RING_CAPACITY - m_tail);
    if (first > length) first = length;
    memcpy(&m_ring[m_tail], data, first);
    if (length > first) memcpy(m_ring, &data[first], length - first);
    m_tail = (uint16_t)((m_tail + length) % HF_CAPTURE_RING_CAPACITY);
    m_used += length;
}

static void ring_read(uint16_t offset, uint8_t *data, uint16_t length) {
    uint16_t first = (uint16_t)(HF_CAPTURE_RING_CAPACITY - offset);
    if (first > length) first = length;
    memcpy(data, &m_ring[offset], first);
    if (length > first) memcpy(&data[first], m_ring, length - first);
}

static uint16_t ring_read_u16(uint16_t offset) {
    uint8_t value[2];
    ring_read(offset, value, sizeof(value));
    return ((uint16_t)value[0] << 8) | value[1];
}

static uint32_t ring_record_sequence(uint16_t offset) {
    uint8_t value[4];
    ring_read((uint16_t)((offset + 4u) % HF_CAPTURE_RING_CAPACITY), value,
              sizeof(value));
    return get_u32(value);
}

static uint64_t clock_update(void) {
    CRITICAL_REGION_ENTER();
    uint32_t now = app_timer_cnt_get();
    uint32_t delta = app_timer_cnt_diff_compute(now, m_last_clock_tick);
    m_elapsed_ticks += delta;
    m_last_clock_tick = now;
    uint64_t elapsed = m_elapsed_ticks;
    CRITICAL_REGION_EXIT();
    return elapsed;
}

void hf_capture_clock_process(void) {
    if (m_state != HF_CAPTURE_STATE_EMPTY) (void)clock_update();
}

static void append_record(uint8_t type, hf_capture_direction_t direction,
                          uint8_t flags, uint16_t bit_length,
                          const uint8_t *data, uint16_t data_length) {
    if (m_state != HF_CAPTURE_STATE_RUNNING ||
            (data_length > 0u && data == NULL)) return;

    uint32_t sequence;
    uint64_t timestamp = clock_update();
    uint16_t body_length = (uint16_t)(HF_CAPTURE_RECORD_BODY_SIZE + data_length);
    uint16_t total = (uint16_t)(body_length + 2u);
    uint8_t header[HF_CAPTURE_RECORD_HEADER_SIZE];

    CRITICAL_REGION_ENTER();
    sequence = m_next_sequence++;
    if (m_observed_records != UINT32_MAX) m_observed_records++;
    if (data_length > HF_CAPTURE_MAX_RECORD_DATA_SIZE ||
            total > HF_CAPTURE_RING_CAPACITY - m_used) {
        if (m_dropped_records != UINT32_MAX) m_dropped_records++;
        m_meta_flags |= HF_CAPTURE_META_FLAG_OVERFLOW;
        m_notification_generation++;
        m_notification_pending = true;
        CRITICAL_REGION_EXIT();
        return;
    }

    put_u16(header, body_length);
    header[2] = HF_CAPTURE_PROTOCOL_VERSION;
    header[3] = type;
    put_u32(&header[4], sequence);
    put_u64(&header[8], timestamp);
    header[16] = (uint8_t)direction;
    header[17] = flags;
    put_u16(&header[18], bit_length);
    put_u16(&header[20], data_length);
    ring_write(header, sizeof(header));
    if (data_length > 0u) ring_write(data, data_length);
    m_stored_records++;
    m_notification_generation++;
    m_notification_pending = true;
    CRITICAL_REGION_EXIT();
}

static void tag_rx_callback(const uint8_t *data, uint16_t bit_length,
                            uint8_t flags) {
    append_record(HF_CAPTURE_RECORD_FRAME,
                  HF_CAPTURE_DIRECTION_READER_TO_CARD, flags, bit_length,
                  data, (uint16_t)((bit_length + 7u) / 8u));
}

static void tag_tx_callback(const uint8_t *data, uint16_t bit_length,
                            uint8_t flags) {
    append_record(HF_CAPTURE_RECORD_FRAME,
                  HF_CAPTURE_DIRECTION_CARD_TO_READER, flags, bit_length,
                  data, (uint16_t)((bit_length + 7u) / 8u));
}

static void tag_field_callback(bool present) {
    uint8_t value = present ? 1u : 0u;
    append_record(HF_CAPTURE_RECORD_FIELD, HF_CAPTURE_DIRECTION_EVENT, 0, 0,
                  &value, sizeof(value));
}

static void reader_trace_callback(bool tx, const uint8_t *data,
                                  uint16_t bit_length, uint8_t status) {
    uint8_t flags = status == 0x00u ? 0u : HF_CAPTURE_FRAME_FLAG_RF_ERROR;
    append_record(HF_CAPTURE_RECORD_FRAME,
                  tx ? HF_CAPTURE_DIRECTION_READER_TO_CARD :
                       HF_CAPTURE_DIRECTION_CARD_TO_READER,
                  flags, bit_length, data,
                  data == NULL ? 0u : (uint16_t)((bit_length + 7u) / 8u));
}

static void detach_source(void) {
    if (m_mode == HF_CAPTURE_MODE_READER) {
        pcd_14a_reader_capture_clear();
    } else {
        nfc_tag_14a_clear_sniff_cb();
        nfc_tag_14a_clear_tx_sniff_cb();
        nfc_tag_14a_clear_field_sniff_cb();
        nfc_tag_14a_set_sniff_passive(false);
        if (m_mode == HF_CAPTURE_MODE_PASSIVE) tag_emulation_sense_run();
    }
}

static bool owner_valid(data_frame_transport_t owner) {
    return owner > DATA_FRAME_TRANSPORT_NONE &&
           owner < DATA_FRAME_TRANSPORT_COUNT;
}

static void ensure_boot_id(void) {
    if (m_boot_id != 0u) return;
    m_boot_id = ((uint32_t)(unsigned int)rand() << 16) ^
                (uint32_t)(unsigned int)rand();
    if (m_boot_id == 0u) m_boot_id = 1u;
    m_next_session_id = m_boot_id;
}

hf_capture_result_t hf_capture_start(hf_capture_mode_t mode,
                                     data_frame_transport_t owner,
                                     uint32_t start_token,
                                     uint32_t *session_id) {
    if (session_id == NULL || mode > HF_CAPTURE_MODE_READER ||
            !owner_valid(owner) || start_token == 0u) {
        return HF_CAPTURE_RESULT_INVALID;
    }
    CRITICAL_REGION_ENTER();
    bool idempotent_retry = m_state == HF_CAPTURE_STATE_RUNNING &&
                            m_mode == mode && m_owner == owner &&
                            m_start_token == start_token;
    bool previous_session_pending = m_state == HF_CAPTURE_STATE_RUNNING ||
                                    (m_state == HF_CAPTURE_STATE_STOPPED &&
                                     m_stored_records > 0u);
    uint32_t active_session_id = m_session_id;
    CRITICAL_REGION_EXIT();
    if (idempotent_retry) {
        *session_id = active_session_id;
        return HF_CAPTURE_RESULT_OK;
    }
    if (previous_session_pending) {
        return HF_CAPTURE_RESULT_BUSY;
    }

    ensure_boot_id();
    memset(m_ring, 0, sizeof(m_ring));
    m_head = 0;
    m_tail = 0;
    m_used = 0;
    m_stored_records = 0;
    m_observed_records = 0;
    m_dropped_records = 0;
    m_next_sequence = 0;
    m_meta_flags = 0;
    m_mode = mode;
    m_owner = owner;
    m_recovery_transport = owner;
    m_start_token = start_token;
    m_notification_pending = false;
    m_last_notification_ticks = 0;
    m_notification_generation = 0;
    m_notification_sent_generation = 0;
    m_elapsed_ticks = 0;
    m_last_clock_tick = app_timer_cnt_get();
    m_session_id = m_next_session_id++;
    if (m_session_id == 0u) m_session_id = m_next_session_id++;
    m_delivered_records = 0u;
    m_has_acknowledged_sequence = false;
    m_last_acknowledged_sequence = 0u;
    m_next_delivery_token = 1u;
    m_last_delivered_token = 0u;
    m_last_acknowledged_token = 0u;
    m_state = HF_CAPTURE_STATE_RUNNING;

    if (mode == HF_CAPTURE_MODE_READER) {
        reader_mode_enter();
        pcd_14a_reader_capture_set(reader_trace_callback);
    } else {
        nfc_tag_14a_set_sniff_cb(tag_rx_callback);
        nfc_tag_14a_set_tx_sniff_cb(tag_tx_callback);
        nfc_tag_14a_set_field_sniff_cb(tag_field_callback);
        nfc_tag_14a_set_sniff_passive(mode == HF_CAPTURE_MODE_PASSIVE);
        tag_mode_enter();
        if (mode == HF_CAPTURE_MODE_PASSIVE) nfc_tag_14a_sense_switch(true);
    }

    *session_id = m_session_id;
    return HF_CAPTURE_RESULT_OK;
}

hf_capture_result_t hf_capture_resume(uint32_t session_id,
                                      uint32_t start_token,
                                      data_frame_transport_t owner) {
    if (!owner_valid(owner) || start_token == 0u) {
        return HF_CAPTURE_RESULT_INVALID;
    }
    if (m_state == HF_CAPTURE_STATE_EMPTY || start_token != m_start_token) {
        return HF_CAPTURE_RESULT_SESSION;
    }
    if (session_id == 0u) {
        if (owner != m_owner &&
                !(m_owner == DATA_FRAME_TRANSPORT_NONE &&
                  owner == m_recovery_transport)) {
            return HF_CAPTURE_RESULT_SESSION;
        }
    } else {
        if (session_id != m_session_id) return HF_CAPTURE_RESULT_SESSION;
    }
    m_owner = owner;
    m_notification_pending = m_stored_records > 0u;
    return HF_CAPTURE_RESULT_OK;
}

void hf_capture_owner_disconnected(data_frame_transport_t owner) {
    if (owner_valid(owner) && owner == m_owner) {
        m_owner = DATA_FRAME_TRANSPORT_NONE;
    }
}

hf_capture_result_t hf_capture_stop(uint32_t session_id,
                                    data_frame_transport_t owner) {
    if (m_state == HF_CAPTURE_STATE_EMPTY || session_id == 0u ||
            session_id != m_session_id || owner != m_owner) {
        return HF_CAPTURE_RESULT_SESSION;
    }
    CRITICAL_REGION_ENTER();
    bool was_running = m_state == HF_CAPTURE_STATE_RUNNING;
    m_state = HF_CAPTURE_STATE_STOPPED;
    CRITICAL_REGION_EXIT();
    if (was_running) detach_source();
    m_notification_pending = m_stored_records > 0u;
    return HF_CAPTURE_RESULT_OK;
}

static bool acknowledge(bool acknowledge_present, uint32_t ack_sequence,
                        uint64_t ack_delivery_token) {
    if (!acknowledge_present) return true;
    if (m_has_acknowledged_sequence &&
            ack_sequence == m_last_acknowledged_sequence &&
            ack_delivery_token == m_last_acknowledged_token) {
        return true;
    }
    if (ack_delivery_token == 0u ||
            ack_delivery_token != m_last_delivered_token) return false;

    uint16_t cursor = m_head;
    uint32_t acknowledged_records = 0u;
    bool found = false;
    for (uint32_t i = 0u; i < m_delivered_records; i++) {
        acknowledged_records++;
        if (ring_record_sequence(cursor) == ack_sequence) {
            found = true;
            break;
        }
        cursor = (uint16_t)((cursor + ring_read_u16(cursor) + 2u) %
                            HF_CAPTURE_RING_CAPACITY);
    }
    if (!found) return false;

    for (uint32_t i = 0u; i < acknowledged_records; i++) {
        uint16_t total = (uint16_t)(ring_read_u16(m_head) + 2u);
        m_head = (uint16_t)((m_head + total) % HF_CAPTURE_RING_CAPACITY);
        m_used -= total;
        m_stored_records--;
    }
    m_delivered_records -= acknowledged_records;
    m_last_acknowledged_sequence = ack_sequence;
    m_last_acknowledged_token = ack_delivery_token;
    m_has_acknowledged_sequence = true;
    if (m_stored_records == 0u) m_head = m_tail;
    return true;
}

uint16_t hf_capture_build_meta(uint32_t session_id, uint8_t *response,
                               uint16_t response_capacity) {
    if (response == NULL || response_capacity < HF_CAPTURE_META_SIZE ||
            m_state == HF_CAPTURE_STATE_EMPTY || session_id != m_session_id) return 0;
    uint64_t elapsed = clock_update();
    CRITICAL_REGION_ENTER();
    uint8_t state = m_state;
    hf_capture_mode_t mode = m_mode;
    uint8_t flags = m_meta_flags;
    uint32_t active_session_id = m_session_id;
    uint32_t next_sequence = m_next_sequence;
    uint32_t stored_records = m_stored_records;
    uint32_t observed_records = m_observed_records;
    uint32_t dropped_records = m_dropped_records;
    uint16_t used = m_used;
    uint32_t first_sequence = stored_records == 0u ? next_sequence :
                              ring_record_sequence(m_head);
    CRITICAL_REGION_EXIT();
    response[0] = HF_CAPTURE_PROTOCOL_VERSION;
    response[1] = state;
    response[2] = (uint8_t)mode;
    response[3] = flags;
    put_u32(&response[4], active_session_id);
    put_u32(&response[8], first_sequence);
    put_u32(&response[12], next_sequence);
    put_u32(&response[16], stored_records);
    put_u32(&response[20], observed_records);
    put_u32(&response[24], dropped_records);
    put_u16(&response[28], used);
    put_u16(&response[30], HF_CAPTURE_RING_CAPACITY);
    put_u64(&response[32], elapsed);
    put_u32(&response[40], m_boot_id);
    put_u32(&response[44], m_start_token);
    return HF_CAPTURE_META_SIZE;
}

hf_capture_result_t hf_capture_get(uint32_t session_id, bool acknowledge_present,
                                   uint32_t ack_sequence,
                                   uint64_t ack_delivery_token,
                                   data_frame_transport_t owner,
                                   uint16_t requested_bytes, uint8_t *response,
                                   uint16_t response_capacity,
                                   uint16_t *response_length) {
    if (response == NULL || response_length == NULL ||
            response_capacity < HF_CAPTURE_MIN_PAGE_SIZE ||
            requested_bytes < HF_CAPTURE_MIN_PAGE_SIZE) {
        return HF_CAPTURE_RESULT_INVALID;
    }
    if (m_state == HF_CAPTURE_STATE_EMPTY || session_id == 0u ||
            session_id != m_session_id || owner != m_owner) {
        return HF_CAPTURE_RESULT_SESSION;
    }
    CRITICAL_REGION_ENTER();
    if (!acknowledge(acknowledge_present, ack_sequence, ack_delivery_token)) {
        CRITICAL_REGION_EXIT();
        return HF_CAPTURE_RESULT_INVALID;
    }
    uint16_t cursor = m_head;
    uint32_t available_records = m_stored_records;
    uint32_t empty_sequence = m_next_sequence;
    CRITICAL_REGION_EXIT();

    uint16_t maximum = requested_bytes;
    if (maximum > response_capacity) maximum = response_capacity;
    uint16_t data_length = 0;
    uint16_t count = 0;
    uint32_t first_sequence = empty_sequence;
    uint32_t next_sequence = empty_sequence;

    while (count < available_records) {
        uint16_t total = (uint16_t)(ring_read_u16(cursor) + 2u);
        if ((uint32_t)HF_CAPTURE_PAGE_HEADER_SIZE + data_length + total > maximum) break;
        uint32_t sequence = ring_record_sequence(cursor);
        if (count == 0u) first_sequence = sequence;
        ring_read(cursor, &response[HF_CAPTURE_PAGE_HEADER_SIZE + data_length], total);
        data_length += total;
        cursor = (uint16_t)((cursor + total) % HF_CAPTURE_RING_CAPACITY);
        next_sequence = sequence + 1u;
        count++;
    }

    uint16_t meta_length = hf_capture_build_meta(session_id, response,
                                                  response_capacity);
    if (meta_length != HF_CAPTURE_META_SIZE) return HF_CAPTURE_RESULT_SESSION;
    put_u32(&response[48], first_sequence);
    put_u32(&response[52], next_sequence);
    put_u16(&response[56], count);
    put_u16(&response[58], data_length);
    put_u32(&response[60], crc32(&response[HF_CAPTURE_PAGE_HEADER_SIZE],
                                  data_length));
    uint64_t delivery_token = 0u;
    if (count > 0u) {
        CRITICAL_REGION_ENTER();
        delivery_token = m_next_delivery_token++;
        if (m_next_delivery_token > INT64_MAX) m_next_delivery_token = 1u;
        m_last_delivered_token = delivery_token;
        if (count > m_delivered_records) m_delivered_records = count;
        CRITICAL_REGION_EXIT();
    }
    put_u64(&response[64], delivery_token);
    *response_length = (uint16_t)(HF_CAPTURE_PAGE_HEADER_SIZE + data_length);
    return HF_CAPTURE_RESULT_OK;
}

uint32_t hf_capture_session_id(void) {
    return m_session_id;
}

data_frame_transport_t hf_capture_owner(void) {
    return m_owner;
}

bool hf_capture_is_active(void) {
    return m_state == HF_CAPTURE_STATE_RUNNING;
}

bool hf_capture_prevents_system_off(void) {
    return m_state == HF_CAPTURE_STATE_RUNNING || m_stored_records > 0u;
}

bool hf_capture_notification_due(void) {
    if (!m_notification_pending || m_state == HF_CAPTURE_STATE_EMPTY) return false;
    uint64_t elapsed = clock_update();
    if (m_last_notification_ticks != 0u &&
            elapsed - m_last_notification_ticks < APP_TIMER_TICKS(500u)) {
        return false;
    }
    CRITICAL_REGION_ENTER();
    m_notification_sent_generation = m_notification_generation;
    CRITICAL_REGION_EXIT();
    return true;
}

#if defined(HF_CAPTURE_TEST)
void hf_capture_test_set_next_sequence(uint32_t sequence) {
    m_next_sequence = sequence;
}
#endif

void hf_capture_notification_sent(void) {
    uint64_t elapsed = clock_update();
    CRITICAL_REGION_ENTER();
    m_last_notification_ticks = elapsed;
    m_notification_pending =
        m_notification_generation != m_notification_sent_generation;
    CRITICAL_REGION_EXIT();
}
