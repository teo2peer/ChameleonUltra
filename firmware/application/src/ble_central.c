/*
 * Central-role BLE GATT harness (SoftDevice S140) + scan-buffer-wide
 * iterator (cybersecurity fork).
 *
 * Scope is operator-selectable per call (CLAUDE.md fork-specific exemption —
 * operator-authorised). ble_central.h documents the per-command contract;
 * the same module implements single-target (the operator-specified central
 * link) and scan-buffer-wide (iterating every address cached by the passive
 * scanner, connecting → exercising → disconnecting each in turn).
 *
 * The firmware holds one central link at a time. Long-running output
 * (discovered characteristics, fuzz / flood / kick log) is buffered here
 * and paged out by the host, matching the request/response
 * command protocol used everywhere else in this firmware.
 */

#include <string.h>

#include "nrf.h"
#include "nordic_common.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_gattc.h"
#include "ble_hci.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "app_error.h"

#include "ble_main.h"
#include "ble_scan.h"
#include "ble_central.h"
#include "rgb_marquee.h"

#define NRF_LOG_MODULE_NAME ble_central
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define APP_BLE_CONN_CFG_TAG        1       // must match the tag used in ble_main.c
#define BLE_CENTRAL_OBSERVER_PRIO   2

#define BLE_PROBE_BATCH_MAX         40
#define BLE_PROBE_ADV_DATA_MAX      31

#define BLE_MAX_CHARS               48      // discovered characteristics retained
#define BLE_FUZZ_LOG_MAX            128     // fuzz-log entries retained
#define BLE_FUZZ_PAYLOAD_MAX        20      // <= default ATT_MTU(23) - 3, avoids DATA_SIZE
#define BLE_FUZZ_LOG_DATA           16      // payload bytes kept per log entry
#define BLE_WRITE_MAX               BLE_CENTRAL_WRITE_MAX   // <= max negotiated ATT_MTU(247) - 3
#define BLE_READ_VALUE_MAX          244     // bytes retained from a GATT read response
#define BLE_NOTIF_LOG_MAX           64      // received notifications/indications retained
#define BLE_NOTIF_DATA_MAX          20      // bytes kept per notification
#define BLE_MAX_DESCS               48      // descriptors retained per full listing
#define BLE_MAX_SERVICES            16      // primary services retained
#define BLE_DEVINFO_VAL_MAX         32      // bytes retained per standard info field

#define BLE_SB_OP_NONE              0
#define BLE_SB_OP_KICK              1
#define BLE_SB_OP_FLOOD             2
#define BLE_SB_PHASE_IDLE           0
#define BLE_SB_PHASE_CONNECTING     1
#define BLE_SB_PHASE_ACTIVE         2
#define BLE_SB_PHASE_DISCONNECTING  3
#define BLE_SB_TICK_MS              50
#define BLE_SB_CONNECT_TIMEOUT_MS   6000
#define BLE_SB_DISCONNECT_TIMEOUT_MS 2000
#define BLE_SB_FLOOD_DEFAULT_COUNT  200     // bounded per peer when host asks for 0=infinite
#define BLE_GATT_TRANSIENT_RETRY_MAX 20

enum {
    BLE_CONN_STATE_IDLE = 0,
    BLE_CONN_STATE_CONNECTING = 1,
    BLE_CONN_STATE_CONNECTED = 2,
    BLE_CONN_STATE_DISCONNECTED = 3,
    BLE_CONN_STATE_CANCELLING = 4,
    BLE_CONN_STATE_DISCONNECTING = 5,
};

typedef enum {
    BLE_GATT_OP_NONE = 0,
    BLE_GATT_OP_CHAR_DISC,
    BLE_GATT_OP_DESC_DISC,
    BLE_GATT_OP_SVC_DISC,
    BLE_GATT_OP_READ,
    BLE_GATT_OP_WRITE,
    BLE_GATT_OP_CCCD_DISC,
    BLE_GATT_OP_SUBSCRIBE,
    BLE_GATT_OP_DEVINFO,
} ble_gatt_op_t;

typedef enum {
    BLE_TIMER_OWNER_NONE = 0,
    BLE_TIMER_OWNER_FUZZ,
    BLE_TIMER_OWNER_FLOOD,
    BLE_TIMER_OWNER_SCAN_BUFFER,
} ble_timer_owner_t;

// ---- discovered-characteristic table -------------------------------------
typedef struct {
    uint16_t value_handle;
    uint8_t  props;             // packed: bit0 broadcast,1 read,2 write_wo_resp,3 write,4 notify,5 indicate,6 auth_signed_wr
    uint8_t  uuid_type;
    uint16_t uuid;
} ble_char_rec_t;

// ---- discovered-descriptor table (full listing) --------------------------
typedef struct {
    uint16_t handle;
    uint16_t uuid;
    uint8_t  uuid_type;
} ble_desc_rec_t;

// ---- discovered primary-service table ------------------------------------
typedef struct {
    uint16_t uuid;
    uint8_t  uuid_type;
    uint16_t start_handle;
    uint16_t end_handle;
} ble_svc_rec_t;

// ---- standard device-info collection -------------------------------------
// Read-only pull of a target's standard GATT profile fields (Generic Access
// name/appearance, Device Information Service, battery level). Each entry maps a
// SIG characteristic UUID to the value handle found in the discovered-char table.
typedef struct {
    uint16_t uuid;              // SIG characteristic UUID (e.g. 0x2A29 manufacturer)
    uint16_t handle;            // value handle from discovery (0 = not exposed by target)
    uint8_t  status;            // 0xFF = absent/not read, else gatt_status of the read (0 = ok)
    uint8_t  len;
    uint8_t  data[BLE_DEVINFO_VAL_MAX];
} ble_devinfo_rec_t;

// ---- fuzz log ------------------------------------------------------------
typedef struct {
    uint16_t index;
    uint8_t  len;               // actual payload length sent
    uint8_t  status;            // 0 = write accepted by stack, else low byte of nrf error
    uint8_t  data[BLE_FUZZ_LOG_DATA];
} ble_fuzz_log_t;

typedef struct {
    uint8_t addr[BLE_GAP_ADDR_LEN];
    uint8_t addr_type;
    int8_t  rssi;
} ble_probe_target_t;

typedef struct {
    uint8_t addr[BLE_GAP_ADDR_LEN];
    uint8_t addr_type;
    int8_t  rssi;
    uint8_t connect_status;
    uint8_t probe_result;
    uint8_t disconnect_reason;
} ble_probe_log_t;

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
static volatile uint8_t m_conn_state = BLE_CONN_STATE_IDLE;
static volatile uint8_t m_disc_state = 0;   // 0 idle,1 discovering,2 done,3 error
static uint8_t          m_last_disc_reason = 0;
static volatile ble_gatt_op_t m_gatt_op = BLE_GATT_OP_NONE;

static ble_char_rec_t   m_chars[BLE_MAX_CHARS];
static volatile uint8_t m_char_count = 0;

static ble_desc_rec_t   m_descs[BLE_MAX_DESCS];
static volatile uint8_t m_desc_count = 0;
static volatile uint8_t m_desc_state = 0;    // 0 idle,1 discovering,2 done,3 error

static ble_svc_rec_t    m_svcs[BLE_MAX_SERVICES];
static volatile uint8_t m_svc_count = 0;
static volatile uint8_t m_svc_state = 0;     // 0 idle,1 discovering,2 done,3 error

// Standard informational characteristics we surface, in display order.
static const uint16_t k_devinfo_uuids[] = {
    0x2A00,  // Device Name              (Generic Access)
    0x2A01,  // Appearance               (Generic Access)
    0x2A29,  // Manufacturer Name String (Device Information)
    0x2A24,  // Model Number String
    0x2A25,  // Serial Number String
    0x2A27,  // Hardware Revision String
    0x2A26,  // Firmware Revision String
    0x2A28,  // Software Revision String
    0x2A23,  // System ID
    0x2A50,  // PnP ID
    0x2A19,  // Battery Level            (Battery Service)
};
#define BLE_DEVINFO_COUNT (sizeof(k_devinfo_uuids) / sizeof(k_devinfo_uuids[0]))

static ble_devinfo_rec_t m_devinfo[BLE_DEVINFO_COUNT];
static volatile uint8_t  m_devinfo_state = 0; // 0 idle,1 running,2 done,3 error
static volatile uint8_t  m_devinfo_idx = 0;   // field currently being read

static volatile uint8_t  m_fuzz_state = 0;  // 0 idle,1 running,2 stopped/finished
static uint16_t          m_fuzz_handle = 0;
static uint16_t          m_fuzz_max = 0;
static volatile uint16_t m_fuzz_sent = 0;
static ble_fuzz_log_t    m_fuzz_log[BLE_FUZZ_LOG_MAX];
static volatile uint16_t m_fuzz_log_count = 0;
static uint32_t          m_fuzz_seed = 0x1234ABCDu;

// ---- flood / link-churn (cybersecurity fork) ------------------------------
// Reuses the m_fuzz_timer for tick scheduling. State is independent so that
// fuzz and flood don't trample each other (only one of them runs at a time;
// starting one implicitly stops the other).
static volatile uint8_t  m_flood_state = 0;   // 0 idle,1 running,2 stopped
static uint16_t          m_flood_handle = 0;
static uint8_t           m_flood_size = 0;    // payload bytes per write (1..MTU-3)
static uint8_t           m_flood_payload[BLE_FUZZ_PAYLOAD_MAX];
static uint16_t          m_flood_max = 0;
static volatile uint32_t m_flood_sent = 0;
static uint8_t           m_gatt_transient_retries = 0;

// Scan-buffer-wide kick / flood iterator. Shares m_fuzz_timer with the fuzzer
// and the single-link WRITE_CMD flood; the timer handler dispatches by state.
static volatile uint8_t  m_kick_sb_state  = 0; // 0 idle, 1 iterating
static uint8_t           m_sb_op          = BLE_SB_OP_NONE;
static uint8_t           m_sb_phase       = BLE_SB_PHASE_IDLE;
static uint8_t           m_kick_sb_cycles = 0;
static uint8_t           m_kick_sb_index  = 0; // current peer index
static uint8_t           m_kick_sb_sub    = 0; // completed reconnect/kick cycles for current peer
static uint16_t          m_sb_peer_sent   = 0; // flood writes sent to current peer
static uint16_t          m_sb_peer_limit  = 0; // bounded per-peer flood limit
static uint16_t          m_sb_wait_ticks  = 0;
static uint16_t          m_sb_flood_stride = 1; // fixed 50 ms scheduler ticks between writes
static uint16_t          m_sb_flood_tick   = 0;
static uint16_t          m_sb_conn_timeout_ticks = (BLE_SB_CONNECT_TIMEOUT_MS / BLE_SB_TICK_MS);
static uint16_t          m_sb_disc_timeout_ticks = (BLE_SB_DISCONNECT_TIMEOUT_MS / BLE_SB_TICK_MS);
static ble_scan_addr_t   m_kick_sb_addrs[BLE_SCAN_MAX_DEVICES];
static uint8_t           m_kick_sb_count  = 0;

// Forward declaration so fuzz_timer_handler() can dispatch to it before it
// is defined further down in the file.
static void scan_buffer_stop(bool disconnect_link);
static void scan_buffer_timer_handler(void *p_context);

