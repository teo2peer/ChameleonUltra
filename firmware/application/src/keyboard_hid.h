#ifndef KEYBOARD_HID_H
#define KEYBOARD_HID_H

#include <stdbool.h>
#include <stdint.h>

#include "app_usbd_class_base.h"
#include "ble.h"

#define KEYBOARD_HID_OUTPUT_USB 1
#define KEYBOARD_HID_OUTPUT_BLE 2

// Registers the BLE HID service on demand. Idempotent: returns NRF_SUCCESS if
// already registered. Never asserts on failure (e.g. NRF_ERROR_NO_MEM when the
// GATT attribute table is exhausted) so the caller can surface the error.
uint32_t keyboard_hid_ble_ensure_registered(void);
bool keyboard_hid_ble_is_registered(void);
app_usbd_class_inst_t const *keyboard_hid_usb_class_instance(void);
void keyboard_hid_on_ble_evt(ble_evt_t const *event);
void keyboard_hid_process(void);
bool keyboard_hid_outputs_ready(uint8_t outputs);
uint32_t keyboard_hid_send_report(uint8_t outputs, uint8_t modifiers, uint8_t usage);
void keyboard_hid_abort(void);
void keyboard_hid_usb_reset(void);
bool keyboard_hid_usb_ready(void);
bool keyboard_hid_ble_ready(void);

#endif
