#ifndef DEVICE_SETTINGS_PAYLOAD_INTERNAL_H
#define DEVICE_SETTINGS_PAYLOAD_INTERNAL_H

#include <stdint.h>
#include <string.h>

#define DEVICE_SETTINGS_V6_PAIRING_KEY_SIZE 6u
#define DEVICE_SETTINGS_V6_PAYLOAD_SIZE 14u

static inline void device_settings_payload_v6(
    uint8_t output[DEVICE_SETTINGS_V6_PAYLOAD_SIZE],
    uint8_t version,
    uint8_t animation,
    uint8_t button_a,
    uint8_t button_b,
    uint8_t long_button_a,
    uint8_t long_button_b,
    uint8_t pairing_enabled,
    const uint8_t pairing_key[DEVICE_SETTINGS_V6_PAIRING_KEY_SIZE],
    uint32_t sleep_timeout_ms
) {
    output[0] = version;
    output[1] = animation;
    output[2] = button_a;
    output[3] = button_b;
    output[4] = long_button_a;
    output[5] = long_button_b;
    output[6] = pairing_enabled;
    memcpy(&output[7], pairing_key, DEVICE_SETTINGS_V6_PAIRING_KEY_SIZE);
    output[13] = (uint8_t)(sleep_timeout_ms / 1000u);
}

#endif
