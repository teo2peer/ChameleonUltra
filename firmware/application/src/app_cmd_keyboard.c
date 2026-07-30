#include "app_cmd_keyboard.h"

#include "app_status.h"
#include "ble_main.h"
#include "keyboard_hid.h"
#include "keyboard_payload.h"
#include "netdata.h"
#include "nrf_error.h"
#include "settings.h"
#include "syssleep.h"
#include "usb_main.h"

#define KEYBOARD_UPLOAD_HEADER_LENGTH    7u
#define KEYBOARD_UPLOAD_MAX_CHUNK        (NETDATA_MAX_DATA_LENGTH - KEYBOARD_UPLOAD_HEADER_LENGTH)

static void put_u16be(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

static void put_u32be(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static uint16_t result_status(keyboard_payload_result_t result) {
    switch (result) {
        case KEYBOARD_PAYLOAD_RESULT_OK:
            return STATUS_SUCCESS;
        case KEYBOARD_PAYLOAD_RESULT_PARAMETER:
        case KEYBOARD_PAYLOAD_RESULT_CRC:
        case KEYBOARD_PAYLOAD_RESULT_FORMAT:
            return STATUS_PAR_ERR;
        case KEYBOARD_PAYLOAD_RESULT_TRANSPORT:
            return STATUS_DEVICE_MODE_ERROR;
        case KEYBOARD_PAYLOAD_RESULT_STATE:
        case KEYBOARD_PAYLOAD_RESULT_UNAVAILABLE:
        default:
            return STATUS_CMD_ERR;
    }
}

static bool command_authorized(void) {
    data_frame_transport_t transport = data_frame_get_transport();
    return (transport == DATA_FRAME_TRANSPORT_USB && is_usb_working()) ||
           (transport == DATA_FRAME_TRANSPORT_BLE &&
            ble_keyboard_link_authorized());
}

data_frame_tx_t *cmd_before_keyboard(uint16_t cmd, uint16_t status,
                                     uint16_t length, uint8_t *data) {
    if (!command_authorized()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    return NULL;
}

data_frame_tx_t *cmd_processor_keyboard_upload_begin(uint16_t cmd, uint16_t status,
                                                      uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 7u) || data[0] != KEYBOARD_PAYLOAD_VERSION) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint32_t upload_id;
    uint16_t next_offset;
    keyboard_payload_result_t result = keyboard_payload_upload_begin(
        cmd_read_u16be(&data[1]), cmd_read_u32be(&data[3]),
        data_frame_get_transport(), &upload_id, &next_offset);
    if (result != KEYBOARD_PAYLOAD_RESULT_OK) {
        return data_frame_make(cmd, result_status(result), 0, NULL);
    }
    uint8_t response[9];
    response[0] = KEYBOARD_PAYLOAD_VERSION;
    put_u32be(&response[1], upload_id);
    put_u16be(&response[5], next_offset);
    put_u16be(&response[7], KEYBOARD_UPLOAD_MAX_CHUNK);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(response), response);
}

data_frame_tx_t *cmd_processor_keyboard_upload_chunk(uint16_t cmd, uint16_t status,
                                                      uint16_t length, uint8_t *data) {
    if (length <= KEYBOARD_UPLOAD_HEADER_LENGTH || data == NULL ||
            data[0] != KEYBOARD_PAYLOAD_VERSION) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint32_t upload_id = cmd_read_u32be(&data[1]);
    uint16_t next_offset;
    keyboard_payload_result_t result = keyboard_payload_upload_chunk(
        upload_id, cmd_read_u16be(&data[5]), &data[7], length - 7u,
        data_frame_get_transport(), &next_offset);
    if (result != KEYBOARD_PAYLOAD_RESULT_OK) {
        return data_frame_make(cmd, result_status(result), 0, NULL);
    }
    uint8_t response[6];
    put_u32be(response, upload_id);
    put_u16be(&response[4], next_offset);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(response), response);
}

data_frame_tx_t *cmd_processor_keyboard_upload_commit(uint16_t cmd, uint16_t status,
                                                       uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 5u) || data[0] != KEYBOARD_PAYLOAD_VERSION) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint32_t commit_id;
    uint32_t crc32;
    uint16_t committed_length;
    keyboard_payload_result_t result = keyboard_payload_upload_commit(
        cmd_read_u32be(&data[1]), data_frame_get_transport(), &commit_id,
        &committed_length, &crc32);
    if (result != KEYBOARD_PAYLOAD_RESULT_OK) {
        return data_frame_make(cmd, result_status(result), 0, NULL);
    }
    uint8_t response[10];
    put_u32be(response, commit_id);
    put_u16be(&response[4], committed_length);
    put_u32be(&response[6], crc32);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(response), response);
}