static volatile uint8_t  m_probe_state = 0;  // 0 idle,1 probing,2 done,3 error
static uint8_t           m_probe_result = 0;
static volatile uint8_t   m_probe_index = 0;
static volatile uint8_t   m_probe_total = 0;
static volatile uint8_t   m_probe_global_mode = 0; // 0 targeted, 1 global batch
static ble_probe_target_t m_probe_targets[BLE_PROBE_BATCH_MAX];
static ble_probe_log_t    m_probe_log[BLE_PROBE_BATCH_MAX];
static volatile uint8_t   m_probe_log_count = 0;
static uint8_t            m_probe_current_index = 0;
static ble_probe_target_t m_probe_current_target;
static volatile uint8_t   m_probe_current_ok = 0;

// Last GATT read result (from the single connected target).
static volatile uint8_t m_read_state = 0;   // 0 idle,1 pending,2 ready
static uint8_t          m_read_status = 0;  // gatt_status of the last read
static uint16_t         m_read_len = 0;
static uint8_t          m_read_value[BLE_READ_VALUE_MAX];

// Last GATT write result (write-with-response to the connected target).
static volatile uint8_t m_write_state = 0;  // 0 idle,1 pending,2 done
static uint8_t          m_write_status = 0; // gatt_status returned by the target

// Received notifications/indications from the connected target (receive-only).
typedef struct {
    uint16_t handle;
    uint8_t  len;
    uint8_t  data[BLE_NOTIF_DATA_MAX];
} ble_notif_t;
static ble_notif_t       m_notif_log[BLE_NOTIF_LOG_MAX];
static volatile uint16_t m_notif_count = 0;

// CCCD lookup for a characteristic (so 'subscribe' uses the real descriptor
// handle instead of assuming value_handle + 1).
static volatile uint8_t  m_cccd_state = 0;   // 0 idle, 1 searching, 2 found, 3 not-found
static uint16_t          m_cccd_handle = 0;

APP_TIMER_DEF(m_fuzz_timer);
static volatile ble_timer_owner_t m_timer_owner = BLE_TIMER_OWNER_NONE;

// Passive scan parameters used only to locate the target during connection
// establishment. active=0 => no scan requests are transmitted.
static const ble_gap_scan_params_t m_init_scan_params = {
    .active        = 0,
    .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
    .scan_phys     = BLE_GAP_PHY_1MBPS,
    .interval      = 0x00A0,    // 100 ms (0.625 ms units)
    .window        = 0x0050,    // 50 ms
    .timeout       = 500,       // 5 s to establish (10 ms units)
};

static const ble_gap_conn_params_t m_conn_params = {
    .min_conn_interval = MSEC_TO_UNITS(20, UNIT_1_25_MS),
    .max_conn_interval = MSEC_TO_UNITS(75, UNIT_1_25_MS),
    .slave_latency     = 0,
    .conn_sup_timeout  = MSEC_TO_UNITS(4000, UNIT_10_MS),
};

static uint8_t char_props_to_byte(ble_gatt_char_props_t p) {
    uint8_t b = 0;
    if (p.broadcast)      b |= 0x01;
    if (p.read)           b |= 0x02;
    if (p.write_wo_resp)  b |= 0x04;
    if (p.write)          b |= 0x08;
    if (p.notify)         b |= 0x10;
    if (p.indicate)       b |= 0x20;
    if (p.auth_signed_wr) b |= 0x40;
    return b;
}

static uint32_t xorshift32(void) {
    uint32_t x = m_fuzz_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    m_fuzz_seed = x;
    return x;
}

// "Interesting" byte values that disproportionately trigger parser bugs:
// signed/length boundaries, bit patterns, and common delimiter/format bytes
// (NUL, LF, CR, space, '%', '/', '\\', 'A').
static const uint8_t k_interesting[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x07, 0x08, 0x0A, 0x0D, 0x10, 0x20,
    0x25, 0x2F, 0x40, 0x41, 0x5C, 0x7E, 0x7F, 0x80, 0x81, 0xFE, 0xFF};

// Build one payload for the given iteration. A deterministic boundary corpus is
// emitted first (empty, signed boundaries, max-len patterns, overflow/format
// probes), then payloads are pseudo-randomly mutated with a strong bias toward
// interesting values and boundary lengths — coverage a pure RNG rarely hits.
static void fuzz_build_payload(uint16_t iteration, uint8_t *buf, uint8_t *out_len) {
    const uint8_t maxp = BLE_FUZZ_PAYLOAD_MAX;
    switch (iteration) {
        case 0: *out_len = 0; return;                                  // empty write
        case 1: buf[0] = 0x00; *out_len = 1; return;                   // single 0x00
        case 2: buf[0] = 0xFF; *out_len = 1; return;                   // single 0xFF
        case 3: buf[0] = 0x7F; *out_len = 1; return;                   // signed boundary
        case 4: buf[0] = 0x80; *out_len = 1; return;                   // signed boundary
        case 5: memset(buf, 0x00, maxp); *out_len = maxp; return;      // max-len zeros
        case 6: memset(buf, 0xFF, maxp); *out_len = maxp; return;      // max-len ones
        case 7: memset(buf, 0x41, maxp); *out_len = maxp; return;      // "AAAA…" overflow probe
        case 8:
            for (uint8_t i = 0; i < maxp; i++) buf[i] = i;
            *out_len = maxp;
            return;                                                    // incrementing
        case 9: {                                                      // format-string probe
            static const char fmt[] = "%s%n%x%p%d";
            uint8_t n = (uint8_t)((sizeof(fmt) - 1 < maxp) ? sizeof(fmt) - 1 : maxp);
            memcpy(buf, fmt, n);
            *out_len = n;
            return;
        }
        case 10:                                                       // alternating bits
            for (uint8_t i = 0; i < maxp; i++) buf[i] = (i & 1) ? 0x55 : 0xAA;
            *out_len = maxp;
            return;
        default:
            break;
    }
    uint32_t r = xorshift32();
    // Bias lengths toward the boundaries (1 and max) that catch off-by-ones.
    uint8_t len;
    switch (r & 7) {
        case 0:  len = 1; break;
        case 1:  len = maxp; break;
        case 2:  len = maxp; break;
        default: len = 1 + (xorshift32() % maxp); break;
    }
    for (uint8_t i = 0; i < len; i++) {
        uint32_t v = xorshift32();
        switch (v & 3) {
            case 0:  // interesting value (highest weight via two cases below too)
            case 1:  buf[i] = k_interesting[(v >> 2) % sizeof(k_interesting)]; break;
            case 2:  buf[i] = (uint8_t)(v >> 8); break;    // uniform random
            default: buf[i] = (uint8_t)i; break;           // positional
        }
    }
    *out_len = len;
}

static bool gatt_write_retryable(ret_code_t err) {
    return err == NRF_ERROR_RESOURCES || err == NRF_ERROR_BUSY ||
           err == BLE_ERROR_GATTC_PROC_NOT_PERMITTED;
}

static ret_code_t timer_start(ble_timer_owner_t owner, uint32_t ticks) {
    if (m_timer_owner != BLE_TIMER_OWNER_NONE) {
        return NRF_ERROR_BUSY;
    }
    m_timer_owner = owner;
    ret_code_t err = app_timer_start(m_fuzz_timer, ticks, NULL);
    if (err != NRF_SUCCESS) {
        m_timer_owner = BLE_TIMER_OWNER_NONE;
    }
    return err;
}

static void timer_stop(ble_timer_owner_t owner) {
    if (m_timer_owner == owner) {
        m_timer_owner = BLE_TIMER_OWNER_NONE;
        (void)app_timer_stop(m_fuzz_timer);
    }
}

static bool gatt_begin(ble_gatt_op_t op) {
    if (m_gatt_op != BLE_GATT_OP_NONE || m_timer_owner != BLE_TIMER_OWNER_NONE) {
        return false;
    }
    m_gatt_op = op;
    return true;
}

static void gatt_finish(ble_gatt_op_t op) {
    if (m_gatt_op == op) {
        m_gatt_op = BLE_GATT_OP_NONE;
    }
}

static void reset_link_async_state(void) {
    m_gatt_op = BLE_GATT_OP_NONE;
    m_disc_state = 0;
    m_char_count = 0;
    m_desc_state = 0;
    m_desc_count = 0;
    m_svc_state = 0;
    m_svc_count = 0;
    m_devinfo_state = 0;
    m_devinfo_idx = 0;
    memset(m_devinfo, 0, sizeof(m_devinfo));
    m_read_state = 0;
    m_read_status = 0;
    m_read_len = 0;
    m_write_state = 0;
    m_write_status = 0;
    m_cccd_state = 0;
    m_cccd_handle = 0;
    m_notif_count = 0;
    m_gatt_transient_retries = 0;
}

static ret_code_t prepare_target_connect(void) {
    // Stop through the scanner API so its logical state and the SoftDevice GAP
    // procedure are reconciled before sd_ble_gap_connect() takes over scanning.
    return ble_scan_stop();
}

static void fail_pending_gatt(uint8_t reason, bool fail_probe) {
    if (m_read_state == 1) {
        m_read_state = 3;
        m_read_status = 0xFF;
        m_read_len = 0;
    }
    if (m_write_state == 1) {
        m_write_state = 3;
        m_write_status = 0xFF;
    }
    if (m_disc_state == 1) {
        m_disc_state = 3;
    }
    if (m_desc_state == 1) {
        m_desc_state = 3;
    }
    if (m_svc_state == 1) {
        m_svc_state = 3;
    }
    if (m_devinfo_state == 1) {
        if (m_devinfo_idx < BLE_DEVINFO_COUNT) {
            m_devinfo[m_devinfo_idx].status = 0xFF;
            m_devinfo[m_devinfo_idx].len = 0;
        }
        m_devinfo_state = 3;
    }
    if (m_cccd_state == 1) {
        m_cccd_state = 3;
    } else {
        m_cccd_state = 0;
    }
    m_cccd_handle = 0;
    m_gatt_op = BLE_GATT_OP_NONE;
    m_notif_count = 0;
    if (fail_probe && m_probe_state == 1) {
        m_probe_state = 3;
        m_probe_result = reason;
    }
}

static void continue_char_discovery(uint16_t next_handle) {
    if (m_char_count >= BLE_MAX_CHARS || next_handle >= 0xFFFF) {
        m_disc_state = 2; // done (table full or range exhausted)
        gatt_finish(BLE_GATT_OP_CHAR_DISC);
        return;
    }
    ble_gattc_handle_range_t range = { .start_handle = next_handle + 1, .end_handle = 0xFFFF };
    ret_code_t err = sd_ble_gattc_characteristics_discover(m_conn_handle, &range);
    if (err != NRF_SUCCESS) {
        m_disc_state = (m_char_count > 0) ? 2 : 3; // partial results still usable
        gatt_finish(BLE_GATT_OP_CHAR_DISC);
    }
}

