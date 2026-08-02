#ifndef APP_CMD_BLE_H
#define APP_CMD_BLE_H

#include "dataframe.h"

data_frame_tx_t *cmd_processor_ble_scan_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_scan_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_scan_get_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_scan_get_results(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_advertising_set(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_advertising_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_link_probe(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_connect(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_disconnect(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_central_state(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_gatt_discover(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_gatt_get_chars(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_fuzz_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_fuzz_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_fuzz_get_log(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_gatt_read(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_gatt_get_read(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_subscribe(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_get_notifications(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_find_cccd(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_get_cccd(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_gatt_write(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_get_write(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_get_mtu(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_desc_discover(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_desc_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_svc_discover(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_svc_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_device_info(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_get_device_info(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_set_addr(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_get_addr(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_radio_set(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_radio_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_flood_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_flood_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_flood_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_kick(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_adv_flood_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_adv_flood_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_adv_lab_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_adv_lab_status(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_ble_adv_lab_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);

#endif
