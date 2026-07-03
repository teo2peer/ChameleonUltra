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

#include "ble_central.h"

#define NRF_LOG_MODULE_NAME ble_central
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define APP_BLE_CONN_CFG_TAG        1       // must match the tag used in ble_main.c
#define BLE_CENTRAL_OBSERVER_PRIO   2

#define BLE_MAX_CHARS               48      // discovered characteristics retained
#define BLE_FUZZ_LOG_MAX            128     // fuzz-log entries retained
#define BLE_FUZZ_PAYLOAD_MAX        20      // <= default ATT_MTU(23) - 3, avoids DATA_SIZE
#define BLE_FUZZ_LOG_DATA           16      // payload bytes kept per log entry
#define BLE_READ_VALUE_MAX          64      // bytes retained from a GATT read response

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

// Last GATT read result (from the single connected target).
static volatile uint8_t m_read_state = 0;   // 0 idle,1 pending,2 ready
static uint8_t          m_read_status = 0;  // gatt_status of the last read
static uint16_t         m_read_len = 0;
static uint8_t          m_read_value[BLE_READ_VALUE_MAX];

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

// Build one payload for the given iteration. A small deterministic corpus of
// boundary cases is emitted first (empty write, single 0x00/0xFF, max-length
// all-zeros / all-ones, incrementing), then payloads are pseudo-randomly
// mutated. The corpus covers edge cases a pure RNG rarely hits.
static void fuzz_build_payload(uint16_t iteration, uint8_t *buf, uint8_t *out_len) {
    const uint8_t maxp = BLE_FUZZ_PAYLOAD_MAX;
    switch (iteration) {
        case 0: *out_len = 0; return;                                         // empty write
        case 1: buf[0] = 0x00; *out_len = 1; return;                          // single 0x00
        case 2: buf[0] = 0xFF; *out_len = 1; return;                          // single 0xFF
        case 3: memset(buf, 0x00, maxp); *out_len = maxp; return;             // max-len zeros
        case 4: memset(buf, 0xFF, maxp); *out_len = maxp; return;             // max-len ones
        case 5:
            for (uint8_t i = 0; i < maxp; i++) buf[i] = i;
            *out_len = maxp;
            return;                                                           // incrementing
        default:
            break;
    }
    uint8_t len = 1 + (xorshift32() % maxp);
    for (uint8_t i = 0; i < len; i++) {
        uint32_t v = xorshift32();
        switch (v & 3) {
            case 0:  buf[i] = 0x00; break;
            case 1:  buf[i] = 0xFF; break;
            case 2:  buf[i] = (uint8_t)(v >> 8); break;
            default: buf[i] = i; break;
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
            }
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            if (gap->conn_handle == m_conn_handle) {
                m_last_disc_reason = gap->params.disconnected.reason;
                m_conn_handle = BLE_CONN_HANDLE_INVALID;
                m_conn_state  = 3;  // disconnected
                if (m_fuzz_state == 1) {
                    // Target dropped the link mid-fuzz: possible crash or a
                    // defensive/parser-driven disconnect. Flag by stopping.
                    m_fuzz_state = 2;
                    app_timer_stop(m_fuzz_timer);
                }
                NRF_LOG_INFO("Central target disconnected, reason 0x%x", m_last_disc_reason);
            }
            break;

        case BLE_GAP_EVT_TIMEOUT:
            if (gap->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN && m_conn_state == 1) {
                m_conn_state = 0;   // connection attempt timed out
                NRF_LOG_INFO("Central connect timed out");
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
    m_conn_state = 1; // connecting

    ret_code_t err = sd_ble_gap_connect(&peer, &m_init_scan_params, &m_conn_params, APP_BLE_CONN_CFG_TAG);
    if (err != NRF_SUCCESS) {
        m_conn_state = 0;
    }
    return err;
}

uint32_t ble_central_disconnect(void) {
    ble_central_fuzz_stop();
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
    m_fuzz_state     = 1; // running
    return app_timer_start(m_fuzz_timer, APP_TIMER_TICKS(interval_ms), NULL);
}

uint32_t ble_central_fuzz_stop(void) {
    if (m_fuzz_state == 1) {
        app_timer_stop(m_fuzz_timer);
        m_fuzz_state = 2;
    }
    return NRF_SUCCESS;
}

uint16_t ble_central_get_state(uint8_t *out, uint16_t out_cap) {
    if (out_cap < 8) {
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
