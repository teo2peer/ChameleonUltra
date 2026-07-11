#ifndef BLE_SCAN_H
#define BLE_SCAN_H

#include <stdbool.h>
#include <stdint.h>

#include "ble_gap.h"

#define BLE_SCAN_MAX_DEVICES 40

typedef struct {
    uint8_t addr[BLE_GAP_ADDR_LEN];
    uint8_t addr_type;
} ble_scan_addr_t;

uint32_t ble_scan_start(uint8_t active);
uint32_t ble_scan_stop(void);
bool is_ble_scanning(void);
uint8_t ble_scan_get_mode(void);
// Synchronize logical state when another GAP procedure implicitly stops scanning.
void ble_scan_mark_inactive(void);
uint8_t ble_scan_get_count(void);

// Wire format per record: addr[6] | addr_type[1] | rssi[1] | adv_len[1]
// | adv[adv_len].
uint16_t ble_scan_copy_records(uint8_t start_index, uint8_t *out, uint16_t out_cap);

// Copy connectable addresses ordered by strongest RSSI first. Copy APIs use a
// coherent point-in-time snapshot while advertising reports continue arriving.
uint8_t ble_scan_copy_addresses(ble_scan_addr_t *out, uint8_t out_cap);

// Called by ble_main's existing SoftDevice observer for each advertising report.
void ble_scan_on_adv_report(const ble_gap_evt_adv_report_t *report);

#endif
