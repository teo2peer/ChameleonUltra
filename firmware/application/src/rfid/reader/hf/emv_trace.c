#include "emv_trace.h"
#include "emv_trace_internal.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "app_status.h"
#include "bsp_delay.h"
#include "bsp_time.h"
#include "bsp_wdt.h"
#include "iso_dep_reader.h"
#include "netdata.h"
#include "rc522.h"

#define TRACE_CAPACITY        12288u
#define TRACE_MAX_AIDS       16u
#define TRACE_MAX_APDUS      512u
#define TRACE_MAX_RECORDS_APP 64u

#define TRACE_STATE_EMPTY    0u
#define TRACE_STATE_RUNNING  1u
#define TRACE_STATE_COMPLETE 2u
#define TRACE_STATE_ABORTED  3u

#define TRACE_FLAG_COMPLETE          0x00000001u
#define TRACE_FLAG_TIMEOUT           0x00000002u
#define TRACE_FLAG_LOG_TRUNCATED     0x00000004u
#define TRACE_FLAG_RF_TRUNCATED      0x00000008u
#define TRACE_FLAG_RESPONSE_TRUNCATED 0x00000010u
#define TRACE_FLAG_APP_LIMIT         0x00000020u
#define TRACE_FLAG_TIMING_VALID      0x00000040u
#define TRACE_FLAG_MAX_PROCESSING    0x00000080u
#define TRACE_FLAG_TRANSPORT_ERROR   0x00000100u
#define TRACE_FLAG_EXPRESS_TRANSIT   0x00000200u

#define RECORD_RF       1u
#define RECORD_APDU     2u
#define RECORD_APP      3u
#define RECORD_SUMMARY  4u

#define STAGE_ACTIVATION  0u
#define STAGE_PPSE        1u
#define STAGE_SELECT_APP  2u
#define STAGE_GET_DATA    3u
#define STAGE_GPO         4u
#define STAGE_READ_AFL    5u
#define STAGE_READ_SCAN   6u
#define STAGE_GET_RESPONSE 7u
#define STAGE_GENERATE_AC 8u
#define STAGE_LOG         9u

typedef struct {
    uint8_t aid[16];
    uint8_t aid_len;
    uint8_t priority;
} emv_aid_t;

typedef enum {
    EMV_SCHEME_OTHER,
    EMV_SCHEME_VISA,
    EMV_SCHEME_MASTERCARD,
} emv_scheme_t;

typedef struct {
    uint8_t options;
    uint8_t max_aids;
    uint8_t max_records;
    uint16_t max_apdus;
    uint32_t budget_ms;
    uint8_t amount[6];
    uint8_t country[2];
    uint8_t currency[2];
    uint8_t date[3];
    uint8_t transaction_type;
    uint8_t cryptogram_type;
    uint8_t terminal_profile;
    uint8_t custom_ttq[4];
    uint8_t polling_profile;
    uint8_t behavior;
    uint8_t poll_retries;
    uint8_t poll_delay_ms;
    uint8_t poll_timeout_ms;
} emv_request_t;

typedef struct {
    uint8_t state;
    uint16_t result_status;
    uint32_t flags;
    uint32_t scan_id;
    uint32_t stored_records;
    uint32_t observed_records;
    uint32_t stored_bytes;
    uint32_t required_bytes;
    uint32_t first_dropped;
    uint16_t application_count;
    uint16_t apdu_count;
    uint32_t elapsed_ms;
    picc_14a_tag_t tag;
    uint8_t arena[TRACE_CAPACITY];
} emv_trace_state_t;

typedef struct {
    const uint8_t *tag;
    uint8_t tag_len;
    const uint8_t *value;
    uint16_t value_len;
    bool constructed;
} tlv_item_t;

static emv_trace_state_t m_trace;
static emv_request_t m_request;
static iso_dep_reader_t m_reader;
static autotimer *m_timer;
static uint8_t m_stage;
static uint8_t m_app_index;
static uint8_t m_attempt;
static bool m_store_disabled;
static uint8_t m_active_ttq[4];
static uint8_t m_active_terminal_type = 0x22u;
static uint8_t m_active_terminal_capabilities[3] = {0xE0, 0x08, 0x00};

static const uint8_t m_ttq_profiles[][4] = {
    {0x36, 0x00, 0xC0, 0x00}, /* automatic/default */
    {0x33, 0x80, 0x40, 0x00}, /* Apple transit */
    {0x32, 0x80, 0x40, 0x00}, /* online, no ODA */
    {0x36, 0x00, 0xC0, 0x00}, /* broad mobile */
    {0x26, 0x80, 0x40, 0x00}, /* qVSDC online */
    {0x22, 0x80, 0x40, 0x00}, /* minimal online */
    {0xB6, 0x00, 0xC0, 0x00}, /* MSD + qVSDC */
};

static bool terminal_profile_valid(uint8_t profile) {
    return profile <= EMV_TERMINAL_PROFILE_MSD_QVSDC ||
           profile == EMV_TERMINAL_PROFILE_CUSTOM ||
           profile == EMV_TERMINAL_PROFILE_SWEEP;
}

static void terminal_profile_select(uint8_t profile) {
    if (profile == EMV_TERMINAL_PROFILE_AUTO) {
        profile = (m_request.options & EMV_TRACE_OPT_EXPRESS_TRANSIT) != 0u ?
                  EMV_TERMINAL_PROFILE_APPLE_TRANSIT :
                  EMV_TERMINAL_PROFILE_BROAD_MOBILE;
    }
    if (profile == EMV_TERMINAL_PROFILE_CUSTOM) {
        memcpy(m_active_ttq, m_request.custom_ttq, sizeof(m_active_ttq));
    } else if (profile <= EMV_TERMINAL_PROFILE_MSD_QVSDC) {
        memcpy(m_active_ttq, m_ttq_profiles[profile], sizeof(m_active_ttq));
    }
    m_active_terminal_type =
        (profile == EMV_TERMINAL_PROFILE_APPLE_TRANSIT ||
         profile == EMV_TERMINAL_PROFILE_QVSDC_ONLINE ||
         profile == EMV_TERMINAL_PROFILE_MINIMAL_ONLINE) ? 0x14u : 0x22u;
    m_active_terminal_capabilities[0] = 0xE0u;
    m_active_terminal_capabilities[1] = 0x08u; /* No-CVM capability. */
    m_active_terminal_capabilities[2] = 0x00u;
}

