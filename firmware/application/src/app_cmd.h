#ifndef APP_CMD_H
#define APP_CMD_H

#include <stddef.h>
#include <stdint.h>
#include "dataframe.h"


typedef data_frame_tx_t *(*cmd_processor)(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);

typedef struct {
    uint16_t cmd;
    cmd_processor cmd_before;
    cmd_processor cmd_processor;
    cmd_processor cmd_after;
} cmd_data_map_t;

static inline bool cmd_payload_exact(uint16_t length, const uint8_t *data, uint16_t expected) {
    return length == expected && (expected == 0u || data != NULL);
}

static inline bool cmd_payload_empty(uint16_t length) {
    return length == 0u;
}

static inline bool cmd_parse_bool(uint8_t value, bool *out) {
    if (value > 1u) return false;
    *out = value != 0u;
    return true;
}

static inline uint16_t cmd_read_u16be(const uint8_t *data) {
    return ((uint16_t)data[0] << 8) | data[1];
}

static inline uint32_t cmd_read_u32be(const uint8_t *data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

static inline bool cmd_parse_count_u8(uint16_t length, uint16_t offset,
                                      uint8_t item_size, uint8_t *count) {
    if (item_size == 0u || length < offset ||
            ((length - offset) % item_size) != 0u) return false;

    uint16_t parsed = (length - offset) / item_size;
    if (parsed == 0u || parsed > UINT8_MAX) return false;
    *count = (uint8_t)parsed;
    return true;
}

void on_data_frame_received(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);

#endif
