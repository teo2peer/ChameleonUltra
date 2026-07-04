#ifndef BLE_CENTRAL_H
#define BLE_CENTRAL_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Directed BLE GATT fuzzing harness (SoftDevice central role).
//
// SCOPE / DESIGN CONSTRAINT: this connects point-to-point to ONE target device,
// specified explicitly by its BLE address by the operator. It enumerates that
// single target's GATT characteristics and writes mutated ("fuzz") payloads to a
// chosen characteristic to exercise the target's input handling. It NEVER
// broadcasts, floods, or emits anything to the wider environment — every write
// goes only to the one connected target. This is a tool to audit a device you
// own/control, not to disrupt nearby devices. ble_central_disconnect() drops the
// link so the target is free to reconnect to its normal source.
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

// Read a characteristic value from the connected target (async — poll copy_read).
uint32_t ble_central_gatt_read(uint16_t value_handle);
// Serialize the last read result. Wire: state[1] | gatt_status[1] | len[1] | data[len].
// state: 0 idle, 1 pending, 2 ready.
uint16_t ble_central_copy_read(uint8_t *out, uint16_t out_cap);

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

// Serialize harness state. Wire format:
// conn_state[1] disc_state[1] char_count[1] fuzz_state[1] fuzz_sent[2 BE]
// target_alive[1] last_disconnect_reason[1] probe_state[1] probe_result[1]
// probe_index[1] probe_total[1].
// conn_state: 0 idle, 1 connecting, 2 connected, 3 disconnected
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