static void polling_profile_values(uint8_t *retries, uint8_t *delay_ms,
                                   uint8_t *timeout_ms) {
    switch (m_request.polling_profile) {
        case EMV_POLLING_PROFILE_FAST:
            *retries = 18u; *delay_ms = 3u; *timeout_ms = 2u;
            break;
        case EMV_POLLING_PROFILE_BALANCED:
            *retries = 40u; *delay_ms = 5u; *timeout_ms = 3u;
            break;
        case EMV_POLLING_PROFILE_PATIENT:
            *retries = 80u; *delay_ms = 8u; *timeout_ms = 4u;
            break;
        default:
            *retries = 30u; *delay_ms = 5u; *timeout_ms = 2u;
            break;
    }
    if (m_request.poll_retries != 0u) *retries = m_request.poll_retries;
    if (m_request.poll_delay_ms != 0u) *delay_ms = m_request.poll_delay_ms;
    if (m_request.poll_timeout_ms != 0u) *timeout_ms = m_request.poll_timeout_ms;
}

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

static uint16_t get_u16(const uint8_t *in) {
    return ((uint16_t)in[0] << 8) | in[1];
}

static uint32_t get_u32(const uint8_t *in) {
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) | in[3];
}

static uint32_t elapsed_ms(void) {
    return m_timer == NULL ? 0u : m_timer->time;
}

static bool budget_available(void) {
    if (m_trace.apdu_count >= m_request.max_apdus) return false;
    if (elapsed_ms() > m_request.budget_ms) {
        m_trace.flags |= TRACE_FLAG_TIMEOUT;
        return false;
    }
    return true;
}

static bool append_record(uint8_t type, uint8_t stage, uint8_t app,
                          uint8_t attempt, uint8_t record_flags,
                          uint16_t status, const uint8_t *payload,
                          uint16_t payload_len, bool rf_record) {
    uint32_t sequence = m_trace.observed_records++;
    uint16_t body_len = (uint16_t)(16u + payload_len);
    uint32_t total = (uint32_t)body_len + 2u;
    if (UINT32_MAX - m_trace.required_bytes < total) {
        m_trace.required_bytes = UINT32_MAX;
    } else {
        m_trace.required_bytes += total;
    }

    if (m_store_disabled || m_trace.stored_bytes + total > TRACE_CAPACITY) {
        if (rf_record) {
            m_trace.flags |= TRACE_FLAG_RF_TRUNCATED;
        } else {
            m_trace.flags |= TRACE_FLAG_LOG_TRUNCATED;
            if (m_trace.first_dropped == UINT32_MAX) m_trace.first_dropped = sequence;
            m_store_disabled = true;
        }
        return false;
    }

    uint8_t *out = &m_trace.arena[m_trace.stored_bytes];
    put_u16(out, body_len);
    out[2] = EMV_TRACE_PROTOCOL_VERSION;
    out[3] = type;
    put_u32(&out[4], sequence);
    out[8] = stage;
    out[9] = app;
    out[10] = attempt;
    out[11] = record_flags;
    put_u16(&out[12], status);
    put_u32(&out[14], (m_request.options & EMV_TRACE_OPT_TIMING) ? elapsed_ms() : 0u);
    if (payload_len > 0u) memcpy(&out[18], payload, payload_len);
    m_trace.stored_bytes += total;
    m_trace.stored_records++;
    return true;
}

static void rf_trace_callback(bool tx, const uint8_t *data,
                              uint16_t bit_length, uint8_t status) {
    if ((m_request.options & EMV_TRACE_OPT_INCLUDE_RF) == 0u) return;
    uint16_t data_len = (uint16_t)((bit_length + 7u) / 8u);
    if (data_len > 64u) data_len = 64u;
    uint8_t payload[69];
    payload[0] = tx ? 0u : 1u;
    put_u16(&payload[1], bit_length);
    put_u16(&payload[3], data_len);
    if (data_len > 0u && data != NULL) memcpy(&payload[5], data, data_len);
    append_record(RECORD_RF, m_stage, m_app_index, m_attempt,
                  bit_length == data_len * 8u ? 0u : 1u,
                  status, payload, (uint16_t)(5u + data_len), true);
}

static bool tlv_next(const uint8_t *data, uint16_t length, uint16_t *offset,
                     tlv_item_t *item) {
    uint16_t i = *offset;
    while (i < length && (data[i] == 0x00u || data[i] == 0xFFu)) i++;
    if (i >= length) return false;
    uint16_t tag_start = i;
    uint8_t first = data[i++];
    if ((first & 0x1Fu) == 0x1Fu) {
        uint8_t count = 0;
        do {
            if (i >= length || count++ >= 3u) return false;
        } while ((data[i++] & 0x80u) != 0u);
    }
    uint16_t tag_len = i - tag_start;
    if (i >= length) return false;
    uint16_t value_len = data[i++];
    if ((value_len & 0x80u) != 0u) {
        uint8_t count = (uint8_t)(value_len & 0x7Fu);
        if (count == 0u || count > 2u || i + count > length) return false;
        value_len = 0;
        while (count-- > 0u) value_len = (uint16_t)((value_len << 8) | data[i++]);
    }
    if ((uint32_t)i + value_len > length) return false;
    item->tag = &data[tag_start];
    item->tag_len = (uint8_t)tag_len;
    item->value = &data[i];
    item->value_len = value_len;
    item->constructed = (first & 0x20u) != 0u;
    *offset = i + value_len;
    return true;
}

static bool tag_equals(const tlv_item_t *item, uint32_t tag) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < item->tag_len; i++) value = (value << 8) | item->tag[i];
    return value == tag;
}

static bool find_tlv(const uint8_t *data, uint16_t length, uint32_t wanted,
                     const uint8_t **value, uint16_t *value_len, uint8_t depth) {
    if (depth > 8u) return false;
    uint16_t offset = 0;
    tlv_item_t item;
    while (tlv_next(data, length, &offset, &item)) {
        if (tag_equals(&item, wanted)) {
            *value = item.value;
            *value_len = item.value_len;
            return true;
        }
        if (item.constructed && find_tlv(item.value, item.value_len, wanted,
                                         value, value_len, depth + 1u)) return true;
    }
    return false;
}

static bool aid_exists(const emv_aid_t *aids, uint8_t count,
                       const uint8_t *aid, uint8_t aid_len) {
    for (uint8_t i = 0; i < count; i++) {
        if (aids[i].aid_len == aid_len && memcmp(aids[i].aid, aid, aid_len) == 0) return true;
    }
    return false;
}

