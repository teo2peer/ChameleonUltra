#ifndef BLE_MAIN_H
#define BLE_MAIN_H

#include <stdbool.h>
#include <stdint.h>

bool is_nus_tx_idle(void);
bool ble_keyboard_link_authorized(void);
uint32_t ble_peripheral_name_set_temporary(const uint8_t *name, uint8_t length);

#endif
