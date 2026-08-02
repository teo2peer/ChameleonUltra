#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_status.h"
#include "iso_dep_session.h"

static uint16_t m_timeout = DEF_COM_TIMEOUT;
static uint8_t m_scan_status = STATUS_HF_TAG_OK;
static picc_14a_tag_t m_scan_tag;
static iso_dep_reader_t *m_reader_address;
static bool m_exchange_ok = true;
static uint8_t m_exchange_count;
static uint8_t m_deselect_count;
static uint8_t m_antenna_off_count;
static uint8_t m_antenna_on_count;
static uint8_t m_reset_count;
static uint8_t m_scan_count;
static uint16_t m_delays[16];
static uint8_t m_delay_count;
static bool m_field_on;
static bool m_annotation_active;
static bool m_annotation_seen_by_scan;
static bool m_annotation_set_ok = true;
static bool m_annotation_timing_ok = true;
static uint8_t m_annotation_frame[32];
static uint8_t m_annotation_length;
static uint8_t m_annotation_retries;
static uint8_t m_annotation_delay_ms;
static uint8_t m_annotation_timeout_ms;
static uint8_t m_annotation_set_count;
static uint8_t m_annotation_timing_count;
static uint8_t m_annotation_clear_count;
static data_frame_transport_t m_transport;
static bool m_disconnect_during_scan;
static bool m_transport_generation_valid;

data_frame_transport_t data_frame_get_transport(void) {
    return m_transport;
}

bool data_frame_current_transport_generation_valid(void) {
    return m_transport_generation_valid;
}

void bsp_delay_ms(uint16_t delay_ms) {
    assert(m_delay_count < sizeof(m_delays) / sizeof(m_delays[0]));
    m_delays[m_delay_count++] = delay_ms;
}

void pcd_14a_reader_reset(void) {
    m_reset_count++;
}

void pcd_14a_reader_antenna_on(void) {
    m_antenna_on_count++;
    m_field_on = true;
}

void pcd_14a_reader_antenna_off(void) {
    m_antenna_off_count++;
    m_field_on = false;
}

uint16_t pcd_14a_reader_timeout_get(void) {
    return m_timeout;
}

void pcd_14a_reader_timeout_set(uint16_t timeout_ms) {
    m_timeout = timeout_ms;
}

uint8_t pcd_14a_reader_scan_auto(picc_14a_tag_t *tag) {
    m_scan_count++;
    m_annotation_seen_by_scan = m_annotation_active;
    if (m_disconnect_during_scan) {
        m_transport_generation_valid = false;
        iso_dep_session_owner_disconnected(m_transport);
    }
    *tag = m_scan_tag;
    return m_scan_status;
}

bool pcd_14a_reader_polling_annotation_set(const uint8_t *frame, uint8_t length) {
    m_annotation_set_count++;
    assert(frame != NULL && length <= sizeof(m_annotation_frame));
    memcpy(m_annotation_frame, frame, length);
    m_annotation_length = length;
    if (!m_annotation_set_ok) return false;
    m_annotation_active = true;
    return true;
}

bool pcd_14a_reader_polling_annotation_timing_set(uint8_t retries,
                                                   uint8_t delay_ms,
                                                   uint8_t timeout_ms) {
    m_annotation_timing_count++;
    m_annotation_retries = retries;
    m_annotation_delay_ms = delay_ms;
    m_annotation_timeout_ms = timeout_ms;
    return m_annotation_timing_ok;
}

void pcd_14a_reader_polling_annotation_clear(void) {
    m_annotation_clear_count++;
    m_annotation_active = false;
}

void iso_dep_reader_init(iso_dep_reader_t *reader, const picc_14a_tag_t *tag) {
    assert(tag->sak & 0x20u);
    memset(reader, 0, sizeof(*reader));
    reader->picc_fsc = 64u;
    m_reader_address = reader;
}