static void collect_templates(const uint8_t *data, uint16_t length,
                              emv_aid_t *aids, uint8_t *count, uint8_t depth) {
    if (depth > 8u || *count >= m_request.max_aids) return;
    uint16_t offset = 0;
    tlv_item_t item;
    while (tlv_next(data, length, &offset, &item)) {
        if (tag_equals(&item, 0x61u)) {
            const uint8_t *aid = NULL;
            const uint8_t *priority = NULL;
            uint16_t aid_len = 0;
            uint16_t priority_len = 0;
            if (find_tlv(item.value, item.value_len, 0x4Fu, &aid, &aid_len, 0) &&
                    aid_len >= 5u && aid_len <= 16u &&
                    !aid_exists(aids, *count, aid, (uint8_t)aid_len)) {
                emv_aid_t *entry = &aids[(*count)++];
                entry->aid_len = (uint8_t)aid_len;
                memcpy(entry->aid, aid, aid_len);
                entry->priority = 0;
                if (find_tlv(item.value, item.value_len, 0x87u,
                             &priority, &priority_len, 0) && priority_len == 1u) {
                    entry->priority = priority[0];
                }
            }
        }
        if (item.constructed) collect_templates(item.value, item.value_len,
                                                 aids, count, depth + 1u);
        if (*count >= m_request.max_aids) return;
    }
}

static void sort_aids(emv_aid_t *aids, uint8_t count) {
    for (uint8_t i = 1; i < count; i++) {
        emv_aid_t current = aids[i];
        uint8_t current_rank = current.priority & 0x0Fu;
        if (current_rank == 0u) current_rank = 0x10u;
        uint8_t j = i;
        while (j > 0u) {
            uint8_t previous_rank = aids[j - 1u].priority & 0x0Fu;
            if (previous_rank == 0u) previous_rank = 0x10u;
            if (previous_rank <= current_rank) break;
            aids[j] = aids[j - 1u];
            j--;
        }
        aids[j] = current;
    }
}

static emv_scheme_t aid_scheme(const emv_aid_t *aid) {
    if (aid->aid_len < 5u) return EMV_SCHEME_OTHER;
    if (memcmp(aid->aid, "\xA0\x00\x00\x00\x03", 5) == 0) return EMV_SCHEME_VISA;
    if (memcmp(aid->aid, "\xA0\x00\x00\x00\x04", 5) == 0) return EMV_SCHEME_MASTERCARD;
    return EMV_SCHEME_OTHER;
}

static void sweep_order(const emv_aid_t *aid, uint8_t *profiles) {
    static const uint8_t visa[] = {
        EMV_TERMINAL_PROFILE_APPLE_TRANSIT,
        EMV_TERMINAL_PROFILE_ONLINE_NO_ODA,
        EMV_TERMINAL_PROFILE_QVSDC_ONLINE,
        EMV_TERMINAL_PROFILE_BROAD_MOBILE,
        EMV_TERMINAL_PROFILE_MINIMAL_ONLINE,
        EMV_TERMINAL_PROFILE_MSD_QVSDC,
    };
    static const uint8_t mastercard[] = {
        EMV_TERMINAL_PROFILE_BROAD_MOBILE,
        EMV_TERMINAL_PROFILE_QVSDC_ONLINE,
        EMV_TERMINAL_PROFILE_APPLE_TRANSIT,
        EMV_TERMINAL_PROFILE_ONLINE_NO_ODA,
        EMV_TERMINAL_PROFILE_MSD_QVSDC,
        EMV_TERMINAL_PROFILE_MINIMAL_ONLINE,
    };
    static const uint8_t other[] = {
        EMV_TERMINAL_PROFILE_QVSDC_ONLINE,
        EMV_TERMINAL_PROFILE_BROAD_MOBILE,
        EMV_TERMINAL_PROFILE_APPLE_TRANSIT,
        EMV_TERMINAL_PROFILE_ONLINE_NO_ODA,
        EMV_TERMINAL_PROFILE_MINIMAL_ONLINE,
        EMV_TERMINAL_PROFILE_MSD_QVSDC,
    };
    const uint8_t *source = visa;
    if ((m_request.behavior & EMV_TRACE_BEHAVIOR_ADAPTIVE_PROFILES) != 0u) {
        emv_scheme_t scheme = aid_scheme(aid);
        source = scheme == EMV_SCHEME_MASTERCARD ? mastercard :
                 scheme == EMV_SCHEME_VISA ? visa : other;
    }
    memcpy(profiles, source, sizeof(visa));
}

static bool activate(picc_14a_tag_t *tag, bool poll) {
    m_stage = STAGE_ACTIVATION;
    pcd_14a_reader_antenna_off();
    bsp_delay_ms(5);
    pcd_14a_reader_reset();
    pcd_14a_reader_antenna_on();
    bsp_delay_ms(8);

    uint8_t status = STATUS_HF_TAG_NO;
    uint16_t attempts = 0;
    do {
        memset(tag, 0, sizeof(*tag));
        status = pcd_14a_reader_scan_auto(tag);
        if (status == STATUS_HF_TAG_OK) break;
        bsp_wdt_feed();
        bsp_delay_ms(20);
    } while (poll && ++attempts < 150u &&
             (m_timer == NULL || elapsed_ms() < 3000u));

    if (status != STATUS_HF_TAG_OK || (tag->sak & 0x20u) == 0u || tag->ats_len < 2u) {
        m_trace.result_status = status == STATUS_HF_TAG_OK ? STATUS_HF_ERR_ATS : status;
        return false;
    }
    bsp_delay_ms(5);
    write_register_single(CommandReg, PCD_IDLE);
    write_register_single(ComIrqReg, 0x7F);
    set_register_mask(FIFOLevelReg, 0x80);
    clear_register_mask(BitFramingReg, 0x80);
    iso_dep_reader_init(&m_reader, tag);
    return true;
}

static void record_apdu(const uint8_t *command, uint16_t command_len,
                        const uint8_t *response, uint16_t response_len,
                        const iso_dep_result_t *exchange) {
    uint16_t payload_len = (uint16_t)(6u + command_len + response_len);
    if (payload_len > 1030u) return;
    static uint8_t payload[1030];
    uint16_t sw = response_len >= 2u ?
                  ((uint16_t)response[response_len - 2u] << 8) | response[response_len - 1u] : UINT16_MAX;
    put_u16(&payload[0], sw);
    put_u16(&payload[2], command_len);
    put_u16(&payload[4], response_len);
    memcpy(&payload[6], command, command_len);
    if (response_len > 0u) memcpy(&payload[6u + command_len], response, response_len);
    uint8_t flags = response_len >= 2u ? 0x01u : 0u;
    if (sw == 0x9000u) flags |= 0x02u;
    if (exchange->error != ISO_DEP_OK) flags |= 0x04u;
    append_record(RECORD_APDU, m_stage, m_app_index, m_attempt, flags,
                  exchange->error, payload, payload_len, false);
}

static bool raw_exchange(const uint8_t *command, uint16_t command_len,
                         uint8_t *response, uint16_t *response_len) {
    if (!budget_available()) return false;
    iso_dep_result_t exchange;
    m_trace.apdu_count++;
    bool ok = iso_dep_reader_transceive(&m_reader, command, command_len,
                                        response, ISO_DEP_READER_MAX_APDU_RESPONSE,
                                        &exchange);
    *response_len = exchange.response_len;
    record_apdu(command, command_len, response, *response_len, &exchange);
    if (!ok) m_trace.flags |= TRACE_FLAG_TRANSPORT_ERROR;
    return ok;
}

