#ifndef BLE_CENTRAL_H
#define BLE_CENTRAL_H

#include <stdbool.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Directed BLE GATT fuzzing harness (SoftDevice central role).
//
// SCOPE / DESIGN CONSTRAINT: this connects point-to-point to ONE target device or environment-wide to all targets discovered by the passive scanner. It is,
// specified explicitly by its BLE address by the operator. It enumerates that
//  GATT characteristics and writes mutated ("fuzz") payloads to a
// chosen characteristic to exercise the target's input handling. It is allowed
// to broadcast or flood.  This is a tool to audit a device you own or an trusted enviroment.
// own/control. ble_central_disconnect() drops the link and frees the target.
// ---------------------------------------------------------------------------

// Create the fuzz timer. Call once after app_timer is initialised.
void     ble_central_init(void);

// Connect to a single target. addr_type is a BLE_GAP_ADDR_TYPE_*; addr is 6 bytes
// (little-endian, as reported by the scanner). Connection completes asynchronously
// — poll ble_central_get_state().
uint32_t ble_central_connect(uint8_t addr_type, const uint8_t *addr);

// Disconnect from (or cancel connecting to) the target. Frees the target.
uint32_t ble_central_disconnect(void);

// Start enumerating the connected target's GATT characteristics (async).
uint32_t ble_central_gatt_discover(void);

uint8_t  ble_central_get_char_count(void);
// Serialize discovered characteristics from start index. Wire format per char:
// value_handle[2 BE] | props[1] | uuid_type[1] | uuid[2 BE]. Returns bytes written.
uint16_t ble_central_copy_chars(uint8_t start_index, uint8_t *out, uint16_t out_cap);

// Enumerate ALL descriptors of the connected target (async — poll copy_descs).
uint32_t ble_central_desc_discover(void);
// Wire: state[1] | per descriptor: handle[2 BE] | uuid_type[1] | uuid[2 BE].
uint16_t ble_central_copy_descs(uint8_t start_index, uint8_t *out, uint16_t out_cap);

// Discover the target's primary services (async — poll copy_svcs).
uint32_t ble_central_svc_discover(void);
// Wire: state[1] | per service: uuid_type[1] | uuid[2 BE] | start[2 BE] | end[2 BE].
uint16_t ble_central_copy_svcs(uint8_t start_index, uint8_t *out, uint16_t out_cap);

// Pull the connected target's standard informational characteristics (GAP name /
// appearance, Device Information Service fields, battery level) with read-only GATT
// reads. Requires 'discover' first (handles come from the char table). Async —
// poll ble_central_copy_devinfo().
uint32_t ble_central_devinfo_start(void);
// Serialize collected device-info. Wire: state[1] | count[1] | per field:
// uuid[2 BE] | status[1] | len[1] | data[len]. state: 0 idle,1 running,2 done,3 error.
// status: 0xFF = characteristic absent, else the ATT read status (0 = ok).
uint16_t ble_central_copy_devinfo(uint8_t *out, uint16_t out_cap);

// Read a characteristic value from the connected target (async — poll copy_read).
uint32_t ble_central_gatt_read(uint16_t value_handle);
// Serialize the last read result. Wire: state[1] | gatt_status[1] | len[1] | data[len].
// state: 0 idle, 1 pending, 2 ready, 3 failed.
uint16_t ble_central_copy_read(uint8_t *out, uint16_t out_cap);

// Maximum payload writable in a single GATT write (<= negotiated ATT_MTU - 3).
// The command layer rejects longer writes rather than silently truncating them.
#define BLE_CENTRAL_WRITE_MAX       244

// Write a value to a characteristic (write-with-response; async — poll get_write_result).
uint32_t ble_central_gatt_write(uint16_t value_handle, const uint8_t *data, uint8_t len);
// Serialize the last write result. Wire: state[1] | gatt_status[1].
// state: 0 idle, 1 pending, 2 done, 3 failed.
uint16_t ble_central_get_write_result(uint8_t *out, uint16_t out_cap);

// Effective ATT MTU of the target link (23 until negotiated / when not connected).
uint16_t ble_central_mtu(void);
// Current maximum single-write payload (negotiated ATT MTU - 3, capped by storage).
uint16_t ble_central_write_max(void);

// Central link state queries (used by ble_radio_set() to safely tear down the
// link before / after a radio toggle).
bool ble_central_is_connected(void);   // true if a central link is currently up
bool ble_central_is_connecting(void);  // true if a connection attempt is in flight

