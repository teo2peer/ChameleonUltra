#include <assert.h>
#include <stdint.h>

#include "crc_utils.h"

int main(void) {
    static const uint8_t halt[] = {0x50, 0x00};
    uint8_t storage[4] = {0xA5, 0, 0, 0x5A};

    calc_14a_crc_lut(halt, sizeof(halt), &storage[1]);
    assert(storage[0] == 0xA5);
    assert(storage[1] == 0x57);
    assert(storage[2] == 0xCD);
    assert(storage[3] == 0x5A);

    calc_14a_crc_lut(NULL, 0, &storage[1]);
    assert(storage[1] == 0x63);
    assert(storage[2] == 0x63);
    return 0;
}