static bool exchange_apdu(const uint8_t *command, uint16_t command_len,
                          uint8_t *logical, uint16_t *logical_len) {
    static uint8_t command_copy[260];
    if (command_len > sizeof(command_copy)) return false;
    memcpy(command_copy, command, command_len);
    static uint8_t response[ISO_DEP_READER_MAX_APDU_RESPONSE];
    uint16_t response_len = 0;
    if (!raw_exchange(command_copy, command_len, response, &response_len) || response_len < 2u) return false;

    if (response[response_len - 2u] == 0x6Cu && command_len >= 5u) {
        command_copy[command_len - 1u] = response[response_len - 1u];
        m_attempt++;
        if (!raw_exchange(command_copy, command_len, response, &response_len) || response_len < 2u) return false;
    }

    uint16_t assembled = 0;
    uint8_t get_response_guard = 0;
    while (response_len >= 2u && response[response_len - 2u] == 0x61u &&
            get_response_guard++ < 4u) {
        uint16_t data_len = response_len - 2u;
        if (assembled + data_len > ISO_DEP_READER_MAX_APDU_RESPONSE - 2u) {
            m_trace.flags |= TRACE_FLAG_RESPONSE_TRUNCATED;
            return false;
        }
        memcpy(&logical[assembled], response, data_len);
        assembled += data_len;
        uint8_t get_response[] = {0x00, 0xC0, 0x00, 0x00, response[response_len - 1u]};
        m_stage = STAGE_GET_RESPONSE;
        m_attempt++;
        if (!raw_exchange(get_response, sizeof(get_response), response, &response_len) || response_len < 2u) return false;
    }
    if (assembled + response_len > ISO_DEP_READER_MAX_APDU_RESPONSE) {
        m_trace.flags |= TRACE_FLAG_RESPONSE_TRUNCATED;
        return false;
    }
    memcpy(&logical[assembled], response, response_len);
    *logical_len = assembled + response_len;
    return true;
}

static bool sw_success(const uint8_t *response, uint16_t length) {
    return length >= 2u && response[length - 2u] == 0x90u && response[length - 1u] == 0x00u;
}

static uint16_t fill_dol(const uint8_t *dol, uint16_t dol_len, uint8_t *out,
                         uint16_t capacity) {
    uint16_t i = 0;
    uint16_t used = 0;
    while (i < dol_len) {
        uint32_t tag = dol[i++];
        if ((tag & 0x1Fu) == 0x1Fu) {
            uint8_t count = 0;
            do {
                if (i >= dol_len || count++ >= 3u) return 0;
                tag = (tag << 8) | dol[i];
            } while ((dol[i++] & 0x80u) != 0u);
        }
        if (i >= dol_len) return 0;
        uint8_t length = dol[i++];
        if (used + length > capacity) return 0;
        memset(&out[used], 0, length);
        const uint8_t *source = NULL;
        uint8_t source_len = 0;
        uint8_t unpredictable[4] = {(uint8_t)rand(), (uint8_t)rand(),
                                    (uint8_t)rand(), (uint8_t)rand()};
        static const uint8_t amount_other[6] = {0};
        static const uint8_t tvr[5] = {0};
        static const uint8_t cvm_results[3] = {0x3F, 0x00, 0x00};
        static const uint8_t floor_limit[4] = {0};
        static const uint8_t additional_capabilities[5] = {0};
        static const uint8_t merchant_category[2] = {0x41, 0x11};
        static const uint8_t merchant_id[15] = "CHAMELEON-LAB  ";
        static const uint8_t merchant_name[20] = "CHAMELEON TRANSIT   ";
        static const uint8_t transaction_category = 0x52;
        uint8_t sequence[4] = {(uint8_t)rand(), (uint8_t)rand(),
                               (uint8_t)rand(), (uint8_t)rand()};
        switch (tag) {
            case 0x9F02: source = m_request.amount; source_len = 6; break;
            case 0x9F03: source = amount_other; source_len = 6; break;
            case 0x9F1A: source = m_request.country; source_len = 2; break;
            case 0x5F2A: source = m_request.currency; source_len = 2; break;
            case 0x95: source = tvr; source_len = 5; break;
            case 0x9A: source = m_request.date; source_len = 3; break;
            case 0x9C: source = &m_request.transaction_type; source_len = 1; break;
            case 0x9F35: source = &m_active_terminal_type; source_len = 1; break;
            case 0x9F37: source = unpredictable; source_len = 4; break;
            case 0x9F66: source = m_active_ttq; source_len = 4; break;
            case 0x9F33: source = m_active_terminal_capabilities; source_len = 3; break;
            case 0x9F40: source = additional_capabilities; source_len = 5; break;
            case 0x9F34: source = cvm_results; source_len = 3; break;
            case 0x9F1B: source = floor_limit; source_len = 4; break;
            case 0x9F15: source = merchant_category; source_len = 2; break;
            case 0x9F16: source = merchant_id; source_len = 15; break;
            case 0x9F4E: source = merchant_name; source_len = 20; break;
            case 0x9F41: source = sequence; source_len = 4; break;
            case 0x9F53: source = &transaction_category; source_len = 1; break;
            default: break;
        }
        if (source != NULL) {
            uint8_t copy = source_len < length ? source_len : length;
            memcpy(&out[used + length - copy], &source[source_len - copy], copy);
        }
        used += length;
    }
    return used;
}

static bool select_application(const emv_aid_t *aid, uint8_t *response,
                               uint16_t *response_len) {
    uint8_t command[22] = {0x00, 0xA4, 0x04, 0x00};
    command[4] = aid->aid_len;
    memcpy(&command[5], aid->aid, aid->aid_len);
    command[5u + aid->aid_len] = 0x00;
    m_stage = STAGE_SELECT_APP;
    m_attempt = 0;
    return exchange_apdu(command, (uint16_t)(6u + aid->aid_len), response, response_len) &&
           sw_success(response, *response_len);
}

static void record_application(const emv_aid_t *aid) {
    uint8_t payload[18];
    payload[0] = aid->aid_len;
    memcpy(&payload[1], aid->aid, aid->aid_len);
    payload[1u + aid->aid_len] = aid->priority;
    append_record(RECORD_APP, STAGE_SELECT_APP, m_app_index, 0, 0,
                  STATUS_SUCCESS, payload, (uint16_t)(2u + aid->aid_len), false);
}