bool iso_dep_reader_transceive(iso_dep_reader_t *reader,
                               const uint8_t *apdu, uint16_t apdu_len,
                               uint8_t *response, uint16_t response_capacity,
                               iso_dep_result_t *result) {
    assert(reader == m_reader_address);
    assert(apdu != NULL && apdu_len > 0u);
    assert(response_capacity >= 2u);
    assert(reader->block_num == (m_exchange_count & 1u));
    m_exchange_count++;
    memset(result, 0, sizeof(*result));
    if (!m_exchange_ok) {
        result->error = ISO_DEP_ERR_CRC;
        result->rf_status = STATUS_HF_ERR_CRC;
        return false;
    }
    response[0] = 0x90u;
    response[1] = 0x00u;
    result->response_len = 2u;
    reader->block_num ^= 1u;
    return true;
}

void iso_dep_reader_deselect(iso_dep_reader_t *reader) {
    assert(reader == m_reader_address);
    m_deselect_count++;
}

static void reset_fixture(void) {
    iso_dep_session_abort();
    memset(&m_scan_tag, 0, sizeof(m_scan_tag));
    m_scan_tag.uid_len = 4u;
    memcpy(m_scan_tag.uid, "\x01\x02\x03\x04", 4u);
    m_scan_tag.atqa[0] = 0x04u;
    m_scan_tag.sak = 0x20u;
    m_scan_tag.ats_len = 2u;
    m_scan_tag.ats[0] = 0x02u;
    m_scan_tag.ats[1] = 0x08u;
    m_timeout = DEF_COM_TIMEOUT;
    m_scan_status = STATUS_HF_TAG_OK;
    m_reader_address = NULL;
    m_exchange_ok = true;
    m_exchange_count = 0u;
    m_deselect_count = 0u;
    m_antenna_off_count = 0u;
    m_antenna_on_count = 0u;
    m_reset_count = 0u;
    m_scan_count = 0u;
    m_delay_count = 0u;
    m_field_on = false;
    m_annotation_active = false;
    m_annotation_seen_by_scan = false;
    m_annotation_set_ok = true;
    m_annotation_timing_ok = true;
    m_annotation_length = 0u;
    m_annotation_retries = 0u;
    m_annotation_delay_ms = 0u;
    m_annotation_timeout_ms = 0u;
    m_annotation_set_count = 0u;
    m_annotation_timing_count = 0u;
    m_annotation_clear_count = 0u;
    m_transport = DATA_FRAME_TRANSPORT_USB;
    m_disconnect_during_scan = false;
    m_transport_generation_valid = true;
    memset(m_delays, 0, sizeof(m_delays));
    memset(m_annotation_frame, 0, sizeof(m_annotation_frame));
}

static uint32_t start_session(picc_14a_tag_t *tag) {
    uint32_t session_id = 0u;
    assert(iso_dep_session_start(tag, &session_id) == STATUS_HF_TAG_OK);
    assert(session_id != 0u);
    return session_id;
}

static void test_start_and_persistent_exchange(void) {
    reset_fixture();
    picc_14a_tag_t tag;
    uint32_t session_id = start_session(&tag);
    assert(iso_dep_session_is_active());
    assert(memcmp(&tag, &m_scan_tag, sizeof(tag)) == 0);
    assert(m_antenna_off_count == 1u);
    assert(m_antenna_on_count == 1u);
    assert(m_reset_count == 1u && m_scan_count == 1u);
    assert(m_delay_count == 2u && m_delays[0] == 5u && m_delays[1] == 8u);
    assert(m_timeout == DEF_COM_TIMEOUT);

    uint8_t response[8];
    iso_dep_result_t result;
    const uint8_t apdu[] = {0x00u, 0x84u, 0x00u, 0x00u, 0x08u};
    assert(iso_dep_session_exchange(session_id + 1u, apdu, sizeof(apdu),
                                    response, sizeof(response), &result) ==
           ISO_DEP_SESSION_EXCHANGE_INVALID);
    assert(m_exchange_count == 0u && iso_dep_session_is_active());
    assert(iso_dep_session_exchange(session_id, apdu, sizeof(apdu),
                                    response, sizeof(response), &result) ==
           ISO_DEP_SESSION_EXCHANGE_OK);
    assert(iso_dep_session_exchange(session_id, apdu, sizeof(apdu),
                                    response, sizeof(response), &result) ==
           ISO_DEP_SESSION_EXCHANGE_OK);
    assert(m_exchange_count == 2u && result.response_len == 2u);

    assert(!iso_dep_session_stop(session_id + 1u));
    assert(iso_dep_session_is_active() && m_deselect_count == 0u);
    assert(iso_dep_session_stop(session_id));
    assert(!iso_dep_session_is_active() && m_deselect_count == 1u);
    assert(!iso_dep_session_stop(session_id));
}