// Issue the next standard-info read, skipping fields the target doesn't expose.
// One GATT read is outstanding at a time (the SoftDevice allows a single GATTC
// read per link); BLE_GATTC_EVT_READ_RSP advances to the next field.
static void devinfo_read_next(void) {
    while (m_devinfo_idx < BLE_DEVINFO_COUNT) {
        ble_devinfo_rec_t *rec = &m_devinfo[m_devinfo_idx];
        if (rec->handle == 0) {
            m_devinfo_idx++;                     // characteristic absent on this target
            continue;
        }
        ret_code_t err = sd_ble_gattc_read(m_conn_handle, rec->handle, 0);
        if (err == NRF_SUCCESS) {
            return;                              // wait for READ_RSP
        }
        rec->status = (uint8_t)(err & 0xFF);     // couldn't even issue the read
        m_devinfo_idx++;
    }
    m_devinfo_state = 2;                          // all fields attempted
    gatt_finish(BLE_GATT_OP_DEVINFO);
}

static void probe_reset_batch_state(void) {
    m_probe_index = 0;
    m_probe_total = 0;
    m_probe_log_count = 0;
    m_probe_current_index = 0;
    m_probe_current_ok = 0;
    memset(&m_probe_current_target, 0, sizeof(m_probe_current_target));
}

static void probe_record_current(uint8_t connect_status, uint8_t probe_result, uint8_t disconnect_reason) {
    if (m_probe_current_index >= m_probe_total || m_probe_current_index >= BLE_PROBE_BATCH_MAX) {
        return;
    }
    ble_probe_log_t *entry = &m_probe_log[m_probe_current_index];
    memcpy(entry->addr, m_probe_current_target.addr, BLE_GAP_ADDR_LEN);
    entry->addr_type = m_probe_current_target.addr_type;
    entry->rssi = m_probe_current_target.rssi;
    entry->connect_status = connect_status;
    entry->probe_result = probe_result;
    entry->disconnect_reason = disconnect_reason;
    if (m_probe_log_count < m_probe_current_index + 1) {
        m_probe_log_count = m_probe_current_index + 1;
    }
}

static uint32_t probe_connect_target(uint8_t addr_type, const uint8_t *addr, int8_t rssi) {
    ble_gap_addr_t peer;
    memset(&peer, 0, sizeof(peer));
    peer.addr_type = addr_type;
    memcpy(peer.addr, addr, BLE_GAP_ADDR_LEN);

    reset_link_async_state();
    m_fuzz_state = 0;
    m_fuzz_sent = 0;
    m_fuzz_log_count = 0;
    m_flood_state = 0;
    m_flood_sent = 0;
    m_last_disc_reason = 0;
    m_probe_result = 0;
    m_probe_current_ok = 0;
    m_probe_state = 1;
    m_conn_state = BLE_CONN_STATE_CONNECTING;

    ret_code_t err = prepare_target_connect();
    if (err == NRF_SUCCESS) {
        err = sd_ble_gap_connect(&peer, &m_init_scan_params, &m_conn_params, APP_BLE_CONN_CFG_TAG);
    }
    if (err != NRF_SUCCESS) {
        m_conn_state = BLE_CONN_STATE_IDLE;
        m_probe_result = (uint8_t)(err & 0xFF);
        if (m_probe_log_count < BLE_PROBE_BATCH_MAX) {
            m_probe_current_target.addr_type = addr_type;
            memcpy(m_probe_current_target.addr, addr, BLE_GAP_ADDR_LEN);
            m_probe_current_target.rssi = rssi;
            m_probe_current_index = m_probe_log_count;
            probe_record_current((uint8_t)(err & 0xFF), (uint8_t)(err & 0xFF), 0);
        }
        if (m_probe_global_mode != 1) {
            m_probe_state = 3;
        }
    }
    return err;
}

static uint32_t probe_load_scan_targets(void) {
    ble_scan_addr_t addresses[BLE_PROBE_BATCH_MAX];
    uint8_t count = ble_scan_copy_addresses(addresses, BLE_PROBE_BATCH_MAX);
    for (uint8_t i = 0; i < count; i++) {
        memcpy(m_probe_targets[i].addr, addresses[i].addr, BLE_GAP_ADDR_LEN);
        m_probe_targets[i].addr_type = addresses[i].addr_type;
        m_probe_targets[i].rssi = 0;
    }

    m_probe_total = count;
    m_probe_log_count = 0;
    m_probe_index = 0;
    return (count > 0) ? NRF_SUCCESS : NRF_ERROR_NOT_FOUND;
}

static uint32_t probe_global_start_next(void);

static uint32_t probe_global_begin(void) {
    if (m_probe_global_mode == 1 || m_probe_state == 1) {
        return NRF_ERROR_BUSY;
    }
    if (m_conn_state != BLE_CONN_STATE_IDLE && m_conn_state != BLE_CONN_STATE_DISCONNECTED) {
        return NRF_ERROR_INVALID_STATE;
    }

    uint32_t err = ble_scan_stop();
    if (err != NRF_SUCCESS) {
        return err;
    }

    probe_reset_batch_state();
    err = probe_load_scan_targets();
    if (err != NRF_SUCCESS) {
        m_probe_state = 3;
        m_probe_result = (uint8_t)(err & 0xFF);
        return err;
    }

    m_probe_global_mode = 1;
    m_probe_state = 1;
    if (m_probe_total == 0) {
        m_probe_global_mode = 0;
        m_probe_state = 3;
        m_probe_result = (uint8_t)(NRF_ERROR_NOT_FOUND & 0xFF);
        return NRF_ERROR_NOT_FOUND;
    }

    return probe_global_start_next();
}

static uint32_t probe_global_start_next(void) {
    while (m_probe_index < m_probe_total) {
        m_probe_current_index = m_probe_index;
        m_probe_current_target = m_probe_targets[m_probe_index];
        m_probe_index++;
        ret_code_t err = probe_connect_target(m_probe_current_target.addr_type, m_probe_current_target.addr, m_probe_current_target.rssi);
        if (err == NRF_SUCCESS) {
            return NRF_SUCCESS;
        }
    }

    m_probe_global_mode = 0;
    m_probe_state = 2;
    m_probe_result = 0;
    return NRF_SUCCESS;
}

static void probe_global_finish_current(uint8_t connect_status, uint8_t probe_result, uint8_t disconnect_reason) {
    probe_record_current(connect_status, probe_result, disconnect_reason);
    m_probe_result = probe_result;
}

