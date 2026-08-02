#ifndef NFC_14A_FRAME_INTERNAL_H
#define NFC_14A_FRAME_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "byte_mirror.h"

static inline size_t nfc_tag_14a_unwrap_frame_checked(
    const uint8_t *frame,
    size_t frame_bits,
    uint8_t *rx,
    uint8_t *rx_parity
) {
    if (frame_bits == 0u) return 0u;
    if (frame_bits < 9u) {
        *rx = *frame;
        return frame_bits;
    }

    size_t rx_bits = frame_bits - (frame_bits / 9u);
    size_t bits_left = frame_bits;
    uint32_t data_pos = 0u;
    const uint8_t *frame_pos = frame;

    while (1) {
        for (uint8_t bit_pos = 0u; bit_pos < 8u; bit_pos++) {
            uint8_t frame_byte = byte_mirror[frame_pos[data_pos]];
            uint8_t data_byte = frame_byte << bit_pos;
            frame_byte = byte_mirror[frame_pos[data_pos + 1u]];
            data_byte |= frame_byte >> (8u - bit_pos);
            rx[data_pos] = byte_mirror[data_byte];
            if (rx_parity != NULL) {
                rx_parity[data_pos] = (frame_byte >> (7u - bit_pos)) & 0x01u;
            }
            data_pos++;
            bits_left -= 9u;
            if (bits_left < 9u) {
                if (bits_left > 0u) {
                    uint8_t trailing = 0u;
                    size_t first_bit = (size_t)data_pos * 9u;
                    for (uint8_t bit = 0u; bit < bits_left; bit++) {
                        size_t source_bit = first_bit + bit;
                        trailing |= ((frame[source_bit / 8u] >> (source_bit % 8u)) & 0x01u) << bit;
                    }
                    rx[data_pos] = trailing;
                }
                return rx_bits;
            }
        }
        frame_pos++;
    }
}

#endif