static void test_failure_aborts_but_idle_session_persists(void) {
    reset_fixture();
    picc_14a_tag_t tag;
    uint32_t session_id = start_session(&tag);
    uint8_t response[8];
    iso_dep_result_t result;
    const uint8_t apdu[] = {0x00u};
    m_exchange_ok = false;
    assert(iso_dep_session_exchange(session_id, apdu, sizeof(apdu),
                                    response, sizeof(response), &result) ==
           ISO_DEP_SESSION_EXCHANGE_RF_ERROR);
    assert(result.error == ISO_DEP_ERR_CRC);
    assert(!iso_dep_session_is_active());
    assert(m_antenna_off_count == 2u);

    reset_fixture();
    session_id = start_session(&tag);
    assert(iso_dep_session_is_active());
    assert(iso_dep_session_stop(session_id));
    assert(!iso_dep_session_is_active());
}

static void test_transport_ownership_and_disconnect_cleanup(void) {
    reset_fixture();
    picc_14a_tag_t tag;
    uint32_t session_id = start_session(&tag);
    uint8_t response[8];
    iso_dep_result_t result;
    const uint8_t apdu[] = {0x00u};

    m_transport = DATA_FRAME_TRANSPORT_BLE;
    assert(iso_dep_session_exchange(session_id, apdu, sizeof(apdu), response,
                                    sizeof(response), &result) ==
           ISO_DEP_SESSION_EXCHANGE_INVALID);
    assert(!iso_dep_session_stop(session_id));
    assert(iso_dep_session_is_active());

    iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_BLE);
    iso_dep_session_process();
    assert(iso_dep_session_is_active());

    iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_USB);
    assert(iso_dep_session_is_active());
    iso_dep_session_process();
    assert(!iso_dep_session_is_active());
    assert(!m_field_on);
}

static void test_disconnect_during_start_cannot_publish_orphan(void) {
    reset_fixture();
    picc_14a_tag_t tag;
    uint32_t session_id = 0u;
    m_transport = DATA_FRAME_TRANSPORT_BLE;
    m_disconnect_during_scan = true;

    assert(iso_dep_session_start(&tag, &session_id) == STATUS_CMD_ERR);
    assert(session_id == 0u);
    assert(!iso_dep_session_is_active());
    assert(!m_field_on);
}

static void test_stale_transport_generation_cannot_start(void) {
    reset_fixture();
    picc_14a_tag_t tag;
    uint32_t session_id = 0u;
    m_transport = DATA_FRAME_TRANSPORT_BLE;
    m_transport_generation_valid = false;

    assert(iso_dep_session_start(&tag, &session_id) == STATUS_CMD_ERR);
    assert(session_id == 0u);
    assert(!iso_dep_session_is_active());
    assert(m_scan_count == 0u);
    assert(!m_field_on);
}

static void test_start_replaces_session_and_rejects_bad_targets(void) {
    reset_fixture();
    picc_14a_tag_t tag;
    uint32_t first = start_session(&tag);
    uint32_t second = start_session(&tag);
    assert(second != 0u && second == first + 1u);
    assert(iso_dep_session_is_active());

    iso_dep_session_abort();
    m_scan_status = STATUS_HF_COLLISION;
    uint32_t unused = 0u;
    assert(iso_dep_session_start(&tag, &unused) == STATUS_HF_COLLISION);
    assert(!iso_dep_session_is_active());

    m_scan_status = STATUS_HF_TAG_OK;
    m_scan_tag.sak = 0x00u;
    assert(iso_dep_session_start(&tag, &unused) == STATUS_HF_ERR_ATS);
    assert(!iso_dep_session_is_active());
}

static void test_plain_start_clears_stale_annotation(void) {
    reset_fixture();
    m_annotation_active = true;
    m_timeout = 73u;
    picc_14a_tag_t tag;
    uint32_t session_id = 0u;
    assert(iso_dep_session_start(&tag, &session_id) == STATUS_HF_TAG_OK);
    assert(!m_annotation_seen_by_scan && !m_annotation_active);
    assert(m_annotation_clear_count == 1u);
    assert(m_timeout == 73u);
    assert(m_field_on && iso_dep_session_is_active());
}