// -------------------------------------------------------------------------
// SoftDevice event handling (central side only). The peripheral/app link is
// handled separately in ble_main.c; there the GAP CONNECTED/DISCONNECTED cases
// are role/handle-guarded so they ignore this central link.
// -------------------------------------------------------------------------
static void ble_central_evt_handler(ble_evt_t const *p_ble_evt, void *p_context) {
    const ble_gap_evt_t   *gap   = &p_ble_evt->evt.gap_evt;
    const ble_gattc_evt_t *gattc = &p_ble_evt->evt.gattc_evt;

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            if (gap->params.connected.role == BLE_GAP_ROLE_CENTRAL) {
                bool cancel_won_race = (m_conn_state == BLE_CONN_STATE_CANCELLING);
                m_conn_handle = gap->conn_handle;
                m_conn_state = BLE_CONN_STATE_CONNECTED;
                NRF_LOG_INFO("Central connected to target, handle 0x%x", m_conn_handle);
                if (cancel_won_race) {
                    ret_code_t err = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                    if (err == NRF_SUCCESS || err == NRF_ERROR_INVALID_STATE) {
                        m_conn_state = BLE_CONN_STATE_DISCONNECTING;
                    }
                } else if (m_probe_global_mode == 1 && m_probe_state == 1) {
                    m_probe_current_ok = 0;
                    ret_code_t err = sd_ble_gap_conn_param_update(m_conn_handle, &m_conn_params);
                    if (err != NRF_SUCCESS) {
                        probe_global_finish_current((uint8_t)(err & 0xFF), (uint8_t)(err & 0xFF), 0);
                        err = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                        if (err == NRF_SUCCESS || err == NRF_ERROR_INVALID_STATE) {
                            m_conn_state = BLE_CONN_STATE_DISCONNECTING;
                        }
                    }
                }
            }
            break;

        case BLE_GAP_EVT_DISCONNECTED: {
            if (gap->conn_handle == m_conn_handle) {
                m_last_disc_reason = gap->params.disconnected.reason;
                m_conn_handle = BLE_CONN_HANDLE_INVALID;
                m_conn_state = BLE_CONN_STATE_DISCONNECTED;
                bool preserve_probe_state = false;
                if (m_probe_global_mode == 1 && m_probe_state == 1) {
                    preserve_probe_state = true;
                    uint8_t connect_status = m_probe_current_ok ? 0 : ((m_probe_result != 0) ? m_probe_result : m_last_disc_reason);
                    uint8_t probe_result = m_probe_current_ok ? 0 : ((m_probe_result != 0) ? m_probe_result : m_last_disc_reason);
                    probe_global_finish_current(connect_status, probe_result, m_last_disc_reason);
                }
                fail_pending_gatt(m_last_disc_reason, !preserve_probe_state);
                if (m_fuzz_state == 1) {
                    // Target dropped the link mid-fuzz: possible crash or a
                    // defensive/parser-driven disconnect. Flag by stopping.
                    m_fuzz_state = 2;
                    timer_stop(BLE_TIMER_OWNER_FUZZ);
                }
                if (m_flood_state == 1 && m_kick_sb_state == 0) {
                    m_flood_state = 2;
                    timer_stop(BLE_TIMER_OWNER_FLOOD);
                    rgb_marquee_set_ble_active_anim(false);
                }
                rgb_marquee_set_ble_test_anim(false);
                NRF_LOG_INFO("Central target disconnected, reason 0x%x", m_last_disc_reason);
                if (preserve_probe_state) {
                    if (m_probe_index < m_probe_total) {
                        m_conn_state = BLE_CONN_STATE_IDLE;
                        (void)probe_global_start_next();
                    } else {
                        m_probe_global_mode = 0;
                        m_probe_state = 2;
                    }
                }
            }
            break;
        }

        case BLE_GAP_EVT_TIMEOUT:
            if (gap->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN &&
                    (m_conn_state == BLE_CONN_STATE_CONNECTING ||
                     m_conn_state == BLE_CONN_STATE_CANCELLING)) {
                bool was_cancelled = (m_conn_state == BLE_CONN_STATE_CANCELLING);
                m_conn_state = BLE_CONN_STATE_DISCONNECTED;
                NRF_LOG_INFO("Central connect timed out");
                if (m_probe_global_mode == 1) {
                    m_probe_current_ok = 0;
                    probe_global_finish_current((uint8_t)(NRF_ERROR_TIMEOUT & 0xFF), (uint8_t)(NRF_ERROR_TIMEOUT & 0xFF), 0);
                    if (m_probe_index < m_probe_total) {
                        (void)probe_global_start_next();
                    } else {
                        m_probe_global_mode = 0;
                        m_probe_state = 2;
                    }
                } else if (!was_cancelled && m_probe_state == 1) {
                    m_probe_state = 3;
                    m_probe_result = (uint8_t)(NRF_ERROR_TIMEOUT & 0xFF);
                }
            }
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            if (gattc->conn_handle == m_conn_handle) {
                uint8_t timeout = (uint8_t)(NRF_ERROR_TIMEOUT & 0xFF);
                fail_pending_gatt(timeout, true);
                m_probe_global_mode = 0;
                m_conn_state = BLE_CONN_STATE_DISCONNECTING;
                if (m_kick_sb_state == 1) {
                    scan_buffer_stop(false);
                } else {
                    if (m_fuzz_state == 1) {
                        m_fuzz_state = 2;
                    }
                    if (m_flood_state == 1) {
                        m_flood_state = 2;
                    }
                    timer_stop(BLE_TIMER_OWNER_FUZZ);
                    timer_stop(BLE_TIMER_OWNER_FLOOD);
                    rgb_marquee_set_ble_test_anim(false);
                    rgb_marquee_set_ble_active_anim(false);
                }
                NRF_LOG_INFO("Central GATT timeout");
            }
            break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
            if (gap->conn_handle == m_conn_handle &&
                    m_conn_state == BLE_CONN_STATE_CONNECTED) {
                ret_code_t err = sd_ble_gap_conn_param_update(
                    m_conn_handle, &gap->params.conn_param_update_request.conn_params);
                if (err != NRF_SUCCESS) {
                    NRF_LOG_WARNING("Central conn-param request failed: 0x%x", err);
                }
            }
            break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE:
            if (gap->conn_handle == m_conn_handle && m_probe_state == 1) {
                m_probe_result = 0;
                NRF_LOG_INFO("Central link probe completed");
                if (m_probe_global_mode == 1) {
                    m_probe_current_ok = 1;
                    probe_global_finish_current(0, 0, 0);
                    ret_code_t err = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                    if (err == NRF_SUCCESS || err == NRF_ERROR_INVALID_STATE) {
                        m_conn_state = BLE_CONN_STATE_DISCONNECTING;
                    }
                } else {
                    m_probe_state = 2;
                }
            }
            break;

        case BLE_GATTC_EVT_CHAR_DISC_RSP:
            if (gattc->conn_handle != m_conn_handle || m_gatt_op != BLE_GATT_OP_CHAR_DISC) {
                break;
            }
            if (gattc->gatt_status == BLE_GATT_STATUS_SUCCESS) {
                const ble_gattc_evt_char_disc_rsp_t *r = &gattc->params.char_disc_rsp;
                uint16_t last = 0;
                for (uint16_t i = 0; i < r->count && m_char_count < BLE_MAX_CHARS; i++) {
                    const ble_gattc_char_t *c = &r->chars[i];
                    ble_char_rec_t *rec = &m_chars[m_char_count++];
                    rec->value_handle = c->handle_value;
                    rec->props        = char_props_to_byte(c->char_props);
                    rec->uuid_type    = c->uuid.type;
                    rec->uuid         = c->uuid.uuid;
                    last = c->handle_value;
                }
                if (last == 0) {
                    m_disc_state = 2;
                    gatt_finish(BLE_GATT_OP_CHAR_DISC);
                } else {
                    continue_char_discovery(last);
                }
            } else {
                m_disc_state = (gattc->gatt_status == BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND) ? 2 : 3;
                gatt_finish(BLE_GATT_OP_CHAR_DISC);
            }
            break;

        case BLE_GATTC_EVT_WRITE_RSP:
            if (gattc->conn_handle != m_conn_handle) {
                break;
            }
            if (m_gatt_op == BLE_GATT_OP_WRITE && m_write_state == 1) {
                m_write_status = gattc->gatt_status;
                m_write_state = (gattc->gatt_status == BLE_GATT_STATUS_SUCCESS) ? 2 : 3;
                gatt_finish(BLE_GATT_OP_WRITE);
            } else if (m_gatt_op == BLE_GATT_OP_SUBSCRIBE) {
                gatt_finish(BLE_GATT_OP_SUBSCRIBE);
            }
            break;

        case BLE_GATTC_EVT_READ_RSP:
            if (gattc->conn_handle != m_conn_handle ||
                    (m_gatt_op != BLE_GATT_OP_READ && m_gatt_op != BLE_GATT_OP_DEVINFO)) {
                break;
            }
            // Route the response into the device-info collector while it runs,
            // instead of the user-read buffer.
            if (m_gatt_op == BLE_GATT_OP_DEVINFO &&
                    m_devinfo_state == 1 && m_devinfo_idx < BLE_DEVINFO_COUNT) {
                ble_devinfo_rec_t *rec = &m_devinfo[m_devinfo_idx];
                rec->status = gattc->gatt_status;
                if (gattc->gatt_status == BLE_GATT_STATUS_SUCCESS) {
                    const ble_gattc_evt_read_rsp_t *r = &gattc->params.read_rsp;
                    uint8_t n = (r->len > BLE_DEVINFO_VAL_MAX) ? BLE_DEVINFO_VAL_MAX : (uint8_t)r->len;
                    memcpy(rec->data, r->data, n);
                    rec->len = n;
                }
                m_devinfo_idx++;
                devinfo_read_next();
                break;
            }
            if (m_gatt_op == BLE_GATT_OP_DEVINFO) {
                m_devinfo_state = 3;
                gatt_finish(BLE_GATT_OP_DEVINFO);
                break;
            }
            m_read_status = gattc->gatt_status;
            if (gattc->gatt_status == BLE_GATT_STATUS_SUCCESS) {
                const ble_gattc_evt_read_rsp_t *r = &gattc->params.read_rsp;
                uint16_t n = r->len;
                if (n > BLE_READ_VALUE_MAX) {
                    n = BLE_READ_VALUE_MAX;
                }
                memcpy(m_read_value, r->data, n);
                m_read_len = n;
            } else {
                m_read_len = 0;
            }
            m_read_state = 2; // ready
            gatt_finish(BLE_GATT_OP_READ);
            break;

        case BLE_GATTC_EVT_HVX: {
            // Notification/indication pushed by the target (receive-only).
            if (gattc->conn_handle != m_conn_handle) {
                break;
            }
            const ble_gattc_evt_hvx_t *hvx = &gattc->params.hvx;
            if (m_notif_count < BLE_NOTIF_LOG_MAX) {
                ble_notif_t *n = &m_notif_log[m_notif_count++];
                n->handle = hvx->handle;
                uint8_t l = MIN(hvx->len, (uint16_t)BLE_NOTIF_DATA_MAX);
                n->len = l;
                memcpy(n->data, hvx->data, l);
            }
            // Indications must be confirmed back to the target.
            if (hvx->type == BLE_GATT_HVX_INDICATION) {
                sd_ble_gattc_hv_confirm(m_conn_handle, hvx->handle);
            }
            break;
        }

        case BLE_GATTC_EVT_DESC_DISC_RSP: {
            if (gattc->conn_handle != m_conn_handle) {
                break;
            }
            // Full descriptor-listing mode (enumerate every descriptor).
            if (m_gatt_op == BLE_GATT_OP_DESC_DISC && m_desc_state == 1) {
                if (gattc->gatt_status != BLE_GATT_STATUS_SUCCESS) {
                    m_desc_state = (gattc->gatt_status == BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND ||
                                    m_desc_count > 0) ? 2 : 3;
                    gatt_finish(BLE_GATT_OP_DESC_DISC);
                    break;
                }
                const ble_gattc_evt_desc_disc_rsp_t *dr = &gattc->params.desc_disc_rsp;
                uint16_t dlast = 0;
                for (uint16_t i = 0; i < dr->count && m_desc_count < BLE_MAX_DESCS; i++) {
                    ble_desc_rec_t *rec = &m_descs[m_desc_count++];
                    rec->handle    = dr->descs[i].handle;
                    rec->uuid      = dr->descs[i].uuid.uuid;
                    rec->uuid_type = dr->descs[i].uuid.type;
                    dlast = dr->descs[i].handle;
                }
                if (m_desc_count < BLE_MAX_DESCS && dlast != 0 && dlast < 0xFFFF) {
                    ble_gattc_handle_range_t range = { .start_handle = dlast + 1, .end_handle = 0xFFFF };
                    if (sd_ble_gattc_descriptors_discover(m_conn_handle, &range) != NRF_SUCCESS) {
                        m_desc_state = 2;
                        gatt_finish(BLE_GATT_OP_DESC_DISC);
                    }
                } else {
                    m_desc_state = 2;
                    gatt_finish(BLE_GATT_OP_DESC_DISC);
                }
                break;
            }
            // Otherwise: CCCD search (looking for UUID 0x2902).
            if (m_gatt_op != BLE_GATT_OP_CCCD_DISC || m_cccd_state != 1) {
                break;
            }
            if (gattc->gatt_status != BLE_GATT_STATUS_SUCCESS) {
                m_cccd_state = 3; // not found
                gatt_finish(BLE_GATT_OP_CCCD_DISC);
                break;
            }
            const ble_gattc_evt_desc_disc_rsp_t *r = &gattc->params.desc_disc_rsp;
            uint16_t last = 0;
            for (uint16_t i = 0; i < r->count; i++) {
                uint16_t uuid = r->descs[i].uuid.uuid;
                last = r->descs[i].handle;
                if (r->descs[i].uuid.type == BLE_UUID_TYPE_BLE &&
                        uuid == BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG) { // 0x2902
                    m_cccd_handle = r->descs[i].handle;
                    m_cccd_state = 2; // found
                    break;
                }
                if (r->descs[i].uuid.type == BLE_UUID_TYPE_BLE &&
                        uuid == BLE_UUID_CHARACTERISTIC) { // 0x2803: reached the next char
                    m_cccd_state = 3;
                    break;
                }
            }
            if (m_cccd_state == 1) {
                // Not decided yet: keep scanning descriptors after `last`.
                if (last == 0 || last >= 0xFFFF) {
                    m_cccd_state = 3;
                } else {
                    ble_gattc_handle_range_t range = { .start_handle = last + 1, .end_handle = 0xFFFF };
                    if (sd_ble_gattc_descriptors_discover(m_conn_handle, &range) != NRF_SUCCESS) {
                        m_cccd_state = 3;
                    }
                }
            }
            if (m_cccd_state != 1) {
                gatt_finish(BLE_GATT_OP_CCCD_DISC);
            }
            break;
        }

        case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP: {
            if (gattc->conn_handle != m_conn_handle || m_svc_state != 1 ||
                    m_gatt_op != BLE_GATT_OP_SVC_DISC) {
                break;
            }
            if (gattc->gatt_status != BLE_GATT_STATUS_SUCCESS) {
                m_svc_state = (gattc->gatt_status == BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND ||
                               m_svc_count > 0) ? 2 : 3;
                gatt_finish(BLE_GATT_OP_SVC_DISC);
                break;
            }
            const ble_gattc_evt_prim_srvc_disc_rsp_t *sr = &gattc->params.prim_srvc_disc_rsp;
            uint16_t slast = 0;
            for (uint16_t i = 0; i < sr->count && m_svc_count < BLE_MAX_SERVICES; i++) {
                ble_svc_rec_t *rec = &m_svcs[m_svc_count++];
                rec->uuid         = sr->services[i].uuid.uuid;
                rec->uuid_type    = sr->services[i].uuid.type;
                rec->start_handle = sr->services[i].handle_range.start_handle;
                rec->end_handle   = sr->services[i].handle_range.end_handle;
                slast = sr->services[i].handle_range.end_handle;
            }
            if (m_svc_count < BLE_MAX_SERVICES && slast != 0 && slast < 0xFFFF) {
                if (sd_ble_gattc_primary_services_discover(m_conn_handle, slast + 1, NULL) != NRF_SUCCESS) {
                    m_svc_state = 2;
                    gatt_finish(BLE_GATT_OP_SVC_DISC);
                }
            } else {
                m_svc_state = 2;
                gatt_finish(BLE_GATT_OP_SVC_DISC);
            }
            break;
        }

        default:
            break;
    }
}
NRF_SDH_BLE_OBSERVER(m_ble_central_obs, BLE_CENTRAL_OBSERVER_PRIO, ble_central_evt_handler, NULL);