static void probe_direct_aids(emv_aid_t *aids, uint8_t *count) {
    static const emv_aid_t candidates[] = {
        {{0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10}, 7, 0}, /* Visa */
        {{0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10}, 7, 0}, /* Mastercard */
        {{0xA0, 0x00, 0x00, 0x00, 0x04, 0x30, 0x60}, 7, 0}, /* Maestro */
        {{0xA0, 0x00, 0x00, 0x00, 0x25, 0x01}, 6, 0},       /* Amex */
        {{0xA0, 0x00, 0x00, 0x01, 0x52, 0x30, 0x10}, 7, 0}, /* Discover */
        {{0xA0, 0x00, 0x00, 0x00, 0x65, 0x10, 0x10}, 7, 0}, /* JCB */
        {{0xA0, 0x00, 0x00, 0x03, 0x33, 0x01, 0x01, 0x01}, 8, 0}, /* UnionPay */
        {{0xA0, 0x00, 0x00, 0x02, 0x77, 0x10, 0x10}, 7, 0}, /* Interac */
    };
    static uint8_t response[ISO_DEP_READER_MAX_APDU_RESPONSE];
    for (uint8_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]) &&
            *count < m_request.max_aids && budget_available(); i++) {
        if (aid_exists(aids, *count, candidates[i].aid, candidates[i].aid_len)) continue;
        uint16_t response_len = 0;
        m_app_index = (uint8_t)(*count + 1u);
        if (select_application(&candidates[i], response, &response_len)) {
            aids[(*count)++] = candidates[i];
        }
    }
    m_app_index = 0;
}

static void get_standard_data(void) {
    static const uint16_t tags[] = {0x9F36, 0x9F13, 0x9F17, 0x9F4F};
    static uint8_t response[ISO_DEP_READER_MAX_APDU_RESPONSE];
    uint16_t response_len;
    for (uint8_t i = 0; i < sizeof(tags) / sizeof(tags[0]) && budget_available(); i++) {
        uint8_t command[] = {0x80, 0xCA, (uint8_t)(tags[i] >> 8), (uint8_t)tags[i], 0x00};
        m_stage = STAGE_GET_DATA;
        m_attempt = i;
        (void)exchange_apdu(command, sizeof(command), response, &response_len);
    }
}

static void read_transaction_log(const uint8_t *fci, uint16_t fci_len) {
    if ((m_request.options & EMV_TRACE_OPT_TRANSACTION_LOG) == 0u || fci_len < 2u) return;
    const uint8_t *entry;
    uint16_t entry_len;
    if (!find_tlv(fci, fci_len - 2u, 0x9F4Du, &entry, &entry_len, 0) ||
            entry_len != 2u) return;
    uint8_t sfi = entry[0] & 0x1Fu;
    uint8_t count = entry[1];
    if (sfi == 0u || sfi > 30u) return;
    if (count > m_request.max_records) count = m_request.max_records;

    static uint8_t response[ISO_DEP_READER_MAX_APDU_RESPONSE];
    uint16_t response_len;
    for (uint8_t record = 1; record <= count && budget_available(); record++) {
        uint8_t command[] = {0x00, 0xB2, record,
                             (uint8_t)((sfi << 3) | 4u), 0x00};
        m_stage = STAGE_LOG;
        m_attempt = record;
        if (!exchange_apdu(command, sizeof(command), response, &response_len) ||
                !sw_success(response, response_len)) break;
    }
}

static uint8_t scan_records(bool grid, uint8_t stage, uint8_t *cdol,
                            uint16_t *cdol_len) {
    uint8_t successful = 0;
    static uint8_t response[ISO_DEP_READER_MAX_APDU_RESPONSE];
    uint16_t response_len;
    for (uint8_t sfi = 1; sfi <= 30u && successful < m_request.max_records; sfi++) {
        bool found_in_sfi = false;
        for (uint8_t record = 1; record <= 16u && successful < m_request.max_records; record++) {
            if (!budget_available()) return successful;
            uint8_t command[] = {0x00, 0xB2, record, (uint8_t)((sfi << 3) | 4u), 0x00};
            m_stage = stage;
            m_attempt = record;
            if (!exchange_apdu(command, sizeof(command), response, &response_len)) return successful;
            if (sw_success(response, response_len)) {
                found_in_sfi = true;
                successful++;
                if (*cdol_len == 0u) {
                    const uint8_t *value;
                    uint16_t value_len;
                    if (find_tlv(response, response_len - 2u, 0x8Cu,
                                 &value, &value_len, 0) && value_len <= 128u) {
                        memcpy(cdol, value, value_len);
                        *cdol_len = value_len;
                    }
                }
            } else if (!grid && (found_in_sfi || record == 1u)) {
                break;
            }
        }
    }
    return successful;
}

static uint8_t read_afl(const uint8_t *gpo, uint16_t gpo_len,
                        uint8_t *cdol, uint16_t *cdol_len) {
    const uint8_t *afl = NULL;
    uint16_t afl_len = 0;
    if (gpo_len < 4u || !sw_success(gpo, gpo_len)) return 0;
    if (gpo[0] == 0x77u) {
        (void)find_tlv(gpo, gpo_len - 2u, 0x94u, &afl, &afl_len, 0);
    } else if (gpo[0] == 0x80u) {
        uint16_t offset = 0;
        tlv_item_t item;
        if (tlv_next(gpo, gpo_len - 2u, &offset, &item) &&
                tag_equals(&item, 0x80u) && item.value_len >= 2u) {
            afl = &item.value[2];
            afl_len = item.value_len - 2u;
        }
    }
    if (afl == NULL || afl_len == 0u || (afl_len & 3u) != 0u) return 0;

    uint8_t successful = 0;
    static uint8_t response[ISO_DEP_READER_MAX_APDU_RESPONSE];
    uint16_t response_len;
    for (uint16_t i = 0; i < afl_len && successful < m_request.max_records; i += 4u) {
        uint8_t sfi = afl[i] >> 3;
        uint8_t first = afl[i + 1u];
        uint8_t last = afl[i + 2u];
        uint8_t oda = afl[i + 3u];
        if ((afl[i] & 7u) != 0u || sfi == 0u || sfi > 30u || first == 0u ||
                last < first || oda > (uint8_t)(last - first + 1u)) continue;
        for (uint16_t record = first; record <= last && successful < m_request.max_records; record++) {
            if (!budget_available()) return successful;
            uint8_t command[] = {0x00, 0xB2, (uint8_t)record,
                                 (uint8_t)((sfi << 3) | 4u), 0x00};
            m_stage = STAGE_READ_AFL;
            m_attempt = (uint8_t)record;
            if (!exchange_apdu(command, sizeof(command), response, &response_len)) return successful;
            if (!sw_success(response, response_len)) continue;
            successful++;
            if (*cdol_len == 0u) {
                const uint8_t *value;
                uint16_t value_len;
                if (find_tlv(response, response_len - 2u, 0x8Cu,
                             &value, &value_len, 0) && value_len <= 128u) {
                    memcpy(cdol, value, value_len);
                    *cdol_len = value_len;
                }
            }
        }
    }
    return successful;
}

