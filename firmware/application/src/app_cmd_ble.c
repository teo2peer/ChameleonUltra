#include "app_cmd_ble.h"

#include "app_cmd.h"
#include "app_status.h"
#include "ble_central.h"
#include "ble_main.h"
#include "ble_scan.h"
#include "netdata.h"

// Command processing is serialized and data_frame_make() copies its payload,
// so all large BLE responses can safely share one scratch buffer.
static uint8_t m_ble_response[NETDATA_MAX_DATA_LENGTH];

static bool cmd_ble_radio_is_on(void) {
    uint8_t state[4];
    return ble_radio_get(state) == NRF_SUCCESS && state[0] != 0;
}

data_frame_tx_t *cmd_processor_ble_scan_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    bool active;
    if (!cmd_payload_exact(length, data, 1u) || !cmd_parse_bool(data[0], &active)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (!cmd_ble_radio_is_on()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    uint32_t err_code = ble_scan_start(active);
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_CMD_ERR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_scan_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint32_t err_code = ble_scan_stop();
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_CMD_ERR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_scan_get_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t count = ble_scan_get_count();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &count);
}

data_frame_tx_t *cmd_processor_ble_scan_get_results(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 1u)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t start_index = data[0];
    uint16_t out_len = ble_scan_copy_records(start_index, m_ble_response, sizeof(m_ble_response));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, m_ble_response);
}

data_frame_tx_t *cmd_processor_ble_advertising_set(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    bool enabled;
    bool erase_bonds;
    if (!cmd_payload_exact(length, data, 2u) ||
            !cmd_parse_bool(data[0], &enabled) || !cmd_parse_bool(data[1], &erase_bonds)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    if (!enabled) {
        advertising_stop();
    } else {
        if (!cmd_ble_radio_is_on()) {
            return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
        }
        advertising_start(erase_bonds);
    }

    uint8_t state = is_ble_advertising();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &state);
}

data_frame_tx_t *cmd_processor_ble_advertising_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t state = is_ble_advertising();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &state);
}

data_frame_tx_t *cmd_processor_ble_link_probe(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    bool global_mode = false;
    if (length > 1u || (length == 1u &&
            (data == NULL || !cmd_parse_bool(data[0], &global_mode)))) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (!cmd_ble_radio_is_on()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    uint32_t err_code = ble_central_link_probe(global_mode);
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_DEVICE_MODE_ERROR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_connect(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 7u) || data[0] > 3u) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (!cmd_ble_radio_is_on()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    uint32_t err_code = ble_central_connect(data[0], &data[1]);
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_CMD_ERR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_disconnect(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint32_t err_code = ble_central_disconnect();
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_CMD_ERR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_central_state(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t out[24];
    uint16_t out_len = ble_central_get_state(out, sizeof(out));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, out);
}

data_frame_tx_t *cmd_processor_ble_gatt_discover(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint32_t err_code = ble_central_gatt_discover();
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_DEVICE_MODE_ERROR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_gatt_get_chars(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 1u)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t start_index = data[0];
    uint16_t out_len = ble_central_copy_chars(start_index, m_ble_response, sizeof(m_ble_response));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, m_ble_response);
}

data_frame_tx_t *cmd_processor_ble_fuzz_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 6u)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint16_t handle = ((uint16_t)data[0] << 8) | data[1];
    uint16_t max_iter = ((uint16_t)data[2] << 8) | data[3];
    uint16_t interval = ((uint16_t)data[4] << 8) | data[5];
    if (handle == 0u || interval == 0u) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint32_t err_code = ble_central_fuzz_start(handle, max_iter, interval);
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_DEVICE_MODE_ERROR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_fuzz_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    ble_central_fuzz_stop();
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_fuzz_get_log(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 2u)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint16_t start_index = cmd_read_u16be(data);
    uint16_t out_len = ble_central_copy_log(start_index, m_ble_response, sizeof(m_ble_response));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, m_ble_response);
}

