#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*nfc_tag_14a_sniff_cb_t)(const uint8_t *, uint16_t, uint8_t);
typedef void (*nfc_tag_14a_field_sniff_cb_t)(bool);

void nfc_tag_14a_set_sniff_cb(nfc_tag_14a_sniff_cb_t callback);
void nfc_tag_14a_clear_sniff_cb(void);
void nfc_tag_14a_set_tx_sniff_cb(nfc_tag_14a_sniff_cb_t callback);
void nfc_tag_14a_clear_tx_sniff_cb(void);
void nfc_tag_14a_set_field_sniff_cb(nfc_tag_14a_field_sniff_cb_t callback);
void nfc_tag_14a_clear_field_sniff_cb(void);
void nfc_tag_14a_set_sniff_passive(bool passive);
void nfc_tag_14a_sense_switch(bool enable);