static void test_apple_transit_annotation_and_cleanup(void) {
    static const uint8_t expected_frame[] = {
        0x6Au, 0x02u, 0xC8u, 0x01u, 0x00u, 0x03u, 0x00u, 0x02u,
        0x79u, 0x00u, 0x00u, 0x00u, 0x00u, 0xC2u, 0xD8u,
    };
    reset_fixture();
    m_timeout = 91u;
    picc_14a_tag_t tag;
    uint32_t session_id = 0u;
    assert(iso_dep_session_start_apple_transit(&tag, &session_id) ==
           STATUS_HF_TAG_OK);
    assert(session_id != 0u && iso_dep_session_is_active() && m_field_on);
    assert(m_annotation_seen_by_scan && !m_annotation_active);
    assert(m_annotation_set_count == 1u && m_annotation_timing_count == 1u);
    assert(m_annotation_clear_count == 2u);
    assert(m_annotation_length == sizeof(expected_frame));
    assert(memcmp(m_annotation_frame, expected_frame, sizeof(expected_frame)) == 0);
    assert(m_annotation_retries == 30u && m_annotation_delay_ms == 5u &&
           m_annotation_timeout_ms == 2u);
    assert(m_timeout == 91u);

    reset_fixture();
    m_timeout = 37u;
    m_scan_status = STATUS_HF_COLLISION;
    assert(iso_dep_session_start_apple_transit(&tag, &session_id) ==
           STATUS_HF_COLLISION);
    assert(!iso_dep_session_is_active() && !m_field_on && !m_annotation_active);
    assert(m_annotation_seen_by_scan && m_annotation_clear_count == 2u);
    assert(m_timeout == 37u);

    reset_fixture();
    m_timeout = 43u;
    m_scan_tag.sak = 0x00u;
    assert(iso_dep_session_start_apple_transit(&tag, &session_id) ==
           STATUS_HF_ERR_ATS);
    assert(!iso_dep_session_is_active() && !m_field_on && !m_annotation_active);
    assert(m_annotation_seen_by_scan && m_annotation_clear_count == 2u);
    assert(m_timeout == 43u);
}

static void test_apple_transit_setup_and_parameter_failures_cleanup(void) {
    picc_14a_tag_t tag;
    uint32_t session_id = 0u;

    reset_fixture();
    m_timeout = 61u;
    session_id = 0xA5A5A5A5u;
    m_annotation_set_ok = false;
    assert(iso_dep_session_start_apple_transit(&tag, &session_id) ==
           STATUS_HF_ERR_STAT);
    assert(m_annotation_set_count == 1u && m_annotation_timing_count == 0u);
    assert(m_annotation_clear_count == 2u && !m_annotation_active);
    assert(!iso_dep_session_is_active() && !m_field_on && m_timeout == 61u);
    assert(session_id == 0u);

    reset_fixture();
    m_annotation_timing_ok = false;
    assert(iso_dep_session_start_apple_transit(&tag, &session_id) ==
           STATUS_HF_ERR_STAT);
    assert(m_annotation_set_count == 1u && m_annotation_timing_count == 1u);
    assert(m_annotation_clear_count == 2u && !m_annotation_active);
    assert(!iso_dep_session_is_active() && !m_field_on);

    reset_fixture();
    assert(iso_dep_session_start_apple_transit(NULL, &session_id) == STATUS_PAR_ERR);
    assert(m_annotation_clear_count == 1u && !m_annotation_active);
    assert(!iso_dep_session_is_active() && !m_field_on);
}

int main(void) {
    test_start_and_persistent_exchange();
    test_failure_aborts_but_idle_session_persists();
    test_transport_ownership_and_disconnect_cleanup();
    test_disconnect_during_start_cannot_publish_orphan();
    test_stale_transport_generation_cannot_start();
    test_start_replaces_session_and_rejects_bad_targets();
    test_plain_start_clears_stale_annotation();
    test_apple_transit_annotation_and_cleanup();
    test_apple_transit_setup_and_parameter_failures_cleanup();
    puts("iso_dep_session tests passed");
    return 0;
}