// Find a characteristic's CCCD descriptor (async — poll get_cccd), so subscribe
// can use the real handle instead of assuming value_handle + 1.
uint32_t ble_central_find_cccd(uint16_t value_handle);
// Serialize the CCCD lookup result. Wire: state[1] | cccd_handle[2 BE].
// state: 0 idle, 1 searching, 2 found, 3 not-found.
uint16_t ble_central_get_cccd(uint8_t *out, uint16_t out_cap);

// Subscribe to notifications/indications on the connected target by writing its
// CCCD. mode: 0 = off, 1 = notifications, 2 = indications. Receive-only.
uint32_t ble_central_subscribe(uint16_t cccd_handle, uint8_t mode);
uint16_t ble_central_notif_count(void);
// Serialize received notifications from start index. Wire per entry:
// handle[2 BE] | len[1] | data[len].
uint16_t ble_central_copy_notifs(uint16_t start_index, uint8_t *out, uint16_t out_cap);

// Start a native BLE link probe. When global_mode is 0 this probes the
// currently connected target. When global_mode is non-zero it runs a batch
// probe across the devices discovered by the passive BLE scanner.
uint32_t ble_central_link_probe(uint8_t global_mode);

// Start fuzzing: write mutated payloads to value_handle every interval_ms, up to
// max_iterations (0 = until stopped). Must be connected.
uint32_t ble_central_fuzz_start(uint16_t value_handle, uint16_t max_iterations, uint16_t interval_ms);
uint32_t ble_central_fuzz_stop(void);

// ---- flood / link-churn (cybersecurity fork) ------------------------------
// Point-to-point stress against ONE already-connected target or environment-wide if no target is selected. payload_size bytes per WRITE_CMD, max_iterations
// (0 = until stop), interval_ms between ticks. Reuses the fuzz timer, so
// starting a flood stops an in-progress fuzzer and vice versa.
uint32_t ble_central_flood_start(uint16_t value_handle, uint8_t payload_size,
                                 uint16_t max_iterations, uint16_t interval_ms);
uint32_t ble_central_flood_stop(void);
uint32_t ble_central_flood_count(void);  // total WRITE_CMDs accepted by the stack

// Force-disconnect target links to test how peer auto-reconnect / back-off
// behaves under churn.
//   ble_central_kick()             = current central link, exactly one cycle
//   ble_central_kick_scan_buffer() = every address cached by the passive
//                                    scanner (1..10 cycles per peer)
uint32_t ble_central_kick(uint8_t cycles);
uint32_t ble_central_kick_scan_buffer(uint8_t cycles);

// Scan-buffer-wide WRITE_CMD flood. Connects to every connectable cached address
// in RSSI order, runs the WRITE_CMD spam on each, then disconnects and moves on.
// max_iterations is per peer; 0 maps to a bounded firmware default so a buffer
// run cannot hang forever on the first address.
uint32_t ble_central_flood_scan_buffer(uint16_t value_handle, uint8_t payload_size,
                                       uint16_t max_iterations, uint16_t interval_ms);

// Serialize harness state. Wire format:
// conn_state[1] disc_state[1] char_count[1] fuzz_state[1] fuzz_sent[2 BE]
// target_alive[1] last_disconnect_reason[1] probe_state[1] probe_result[1]
// probe_index[1] probe_total[1]. Newer firmware appends, when output capacity permits:
// flood_state[1] flood_sent[4 BE] read_state[1] write_state[1] notif_count[2 BE].
// conn_state: 0 idle, 1 connecting, 2 connected, 3 disconnected,
// 4 cancelling (awaiting GAP timeout), 5 disconnecting (awaiting GAP disconnect)
// disc_state: 0 idle, 1 discovering, 2 done, 3 error
// fuzz_state: 0 idle, 1 running, 2 stopped/finished
uint16_t ble_central_get_state(uint8_t *out, uint16_t out_cap);

// Serialize the probe batch log from start index. Wire format per entry:
// addr[6] | addr_type[1] | rssi[1] | connect_status[1] | probe_result[1] |
// disconnect_reason[1]. Returns bytes written.
uint16_t ble_central_copy_probe_log(uint8_t start_index, uint8_t *out, uint16_t out_cap);

// Serialize fuzz log from start index. Wire format per entry:
// index[2 BE] | payload_len[1] | write_status[1] | data[min(payload_len, 16)].
uint16_t ble_central_copy_log(uint16_t start_index, uint8_t *out, uint16_t out_cap);

#endif
