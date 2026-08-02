#pragma once

#include "rfid/nfctag/tag_emulation.h"

int nfc_tag_mf0_ntag_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer);
int nfc_tag_mf0_ntag_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
void nfc_tag_mf0_ntag_data_save_failcb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool nfc_tag_mf0_ntag_data_factory(uint8_t slot, tag_specific_type_t type);
