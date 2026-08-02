#pragma once

#include "rfid/nfctag/tag_emulation.h"

int nfc_tag_14a_4_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer);
int nfc_tag_14a_4_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool nfc_tag_14a_4_data_factory(uint8_t slot, tag_specific_type_t type);