static bool profile_retry_status(const uint8_t *response, uint16_t response_len) {
    if (response_len < 2u) return false;
    uint16_t sw = ((uint16_t)response[response_len - 2u] << 8) |
                  response[response_len - 1u];
    return sw == 0x6985u || sw == 0x6986u || sw == 0x6A80u;
}

static bool process_application(const emv_aid_t *aid, bool maximum,
                                picc_14a_tag_t *tag) {
    static uint8_t response[ISO_DEP_READER_MAX_APDU_RESPONSE];
    uint16_t response_len = 0;
    static uint8_t fci[ISO_DEP_READER_MAX_APDU_RESPONSE];
    static uint8_t cdol[128];
    uint16_t cdol_len = 0;

    record_application(aid);
    if (!select_application(aid, response, &response_len)) return true;
    uint16_t fci_len = response_len;
    memcpy(fci, response, response_len);

    if (!maximum) {
        get_standard_data();
        read_transaction_log(fci, fci_len);
        (void)scan_records((m_request.options & EMV_TRACE_OPT_RECORD_GRID) != 0u,
                           STAGE_READ_SCAN, cdol, &cdol_len);
        return true;
    }

    uint8_t sweep_profiles[6];
    sweep_order(aid, sweep_profiles);
    uint8_t requested_profile = m_request.terminal_profile;
    uint8_t profile_count = requested_profile == EMV_TERMINAL_PROFILE_SWEEP ?
                            sizeof(sweep_profiles) : 1u;
    static uint8_t pdol_data[128];
    static uint8_t gpo[136];
    uint16_t data_len = 0;
    bool gpo_ok = false;
    for (uint8_t profile_index = 0; profile_index < profile_count; profile_index++) {
        if (profile_index > 0u) {
            if ((m_request.behavior & EMV_TRACE_BEHAVIOR_REACQUIRE_PROFILES) != 0u) {
                iso_dep_reader_deselect(&m_reader);
                if (!activate(tag, true)) return false;
                if (!emv_trace_same_uid(m_trace.tag.uid, m_trace.tag.uid_len,
                                        tag->uid, tag->uid_len)) {
                    m_trace.result_status = STATUS_HF_ERR_STAT;
                    return false;
                }
            }
            if (!select_application(aid, response, &response_len)) break;
            fci_len = response_len;
            memcpy(fci, response, response_len);
        }
        uint8_t profile = requested_profile == EMV_TERMINAL_PROFILE_SWEEP ?
                          sweep_profiles[profile_index] : requested_profile;
        terminal_profile_select(profile);

        const uint8_t *pdol = NULL;
        uint16_t pdol_len = 0;
        (void)find_tlv(fci, fci_len >= 2u ? fci_len - 2u : 0u,
                       0x9F38u, &pdol, &pdol_len, 0);
        data_len = pdol == NULL ? 0u :
                   fill_dol(pdol, pdol_len, pdol_data, sizeof(pdol_data));
        if (pdol != NULL && data_len == 0u) return true;

        uint16_t gpo_len = 0;
        gpo[gpo_len++] = 0x80; gpo[gpo_len++] = 0xA8;
        gpo[gpo_len++] = 0x00; gpo[gpo_len++] = 0x00;
        gpo[gpo_len++] = (uint8_t)(data_len + 2u);
        gpo[gpo_len++] = 0x83; gpo[gpo_len++] = (uint8_t)data_len;
        if (data_len > 0u) memcpy(&gpo[gpo_len], pdol_data, data_len);
        gpo_len += data_len;
        gpo[gpo_len++] = 0x00;
        m_stage = STAGE_GPO;
        m_attempt = profile_index;
        bool exchanged = exchange_apdu(gpo, gpo_len, response, &response_len);
        gpo_ok = exchanged && sw_success(response, response_len);
        if (gpo_ok || !exchanged ||
                !profile_retry_status(response, response_len)) break;
    }
    if (!gpo_ok && data_len > 0u &&
            (m_request.options & EMV_TRACE_OPT_EXPRESS_TRANSIT) == 0u &&
            (m_request.options & EMV_TRACE_OPT_PDOL_FALLBACK) != 0u) {
        static const uint8_t empty_gpo[] = {0x80, 0xA8, 0x00, 0x00,
                                            0x02, 0x83, 0x00, 0x00};
        m_stage = STAGE_GPO;
        m_attempt = 1;
        gpo_ok = exchange_apdu(empty_gpo, sizeof(empty_gpo), response, &response_len) &&
                 sw_success(response, response_len);
    }
    if (!gpo_ok) return true;

    const uint8_t *application_cryptogram;
    uint16_t application_cryptogram_len;
    bool gpo_has_cryptogram = find_tlv(response, response_len - 2u, 0x9F26u,
                                       &application_cryptogram,
                                       &application_cryptogram_len, 0) &&
                              application_cryptogram_len == 8u;

    uint8_t records = read_afl(response, response_len, cdol, &cdol_len);
    if (records == 0u) {
        (void)scan_records((m_request.options & EMV_TRACE_OPT_RECORD_GRID) != 0u,
                           STAGE_READ_SCAN, cdol, &cdol_len);
    }

    bool visa = aid->aid_len >= 5u &&
                memcmp(aid->aid, "\xA0\x00\x00\x00\x03", 5) == 0;
    if ((!visa || !gpo_has_cryptogram) && cdol_len > 0u &&
            m_request.cryptogram_type != 0xFFu) {
        static uint8_t cdol_data[192];
        uint16_t cdol_data_len = fill_dol(cdol, cdol_len, cdol_data, sizeof(cdol_data));
        if (cdol_data_len > 0u && cdol_data_len <= 180u) {
            static uint8_t gac[190];
            uint16_t gac_len = 0;
            gac[gac_len++] = 0x80; gac[gac_len++] = 0xAE;
            gac[gac_len++] = m_request.cryptogram_type & 0xC0u;
            gac[gac_len++] = 0x00;
            gac[gac_len++] = (uint8_t)cdol_data_len;
            memcpy(&gac[gac_len], cdol_data, cdol_data_len);
            gac_len += cdol_data_len;
            gac[gac_len++] = 0x00;
            m_stage = STAGE_GENERATE_AC;
            m_attempt = 0;
            (void)exchange_apdu(gac, gac_len, response, &response_len);
        }
    }
    get_standard_data();
    read_transaction_log(fci, fci_len);
    return true;
}

