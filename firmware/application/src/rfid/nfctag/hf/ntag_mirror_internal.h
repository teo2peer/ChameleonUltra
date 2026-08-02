#ifndef NTAG_MIRROR_INTERNAL_H
#define NTAG_MIRROR_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define NTAG_MIRROR_PAGE_SIZE 4u
#define NTAG_MIRROR_UID_SIZE 14u
#define NTAG_MIRROR_COUNTER_SIZE 6u
#define NTAG_MIRROR_UID_COUNTER_SIZE 21u

static inline uint8_t ntag_mirror_hex_digit(uint8_t value) {
    return (uint8_t)(value < 10u ? ('0' + value) : ('A' + value - 10u));
}

static inline void ntag_mirror_bytes_to_hex(
    const uint8_t *bytes,
    size_t length,
    uint8_t *output
) {
    for (size_t index = 0; index < length; index++) {
        output[index * 2u] = ntag_mirror_hex_digit(bytes[index] >> 4u);
        output[index * 2u + 1u] = ntag_mirror_hex_digit(bytes[index] & 0x0fu);
    }
}

static inline void ntag_mirror_render_uid(const uint8_t uid[7], uint8_t output[NTAG_MIRROR_UID_SIZE]) {
    ntag_mirror_bytes_to_hex(uid, 7u, output);
}

static inline void ntag_mirror_render_counter(
    const uint8_t counter[3],
    uint8_t output[NTAG_MIRROR_COUNTER_SIZE]
) {
    ntag_mirror_bytes_to_hex(counter, 3u, output);
}

static inline void ntag_mirror_render_uid_counter(
    const uint8_t uid[7],
    const uint8_t counter[3],
    uint8_t output[NTAG_MIRROR_UID_COUNTER_SIZE]
) {
    ntag_mirror_render_uid(uid, output);
    output[NTAG_MIRROR_UID_SIZE] = 'x';
    ntag_mirror_render_counter(counter, &output[NTAG_MIRROR_UID_SIZE + 1u]);
}

static inline size_t ntag_mirror_overlay_page(
    uint8_t page[NTAG_MIRROR_PAGE_SIZE],
    uint8_t page_number,
    uint8_t first_page,
    uint8_t first_page_offset,
    const uint8_t *mirror,
    size_t mirror_size
) {
    if (page == NULL || mirror == NULL || page_number < first_page ||
        first_page_offset >= NTAG_MIRROR_PAGE_SIZE) {
        return 0u;
    }

    size_t page_offset = (size_t)(page_number - first_page) * NTAG_MIRROR_PAGE_SIZE;
    size_t destination_offset = page_offset == 0u ? first_page_offset : 0u;
    if (page_offset != 0u) {
        page_offset -= first_page_offset;
    }
    if (page_offset >= mirror_size) {
        return 0u;
    }

    size_t copy_size = mirror_size - page_offset;
    size_t destination_remaining = NTAG_MIRROR_PAGE_SIZE - destination_offset;
    if (copy_size > destination_remaining) {
        copy_size = destination_remaining;
    }
    memcpy(&page[destination_offset], &mirror[page_offset], copy_size);
    return copy_size;
}

#endif
