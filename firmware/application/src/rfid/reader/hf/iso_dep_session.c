#include "iso_dep_session.h"

#include <string.h>

#include "app_status.h"
#include "bsp_delay.h"
#include "rc522.h"

static iso_dep_reader_t m_reader;
static uint32_t m_session_id;
static uint32_t m_next_session_id = 1u;
static bool m_active;
static data_frame_transport_t m_owner_transport = DATA_FRAME_TRANSPORT_NONE;
static volatile bool m_owner_disconnected;

static uint32_t allocate_session_id(void) {
    uint32_t id = m_next_session_id++;
    if (id == 0u) id = m_next_session_id++;
    return id;
}

bool iso_dep_session_is_active(void) {
    return m_active;
}

bool iso_dep_session_is_current_transport_owner(void) {
    return m_active && data_frame_get_transport() == m_owner_transport;
}

void iso_dep_session_abort(void) {
    if (!m_active) return;
    m_active = false;
    m_session_id = 0u;
    m_owner_transport = DATA_FRAME_TRANSPORT_NONE;
    m_owner_disconnected = false;
    memset(&m_reader, 0, sizeof(m_reader));
    pcd_14a_reader_antenna_off();
}

static uint8_t iso_dep_session_start_common(picc_14a_tag_t *tag,
                                             uint32_t *session_id) {
    data_frame_transport_t owner = data_frame_get_transport();
    if (owner <= DATA_FRAME_TRANSPORT_NONE || owner >= DATA_FRAME_TRANSPORT_COUNT) {
        return STATUS_PAR_ERR;
    }
    if (!data_frame_current_transport_generation_valid()) return STATUS_CMD_ERR;
    iso_dep_session_abort();
    pcd_14a_reader_antenna_off();
    if (tag == NULL || session_id == NULL) return STATUS_PAR_ERR;
    *session_id = 0u;
    memset(tag, 0, sizeof(*tag));

    bsp_delay_ms(5);
    pcd_14a_reader_reset();
    pcd_14a_reader_antenna_on();
    bsp_delay_ms(8);

    uint16_t previous_timeout = pcd_14a_reader_timeout_get();
    pcd_14a_reader_timeout_set(200u);
    uint8_t status = pcd_14a_reader_scan_auto(tag);
    pcd_14a_reader_timeout_set(previous_timeout);
    if (status != STATUS_HF_TAG_OK) {
        pcd_14a_reader_antenna_off();
        return status;
    }
    if ((tag->sak & 0x20u) == 0u || tag->ats_len < 2u) {
        pcd_14a_reader_antenna_off();
        return STATUS_HF_ERR_ATS;
    }

    iso_dep_reader_init(&m_reader, tag);
    m_session_id = allocate_session_id();
    m_owner_transport = owner;
    m_active = true;
    if (!data_frame_current_transport_generation_valid()) {
        iso_dep_session_abort();
        return STATUS_CMD_ERR;
    }
    *session_id = m_session_id;
    return STATUS_HF_TAG_OK;
}

uint8_t iso_dep_session_start(picc_14a_tag_t *tag, uint32_t *session_id) {
    pcd_14a_reader_polling_annotation_clear();
    return iso_dep_session_start_common(tag, session_id);
}

uint8_t iso_dep_session_start_apple_transit(picc_14a_tag_t *tag,
                                             uint32_t *session_id) {
    static const uint8_t ecp2_tfl[] = {
        0x6Au, 0x02u, 0xC8u, 0x01u, 0x00u, 0x03u, 0x00u, 0x02u,
        0x79u, 0x00u, 0x00u, 0x00u, 0x00u, 0xC2u, 0xD8u,
    };
    uint8_t status = STATUS_HF_ERR_STAT;

    pcd_14a_reader_polling_annotation_clear();
    if (tag == NULL || session_id == NULL) {
        iso_dep_session_abort();
        pcd_14a_reader_antenna_off();
        return STATUS_PAR_ERR;
    }
    *session_id = 0u;
    memset(tag, 0, sizeof(*tag));
    if (pcd_14a_reader_polling_annotation_set(ecp2_tfl, sizeof(ecp2_tfl)) &&
            pcd_14a_reader_polling_annotation_timing_set(30u, 5u, 2u)) {
        status = iso_dep_session_start_common(tag, session_id);
    } else {
        iso_dep_session_abort();
        pcd_14a_reader_antenna_off();
    }
    pcd_14a_reader_polling_annotation_clear();
    return status;
}

iso_dep_session_exchange_status_t iso_dep_session_exchange(
    uint32_t session_id, const uint8_t *apdu, uint16_t apdu_len,
    uint8_t *response, uint16_t response_capacity, iso_dep_result_t *result) {
    if (!iso_dep_session_is_current_transport_owner() || session_id == 0u ||
            session_id != m_session_id) {
        if (result != NULL) memset(result, 0, sizeof(*result));
        return ISO_DEP_SESSION_EXCHANGE_INVALID;
    }

    if (!iso_dep_reader_transceive(&m_reader, apdu, apdu_len,
                                   response, response_capacity, result)) {
        iso_dep_session_abort();
        return ISO_DEP_SESSION_EXCHANGE_RF_ERROR;
    }
    return ISO_DEP_SESSION_EXCHANGE_OK;
}

bool iso_dep_session_stop(uint32_t session_id) {
    if (!iso_dep_session_is_current_transport_owner() || session_id == 0u ||
            session_id != m_session_id) return false;
    iso_dep_reader_deselect(&m_reader);
    iso_dep_session_abort();
    return true;
}

void iso_dep_session_owner_disconnected(data_frame_transport_t transport) {
    if (transport <= DATA_FRAME_TRANSPORT_NONE || transport >= DATA_FRAME_TRANSPORT_COUNT) {
        return;
    }
    if (m_active && transport == m_owner_transport) {
        m_owner_disconnected = true;
    }
}

void iso_dep_session_process(void) {
    if (m_owner_disconnected) iso_dep_session_abort();
}