static uint32_t trace_crc32(void) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < m_trace.stored_bytes; i++) {
        crc ^= m_trace.arena[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return ~crc;
}

static bool parse_request(const uint8_t *data, uint16_t length) {
    if (data == NULL || length < 2u || data[0] != EMV_TRACE_PROTOCOL_VERSION) return false;
    bool has_profile = (data[1] & EMV_TRACE_OPT_TERMINAL_PROFILE) != 0u;
    if ((!has_profile && length != EMV_TRACE_START_REQUEST_SIZE) ||
            (has_profile && length != EMV_TRACE_START_REQUEST_PROFILE_SIZE &&
             length != EMV_TRACE_START_REQUEST_BEHAVIOR_SIZE)) return false;
    memset(&m_request, 0, sizeof(m_request));
    m_request.options = data[1];
    m_request.max_aids = data[2] == 0u ? 8u : data[2];
    m_request.max_records = data[3] == 0u ? 32u : data[3];
    m_request.max_apdus = get_u16(&data[4]);
    if (m_request.max_apdus == 0u) m_request.max_apdus = 128u;
    m_request.budget_ms = get_u32(&data[6]);
    if (m_request.budget_ms == 0u) m_request.budget_ms = 12000u;
    memcpy(m_request.amount, &data[10], 6);
    memcpy(m_request.country, &data[16], 2);
    memcpy(m_request.currency, &data[18], 2);
    memcpy(m_request.date, &data[20], 3);
    m_request.transaction_type = data[23];
    m_request.cryptogram_type = data[24];
    m_request.terminal_profile = EMV_TERMINAL_PROFILE_AUTO;
    if (has_profile) {
        m_request.terminal_profile = data[25];
        memcpy(m_request.custom_ttq, &data[26], sizeof(m_request.custom_ttq));
        if (!terminal_profile_valid(m_request.terminal_profile)) return false;
        if (length == EMV_TRACE_START_REQUEST_BEHAVIOR_SIZE) {
            m_request.polling_profile = data[30];
            m_request.behavior = data[31];
            m_request.poll_retries = data[32];
            m_request.poll_delay_ms = data[33];
            m_request.poll_timeout_ms = data[34];
            if (m_request.polling_profile > EMV_POLLING_PROFILE_PATIENT ||
                    (m_request.behavior & ~EMV_TRACE_BEHAVIOR_ALL) != 0u ||
                    m_request.poll_retries > 100u ||
                    m_request.poll_delay_ms > 20u ||
                    m_request.poll_timeout_ms > 10u) return false;
        }
    }
    return m_request.max_aids <= TRACE_MAX_AIDS &&
           m_request.max_records <= TRACE_MAX_RECORDS_APP &&
           m_request.max_apdus <= TRACE_MAX_APDUS &&
           m_request.budget_ms <= 30000u &&
           (m_request.cryptogram_type == 0xFFu ||
            m_request.cryptogram_type == 0x00u ||
            m_request.cryptogram_type == 0x40u ||
            m_request.cryptogram_type == 0x80u);
}

uint16_t emv_trace_start(const uint8_t *request, uint16_t request_length,
                         uint8_t *response, uint16_t response_capacity,
                         uint16_t *status) {
    if (status == NULL || response == NULL || response_capacity < 10u ||
            !parse_request(request, request_length)) {
        if (status != NULL) *status = STATUS_PAR_ERR;
        return 0;
    }

    uint32_t next_scan_id = m_trace.scan_id + 1u;
    if (next_scan_id == 0u) next_scan_id = 1u;
    memset(&m_trace, 0, sizeof(m_trace));
    m_trace.scan_id = next_scan_id;
    m_trace.state = TRACE_STATE_RUNNING;
    m_trace.result_status = STATUS_HF_TAG_NO;
    m_trace.first_dropped = UINT32_MAX;
    if (m_request.options & EMV_TRACE_OPT_TIMING) m_trace.flags |= TRACE_FLAG_TIMING_VALID;
    if (m_request.options & EMV_TRACE_OPT_MAX_PROCESSING) m_trace.flags |= TRACE_FLAG_MAX_PROCESSING;
    if (m_request.options & EMV_TRACE_OPT_EXPRESS_TRANSIT) m_trace.flags |= TRACE_FLAG_EXPRESS_TRANSIT;
    m_store_disabled = false;
    m_stage = STAGE_ACTIVATION;
    m_app_index = 0;
    m_attempt = 0;
    terminal_profile_select(m_request.terminal_profile);
    m_timer = bsp_obtain_timer(0);
    pcd_14a_reader_trace_set(rf_trace_callback);

    static const uint8_t ecp2_tfl[] = {
        0x6A, 0x02, 0xC8, 0x01, 0x00, 0x03, 0x00, 0x02,
        0x79, 0x00, 0x00, 0x00, 0x00, 0xC2, 0xD8
    };
    if ((m_request.options & EMV_TRACE_OPT_EXPRESS_TRANSIT) != 0u) {
        uint8_t retries;
        uint8_t delay_ms;
        uint8_t timeout_ms;
        polling_profile_values(&retries, &delay_ms, &timeout_ms);
        if (!pcd_14a_reader_polling_annotation_set(ecp2_tfl, sizeof(ecp2_tfl)) ||
                !pcd_14a_reader_polling_annotation_timing_set(
                    retries, delay_ms, timeout_ms)) {
            m_trace.result_status = STATUS_PAR_ERR;
            m_trace.state = TRACE_STATE_ABORTED;
            goto finish;
        }
    }

    static picc_14a_tag_t tag;
    if (!activate(&tag, true)) {
        m_trace.state = TRACE_STATE_ABORTED;
        goto finish;
    }
    memcpy(&m_trace.tag, &tag, sizeof(tag));
    m_trace.result_status = STATUS_HF_TAG_OK;

    static const uint8_t ppse[] = {
        0x00, 0xA4, 0x04, 0x00, 0x0E,
        0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
        0x44, 0x44, 0x46, 0x30, 0x31, 0x00
    };
    static uint8_t ppse_response[ISO_DEP_READER_MAX_APDU_RESPONSE];
    uint16_t ppse_len = 0;
    m_stage = STAGE_PPSE;
    bool ppse_exchanged = exchange_apdu(ppse, sizeof(ppse),
                                        ppse_response, &ppse_len);
    bool ppse_ok = ppse_exchanged && sw_success(ppse_response, ppse_len);
    if (!ppse_ok &&
            (m_request.behavior & EMV_TRACE_BEHAVIOR_DIRECT_AID_FALLBACK) == 0u) {
        m_trace.result_status = STATUS_HF_ERR_STAT;
        m_trace.state = TRACE_STATE_ABORTED;
        goto finish;
    }

    static emv_aid_t aids[TRACE_MAX_AIDS];
    memset(aids, 0, sizeof(aids));
    uint8_t aid_count = 0;
    if (ppse_ok) {
        collect_templates(ppse_response, ppse_len - 2u, aids, &aid_count, 0);
    }
    if (aid_count == 0u &&
            (m_request.behavior & EMV_TRACE_BEHAVIOR_DIRECT_AID_FALLBACK) != 0u) {
        probe_direct_aids(aids, &aid_count);
    }
    sort_aids(aids, aid_count);
    if (aid_count == m_request.max_aids) m_trace.flags |= TRACE_FLAG_APP_LIMIT;
    m_trace.application_count = aid_count;

    bool maximum = (m_request.options & EMV_TRACE_OPT_MAX_PROCESSING) != 0u;
    for (uint8_t i = 0; i < aid_count && budget_available(); i++) {
        m_app_index = i + 1u;
        if (!process_application(&aids[i], maximum, &tag)) {
            m_trace.state = TRACE_STATE_ABORTED;
            goto finish;
        }
    }
    m_trace.state = TRACE_STATE_COMPLETE;
    m_trace.flags |= TRACE_FLAG_COMPLETE;

finish:
    iso_dep_reader_deselect(&m_reader);
    pcd_14a_reader_polling_annotation_clear();
    pcd_14a_reader_trace_clear();
    pcd_14a_reader_timeout_set(DEF_COM_TIMEOUT);
    pcd_14a_reader_antenna_off();
    m_trace.elapsed_ms = elapsed_ms();
    if (m_timer != NULL) {
        bsp_return_timer(m_timer);
        m_timer = NULL;
    }
    uint8_t summary[12];
    put_u32(&summary[0], m_trace.stored_records);
    put_u32(&summary[4], m_trace.observed_records);
    put_u32(&summary[8], m_trace.flags);
    append_record(RECORD_SUMMARY, 0xFFu, 0, 0, 0,
                  m_trace.result_status, summary, sizeof(summary), false);

    response[0] = EMV_TRACE_PROTOCOL_VERSION;
    response[1] = m_trace.state;
    put_u32(&response[2], m_trace.scan_id);
    put_u32(&response[6], m_trace.flags);
    *status = m_trace.tag.uid_len > 0u ? STATUS_HF_TAG_OK : STATUS_HF_TAG_NO;
    return 10u;
}

uint16_t emv_trace_meta(const uint8_t *request, uint16_t request_length,
                        uint8_t *response, uint16_t response_capacity,
                        uint16_t *status) {
    if (status == NULL || response == NULL || request == NULL || request_length != 5u ||
            request[0] != EMV_TRACE_PROTOCOL_VERSION || get_u32(&request[1]) != m_trace.scan_id ||
            m_trace.state == TRACE_STATE_EMPTY) {
        if (status != NULL) *status = STATUS_PAR_ERR;
        return 0;
    }
    uint16_t needed = (uint16_t)(47u + m_trace.tag.uid_len + m_trace.tag.ats_len);
    if (response_capacity < needed) {
        *status = STATUS_MEM_ERR;
        return 0;
    }
    uint16_t o = 0;
    response[o++] = EMV_TRACE_PROTOCOL_VERSION;
    response[o++] = m_trace.state;
    put_u16(&response[o], m_trace.result_status); o += 2;
    put_u32(&response[o], m_trace.flags); o += 4;
    put_u32(&response[o], m_trace.scan_id); o += 4;
    put_u32(&response[o], m_trace.stored_records); o += 4;
    put_u32(&response[o], m_trace.observed_records); o += 4;
    put_u32(&response[o], m_trace.stored_bytes); o += 4;
    put_u32(&response[o], m_trace.required_bytes); o += 4;
    put_u32(&response[o], m_trace.first_dropped); o += 4;
    put_u32(&response[o], trace_crc32()); o += 4;
    put_u16(&response[o], m_trace.application_count); o += 2;
    put_u32(&response[o], m_trace.elapsed_ms); o += 4;
    response[o++] = m_trace.tag.uid_len;
    memcpy(&response[o], m_trace.tag.uid, m_trace.tag.uid_len); o += m_trace.tag.uid_len;
    memcpy(&response[o], m_trace.tag.atqa, 2); o += 2;
    response[o++] = m_trace.tag.sak;
    response[o++] = m_trace.tag.ats_len;
    memcpy(&response[o], m_trace.tag.ats, m_trace.tag.ats_len); o += m_trace.tag.ats_len;
    *status = STATUS_SUCCESS;
    return o;
}

uint16_t emv_trace_get(const uint8_t *request, uint16_t request_length,
                       uint8_t *response, uint16_t response_capacity,
                       uint16_t *status) {
    if (status == NULL || response == NULL || request == NULL || request_length != 11u ||
            request[0] != EMV_TRACE_PROTOCOL_VERSION || get_u32(&request[1]) != m_trace.scan_id ||
            m_trace.state == TRACE_STATE_EMPTY) {
        if (status != NULL) *status = STATUS_PAR_ERR;
        return 0;
    }
    uint32_t start = get_u32(&request[5]);
    uint16_t requested = get_u16(&request[9]);
    if (requested == 0u || requested > response_capacity) requested = response_capacity;
    if (requested < 18u || start > m_trace.stored_records) {
        *status = STATUS_PAR_ERR;
        return 0;
    }

    uint32_t byte_offset = 0;
    uint32_t index = 0;
    while (index < start && byte_offset + 2u <= m_trace.stored_bytes) {
        uint16_t length = get_u16(&m_trace.arena[byte_offset]);
        byte_offset += (uint32_t)length + 2u;
        index++;
    }
    if (index != start || byte_offset > m_trace.stored_bytes) {
        *status = STATUS_CMD_ERR;
        return 0;
    }

    uint16_t data_len = 0;
    uint16_t count = 0;
    uint32_t cursor = byte_offset;
    while (index + count < m_trace.stored_records && cursor + 2u <= m_trace.stored_bytes) {
        uint16_t length = get_u16(&m_trace.arena[cursor]);
        uint32_t total = (uint32_t)length + 2u;
        if (cursor + total > m_trace.stored_bytes || 18u + data_len + total > requested) break;
        memcpy(&response[18u + data_len], &m_trace.arena[cursor], total);
        data_len += total;
        cursor += total;
        count++;
    }
    if (count == 0u && start < m_trace.stored_records) {
        *status = STATUS_PAR_ERR;
        return 0;
    }
    uint32_t next = start + count;
    response[0] = EMV_TRACE_PROTOCOL_VERSION;
    response[1] = (next < m_trace.stored_records ? 0x01u : 0x02u) |
                  ((m_trace.flags & TRACE_FLAG_LOG_TRUNCATED) ? 0x04u : 0u);
    put_u32(&response[2], m_trace.scan_id);
    put_u32(&response[6], start);
    put_u32(&response[10], next);
    put_u16(&response[14], count);
    put_u16(&response[16], data_len);
    *status = STATUS_SUCCESS;
    return (uint16_t)(18u + data_len);
}