data_frame_tx_t *cmd_processor_keyboard_run(uint16_t cmd, uint16_t status,
                                            uint16_t length, uint8_t *data) {
    if (!command_authorized()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    if (!cmd_payload_exact(length, data, 6u) || data[0] != KEYBOARD_PAYLOAD_VERSION) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    // Gate on the boot-time value: HID is exposed (USB interface) and normally
    // registered (BLE service) at boot, so toggling the setting needs a reboot.
    if (!settings_get_keyboard_hid_enable_first_load()) {
        return data_frame_make(cmd, STATUS_NOT_IMPLEMENTED, 0, NULL);
    }
    // data[5] is the HID output bitmask. Registering the BLE HID service is a
    // no-op when it was already registered at boot; surface a failure instead
    // of emitting reports into an unregistered service.
    if ((data[5] & KEYBOARD_HID_OUTPUT_BLE) != 0 &&
            keyboard_hid_ble_ensure_registered() != NRF_SUCCESS) {
        return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
    }
    uint32_t run_id;
    data_frame_transport_t transport = data_frame_get_transport();
    keyboard_payload_result_t result = keyboard_payload_run(
        cmd_read_u32be(&data[1]), data[5], transport, &run_id);
    if (result != KEYBOARD_PAYLOAD_RESULT_OK) {
        return data_frame_make(cmd, result_status(result), 0, NULL);
    }
    sleep_timer_stop();
    uint8_t response[4];
    put_u32be(response, run_id);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(response), response);
}

data_frame_tx_t *cmd_processor_keyboard_cancel(uint16_t cmd, uint16_t status,
                                               uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    keyboard_payload_cancel();
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

data_frame_tx_t *cmd_processor_keyboard_get_status(uint16_t cmd, uint16_t status,
                                                   uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    keyboard_payload_status_t payload_status;
    keyboard_payload_get_status(&payload_status);
    uint8_t response[28];
    response[0] = KEYBOARD_PAYLOAD_VERSION;
    response[1] = payload_status.state;
    response[2] = payload_status.error;
    response[3] = payload_status.outputs;
    put_u32be(&response[4], payload_status.upload_id);
    put_u32be(&response[8], payload_status.commit_id);
    put_u32be(&response[12], payload_status.run_id);
    put_u16be(&response[16], payload_status.expected);
    put_u16be(&response[18], payload_status.received);
    put_u16be(&response[20], payload_status.pc);
    put_u16be(&response[22], payload_status.length);
    put_u32be(&response[24], payload_status.crc32);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(response), response);
}

data_frame_tx_t *cmd_processor_keyboard_clear(uint16_t cmd, uint16_t status,
                                              uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    keyboard_payload_result_t result = keyboard_payload_clear();
    return data_frame_make(cmd, result_status(result), 0, NULL);
}

data_frame_tx_t *cmd_processor_keyboard_set_temp_ble_name(uint16_t cmd, uint16_t status,
                                                          uint16_t length, uint8_t *data) {
    if (length < 2u || data == NULL || data[0] != KEYBOARD_PAYLOAD_VERSION ||
            data[1] > BLE_TEMPORARY_NAME_MAX_LENGTH || length != (uint16_t)(2u + data[1])) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint32_t error = ble_peripheral_name_set_temporary(
        data[1] == 0u ? NULL : &data[2], data[1]);
    if (error != NRF_SUCCESS) {
        return data_frame_make(cmd,
                               error == NRF_ERROR_INVALID_PARAM ? STATUS_PAR_ERR : STATUS_CMD_ERR,
                               0, NULL);
    }
    uint8_t response[2u + BLE_TEMPORARY_NAME_MAX_LENGTH];
    response[0] = KEYBOARD_PAYLOAD_VERSION;
    response[1] = ble_peripheral_name_get(&response[2]);
    return data_frame_make(cmd, STATUS_SUCCESS, 2u + response[1], response);
}

data_frame_tx_t *cmd_processor_keyboard_arm_ble(uint16_t cmd, uint16_t status,
                                                uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 5u) || data[0] != KEYBOARD_PAYLOAD_VERSION) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    // Gate on the boot-time value (see cmd_processor_keyboard_run): toggling the
    // feature requires a reboot to expose USB HID and register the BLE service.
    if (!settings_get_keyboard_hid_enable_first_load()) {
        return data_frame_make(cmd, STATUS_NOT_IMPLEMENTED, 0, NULL);
    }
    // When enabled, the HID service is normally registered at boot; this only
    // registers on demand (and surfaces NRF_ERROR_NO_MEM as a status instead of
    // resetting) if that boot registration failed.
    if (keyboard_hid_ble_ensure_registered() != NRF_SUCCESS) {
        return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
    }
    uint32_t run_id;
    keyboard_payload_result_t result = keyboard_payload_arm_ble(
        cmd_read_u32be(&data[1]), &run_id);
    if (result != KEYBOARD_PAYLOAD_RESULT_OK) {
        return data_frame_make(cmd, result_status(result), 0, NULL);
    }
    uint32_t error = ble_keyboard_advertising_start();
    if (error != NRF_SUCCESS) {
        keyboard_payload_cancel();
        return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
    }
    sleep_timer_stop();
    uint8_t response[4];
    put_u32be(response, run_id);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(response), response);
}
