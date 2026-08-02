#ifndef ISO_DEP_READER_H
#define ISO_DEP_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rc522.h"

#define ISO_DEP_READER_MAX_APDU_RESPONSE 512u
#define ISO_DEP_READER_MAX_CHAIN_BLOCKS  32u
#define ISO_DEP_READER_MAX_WTX           64u
#define ISO_DEP_READER_MAX_WTX_TOTAL_MS  5000u

typedef enum {
    ISO_DEP_OK = 0,
    ISO_DEP_ERR_PARAM,
    ISO_DEP_ERR_TRANSPORT,
    ISO_DEP_ERR_CRC,
    ISO_DEP_ERR_BLOCK,
    ISO_DEP_ERR_SEQUENCE,
    ISO_DEP_ERR_OVERFLOW,
    ISO_DEP_ERR_TIMEOUT,
} iso_dep_error_t;

typedef struct {
    uint8_t block_num;    // next PCD I-block number (kept for command diagnostics)
    uint8_t rx_block_num; // next expected PICC I-block number
    uint8_t cid;
    uint8_t picc_fsc;
    uint16_t frame_timeout_ms;
    bool cid_enabled;
} iso_dep_reader_t;

typedef struct {
    iso_dep_error_t error;
    uint8_t rf_status;
    uint8_t chain_blocks;
    uint8_t wtx_count;
    uint16_t response_len;
} iso_dep_result_t;

void iso_dep_reader_init(iso_dep_reader_t *reader, const picc_14a_tag_t *tag);
bool iso_dep_reader_transceive(iso_dep_reader_t *reader,
                               const uint8_t *apdu, uint16_t apdu_len,
                               uint8_t *response, uint16_t response_capacity,
                               iso_dep_result_t *result);
void iso_dep_reader_deselect(iso_dep_reader_t *reader);

#endif
