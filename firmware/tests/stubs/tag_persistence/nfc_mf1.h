#pragma once

#include "rfid/nfctag/tag_emulation.h"

typedef enum {
    NFC_TAG_MF1_WRITE_NORMAL = 0u,
    NFC_TAG_MF1_WRITE_DENIED = 1u,
    NFC_TAG_MF1_WRITE_DECEIVE = 2u,
    NFC_TAG_MF1_WRITE_SHADOW = 3u,
    NFC_TAG_MF1_WRITE_SHADOW_REQ = 4u,
} nfc_tag_mf1_write_mode_t;

int nfc_tag_mf1_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer);
int nfc_tag_mf1_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
void nfc_tag_mf1_data_save_failcb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool nfc_tag_mf1_data_factory(uint8_t slot, tag_specific_type_t type);
nfc_tag_mf1_write_mode_t nfc_tag_mf1_get_write_mode(void);
