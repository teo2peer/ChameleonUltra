#ifndef BLE_MAIN_H
#define BLE_MAIN_H

#include "ble_bas.h"
#include "ble_gatts.h"
#include "ble_nus.h"
#include "nrfx_saadc.h"

extern uint16_t batt_lvl_in_milli_volts;
extern uint8_t percentage_batt_lvl;

typedef void (*lf_adc_callback_t)(nrf_saadc_value_t *, size_t);

void ble_slave_init(void);
void advertising_start(bool erase_bonds);
void advertising_stop(void);
void delete_bonds_all(void);
void nus_data_response(uint8_t *p_data, uint16_t length);
bool is_nus_working(void);
void set_ble_connect_key(uint8_t *key);

void register_lf_adc_callback(lf_adc_callback_t cb);
void unregister_lf_adc_callback(void);

// ---------------------------------------------------------------------------
// Passive BLE scanner (SoftDevice observer role).
//
// Listen-only: scanning is started in PASSIVE mode (active=0), so the device
// never emits scan requests or any other packet. It purely receives the
// advertisements that nearby devices already broadcast. There is no central
// connection and no transmission of any kind here.
// ---------------------------------------------------------------------------
uint32_t ble_scan_start(void);   // clear results and start a passive scan
uint32_t ble_scan_stop(void);    // stop the passive scan
uint8_t  ble_scan_get_count(void);
// Serialize discovered records starting at start_index into out (max out_cap
// bytes). Wire format per record: addr[6] | addr_type[1] | rssi[1] | adv_len[1]
// | adv[adv_len]. Returns the number of bytes written.
uint16_t ble_scan_copy_records(uint8_t start_index, uint8_t *out, uint16_t out_cap);

#endif
