#include <string.h>

#include "nordic_common.h"
#include "nrf_sdh_ble.h"
#include "app_util_platform.h"

#include "ble_scan.h"

#define BLE_SCAN_ADV_DATA_MAX 31

typedef struct {
    uint8_t addr[BLE_GAP_ADDR_LEN];
    uint8_t addr_type;
    int8_t rssi;
    bool connectable;
    uint8_t primary_data_len;
    uint8_t adv_data_len;
    uint8_t adv_data[BLE_SCAN_ADV_DATA_MAX];
} ble_scan_record_t;

static ble_scan_record_t m_scan_records[BLE_SCAN_MAX_DEVICES];
static ble_scan_record_t m_scan_snapshot[BLE_SCAN_MAX_DEVICES];
static volatile uint8_t m_scan_count = 0;
static volatile bool m_scan_active = false;
static volatile uint8_t m_scan_mode = 0;

static uint8_t m_scan_buffer_data[BLE_GAP_SCAN_BUFFER_MIN];
static ble_data_t m_scan_buffer = {m_scan_buffer_data, BLE_GAP_SCAN_BUFFER_MIN};

static ble_gap_scan_params_t m_scan_params = {
    .active = 0,
    .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
    .scan_phys = BLE_GAP_PHY_1MBPS,
    .interval = MSEC_TO_UNITS(100, UNIT_0_625_MS),
    .window = MSEC_TO_UNITS(50, UNIT_0_625_MS),
    .timeout = 0,
};

static uint8_t ble_scan_take_snapshot(void) {
    uint8_t count;

    CRITICAL_REGION_ENTER();
    count = m_scan_count;
    memcpy(m_scan_snapshot, m_scan_records, count * sizeof(m_scan_snapshot[0]));
    CRITICAL_REGION_EXIT();
    return count;
}

static void ble_scan_record_update(const ble_gap_evt_adv_report_t *report) {
    uint8_t adv_len = MIN(report->data.len, (uint16_t)BLE_SCAN_ADV_DATA_MAX);

    for (uint8_t i = 0; i < m_scan_count; i++) {
        if (m_scan_records[i].addr_type == report->peer_addr.addr_type &&
                memcmp(m_scan_records[i].addr, report->peer_addr.addr, BLE_GAP_ADDR_LEN) == 0) {
            m_scan_records[i].rssi = report->rssi;
            m_scan_records[i].connectable = m_scan_records[i].connectable || (report->type.connectable != 0);
            if (report->type.scan_response) {
                uint8_t remaining = BLE_SCAN_ADV_DATA_MAX - m_scan_records[i].primary_data_len;
                uint8_t copy_len = MIN(adv_len, remaining);
                memcpy(m_scan_records[i].adv_data + m_scan_records[i].primary_data_len,
                       report->data.p_data, copy_len);
                m_scan_records[i].adv_data_len = m_scan_records[i].primary_data_len + copy_len;
            } else {
                m_scan_records[i].primary_data_len = adv_len;
                m_scan_records[i].adv_data_len = adv_len;
                memcpy(m_scan_records[i].adv_data, report->data.p_data, adv_len);
            }
            return;
        }
    }

    if (m_scan_count >= BLE_SCAN_MAX_DEVICES) {
        return;
    }

    ble_scan_record_t *rec = &m_scan_records[m_scan_count];
    memcpy(rec->addr, report->peer_addr.addr, BLE_GAP_ADDR_LEN);
    rec->addr_type = report->peer_addr.addr_type;
    rec->rssi = report->rssi;
    rec->connectable = (report->type.connectable != 0);
    rec->primary_data_len = report->type.scan_response ? 0 : adv_len;
    rec->adv_data_len = adv_len;
    memcpy(rec->adv_data, report->data.p_data, adv_len);
    m_scan_count++;
}

