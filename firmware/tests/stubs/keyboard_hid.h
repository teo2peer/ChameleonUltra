#ifndef KEYBOARD_HID_H
#define KEYBOARD_HID_H

#include <stdbool.h>
#include <stdint.h>

bool keyboard_hid_outputs_ready(uint8_t outputs);
bool keyboard_hid_ble_ready(void);
uint32_t keyboard_hid_send_report(uint8_t outputs, uint8_t modifiers,
                                  uint8_t usage);
void keyboard_hid_abort(void);

#endif