data_frame_tx_t *cmd_processor_ble_gatt_read(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 2u) || (data[0] == 0u && data[1] == 0u)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint16_t handle = ((uint16_t)data[0] << 8) | data[1];
    uint32_t err_code = ble_central_gatt_read(handle);
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_DEVICE_MODE_ERROR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_gatt_get_read(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t out[3 + 244];
    uint16_t out_len = ble_central_copy_read(out, sizeof(out));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, out);
}

data_frame_tx_t *cmd_processor_ble_subscribe(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 3u) ||
            (data[0] == 0 && data[1] == 0) || data[2] > 2) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint16_t cccd = ((uint16_t)data[0] << 8) | data[1];
    uint32_t err_code = ble_central_subscribe(cccd, data[2]);
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_DEVICE_MODE_ERROR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_get_notifications(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 2u)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint16_t start_index = cmd_read_u16be(data);
    uint16_t out_len = ble_central_copy_notifs(start_index, m_ble_response, sizeof(m_ble_response));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, m_ble_response);
}

data_frame_tx_t *cmd_processor_ble_find_cccd(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 2u) ||
            (data[0] == 0 && data[1] == 0) || (data[0] == 0xFF && data[1] == 0xFF)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint16_t value_handle = ((uint16_t)data[0] << 8) | data[1];
    uint32_t err_code = ble_central_find_cccd(value_handle);
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_DEVICE_MODE_ERROR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_get_cccd(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t out[3];
    uint16_t out_len = ble_central_get_cccd(out, sizeof(out));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, out);
}

data_frame_tx_t *cmd_processor_ble_gatt_write(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length < 3u || data == NULL || (data[0] == 0u && data[1] == 0u)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint16_t data_len = length - 2;
    if (data_len > BLE_CENTRAL_WRITE_MAX || data_len > ble_central_write_max()) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint16_t handle = ((uint16_t)data[0] << 8) | data[1];
    uint32_t err_code = ble_central_gatt_write(handle, &data[2], (uint8_t)data_len);
    uint16_t result = (err_code == NRF_SUCCESS) ? STATUS_SUCCESS :
                      (err_code == NRF_ERROR_DATA_SIZE || err_code == NRF_ERROR_INVALID_PARAM) ? STATUS_PAR_ERR :
                      STATUS_DEVICE_MODE_ERROR;
    return data_frame_make(cmd, result, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_get_write(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t out[2];
    uint16_t out_len = ble_central_get_write_result(out, sizeof(out));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, out);
}

data_frame_tx_t *cmd_processor_ble_get_mtu(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint16_t mtu = ble_central_mtu();
    uint8_t out[2] = {(mtu >> 8) & 0xFF, mtu & 0xFF};
    return data_frame_make(cmd, STATUS_SUCCESS, 2, out);
}

data_frame_tx_t *cmd_processor_ble_desc_discover(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint32_t err_code = ble_central_desc_discover();
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_DEVICE_MODE_ERROR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_desc_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 1u)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t start_index = data[0];
    uint16_t out_len = ble_central_copy_descs(start_index, m_ble_response, sizeof(m_ble_response));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, m_ble_response);
}

data_frame_tx_t *cmd_processor_ble_svc_discover(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint32_t err_code = ble_central_svc_discover();
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_DEVICE_MODE_ERROR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_svc_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 1u)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t start_index = data[0];
    uint16_t out_len = ble_central_copy_svcs(start_index, m_ble_response, sizeof(m_ble_response));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, m_ble_response);
}

