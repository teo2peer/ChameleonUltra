#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "device_settings_payload_internal.h"

int main(void) {
    static const uint8_t pairing_key[DEVICE_SETTINGS_V6_PAIRING_KEY_SIZE] =
        {'6', '5', '4', '3', '2', '1'};
    static const uint8_t expected[DEVICE_SETTINGS_V6_PAYLOAD_SIZE] = {
        6, 3, 1, 2, 4, 5, 1, '6', '5', '4', '3', '2', '1', 42,
    };
    uint8_t payload[DEVICE_SETTINGS_V6_PAYLOAD_SIZE];
    memset(payload, 0xa5, sizeof(payload));

    device_settings_payload_v6(
        payload,
        6,
        3,
        1,
        2,
        4,
        5,
        1,
        pairing_key,
        42000u
    );

    assert(sizeof(payload) == 14u);
    assert(memcmp(payload, expected, sizeof(expected)) == 0);
    return 0;
}