// -------------------------------------------------------------------------
// Fuzz driver — one mutated write per timer tick to the single target link.
// -------------------------------------------------------------------------
static void fuzz_timer_handler(void *p_context) {
    // The same timer drives the fuzzer, the WRITE_CMD flood, and the
    // scan-buffer-wide iteration (kick-sb / flood-sb). They're mutually
    // exclusive at the state level (starting one stops the other).

    if (m_timer_owner == BLE_TIMER_OWNER_NONE) {
        (void)app_timer_stop(m_fuzz_timer);
        return;
    }
    if (m_timer_owner == BLE_TIMER_OWNER_SCAN_BUFFER) {
        scan_buffer_timer_handler(p_context);
        return;
    }

    if (m_conn_handle == BLE_CONN_HANDLE_INVALID || m_conn_state != BLE_CONN_STATE_CONNECTED) {
        if (m_timer_owner == BLE_TIMER_OWNER_FUZZ) {
            m_fuzz_state = 2;
            timer_stop(BLE_TIMER_OWNER_FUZZ);
            rgb_marquee_set_ble_test_anim(false);
        } else if (m_timer_owner == BLE_TIMER_OWNER_FLOOD) {
            m_flood_state = 2;
            timer_stop(BLE_TIMER_OWNER_FLOOD);
            rgb_marquee_set_ble_active_anim(false);
        }
        return;
    }

    if (m_timer_owner == BLE_TIMER_OWNER_FLOOD && m_flood_state == 1) {
        if (m_flood_max != 0 && m_flood_sent >= m_flood_max) {
            m_flood_state = 2;
            timer_stop(BLE_TIMER_OWNER_FLOOD);
            rgb_marquee_set_ble_active_anim(false);
            return;
        }
        ble_gattc_write_params_t w = {
            .write_op = BLE_GATT_OP_WRITE_CMD,
            .flags    = 0,
            .handle   = m_flood_handle,
            .offset   = 0,
            .len      = m_flood_size,
            .p_value  = m_flood_payload,
        };
        ret_code_t err = sd_ble_gattc_write(m_conn_handle, &w);
        if (err == NRF_SUCCESS) {
            m_flood_sent++;
            m_gatt_transient_retries = 0;
        } else if (!gatt_write_retryable(err) ||
                   ++m_gatt_transient_retries >= BLE_GATT_TRANSIENT_RETRY_MAX) {
            m_flood_state = 2;
            timer_stop(BLE_TIMER_OWNER_FLOOD);
            rgb_marquee_set_ble_active_anim(false);
        }
        return;
    }

    if (m_timer_owner != BLE_TIMER_OWNER_FUZZ || m_fuzz_state != 1) {
        return;
    }
    if (m_fuzz_max != 0 && m_fuzz_sent >= m_fuzz_max) {
        m_fuzz_state = 2;
        timer_stop(BLE_TIMER_OWNER_FUZZ);
        rgb_marquee_set_ble_test_anim(false);
        return;
    }

    uint8_t payload[BLE_FUZZ_PAYLOAD_MAX];
    uint8_t len;
    fuzz_build_payload(m_fuzz_sent, payload, &len);

    ble_gattc_write_params_t w = {
        .write_op = BLE_GATT_OP_WRITE_CMD,  // write-without-response (fire and forget)
        .flags    = 0,
        .handle   = m_fuzz_handle,
        .offset   = 0,
        .len      = len,
        .p_value  = payload,
    };
    ret_code_t err = sd_ble_gattc_write(m_conn_handle, &w);

    if (m_fuzz_log_count < BLE_FUZZ_LOG_MAX) {
        ble_fuzz_log_t *e = &m_fuzz_log[m_fuzz_log_count++];
        e->index  = m_fuzz_sent;
        e->len    = len;
        e->status = (err == NRF_SUCCESS) ? 0 : (uint8_t)(err & 0xFF);
        memcpy(e->data, payload, MIN(len, (uint8_t)BLE_FUZZ_LOG_DATA));
    }
    if (err == NRF_SUCCESS) {
        m_fuzz_sent++;
        m_gatt_transient_retries = 0;
    } else if (!gatt_write_retryable(err) ||
               ++m_gatt_transient_retries >= BLE_GATT_TRANSIENT_RETRY_MAX) {
        m_fuzz_state = 2;
        timer_stop(BLE_TIMER_OWNER_FUZZ);
        rgb_marquee_set_ble_test_anim(false);
    }
}

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------
void ble_central_init(void) {
    ret_code_t err = app_timer_create(&m_fuzz_timer, APP_TIMER_MODE_REPEATED, fuzz_timer_handler);
    APP_ERROR_CHECK(err);
}

uint32_t ble_central_connect(uint8_t addr_type, const uint8_t *addr) {
    if (m_probe_global_mode == 1 || m_kick_sb_state == 1) {
        return NRF_ERROR_BUSY;
    }
    if (addr == NULL) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (m_conn_state != BLE_CONN_STATE_IDLE && m_conn_state != BLE_CONN_STATE_DISCONNECTED) {
        return NRF_ERROR_INVALID_STATE; // already connecting/connected to a target
    }
    ret_code_t err = prepare_target_connect();
    if (err != NRF_SUCCESS) {
        return err;
    }
    ble_gap_addr_t peer;
    memset(&peer, 0, sizeof(peer));
    peer.addr_type = addr_type;
    memcpy(peer.addr, addr, BLE_GAP_ADDR_LEN);

    reset_link_async_state();
    m_fuzz_state = 0;
    m_fuzz_sent = 0;
    m_fuzz_log_count = 0;
    m_flood_state = 0;
    m_flood_sent = 0;
    m_last_disc_reason = 0;
    m_probe_state = 0;
    m_probe_result = 0;
    m_conn_state = BLE_CONN_STATE_CONNECTING;

    err = sd_ble_gap_connect(&peer, &m_init_scan_params, &m_conn_params, APP_BLE_CONN_CFG_TAG);
    if (err != NRF_SUCCESS) {
        m_conn_state = BLE_CONN_STATE_IDLE;
    }
    return err;
}

uint32_t ble_central_disconnect(void) {
    if (m_kick_sb_state == 1) {
        scan_buffer_stop(true);
        return NRF_SUCCESS;
    }
    ble_central_flood_stop();
    ble_central_fuzz_stop();
    if (m_probe_global_mode == 1) {
        m_probe_global_mode = 0;
        m_probe_state = 0;
        m_probe_result = 0;
    }
    if (m_conn_state == BLE_CONN_STATE_CANCELLING ||
            m_conn_state == BLE_CONN_STATE_DISCONNECTING) {
        return NRF_SUCCESS;
    }
    if (m_conn_state == BLE_CONN_STATE_CONNECTING && m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        ret_code_t err = sd_ble_gap_connect_cancel();
        if (err == NRF_SUCCESS || err == NRF_ERROR_INVALID_STATE) {
            m_conn_state = BLE_CONN_STATE_CANCELLING;
            return NRF_SUCCESS;
        }
        return err;
    }
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_SUCCESS;
    }
    ret_code_t err = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (err == NRF_SUCCESS || err == NRF_ERROR_INVALID_STATE) {
        m_conn_state = BLE_CONN_STATE_DISCONNECTING;
        return NRF_SUCCESS;
    }
    return err;
}

