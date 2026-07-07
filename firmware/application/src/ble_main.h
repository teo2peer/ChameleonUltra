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
bool is_ble_advertising(void);
bool is_ble_scanning(void);  // true while the passive scanner is running
void delete_bonds_all(void);
void nus_data_response(uint8_t *p_data, uint16_t length);
bool is_nus_working(void);
void set_ble_connect_key(uint8_t *key);

void register_lf_adc_callback(lf_adc_callback_t cb);
void unregister_lf_adc_callback(void);

// ---------------------------------------------------------------------------
// BLE identity & radio toggle (added in the cybersecurity fork).
//
// Identity + radio power always mutate OUR radio (they're settings on us, so
// they don't have a "scope" selector — they're inherently local). The
// environment-wide-broadcast tools live in the BLE 7050 block and in
// ble_central.{h,c} (scan-buffer-wide kick/flood).
// ---------------------------------------------------------------------------

// Address-set modes for ble_addr_set().
typedef enum {
    BLE_ADDR_MODE_RESTORE_ORIGINAL = 0, // restore the FICR-derived original address
    BLE_ADDR_MODE_RANDOM_STATIC     = 1, // host-provided 6-byte static-random address
    BLE_ADDR_MODE_RANDOM_PRIVATE    = 2, // firmware-generated random private resolvable (RPA)
    BLE_ADDR_MODE_RANDOM_NONRESOLV  = 3, // firmware-generated random private non-resolvable
} ble_addr_mode_t;

// Apply a new BLE GAP address. mode selects the source:
//   0 - restore the original (NRF_FICR->DEVICEADDR with the 0xC000 static bit pattern)
//   1 - static-random from host (data[0..5] holds 6 bytes LE order)
//   2 - firmware-generated random private resolvable (cycles via SoftDevice)
//   3 - firmware-generated random private non-resolvable
// Returns NRF_SUCCESS, NRF_ERROR_BUSY (a link is active — caller should disconnect
// first), or NRF_ERROR_INVALID_PARAM.
uint32_t ble_addr_set(uint8_t mode, const uint8_t *addr_le);

// Read the currently-active BLE GAP address. addr_type out is one of
// BLE_GAP_ADDR_TYPE_*. addr_out receives 6 bytes in LE order.
uint32_t ble_addr_get(uint8_t *addr_type, uint8_t *addr_out);

// Toggle our own radio on/off. On: starts normal peripheral advertising.
// Off: stops advertising + scan + drops any active central link (silent / stealth).
uint32_t ble_radio_set(uint8_t on);

// State snapshot. out[0]=radio_on (1/0), out[1]=advertising, out[2]=scanning,
// out[3]=central link active.
uint32_t ble_radio_get(uint8_t *out);

// ---- scan-buffer snapshot (used by scan-buffer-wide stress / kick) -------
// Compact address+type pair, little-endian address, BLE_GAP_ADDR_TYPE_*.
typedef struct {
    uint8_t addr[BLE_GAP_ADDR_LEN];
    uint8_t addr_type;
} ble_scan_addr_t;

// Copy up to out_cap connectable addresses from the passive scanner's record
// buffer, strongest RSSI first. Returns the number actually written.
uint8_t  ble_scan_copy_addresses(ble_scan_addr_t *out, uint8_t out_cap);

// ---- environment-wide broadcast (full 2.4 GHz BLE spectrum spam) --------
// Start a non-connectable advertising flood. fill_byte fills the maximum
// payload (31-byte legacy / 255-byte extended). interval_ms is clamped to
// the regulatory minimum (~100ms for legacy non-connectable). Stops any
// normal peripheral advertising currently in progress and restores nothing
// on stop (the operator can re-enable normal adv with ble_radio_set /
// advertising_start afterwards).
uint32_t ble_adv_flood_start(uint8_t fill_byte, uint16_t interval_ms);
uint32_t ble_adv_flood_stop(void);

// ---------------------------------------------------------------------------
// Passive BLE scanner (SoftDevice observer role).
//
// Listen-only: scanning is started in PASSIVE mode (active=0), so the device
// never emits scan requests or any other packet. It purely receives the
// advertisements that nearby devices already broadcast. There is no central
// connection and no transmission of any kind here.
// ---------------------------------------------------------------------------
// Max distinct devices retained per passive scan. Shared so ble_central.c's
// batch link-probe can bound its iteration over the scanner records.
#define BLE_SCAN_MAX_DEVICES 40

uint16_t ble_link_mtu(uint16_t conn_handle); // effective ATT MTU for a connection

uint32_t ble_scan_start(uint8_t active); // start a scan (0=passive listen-only, 1=active)
uint32_t ble_scan_stop(void);    // stop the scan
uint8_t  ble_scan_get_count(void);
// Serialize discovered records starting at start_index into out (max out_cap
// bytes). Wire format per record: addr[6] | addr_type[1] | rssi[1] | adv_len[1]
// | adv[adv_len]. Returns the number of bytes written.
uint16_t ble_scan_copy_records(uint8_t start_index, uint8_t *out, uint16_t out_cap);

#endif
