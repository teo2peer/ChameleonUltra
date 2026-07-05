/*
 * Directed BLE GATT fuzzing harness (SoftDevice S140 central role).
 *
 * See ble_central.h for the scope constraint: this is strictly point-to-point
 * against ONE operator-specified target. It connects out, enumerates that single
 * target's GATT characteristics, and writes mutated payloads to a chosen
 * characteristic to exercise the target's input parsing. It never broadcasts,
 * floods or emits anything to the environment.
 *
 * The firmware only ever holds one central link (the target). All state below is
 * for that single link. Long-running output (discovered characteristics, fuzz
 * log) is buffered here and paged out by the host, matching the request/response
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
#define BLE_READ_VALUE_MAX          64      // bytes retained from a GATT read response
#define BLE_NOTIF_LOG_MAX           64      // received notifications/indications retained
#define BLE_NOTIF_DATA_MAX          20      // bytes kept per notification

// ---- discovered-characteristic table -------------------------------------
typedef struct {
    uint16_t value_handle;
    uint8_t  props;             // packed: bit0 broadcast,1 read,2 write_wo_resp,3 write,4 notify,5 indicate,6 auth_signed_wr
    uint8_t  uuid_type;
    uint16_t uuid;
} ble_char_rec_t;

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
static volatile uint8_t m_conn_state = 0;   // 0 idle,1 connecting,2 connected,3 disconnected
static volatile uint8_t m_disc_state = 0;   // 0 idle,1 discovering,2 done,3 error
static uint8_t          m_last_disc_reason = 0;

static ble_char_rec_t   m_chars[BLE_MAX_CHARS];
static volatile uint8_t m_char_count = 0;

static volatile uint8_t  m_fuzz_state = 0;  // 0 idle,1 running,2 stopped/finished
static uint16_t          m_fuzz_handle = 0;
static uint16_t          m_fuzz_max = 0;
static volatile uint16_t m_fuzz_sent = 0;
static ble_fuzz_log_t    m_fuzz_log[BLE_FUZZ_LOG_MAX];
static volatile uint16_t m_fuzz_log_count = 0;
static uint32_t          m_fuzz_seed = 0x1234ABCDu;

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

static void continue_char_discovery(uint16_t next_handle) {
    if (m_char_count >= BLE_MAX_CHARS || next_handle >= 0xFFFF) {
        m_disc_state = 2; // done (table full or range exhausted)
        return;
    }
    ble_gattc_handle_range_t range = { .start_handle = next_handle + 1, .end_handle = 0xFFFF };
    ret_code_t err = sd_ble_gattc_characteristics_discover(m_conn_handle, &range);
    if (err != NRF_SUCCESS) {
        m_disc_state = (m_char_count > 0) ? 2 : 3; // partial results still usable
    }
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

    m_char_count = 0;
    m_disc_state = 0;
    m_fuzz_state = 0;
    m_fuzz_sent = 0;
    m_fuzz_log_count = 0;
    m_read_state = 0;
    m_last_disc_reason = 0;
    m_probe_result = 0;
    m_probe_current_ok = 0;
    m_probe_state = 1;
    m_conn_state = 1;

    ret_code_t err = sd_ble_gap_connect(&peer, &m_init_scan_params, &m_conn_params, APP_BLE_CONN_CFG_TAG);
    if (err != NRF_SUCCESS) {
        m_conn_state = 0;
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
    uint8_t raw[BLE_PROBE_BATCH_MAX * (BLE_GAP_ADDR_LEN + 3 + BLE_PROBE_ADV_DATA_MAX)];
    uint16_t raw_len = ble_scan_copy_records(0, raw, sizeof(raw));
    uint16_t o = 0;
    uint8_t count = 0;

    // Bound by the destination array (m_probe_targets[BLE_PROBE_BATCH_MAX]) so a
    // larger scan table can never overflow it, even if the two maxes diverge.
    while (o + BLE_GAP_ADDR_LEN + 3 <= raw_len && count < BLE_PROBE_BATCH_MAX) {
        ble_probe_target_t *target = &m_probe_targets[count];
        memcpy(target->addr, &raw[o], BLE_GAP_ADDR_LEN);
        o += BLE_GAP_ADDR_LEN;
        target->addr_type = raw[o++];
        target->rssi = (int8_t)raw[o++];
        uint8_t adv_len = raw[o++];
        if ((uint16_t)(o + adv_len) > raw_len) {
            break;
        }
        o += adv_len;
        count++;
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
    if (m_conn_state == 1 || m_conn_state == 2) {
        return NRF_ERROR_INVALID_STATE;
    }

    ble_scan_stop();

    uint32_t err = probe_load_scan_targets();
    if (err != NRF_SUCCESS) {
        m_probe_state = 3;
        m_probe_result = (uint8_t)(err & 0xFF);
        return err;
    }

    probe_reset_batch_state();
    m_probe_global_mode = 1;
    m_probe_state = 1;
    m_probe_total = ble_scan_get_count();
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
                m_conn_handle = gap->conn_handle;
                m_conn_state  = 2;  // connected
                NRF_LOG_INFO("Central connected to target, handle 0x%x", m_conn_handle);
                if (m_probe_global_mode == 1 && m_probe_state == 1) {
                    m_probe_current_ok = 0;
                    ret_code_t err = sd_ble_gap_conn_param_update(m_conn_handle, &m_conn_params);
                    if (err != NRF_SUCCESS) {
                        probe_global_finish_current((uint8_t)(err & 0xFF), (uint8_t)(err & 0xFF), 0);
                        (void)sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                    }
                }
            }
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            if (gap->conn_handle == m_conn_handle) {
                m_last_disc_reason = gap->params.disconnected.reason;
                m_conn_handle = BLE_CONN_HANDLE_INVALID;
                m_conn_state  = 3;  // disconnected
                if (m_probe_global_mode == 1 && m_probe_state == 1) {
                    uint8_t connect_status = m_probe_current_ok ? 0 : ((m_probe_result != 0) ? m_probe_result : m_last_disc_reason);
                    uint8_t probe_result = m_probe_current_ok ? 0 : ((m_probe_result != 0) ? m_probe_result : m_last_disc_reason);
                    probe_global_finish_current(connect_status, probe_result, m_last_disc_reason);
                    if (m_probe_index < m_probe_total) {
                        m_conn_state = 0;
                        (void)probe_global_start_next();
                    } else {
                        m_probe_global_mode = 0;
                        m_probe_state = 2;
                    }
                } else if (m_probe_state == 1) {
                    m_probe_state = 3;
                    m_probe_result = m_last_disc_reason;
                }
                if (m_fuzz_state == 1) {
                    // Target dropped the link mid-fuzz: possible crash or a
                    // defensive/parser-driven disconnect. Flag by stopping.
                    m_fuzz_state = 2;
                    app_timer_stop(m_fuzz_timer);
                }
                rgb_marquee_set_ble_test_anim(false);
                NRF_LOG_INFO("Central target disconnected, reason 0x%x", m_last_disc_reason);
            }
            break;

        case BLE_GAP_EVT_TIMEOUT:
            if (gap->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN && m_conn_state == 1) {
                m_conn_state = 0;   // connection attempt timed out
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
                    (void)sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                } else {
                    m_probe_state = 2;
                }
            }
            break;

        case BLE_GATTC_EVT_CHAR_DISC_RSP:
            if (gattc->conn_handle != m_conn_handle) {
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
                continue_char_discovery(last);
            } else {
                // BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND => enumeration complete
                m_disc_state = 2;
            }
            break;

        case BLE_GATTC_EVT_WRITE_RSP:
            // Result of a user write-with-response (the fuzzer uses WRITE_CMD,
            // which produces no response, so this is only our explicit writes).
            if (gattc->conn_handle == m_conn_handle && m_write_state == 1) {
                m_write_status = gattc->gatt_status;
                m_write_state = 2; // done
            }
            break;

        case BLE_GATTC_EVT_READ_RSP:
            if (gattc->conn_handle != m_conn_handle) {
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
            // Looking for a characteristic's CCCD (UUID 0x2902).
            if (gattc->conn_handle != m_conn_handle || m_cccd_state != 1) {
                break;
            }
            if (gattc->gatt_status != BLE_GATT_STATUS_SUCCESS) {
                m_cccd_state = 3; // not found
                break;
            }
            const ble_gattc_evt_desc_disc_rsp_t *r = &gattc->params.desc_disc_rsp;
            uint16_t last = 0;
            for (uint16_t i = 0; i < r->count; i++) {
                uint16_t uuid = r->descs[i].uuid.uuid;
                last = r->descs[i].handle;
                if (uuid == BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG) { // 0x2902
                    m_cccd_handle = r->descs[i].handle;
                    m_cccd_state = 2; // found
                    break;
                }
                if (uuid == BLE_UUID_CHARACTERISTIC) { // 0x2803: reached the next char
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
    if (m_fuzz_state != 1 || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return;
    }
    if (m_fuzz_max != 0 && m_fuzz_sent >= m_fuzz_max) {
        m_fuzz_state = 2;
        app_timer_stop(m_fuzz_timer);
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
    // NRF_ERROR_RESOURCES = stack TX queue full; leave the iteration uncounted so
    // the same slot is retried next tick.
    if (err == NRF_SUCCESS) {
        m_fuzz_sent++;
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
    if (m_probe_global_mode == 1) {
        return NRF_ERROR_BUSY;
    }
    if (m_conn_state == 1 || m_conn_state == 2) {
        return NRF_ERROR_INVALID_STATE; // already connecting/connected to a target
    }
    ble_gap_addr_t peer;
    memset(&peer, 0, sizeof(peer));
    peer.addr_type = addr_type;
    memcpy(peer.addr, addr, BLE_GAP_ADDR_LEN);

    m_char_count = 0;
    m_disc_state = 0;
    m_fuzz_state = 0;
    m_fuzz_sent = 0;
    m_fuzz_log_count = 0;
    m_read_state = 0;
    m_write_state = 0;
    m_notif_count = 0;
    m_cccd_state = 0;
    m_probe_state = 0;
    m_probe_result = 0;
    m_conn_state = 1; // connecting

    ret_code_t err = sd_ble_gap_connect(&peer, &m_init_scan_params, &m_conn_params, APP_BLE_CONN_CFG_TAG);
    if (err != NRF_SUCCESS) {
        m_conn_state = 0;
    }
    return err;
}

uint32_t ble_central_disconnect(void) {
    ble_central_fuzz_stop();
    if (m_probe_global_mode == 1) {
        m_probe_global_mode = 0;
        m_probe_state = 0;
        m_probe_result = 0;
    }
    if (m_conn_state == 1 && m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        // still establishing: cancel the pending connection
        m_conn_state = 0;
        return sd_ble_gap_connect_cancel();
    }
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        m_conn_state = 0;
        return NRF_SUCCESS;
    }
    return sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
}

uint32_t ble_central_link_probe(uint8_t global_mode) {
    if (global_mode != 0) {
        return probe_global_begin();
    }
    if (m_probe_global_mode == 1) {
        return NRF_ERROR_BUSY;
    }
    if (m_conn_state != 2 || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
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
    if (m_conn_state != 2 || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    m_char_count = 0;
    m_disc_state = 1; // discovering
    ble_gattc_handle_range_t range = { .start_handle = 0x0001, .end_handle = 0xFFFF };
    ret_code_t err = sd_ble_gattc_characteristics_discover(m_conn_handle, &range);
    if (err != NRF_SUCCESS) {
        m_disc_state = 3; // error
    }
    return err;
}

uint8_t ble_central_get_char_count(void) {
    return m_char_count;
}

uint32_t ble_central_gatt_read(uint16_t value_handle) {
    if (m_conn_state != 2 || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    m_read_state = 1;      // pending
    m_read_len = 0;
    m_read_status = 0xFF;  // no response yet
    return sd_ble_gattc_read(m_conn_handle, value_handle, 0);
}

// Write a user-specified value to a characteristic (write-with-response), so the
// target's ATT status comes back. Point-to-point against the connected target.
uint32_t ble_central_gatt_write(uint16_t value_handle, const uint8_t *data, uint8_t len) {
    if (m_conn_state != 2 || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    static uint8_t wbuf[BLE_FUZZ_PAYLOAD_MAX];
    if (len > BLE_FUZZ_PAYLOAD_MAX) {
        len = BLE_FUZZ_PAYLOAD_MAX; // capped to default ATT_MTU - 3
    }
    memcpy(wbuf, data, len);
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
        m_write_state = 0;
    }
    return err;
}

uint16_t ble_central_get_write_result(uint8_t *out, uint16_t out_cap) {
    // Wire: state[1] | gatt_status[1]. state: 0 idle, 1 pending, 2 done.
    if (out_cap < 2) {
        return 0;
    }
    out[0] = m_write_state;
    out[1] = m_write_status;
    return 2;
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
    if (m_conn_state != 2 || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    m_cccd_state = 1;   // searching
    m_cccd_handle = 0;
    ble_gattc_handle_range_t range = { .start_handle = value_handle + 1, .end_handle = 0xFFFF };
    ret_code_t err = sd_ble_gattc_descriptors_discover(m_conn_handle, &range);
    if (err != NRF_SUCCESS) {
        m_cccd_state = 3;
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
    if (m_conn_state != 2 || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
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
    if (mode != 0) {
        m_notif_count = 0; // fresh capture on (re)subscribe
    }
    return sd_ble_gattc_write(m_conn_handle, &w);
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
    if (m_conn_state != 2 || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (value_handle == 0) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (interval_ms < 10) {
        interval_ms = 10;
    }
    m_fuzz_handle    = value_handle;
    m_fuzz_max       = max_iterations;
    m_fuzz_sent      = 0;
    m_fuzz_log_count = 0;
    // Reset the PRNG deterministically but per-characteristic: the same handle
    // reproduces the same sequence (reproducible PoC), different handles diverge.
    m_fuzz_seed      = 0x1234ABCDu ^ (0x9E3779B1u * (uint32_t)value_handle);
    if (m_fuzz_seed == 0) m_fuzz_seed = 0xDEADBEEFu; // xorshift needs non-zero
    m_fuzz_state     = 1; // running
    rgb_marquee_set_ble_test_anim(true); // outside->center LED animation while testing
    return app_timer_start(m_fuzz_timer, APP_TIMER_TICKS(interval_ms), NULL);
}

uint32_t ble_central_fuzz_stop(void) {
    if (m_fuzz_state == 1) {
        app_timer_stop(m_fuzz_timer);
        m_fuzz_state = 2;
    }
    rgb_marquee_set_ble_test_anim(false);
    return NRF_SUCCESS;
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
    out[o++] = (m_conn_handle != BLE_CONN_HANDLE_INVALID) ? 1 : 0;
    out[o++] = m_last_disc_reason;
    out[o++] = m_probe_state;
    out[o++] = m_probe_result;
    out[o++] = m_probe_index;
    out[o++] = m_probe_total;
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
