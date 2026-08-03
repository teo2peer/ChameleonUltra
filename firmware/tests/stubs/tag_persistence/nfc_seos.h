#ifndef TEST_NFC_SEOS_H
#define TEST_NFC_SEOS_H

#include <stdbool.h>
#include <stdint.h>

#include "rfid/nfctag/tag_emulation.h"

int nfc_tag_seos_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer);
int nfc_tag_seos_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool nfc_tag_seos_data_factory(uint8_t slot, tag_specific_type_t type);

#endif