data_frame_tx_t *cmd_processor_ble_device_info(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint32_t err_code = ble_central_devinfo_start();
    return data_frame_make(cmd, err_code == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_DEVICE_MODE_ERROR, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_get_device_info(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t out[512];
    uint16_t out_len = ble_central_copy_devinfo(out, sizeof(out));
    return data_frame_make(cmd, STATUS_SUCCESS, out_len, out);
}

data_frame_tx_t *cmd_processor_ble_set_addr(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length < 1u || data == NULL || data[0] > 3u ||
            (data[0] == BLE_ADDR_MODE_RANDOM_STATIC ? length != 7u : length != 1u)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    const uint8_t *addr_le = (data[0] == BLE_ADDR_MODE_RANDOM_STATIC) ? &data[1] : NULL;
    uint32_t err = ble_addr_set(data[0], addr_le);
    if (err == NRF_ERROR_BUSY) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    if (err != NRF_SUCCESS) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_get_addr(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t out[1 + BLE_GAP_ADDR_LEN];
    uint32_t err = ble_addr_get(&out[0], &out[1]);
    if (err != NRF_SUCCESS) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(out), out);
}

data_frame_tx_t *cmd_processor_ble_radio_set(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    bool radio_on;
    if (!cmd_payload_exact(length, data, 1u) || !cmd_parse_bool(data[0], &radio_on)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint32_t err = ble_radio_set(radio_on);
    if (err != NRF_SUCCESS) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_radio_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t out[4];
    uint32_t err = ble_radio_get(out);
    if (err != NRF_SUCCESS) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(out), out);
}

data_frame_tx_t *cmd_processor_ble_flood_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 8u) || data[0] > 2) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t scope = data[0];
    uint16_t value_handle = ((uint16_t)data[1] << 8) | data[2];
    uint8_t payload_size = data[3];
    uint16_t max_iter = ((uint16_t)data[4] << 8) | data[5];
    uint16_t interval_ms = ((uint16_t)data[6] << 8) | data[7];
    if (interval_ms < 1) {
        interval_ms = 1;
    }
    if (!cmd_ble_radio_is_on()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }

    uint32_t err;
    switch (scope) {
        case 0:
            err = ble_central_flood_start(value_handle, payload_size, max_iter, interval_ms);
            break;
        case 1:
            err = ble_central_flood_scan_buffer(value_handle, payload_size, max_iter, interval_ms);
            break;
        case 2:
            err = ble_adv_flood_start((uint8_t)value_handle, interval_ms);
            break;
        default:
            err = NRF_ERROR_INVALID_PARAM;
            break;
    }

    if (err == NRF_ERROR_INVALID_STATE || err == NRF_ERROR_BUSY ||
            (scope == 2 && (err == NRF_ERROR_NO_MEM || err == NRF_ERROR_RESOURCES))) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    if (err != NRF_SUCCESS) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_flood_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint32_t central_err = ble_central_flood_stop();
    uint32_t adv_err = ble_adv_flood_stop();
    return data_frame_make(cmd,
                           central_err == NRF_SUCCESS && adv_err == NRF_SUCCESS ?
                           STATUS_SUCCESS : STATUS_CMD_ERR,
                           0, NULL);
}

data_frame_tx_t *cmd_processor_ble_flood_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint32_t n = ble_central_flood_count();
    uint8_t out[4];
    out[0] = (n >> 24) & 0xFF;
    out[1] = (n >> 16) & 0xFF;
    out[2] = (n >> 8) & 0xFF;
    out[3] = n & 0xFF;
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(out), out);
}

data_frame_tx_t *cmd_processor_ble_kick(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 2u) ||
            data[1] == 0 || data[1] > 10 || data[0] > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (data[0] == 0 && data[1] != 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (!cmd_ble_radio_is_on()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    uint32_t err;
    if (data[0] == 0) {
        err = ble_central_kick(data[1]);
    } else {
        err = ble_central_kick_scan_buffer(data[1]);
    }
    if (err == NRF_ERROR_INVALID_STATE || err == NRF_ERROR_BUSY) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    if (err != NRF_SUCCESS) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_adv_flood_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 3u) || data[0] != 2) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (!cmd_ble_radio_is_on()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    uint8_t fill = data[1];
    uint8_t interval_units = data[2];
    if (interval_units < 1 || interval_units > 102) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint16_t interval_ms = (uint16_t)interval_units * 100u;
    uint32_t err = ble_adv_flood_start(fill, interval_ms);
    if (err == NRF_ERROR_INVALID_STATE || err == NRF_ERROR_BUSY ||
            err == NRF_ERROR_NO_MEM || err == NRF_ERROR_RESOURCES) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    if (err != NRF_SUCCESS) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

data_frame_tx_t *cmd_processor_ble_adv_flood_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint32_t err = ble_adv_flood_stop();
    return data_frame_make(cmd, err == NRF_SUCCESS ? STATUS_SUCCESS : STATUS_CMD_ERR,
                           0, NULL);
}
