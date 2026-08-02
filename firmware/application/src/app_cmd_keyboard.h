#ifndef APP_CMD_KEYBOARD_H
#define APP_CMD_KEYBOARD_H

#include "app_cmd.h"

data_frame_tx_t *cmd_before_keyboard(uint16_t cmd, uint16_t status,
                                     uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_keyboard_upload_begin(uint16_t cmd, uint16_t status,
                                                      uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_keyboard_upload_chunk(uint16_t cmd, uint16_t status,
                                                      uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_keyboard_upload_commit(uint16_t cmd, uint16_t status,
                                                       uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_keyboard_run(uint16_t cmd, uint16_t status,
                                            uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_keyboard_cancel(uint16_t cmd, uint16_t status,
                                               uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_keyboard_get_status(uint16_t cmd, uint16_t status,
                                                   uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_keyboard_clear(uint16_t cmd, uint16_t status,
                                               uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_keyboard_set_temp_ble_name(uint16_t cmd, uint16_t status,
                                                          uint16_t length, uint8_t *data);
data_frame_tx_t *cmd_processor_keyboard_arm_ble(uint16_t cmd, uint16_t status,
                                                uint16_t length, uint8_t *data);

#endif