uint32_t ble_central_link_probe(uint8_t global_mode) {
    if (m_kick_sb_state == 1) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (global_mode != 0) {
        return probe_global_begin();
    }
    if (m_probe_global_mode == 1) {
        return NRF_ERROR_BUSY;
    }
    if (m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (m_probe_state == 1) {
        return NRF_ERROR_BUSY;
    }

    m_probe_state = 1;
    m_probe_result = 0;

    ret_code_t err = sd_ble_gap_conn_param_update(m_conn_handle, &m_conn_params);
    if (err != NRF_SUCCESS) {
        m_probe_state = 3;
        m_probe_result = (uint8_t)(err & 0xFF);
    }
    return err;
}

uint32_t ble_central_gatt_discover(void) {
    if (m_kick_sb_state == 1 || m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (!gatt_begin(BLE_GATT_OP_CHAR_DISC)) {
        return NRF_ERROR_BUSY;
    }
    m_char_count = 0;
    m_disc_state = 1; // discovering
    ble_gattc_handle_range_t range = { .start_handle = 0x0001, .end_handle = 0xFFFF };
    ret_code_t err = sd_ble_gattc_characteristics_discover(m_conn_handle, &range);
    if (err != NRF_SUCCESS) {
        m_disc_state = 3; // error
        gatt_finish(BLE_GATT_OP_CHAR_DISC);
    }
    return err;
}

uint8_t ble_central_get_char_count(void) {
    return m_char_count;
}

// ---- full descriptor listing --------------------------------------------
uint32_t ble_central_desc_discover(void) {
    if (m_kick_sb_state == 1 || m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (!gatt_begin(BLE_GATT_OP_DESC_DISC)) {
        return NRF_ERROR_BUSY;
    }
    m_desc_count = 0;
    m_desc_state = 1; // discovering
    ble_gattc_handle_range_t range = { .start_handle = 0x0001, .end_handle = 0xFFFF };
    ret_code_t err = sd_ble_gattc_descriptors_discover(m_conn_handle, &range);
    if (err != NRF_SUCCESS) {
        m_desc_state = 3;
        gatt_finish(BLE_GATT_OP_DESC_DISC);
    }
    return err;
}

uint16_t ble_central_copy_descs(uint8_t start_index, uint8_t *out, uint16_t out_cap) {
    // Wire: state[1] | per descriptor: handle[2 BE] | uuid_type[1] | uuid[2 BE]
    // state: 0 idle, 1 discovering, 2 done, 3 error.
    if (out_cap < 1) {
        return 0;
    }
    out[0] = m_desc_state;
    uint16_t o = 1;
    for (uint8_t i = start_index; i < m_desc_count; i++) {
        if (o + 5 > out_cap) {
            break;
        }
        ble_desc_rec_t *r = &m_descs[i];
        out[o++] = (r->handle >> 8) & 0xFF;
        out[o++] = r->handle & 0xFF;
        out[o++] = r->uuid_type;
        out[o++] = (r->uuid >> 8) & 0xFF;
        out[o++] = r->uuid & 0xFF;
    }
    return o;
}

// ---- primary service discovery ------------------------------------------
uint32_t ble_central_svc_discover(void) {
    if (m_kick_sb_state == 1 || m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (!gatt_begin(BLE_GATT_OP_SVC_DISC)) {
        return NRF_ERROR_BUSY;
    }
    m_svc_count = 0;
    m_svc_state = 1; // discovering
    ret_code_t err = sd_ble_gattc_primary_services_discover(m_conn_handle, 0x0001, NULL);
    if (err != NRF_SUCCESS) {
        m_svc_state = 3;
        gatt_finish(BLE_GATT_OP_SVC_DISC);
    }
    return err;
}

uint16_t ble_central_copy_svcs(uint8_t start_index, uint8_t *out, uint16_t out_cap) {
    // Wire: state[1] | per service: uuid_type[1] | uuid[2 BE] | start[2 BE] | end[2 BE]
    // state: 0 idle, 1 discovering, 2 done, 3 error.
    if (out_cap < 1) {
        return 0;
    }
    out[0] = m_svc_state;
    uint16_t o = 1;
    for (uint8_t i = start_index; i < m_svc_count; i++) {
        if (o + 7 > out_cap) {
            break;
        }
        ble_svc_rec_t *r = &m_svcs[i];
        out[o++] = r->uuid_type;
        out[o++] = (r->uuid >> 8) & 0xFF;
        out[o++] = r->uuid & 0xFF;
        out[o++] = (r->start_handle >> 8) & 0xFF;
        out[o++] = r->start_handle & 0xFF;
        out[o++] = (r->end_handle >> 8) & 0xFF;
        out[o++] = r->end_handle & 0xFF;
    }
    return o;
}

// ---- standard device-info pull -------------------------------------------
// Read the connected target's standard informational characteristics with
// read-only GATT reads. Handles come from the discovered-characteristic table,
// so 'discover' must have run first. Point-to-point against the one target or environment-wide.
uint32_t ble_central_devinfo_start(void) {
    if (m_kick_sb_state == 1 || m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (m_char_count == 0) {
        return NRF_ERROR_INVALID_STATE; // host must run 'discover' first
    }
    if (!gatt_begin(BLE_GATT_OP_DEVINFO)) {
        return NRF_ERROR_BUSY;
    }
    // Build the worklist: match each standard UUID to a discovered value handle.
    for (uint8_t i = 0; i < BLE_DEVINFO_COUNT; i++) {
        ble_devinfo_rec_t *rec = &m_devinfo[i];
        rec->uuid   = k_devinfo_uuids[i];
        rec->handle = 0;
        rec->status = 0xFF;         // absent until matched / read
        rec->len    = 0;
        for (uint8_t j = 0; j < m_char_count; j++) {
            if (m_chars[j].uuid_type == BLE_UUID_TYPE_BLE &&
                m_chars[j].uuid == k_devinfo_uuids[i]) {
                rec->handle = m_chars[j].value_handle;
                break;
            }
        }
    }
    m_devinfo_state = 1;
    m_devinfo_idx = 0;
    devinfo_read_next();
    return NRF_SUCCESS;
}

uint16_t ble_central_copy_devinfo(uint8_t *out, uint16_t out_cap) {
    // Wire: state[1] | count[1] | per field: uuid[2 BE] | status[1] | len[1] | data[len]
    // state: 0 idle,1 running,2 done,3 error. status: 0xFF absent, else gatt_status.
    if (out_cap < 2) {
        return 0;
    }
    out[0] = m_devinfo_state;
    out[1] = BLE_DEVINFO_COUNT;
    uint16_t o = 2;
    for (uint8_t i = 0; i < BLE_DEVINFO_COUNT; i++) {
        ble_devinfo_rec_t *r = &m_devinfo[i];
        uint8_t n = (r->len > BLE_DEVINFO_VAL_MAX) ? BLE_DEVINFO_VAL_MAX : r->len;
        if (o + 4 + n > out_cap) {
            break;
        }
        out[o++] = (r->uuid >> 8) & 0xFF;
        out[o++] = r->uuid & 0xFF;
        out[o++] = r->status;
        out[o++] = n;
        memcpy(out + o, r->data, n);
        o += n;
    }
    return o;
}

uint32_t ble_central_gatt_read(uint16_t value_handle) {
    if (m_kick_sb_state == 1 || m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (value_handle == 0) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (!gatt_begin(BLE_GATT_OP_READ)) {
        return NRF_ERROR_BUSY;
    }
    m_read_state = 1;      // pending
    m_read_len = 0;
    m_read_status = 0xFF;  // no response yet
    ret_code_t err = sd_ble_gattc_read(m_conn_handle, value_handle, 0);
    if (err != NRF_SUCCESS) {
        m_read_state = 3;
        m_read_status = (uint8_t)(err & 0xFF);
        gatt_finish(BLE_GATT_OP_READ);
    }
    return err;
}

// Write a user-specified value to a characteristic (write-with-response), so the
// target's ATT status comes back. Point-to-point against the connected target or environment-wide if no target is selected.
uint32_t ble_central_gatt_write(uint16_t value_handle, const uint8_t *data, uint8_t len) {
    if (m_kick_sb_state == 1 || m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (value_handle == 0 || (len > 0 && data == NULL)) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (len > ble_central_write_max()) {
        return NRF_ERROR_DATA_SIZE;
    }
    if (!gatt_begin(BLE_GATT_OP_WRITE)) {
        return NRF_ERROR_BUSY;
    }
    static uint8_t wbuf[BLE_WRITE_MAX];
    if (len > 0) {
        memcpy(wbuf, data, len);
    }
    m_write_state  = 1;    // pending
    m_write_status = 0xFF;
    ble_gattc_write_params_t w = {
        .write_op = BLE_GATT_OP_WRITE_REQ,
        .flags    = 0,
        .handle   = value_handle,
        .offset   = 0,
        .len      = len,
        .p_value  = wbuf,
    };
    ret_code_t err = sd_ble_gattc_write(m_conn_handle, &w);
    if (err != NRF_SUCCESS) {
        m_write_state = 3;
        m_write_status = (uint8_t)(err & 0xFF);
        gatt_finish(BLE_GATT_OP_WRITE);
    }
    return err;
}

uint16_t ble_central_get_write_result(uint8_t *out, uint16_t out_cap) {
    // Wire: state[1] | gatt_status[1]. state: 0 idle, 1 pending, 2 done, 3 failed.
    if (out_cap < 2) {
        return 0;
    }
    out[0] = m_write_state;
    out[1] = m_write_status;
    return 2;
}

uint16_t ble_central_mtu(void) {
    if (m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return 23; // default ATT MTU when not connected
    }
    return ble_link_mtu(m_conn_handle);
}

uint16_t ble_central_write_max(void) {
    uint16_t mtu = ble_central_mtu();
    if (mtu <= 3) {
        return 0;
    }
    return MIN((uint16_t)(mtu - 3), (uint16_t)BLE_WRITE_MAX);
}

uint16_t ble_central_copy_read(uint8_t *out, uint16_t out_cap) {
    // Wire: state[1] | gatt_status[1] | len[1] | data[len]
    if (out_cap < 3) {
        return 0;
    }
    uint8_t n = (m_read_len > BLE_READ_VALUE_MAX) ? BLE_READ_VALUE_MAX : (uint8_t)m_read_len;
    if (3 + n > out_cap) {
        n = out_cap - 3;
    }
    uint16_t o = 0;
    out[o++] = m_read_state;
    out[o++] = m_read_status;
    out[o++] = n;
    memcpy(out + o, m_read_value, n);
    o += n;
    return o;
}

// Subscribe to notifications/indications by writing the target's CCCD.
// mode: 0 = off, 1 = notifications, 2 = indications.
uint32_t ble_central_find_cccd(uint16_t value_handle) {
    if (m_kick_sb_state == 1 || m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (value_handle == 0 || value_handle == 0xFFFF) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (!gatt_begin(BLE_GATT_OP_CCCD_DISC)) {
        return NRF_ERROR_BUSY;
    }
    m_cccd_state = 1;   // searching
    m_cccd_handle = 0;
    ble_gattc_handle_range_t range = { .start_handle = value_handle + 1, .end_handle = 0xFFFF };
    ret_code_t err = sd_ble_gattc_descriptors_discover(m_conn_handle, &range);
    if (err != NRF_SUCCESS) {
        m_cccd_state = 3;
        gatt_finish(BLE_GATT_OP_CCCD_DISC);
    }
    return err;
}

uint16_t ble_central_get_cccd(uint8_t *out, uint16_t out_cap) {
    // Wire: state[1] | cccd_handle[2 BE]. state: 0 idle,1 searching,2 found,3 not-found.
    if (out_cap < 3) {
        return 0;
    }
    out[0] = m_cccd_state;
    out[1] = (m_cccd_handle >> 8) & 0xFF;
    out[2] = m_cccd_handle & 0xFF;
    return 3;
}

uint32_t ble_central_subscribe(uint16_t cccd_handle, uint8_t mode) {
    if (m_kick_sb_state == 1 || m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (cccd_handle == 0 || mode > 2) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (!gatt_begin(BLE_GATT_OP_SUBSCRIBE)) {
        return NRF_ERROR_BUSY;
    }
    uint16_t cccd = (mode == 1) ? 0x0001 : (mode == 2) ? 0x0002 : 0x0000;
    static uint8_t val[2];
    val[0] = cccd & 0xFF;
    val[1] = (cccd >> 8) & 0xFF;
    ble_gattc_write_params_t w = {
        .write_op = BLE_GATT_OP_WRITE_REQ,
        .flags    = 0,
        .handle   = cccd_handle,
        .offset   = 0,
        .len      = 2,
        .p_value  = val,
    };
    ret_code_t err = sd_ble_gattc_write(m_conn_handle, &w);
    if (err != NRF_SUCCESS) {
        gatt_finish(BLE_GATT_OP_SUBSCRIBE);
    }
    if (err == NRF_SUCCESS && mode != 0) {
        m_notif_count = 0; // fresh capture on (re)subscribe
    }
    return err;
}

uint16_t ble_central_notif_count(void) {
    return m_notif_count;
}

uint16_t ble_central_copy_notifs(uint16_t start_index, uint8_t *out, uint16_t out_cap) {
    // Wire per entry: handle[2 BE] | len[1] | data[len]
    uint16_t o = 0;
    for (uint16_t i = start_index; i < m_notif_count; i++) {
        ble_notif_t *n = &m_notif_log[i];
        uint16_t rec = 2 + 1 + n->len;
        if (o + rec > out_cap) {
            break;
        }
        out[o++] = (n->handle >> 8) & 0xFF;
        out[o++] = n->handle & 0xFF;
        out[o++] = n->len;
        memcpy(out + o, n->data, n->len);
        o += n->len;
    }
    return o;
}

uint16_t ble_central_copy_chars(uint8_t start_index, uint8_t *out, uint16_t out_cap) {
    uint16_t o = 0;
    for (uint8_t i = start_index; i < m_char_count; i++) {
        if (o + 6 > out_cap) {
            break;
        }
        ble_char_rec_t *r = &m_chars[i];
        out[o++] = (r->value_handle >> 8) & 0xFF;
        out[o++] = r->value_handle & 0xFF;
        out[o++] = r->props;
        out[o++] = r->uuid_type;
        out[o++] = (r->uuid >> 8) & 0xFF;
        out[o++] = r->uuid & 0xFF;
    }
    return o;
}

uint32_t ble_central_fuzz_start(uint16_t value_handle, uint16_t max_iterations, uint16_t interval_ms) {
    if (m_kick_sb_state == 1 || m_conn_state != BLE_CONN_STATE_CONNECTED || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (value_handle == 0) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (m_gatt_op != BLE_GATT_OP_NONE) {
        return NRF_ERROR_BUSY;
    }
    if (m_flood_state == 1) {
        ble_central_flood_stop();
    }
    if (m_fuzz_state == 1) {
        ble_central_fuzz_stop();
    }
    if (interval_ms < 10) {
        interval_ms = 10;
    }
    m_fuzz_handle    = value_handle;
    m_fuzz_max       = max_iterations;
    m_fuzz_sent      = 0;
    m_fuzz_log_count = 0;
    m_gatt_transient_retries = 0;
    // Reset the PRNG deterministically but per-characteristic: the same handle
    // reproduces the same sequence (reproducible PoC), different handles diverge.
    m_fuzz_seed      = 0x1234ABCDu ^ (0x9E3779B1u * (uint32_t)value_handle);
    if (m_fuzz_seed == 0) m_fuzz_seed = 0xDEADBEEFu; // xorshift needs non-zero
    m_fuzz_state     = 1; // running
    rgb_marquee_set_ble_test_anim(true); // outside->center LED animation while testing
    ret_code_t err = timer_start(BLE_TIMER_OWNER_FUZZ, APP_TIMER_TICKS(interval_ms));
    if (err != NRF_SUCCESS) {
        m_fuzz_state = 0;
        rgb_marquee_set_ble_test_anim(false);
    } else if (m_fuzz_state != 1 || m_conn_state != BLE_CONN_STATE_CONNECTED ||
               m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        (void)app_timer_stop(m_fuzz_timer);
        if (m_timer_owner == BLE_TIMER_OWNER_FUZZ) {
            m_timer_owner = BLE_TIMER_OWNER_NONE;
        }
        m_fuzz_state = 2;
        rgb_marquee_set_ble_test_anim(false);
        err = NRF_ERROR_INVALID_STATE;
    }
    return err;
}

uint32_t ble_central_fuzz_stop(void) {
    if (m_fuzz_state == 1) {
        timer_stop(BLE_TIMER_OWNER_FUZZ);
        m_fuzz_state = 2;
    }
    rgb_marquee_set_ble_test_anim(false);
    return NRF_SUCCESS;
}

// ---- flood / link-churn (cybersecurity fork) ------------------------------
// Point-to-point stress against ONE already-connected target or environment-wide if no target is selected. The payload is
// fixed (the caller picks it), no L2CAP framing tricks — just rapid
// WRITE_CMD (no-response) so the SoftDevice TX queue runs as hot as it can.
// Caller is responsible for legal / authorised use (see CLAUDE.md
// fork-specific exemption).

uint32_t ble_central_flood_start(uint16_t value_handle, uint8_t payload_size, uint16_t max_iterations, uint16_t interval_ms) {
    if (m_kick_sb_state == 1 || m_conn_handle == BLE_CONN_HANDLE_INVALID ||
            m_conn_state != BLE_CONN_STATE_CONNECTED) {
        return NRF_ERROR_INVALID_STATE; // need an active central link
    }
    if (value_handle == 0 || payload_size == 0 || payload_size > BLE_FUZZ_PAYLOAD_MAX) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (m_gatt_op != BLE_GATT_OP_NONE) {
        return NRF_ERROR_BUSY;
    }
    // Stop the fuzzer if it was running — same timer, can't share.
    if (m_fuzz_state == 1) {
        ble_central_fuzz_stop();
    }
    if (m_flood_state == 1) {
        timer_stop(BLE_TIMER_OWNER_FLOOD);
        m_flood_state = 0;
    }
    m_flood_handle = value_handle;
    m_flood_size   = payload_size;
    m_flood_max    = max_iterations;       // 0 = until stop
    m_flood_sent   = 0;
    m_gatt_transient_retries = 0;
    // Deterministic payload: a counter-pattern so the target can recognise / filter
    // if it wants to. Each tick sends the same bytes; the volume is the point.
    for (uint8_t i = 0; i < payload_size; i++) {
        m_flood_payload[i] = (uint8_t)((i * 31u + 0xA5u) & 0xFF);
    }
    m_flood_state  = 1;
    rgb_marquee_set_ble_active_anim(true);
    ret_code_t err = timer_start(BLE_TIMER_OWNER_FLOOD, APP_TIMER_TICKS(interval_ms));
    if (err != NRF_SUCCESS) {
        m_flood_state = 0;
        rgb_marquee_set_ble_active_anim(false);
    } else if (m_flood_state != 1 || m_conn_state != BLE_CONN_STATE_CONNECTED ||
               m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        (void)app_timer_stop(m_fuzz_timer);
        if (m_timer_owner == BLE_TIMER_OWNER_FLOOD) {
            m_timer_owner = BLE_TIMER_OWNER_NONE;
        }
        m_flood_state = 2;
        rgb_marquee_set_ble_active_anim(false);
        err = NRF_ERROR_INVALID_STATE;
    }
    return err;
}

uint32_t ble_central_flood_stop(void) {
    if (m_kick_sb_state == 1 && m_sb_op == BLE_SB_OP_FLOOD) {
        scan_buffer_stop(true);
        return NRF_SUCCESS;
    }
    if (m_flood_state == 1) {
        timer_stop(BLE_TIMER_OWNER_FLOOD);
        m_flood_state = 2;
    }
    rgb_marquee_set_ble_active_anim(false);
    return NRF_SUCCESS;
}

uint32_t ble_central_flood_count(void) {
    return m_flood_sent;
}

// "Kick" — force-disconnect the current central link once. Repeated churn is
// handled by the scan-buffer path, which reconnects between cycles.
uint32_t ble_central_kick(uint8_t cycles) {
    if (cycles != 1) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (m_kick_sb_state == 1 || m_conn_state != BLE_CONN_STATE_CONNECTED) {
        return NRF_ERROR_INVALID_STATE; // no link to kick
    }
    ble_central_flood_stop();
    ble_central_fuzz_stop();
    // A single central link can only be disconnected once. Repeated churn is
    // implemented by the scan-buffer path, which reconnects between cycles.
    ret_code_t err = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (err == NRF_SUCCESS || err == NRF_ERROR_INVALID_STATE) {
        m_conn_state = BLE_CONN_STATE_DISCONNECTING;
        return NRF_SUCCESS;
    }
    return err;
}

static void scan_buffer_reset_state(void) {
    m_kick_sb_state = 0;
    m_sb_op = BLE_SB_OP_NONE;
    m_sb_phase = BLE_SB_PHASE_IDLE;
    m_kick_sb_index = 0;
    m_kick_sb_sub = 0;
    m_sb_peer_sent = 0;
    m_sb_wait_ticks = 0;
    m_sb_flood_tick = 0;
}

static void scan_buffer_stop(bool disconnect_link) {
    if (disconnect_link) {
        if (m_conn_state == BLE_CONN_STATE_CONNECTING && m_conn_handle == BLE_CONN_HANDLE_INVALID) {
            ret_code_t err = sd_ble_gap_connect_cancel();
            if (err == NRF_SUCCESS || err == NRF_ERROR_INVALID_STATE) {
                m_conn_state = BLE_CONN_STATE_CANCELLING;
            }
        } else if (m_conn_handle != BLE_CONN_HANDLE_INVALID) {
            ret_code_t err = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err == NRF_SUCCESS || err == NRF_ERROR_INVALID_STATE) {
                m_conn_state = BLE_CONN_STATE_DISCONNECTING;
            }
        }
    }
    if (m_sb_op == BLE_SB_OP_FLOOD && m_flood_state == 1) {
        m_flood_state = 2;
    }
    timer_stop(BLE_TIMER_OWNER_SCAN_BUFFER);
    scan_buffer_reset_state();
    rgb_marquee_set_ble_active_anim(false);
}

static void scan_buffer_start_next_peer(void) {
    while (m_kick_sb_state == 1 && m_kick_sb_index < m_kick_sb_count) {
        ble_gap_addr_t addr = {0};
        memcpy(addr.addr, m_kick_sb_addrs[m_kick_sb_index].addr, BLE_GAP_ADDR_LEN);
        addr.addr_type = m_kick_sb_addrs[m_kick_sb_index].addr_type;

        reset_link_async_state();
        m_conn_state = BLE_CONN_STATE_CONNECTING;
        m_last_disc_reason = 0;
        m_sb_phase = BLE_SB_PHASE_CONNECTING;
        m_sb_wait_ticks = 0;
        m_sb_peer_sent = 0;
        m_sb_flood_tick = 0;
        m_gatt_transient_retries = 0;

        ret_code_t err = prepare_target_connect();
        if (err == NRF_SUCCESS) {
            err = sd_ble_gap_connect(&addr, &m_init_scan_params, &m_conn_params,
                                     APP_BLE_CONN_CFG_TAG);
        }
        if (err == NRF_SUCCESS) {
            return;
        }

        m_conn_state = BLE_CONN_STATE_IDLE;
        m_last_disc_reason = (uint8_t)(err & 0xFF);
        m_kick_sb_index++;
        m_kick_sb_sub = 0;
    }

    scan_buffer_stop(false);
}

static void scan_buffer_disconnect_current(void) {
    m_sb_phase = BLE_SB_PHASE_DISCONNECTING;
    m_sb_wait_ticks = 0;
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID || m_conn_state != BLE_CONN_STATE_CONNECTED) {
        return;
    }
    ret_code_t err = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (err == NRF_SUCCESS || err == NRF_ERROR_INVALID_STATE) {
        m_conn_state = BLE_CONN_STATE_DISCONNECTING;
    } else {
        m_last_disc_reason = (uint8_t)(err & 0xFF);
        scan_buffer_stop(false);
    }
}

// Scan-buffer-wide operations: every cached connectable address is attempted in
// RSSI order. One bad or non-responsive peer is skipped instead of stalling the
// whole run.
static void scan_buffer_timer_handler(void *p_context) {
    UNUSED_PARAMETER(p_context);

    if (m_kick_sb_state != 1) {
        timer_stop(BLE_TIMER_OWNER_SCAN_BUFFER);
        return;
    }

    if (m_kick_sb_index >= m_kick_sb_count) {
        scan_buffer_stop(false);
        return;
    }

    switch (m_sb_phase) {
    case BLE_SB_PHASE_IDLE:
        scan_buffer_start_next_peer();
        return;

    case BLE_SB_PHASE_CONNECTING:
        if (m_conn_state == BLE_CONN_STATE_CONNECTED && m_conn_handle != BLE_CONN_HANDLE_INVALID) {
            m_sb_phase = BLE_SB_PHASE_ACTIVE;
            m_sb_wait_ticks = 0;
            return;
        }
        if ((m_conn_state == BLE_CONN_STATE_IDLE || m_conn_state == BLE_CONN_STATE_DISCONNECTED) &&
                m_conn_handle == BLE_CONN_HANDLE_INVALID) {
            m_kick_sb_index++;
            m_kick_sb_sub = 0;
            scan_buffer_start_next_peer();
            return;
        }
        if (m_conn_state == BLE_CONN_STATE_CONNECTING &&
                ++m_sb_wait_ticks >= m_sb_conn_timeout_ticks) {
            ret_code_t err = sd_ble_gap_connect_cancel();
            if (err == NRF_SUCCESS || err == NRF_ERROR_INVALID_STATE) {
                m_conn_state = BLE_CONN_STATE_CANCELLING;
            }
        }
        return;

    case BLE_SB_PHASE_ACTIVE:
        if (m_conn_handle == BLE_CONN_HANDLE_INVALID) {
            m_kick_sb_index++;
            m_kick_sb_sub = 0;
            scan_buffer_start_next_peer();
            return;
        }
        if (m_conn_state != BLE_CONN_STATE_CONNECTED) {
            return;
        }

        if (m_sb_op == BLE_SB_OP_KICK) {
            m_kick_sb_sub++;
            scan_buffer_disconnect_current();
            return;
        }

        if (m_sb_op == BLE_SB_OP_FLOOD) {
            if (m_sb_peer_sent >= m_sb_peer_limit) {
                scan_buffer_disconnect_current();
                return;
            }
            if (m_sb_flood_tick > 0) {
                m_sb_flood_tick--;
                return;
            }

            ble_gattc_write_params_t w = {
                .write_op = BLE_GATT_OP_WRITE_CMD,
                .flags    = 0,
                .handle   = m_flood_handle,
                .offset   = 0,
                .len      = m_flood_size,
                .p_value  = m_flood_payload,
            };
            ret_code_t err = sd_ble_gattc_write(m_conn_handle, &w);
            if (err == NRF_SUCCESS) {
                m_sb_flood_tick = (m_sb_flood_stride > 0) ? (m_sb_flood_stride - 1) : 0;
                m_sb_peer_sent++;
                m_flood_sent++;
                m_gatt_transient_retries = 0;
            } else if (!gatt_write_retryable(err) ||
                       ++m_gatt_transient_retries >= BLE_GATT_TRANSIENT_RETRY_MAX) {
                m_last_disc_reason = (uint8_t)(err & 0xFF);
                scan_buffer_disconnect_current();
            }
            return;
        }

        scan_buffer_stop(true);
        return;

    case BLE_SB_PHASE_DISCONNECTING:
        if (m_conn_handle == BLE_CONN_HANDLE_INVALID &&
                m_conn_state == BLE_CONN_STATE_DISCONNECTED) {
            if (m_sb_op == BLE_SB_OP_KICK && m_kick_sb_sub < m_kick_sb_cycles) {
                m_conn_state = BLE_CONN_STATE_IDLE;
                m_sb_phase = BLE_SB_PHASE_IDLE;
            } else {
                m_kick_sb_index++;
                m_kick_sb_sub = 0;
                m_conn_state = BLE_CONN_STATE_IDLE;
                scan_buffer_start_next_peer();
            }
            return;
        }
        if (++m_sb_wait_ticks >= m_sb_disc_timeout_ticks) {
            scan_buffer_stop(true);
        }
        return;

    default:
        scan_buffer_stop(true);
        return;
    }
}

uint32_t ble_central_kick_scan_buffer(uint8_t cycles) {
    if (cycles == 0 || cycles > 10) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (m_kick_sb_state == 1) {
        return NRF_ERROR_BUSY;
    }
    if (m_conn_state != BLE_CONN_STATE_IDLE && m_conn_state != BLE_CONN_STATE_DISCONNECTED) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (m_timer_owner != BLE_TIMER_OWNER_NONE) {
        return NRF_ERROR_BUSY;
    }
    ret_code_t err = ble_scan_stop();
    if (err != NRF_SUCCESS) {
        return err;
    }
    m_kick_sb_count = ble_scan_copy_addresses(m_kick_sb_addrs, BLE_SCAN_MAX_DEVICES);
    if (m_kick_sb_count == 0) {
        // Distinct code from "no link": caller maps to STATUS_PAR_ERR
        // (the scan buffer is operator-side input, not a device-mode issue).
        return NRF_ERROR_NO_MEM;
    }
    // Stop any in-progress flood or fuzzer — we share m_fuzz_timer.
    if (m_flood_state == 1) {
        ble_central_flood_stop();
    }
    if (m_fuzz_state == 1) {
        ble_central_fuzz_stop();
    }
    m_sb_op = BLE_SB_OP_KICK;
    m_sb_phase = BLE_SB_PHASE_IDLE;
    m_kick_sb_cycles = cycles;
    m_kick_sb_index = 0;
    m_kick_sb_sub = 0;
    m_sb_wait_ticks = 0;
    m_kick_sb_state = 1;
    rgb_marquee_set_ble_active_anim(true);
    err = timer_start(BLE_TIMER_OWNER_SCAN_BUFFER, APP_TIMER_TICKS(BLE_SB_TICK_MS));
    if (err != NRF_SUCCESS) {
        scan_buffer_stop(false);
    }
    return err;
}

// Scan-buffer-wide WRITE_CMD flood. For every cached address: connect →
// spam WRITE_CMDs → disconnect → next. max_iterations is per-peer; 0 maps to a
// bounded default so a buffer-wide run cannot hang forever on the first peer.
uint32_t ble_central_flood_scan_buffer(uint16_t value_handle, uint8_t payload_size,
                                       uint16_t max_iterations, uint16_t interval_ms) {
    if (value_handle == 0 || payload_size == 0 || payload_size > BLE_FUZZ_PAYLOAD_MAX) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (m_kick_sb_state == 1) {
        return NRF_ERROR_BUSY;
    }
    if (m_conn_state != BLE_CONN_STATE_IDLE && m_conn_state != BLE_CONN_STATE_DISCONNECTED) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (m_timer_owner != BLE_TIMER_OWNER_NONE) {
        return NRF_ERROR_BUSY;
    }
    ret_code_t err = ble_scan_stop();
    if (err != NRF_SUCCESS) {
        return err;
    }
    // Stop any in-progress fuzzer / flood / kick-sb.
    if (m_fuzz_state == 1)  ble_central_fuzz_stop();
    if (m_flood_state == 1) ble_central_flood_stop();
    m_kick_sb_count = ble_scan_copy_addresses(m_kick_sb_addrs, BLE_SCAN_MAX_DEVICES);
    if (m_kick_sb_count == 0) {
        // Distinct code from "no link" → caller maps to STATUS_PAR_ERR.
        return NRF_ERROR_NO_MEM;
    }
    // Store the per-peer WRITE_CMD spam parameters for the timer handler.
    m_flood_handle  = value_handle;
    m_flood_size    = payload_size;
    m_flood_max     = max_iterations;
    m_sb_peer_limit = max_iterations == 0 ? BLE_SB_FLOOD_DEFAULT_COUNT : max_iterations;
    m_flood_sent    = 0;
    for (uint8_t i = 0; i < payload_size; i++) {
        m_flood_payload[i] = (uint8_t)((i * 31u + 0xA5u) & 0xFF);
    }
    m_sb_flood_stride = (interval_ms + BLE_SB_TICK_MS - 1) / BLE_SB_TICK_MS;
    if (m_sb_flood_stride == 0) {
        m_sb_flood_stride = 1;
    }
    m_sb_flood_tick = 0;
    m_sb_op = BLE_SB_OP_FLOOD;
    m_sb_phase = BLE_SB_PHASE_IDLE;
    m_kick_sb_cycles = 0;
    m_kick_sb_index = 0;
    m_kick_sb_sub = 0;
    m_sb_wait_ticks = 0;
    m_flood_state  = 1;
    m_kick_sb_state = 1;
    rgb_marquee_set_ble_active_anim(true);
    err = timer_start(BLE_TIMER_OWNER_SCAN_BUFFER, APP_TIMER_TICKS(BLE_SB_TICK_MS));
    if (err != NRF_SUCCESS) {
        scan_buffer_stop(false);
        m_flood_state = 0;
    }
    return err;
}

uint16_t ble_central_get_state(uint8_t *out, uint16_t out_cap) {
    if (out_cap < 12) {
        return 0;
    }
    uint16_t o = 0;
    out[o++] = m_conn_state;
    out[o++] = m_disc_state;
    out[o++] = m_char_count;
    out[o++] = m_fuzz_state;
    out[o++] = (m_fuzz_sent >> 8) & 0xFF;
    out[o++] = m_fuzz_sent & 0xFF;
    out[o++] = (m_conn_state == BLE_CONN_STATE_CONNECTED &&
                m_conn_handle != BLE_CONN_HANDLE_INVALID) ? 1 : 0;
    out[o++] = m_last_disc_reason;
    out[o++] = m_probe_state;
    out[o++] = m_probe_result;
    out[o++] = m_probe_index;
    out[o++] = m_probe_total;
    if (out_cap >= 21) {
        out[o++] = m_flood_state;
        out[o++] = (m_flood_sent >> 24) & 0xFF;
        out[o++] = (m_flood_sent >> 16) & 0xFF;
        out[o++] = (m_flood_sent >> 8) & 0xFF;
        out[o++] = m_flood_sent & 0xFF;
        out[o++] = m_read_state;
        out[o++] = m_write_state;
        out[o++] = (m_notif_count >> 8) & 0xFF;
        out[o++] = m_notif_count & 0xFF;
    }
    return o;
}

uint16_t ble_central_copy_probe_log(uint8_t start_index, uint8_t *out, uint16_t out_cap) {
    uint16_t o = 0;
    for (uint8_t i = start_index; i < m_probe_log_count; i++) {
        ble_probe_log_t *e = &m_probe_log[i];
        uint16_t rec = BLE_GAP_ADDR_LEN + 1 + 1 + 1 + 1 + 1;
        if (o + rec > out_cap) {
            break;
        }
        memcpy(out + o, e->addr, BLE_GAP_ADDR_LEN);
        o += BLE_GAP_ADDR_LEN;
        out[o++] = e->addr_type;
        out[o++] = (uint8_t)e->rssi;
        out[o++] = e->connect_status;
        out[o++] = e->probe_result;
        out[o++] = e->disconnect_reason;
    }
    return o;
}

uint16_t ble_central_copy_log(uint16_t start_index, uint8_t *out, uint16_t out_cap) {
    uint16_t o = 0;
    for (uint16_t i = start_index; i < m_fuzz_log_count; i++) {
        ble_fuzz_log_t *e = &m_fuzz_log[i];
        uint8_t dlen = MIN(e->len, (uint8_t)BLE_FUZZ_LOG_DATA);
        uint16_t rec = 2 + 1 + 1 + dlen;
        if (o + rec > out_cap) {
            break;
        }
        out[o++] = (e->index >> 8) & 0xFF;
        out[o++] = e->index & 0xFF;
        out[o++] = e->len;
        out[o++] = e->status;
        memcpy(out + o, e->data, dlen);
        o += dlen;
    }
    return o;
}


// Central link state queries used by ble_radio_set() to tear down the
// central connection cleanly on a radio toggle.
bool ble_central_is_connected(void) {
    return m_conn_state == BLE_CONN_STATE_CONNECTED;
}

bool ble_central_is_connecting(void) {
    return m_conn_state == BLE_CONN_STATE_CONNECTING ||
           m_conn_state == BLE_CONN_STATE_CANCELLING ||
           m_conn_state == BLE_CONN_STATE_DISCONNECTING ||
           m_kick_sb_state == 1;
}