uint32_t ble_scan_start(uint8_t active) {
    if (m_scan_active) {
        return NRF_SUCCESS;
    }
    uint8_t requested_mode = active ? 1 : 0;
    m_scan_params.active = requested_mode;
    m_scan_buffer.len = BLE_GAP_SCAN_BUFFER_MIN;
    uint32_t err_code = sd_ble_gap_scan_start(&m_scan_params, &m_scan_buffer);
    if (err_code == NRF_SUCCESS) {
        CRITICAL_REGION_ENTER();
        m_scan_count = 0;
        m_scan_mode = requested_mode;
        m_scan_active = true;
        CRITICAL_REGION_EXIT();
    }
    return err_code;
}

uint32_t ble_scan_stop(void) {
    // Always reconcile with the SoftDevice. The logical flag can lag an
    // implicit GAP stop (for example when a connection procedure starts).
    uint32_t err_code = sd_ble_gap_scan_stop();
    if (err_code == NRF_SUCCESS || err_code == NRF_ERROR_INVALID_STATE) {
        ble_scan_mark_inactive();
        return NRF_SUCCESS;
    }
    return err_code;
}

bool is_ble_scanning(void) {
    return m_scan_active;
}

uint8_t ble_scan_get_mode(void) {
    return m_scan_mode;
}

void ble_scan_mark_inactive(void) {
    CRITICAL_REGION_ENTER();
    m_scan_active = false;
    CRITICAL_REGION_EXIT();
}

uint8_t ble_scan_get_count(void) {
    uint8_t count;
    CRITICAL_REGION_ENTER();
    count = m_scan_count;
    CRITICAL_REGION_EXIT();
    return count;
}

uint16_t ble_scan_copy_records(uint8_t start_index, uint8_t *out, uint16_t out_cap) {
    if (out == NULL || out_cap == 0) {
        return 0;
    }

    uint8_t count = ble_scan_take_snapshot();
    uint16_t offset = 0;
    for (uint8_t i = start_index; i < count; i++) {
        const ble_scan_record_t *rec = &m_scan_snapshot[i];
        uint16_t rec_size = BLE_GAP_ADDR_LEN + 3 + rec->adv_data_len;
        if (offset + rec_size > out_cap) {
            break;
        }
        memcpy(out + offset, rec->addr, BLE_GAP_ADDR_LEN);
        offset += BLE_GAP_ADDR_LEN;
        out[offset++] = rec->addr_type;
        out[offset++] = (uint8_t)rec->rssi;
        out[offset++] = rec->adv_data_len;
        memcpy(out + offset, rec->adv_data, rec->adv_data_len);
        offset += rec->adv_data_len;
    }
    return offset;
}

uint8_t ble_scan_copy_addresses(ble_scan_addr_t *out, uint8_t out_cap) {
    if (out == NULL || out_cap == 0) {
        return 0;
    }

    uint8_t count = ble_scan_take_snapshot();
    bool used[BLE_SCAN_MAX_DEVICES] = {0};
    uint8_t n = 0;
    while (n < out_cap) {
        uint8_t best = 0xFF;
        int8_t best_rssi = -128;

        for (uint8_t i = 0; i < count; i++) {
            if (used[i] || !m_scan_snapshot[i].connectable ||
                    m_scan_snapshot[i].addr_type == BLE_GAP_ADDR_TYPE_ANONYMOUS) {
                continue;
            }
            if (best == 0xFF || m_scan_snapshot[i].rssi > best_rssi) {
                best = i;
                best_rssi = m_scan_snapshot[i].rssi;
            }
        }

        if (best == 0xFF) {
            break;
        }

        used[best] = true;
        memcpy(out[n].addr, m_scan_snapshot[best].addr, BLE_GAP_ADDR_LEN);
        out[n].addr_type = m_scan_snapshot[best].addr_type;
        n++;
    }
    return n;
}

void ble_scan_on_adv_report(const ble_gap_evt_adv_report_t *report) {
    if (!m_scan_active) {
        return;
    }

    ble_scan_record_update(report);
    // Each report consumes the SoftDevice scan buffer, so hand it back. An
    // error is benign if the scan was stopped while this event was pending.
    m_scan_buffer.len = BLE_GAP_SCAN_BUFFER_MIN;
    if (sd_ble_gap_scan_start(NULL, &m_scan_buffer) != NRF_SUCCESS) {
        ble_scan_mark_inactive();
    }
}
