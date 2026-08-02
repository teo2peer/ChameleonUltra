#ifndef MF1_KEY_ACCESS_INTERNAL_H
#define MF1_KEY_ACCESS_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MF1_KEY_ACCESS_SIZE 6u

static inline bool mf1_copy_nonzero_key(
    uint8_t destination[MF1_KEY_ACCESS_SIZE],
    const uint8_t source[MF1_KEY_ACCESS_SIZE]
) {
    uint8_t nonzero = 0u;
    for (size_t index = 0; index < MF1_KEY_ACCESS_SIZE; index++) {
        nonzero |= source[index];
    }
    if (nonzero == 0u) {
        return false;
    }
    memcpy(destination, source, MF1_KEY_ACCESS_SIZE);
    return true;
}

#endif
