/**
 * @file nfc_14a_4.c
 * @brief ISO14443-4 T=CL emulation for ChameleonUltra
 *
 * Implements a full ISO14443-4 tag emulator with a static APDU response
 * table.  The table is populated by the host before field activation, so
 * the firmware can respond to an EMV reader autonomously without any USB
 * communication while the RF field is active.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <string.h>
#include "nfc_14a_4.h"
#include "nfc_14a.h"
#include "tag_emulation.h"
#include "tag_persistence.h"
#include "fds_util.h"
#include "nrf_log.h"
#include "app_util_platform.h"

/* ------------------------------------------------------------------ */
/*  PCB byte constants (ISO14443-4 §7)                                 */
/* ------------------------------------------------------------------ */
#define PCB_IBLOCK_MASK     0xE2
#define PCB_IBLOCK_VAL      0x02
#define PCB_RBLOCK_MASK     0xE6
#define PCB_RBLOCK_VAL      0xA2   /* R(ACK) = 0xA2/0xA3, R(NAK) = 0xB2/0xB3 */
#define PCB_SBLOCK_MASK     0xC0
#define PCB_SBLOCK_VAL      0xC0
#define PCB_BLOCK_NUM       0x01
#define PCB_NAD_FOLLOWING   0x04
#define PCB_CID_FOLLOWING   0x08
#define PCB_CHAIN           0x10
#define PCB_RBLOCK_NAK      0x10
#define PCB_SBLOCK_WTX      0xF2  /* 0xFA when CID follows */
#define PCB_SBLOCK_DESELECT 0xC2
#define PCB_PPS              0xD0
#define WTX_VALUE           0x3B   /* WTXM=59 (~3s extra wait) */

static inline bool is_iblock(uint8_t pcb) {
    return (pcb & PCB_IBLOCK_MASK) == PCB_IBLOCK_VAL;
}
static inline bool is_rblock(uint8_t pcb) {
    return (pcb & PCB_RBLOCK_MASK) == PCB_RBLOCK_VAL;
}
static inline bool is_sblock(uint8_t pcb) {
    return (pcb & PCB_SBLOCK_MASK) == PCB_SBLOCK_VAL;
}

/* ------------------------------------------------------------------ */
/*  Module state                                                        */
/* ------------------------------------------------------------------ */
static nfc_tag_14a_4_information_t *m_tag_information = NULL;

/* Shadow coll-res references into m_tag_information */
static nfc_tag_14a_coll_res_reference_t m_shadow_coll_res;

/* T=CL session state */
static uint8_t  m_rx_block_num   = 0;
static uint8_t  m_tx_block_num   = 0;
static uint16_t m_pcd_frame_size = 256;
static bool     m_cid_available  = false;
static bool     m_nad_available  = false;
static bool     m_cid_active     = false;
static uint8_t  m_cid            = 0;
static bool     m_nad_active     = false;
static uint8_t  m_nad            = 0;
static uint8_t  m_apdu_buf[NFC_14A_4_MAX_APDU];
static uint16_t m_apdu_len       = 0;
static bool     m_rx_chaining    = false;
static volatile bool m_apdu_pending = false;
static uint8_t  m_resp_buf[NFC_14A_4_MAX_APDU];
static uint16_t m_resp_len       = 0;
static uint16_t m_resp_offset    = 0;
static volatile bool m_response_ready = false;
static bool     m_wtx_pending    = false;
static uint8_t  m_wtxm           = WTX_VALUE;

/* TX scratch buffer */
static uint8_t m_tx_buf[NFC_14A_4_MAX_APDU + 4];
static uint8_t m_last_reply[NFC_14A_4_MAX_APDU + 4];
static uint16_t m_last_reply_len = 0;
static bool m_last_reply_valid = false;
static uint8_t m_last_iblock[NFC_14A_4_MAX_APDU + 4];
static uint16_t m_last_iblock_len = 0;
static uint8_t m_last_iblock_num = 0;
static bool m_last_iblock_chained = false;
static bool m_last_iblock_valid = false;

/* Debug counters — readable via hf 14a debug */
static uint8_t  m_dbg_iblocks_rx  = 0;  /* I-blocks received */
static uint8_t  m_dbg_iblocks_tx  = 0;  /* I-blocks sent */
static uint8_t  m_dbg_last_rx_pcb = 0;  /* PCB of last received I-block */
static uint8_t  m_dbg_last_match  = 0;  /* last find_static_response result */

/* Static APDU response table (RAM copy, populated from m_tag_information) */
static nfc_tag_14a_4_static_response_t m_static_resp[NFC_14A_4_MAX_STATIC_RESPONSES];
static uint8_t m_static_resp_count = 0;

/* Large response overflow (RAM only, > NFC_14A_4_MAX_STATIC_RESP_LEN bytes).
 * NOT persisted to flash. Must reload via emv load after power cycle. */
typedef struct {
    uint8_t  cmd[NFC_14A_4_MAX_STATIC_CMD_LEN];
    uint8_t  cmd_len;
    uint8_t  resp[NFC_14A_4_MAX_LARGE_RESP_LEN];
    uint16_t resp_len;
} nfc_tag_14a_4_large_response_t;
static nfc_tag_14a_4_large_response_t m_large_resp[NFC_14A_4_MAX_LARGE_RESPONSES];
static uint8_t m_large_resp_count = 0;

/* ------------------------------------------------------------------ */
/*  Static response table                                               */
/* ------------------------------------------------------------------ */

void nfc_tag_14a_4_add_static_response(const uint8_t *cmd,  uint8_t cmd_len,
                                       const uint8_t *resp, uint16_t resp_len) {
    if (cmd == NULL || cmd_len == 0 || cmd_len > NFC_14A_4_MAX_STATIC_CMD_LEN ||
            (resp == NULL && resp_len != 0) ||
            resp_len > NFC_14A_4_MAX_LARGE_RESP_LEN) return;

    if (resp_len > NFC_14A_4_MAX_STATIC_RESP_LEN) {
        /* Large response: RAM-only overflow table */
        if (m_large_resp_count >= NFC_14A_4_MAX_LARGE_RESPONSES) return;
        nfc_tag_14a_4_large_response_t *le = &m_large_resp[m_large_resp_count];
        le->cmd_len  = cmd_len;
        le->resp_len = resp_len;
        memcpy(le->cmd,  cmd,  cmd_len);
        memcpy(le->resp, resp, resp_len);
        m_large_resp_count++;
        return;
    }

    /* Normal response: flash-backed table */
    if (m_static_resp_count >= NFC_14A_4_MAX_STATIC_RESPONSES) return;
    nfc_tag_14a_4_static_response_t *e = &m_static_resp[m_static_resp_count];
    e->cmd_len  = cmd_len;
    e->resp_len = (uint8_t)resp_len;
    memcpy(e->cmd,  cmd,  cmd_len);
    memcpy(e->resp, resp, resp_len);
    m_static_resp_count++;
    if (m_tag_information &&
            m_tag_information->static_resp_count < NFC_14A_4_MAX_STATIC_RESPONSES) {
        uint8_t index = m_tag_information->static_resp_count;
        memcpy(&m_tag_information->static_resp[index], e, sizeof(*e));
        m_tag_information->static_resp_count = index + 1u;
    }
}

void nfc_tag_14a_4_clear_static_responses(void) {
    m_static_resp_count = 0;
    m_large_resp_count  = 0;
    if (m_tag_information) {
        m_tag_information->static_resp_count = 0;
    }
}

static bool find_static_response(const uint8_t *apdu, uint16_t apdu_len,
                                 uint8_t **resp_out, uint16_t *resp_len_out) {
    /* Flash-backed table */
    for (uint8_t i = 0; i < m_static_resp_count; i++) {
        nfc_tag_14a_4_static_response_t *e = &m_static_resp[i];
        if (apdu_len >= e->cmd_len &&
                memcmp(apdu, e->cmd, e->cmd_len) == 0) {
            *resp_out     = e->resp;
            *resp_len_out = e->resp_len;
            return true;
        }
    }
    /* RAM-only large response table */
    for (uint8_t i = 0; i < m_large_resp_count; i++) {
        nfc_tag_14a_4_large_response_t *e = &m_large_resp[i];
        if (apdu_len >= e->cmd_len &&
                memcmp(apdu, e->cmd, e->cmd_len) == 0) {
            *resp_out     = e->resp;
            *resp_len_out = e->resp_len;
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/*  TX helpers                                                          */
/* ------------------------------------------------------------------ */

static void send_frame(const uint8_t *data, uint16_t len) {
    if (data == NULL || len == 0u || len > sizeof(m_last_reply)) return;
    memcpy(m_last_reply, data, len);
    m_last_reply_len = len;
    m_last_reply_valid = true;
    nfc_tag_14a_tx_bytes(m_last_reply, len, true);
}

static void resend_last_reply(void) {
    if (m_last_reply_valid) {
        nfc_tag_14a_tx_bytes(m_last_reply, m_last_reply_len, true);
    }
}

static bool parse_cid(uint8_t pcb, const uint8_t *data, uint16_t len,
                      uint8_t *offset) {
    if ((pcb & PCB_CID_FOLLOWING) == 0) {
        return !m_cid_active;
    }
    if (*offset >= len || !m_cid_available || (data[*offset] & 0xF0) != 0 ||
            (data[*offset] & 0x0F) != m_cid) {
        return false;
    }
    m_cid_active = true;
    (*offset)++;
    return true;
}

static bool pps_request_valid(const uint8_t *data, uint16_t len,
                              bool cid_available, uint8_t cid) {
    if (data == NULL || len < 2u || len > 3u ||
            (data[0] & 0xF0u) != PCB_PPS) {
        return false;
    }
    uint8_t expected_cid = cid_available ? (cid & 0x0Fu) : 0u;
    if ((data[0] & 0x0Fu) != expected_cid ||
            (data[1] & 0x0Fu) != 0x01u || (data[1] & 0xE0u) != 0u) {
        return false;
    }
    bool pps1_present = (data[1] & 0x10u) != 0u;
    return len == (pps1_present ? 3u : 2u);
}

static void send_rblock(bool nak, uint8_t block_num) {
    uint8_t off = 0;
    m_tx_buf[off++] = PCB_RBLOCK_VAL |
                      (nak ? PCB_RBLOCK_NAK : 0) |
                      (block_num & PCB_BLOCK_NUM) |
                      (m_cid_active ? PCB_CID_FOLLOWING : 0);
    if (m_cid_active) m_tx_buf[off++] = m_cid;
    send_frame(m_tx_buf, off);
}

static uint16_t response_inf_capacity(void) {
    uint16_t overhead = 1 + NFC_TAG_14A_CRC_LENGTH;
    if (m_cid_active) overhead++;
    if (m_nad_active) overhead++;
    return m_pcd_frame_size > overhead ? m_pcd_frame_size - overhead : 0;
}

static void send_next_iblock(void) {
    uint16_t capacity = response_inf_capacity();
    if (m_resp_len > NFC_14A_4_MAX_APDU || m_resp_offset > m_resp_len) return;
    uint16_t remaining = m_resp_len - m_resp_offset;
    uint16_t chunk = remaining > capacity ? capacity : remaining;
    bool chained = chunk < remaining;
    uint8_t pcb = PCB_IBLOCK_VAL | (m_tx_block_num & PCB_BLOCK_NUM);
    if (capacity == 0) return;
    if (chained) pcb |= PCB_CHAIN;
    if (m_cid_active) pcb |= PCB_CID_FOLLOWING;
    if (m_nad_active) pcb |= PCB_NAD_FOLLOWING;
    uint8_t off = 0;
    m_tx_buf[off++] = pcb;
    if (m_cid_active) m_tx_buf[off++] = m_cid;
    if (m_nad_active) m_tx_buf[off++] = m_nad;
    memcpy(&m_tx_buf[off], &m_resp_buf[m_resp_offset], chunk);
    off += chunk;

    memcpy(m_last_iblock, m_tx_buf, off);
    m_last_iblock_len = off;
    m_last_iblock_num = m_tx_block_num;
    m_last_iblock_chained = chained;
    m_last_iblock_valid = true;
    m_resp_offset += chunk;
    m_tx_block_num ^= 1;
    if (!chained) m_rx_block_num = m_tx_block_num;
    m_dbg_iblocks_tx++;
    send_frame(m_tx_buf, off);
}

static void start_response(void) {
    m_resp_offset = 0;
    m_response_ready = false;
    m_wtx_pending = false;
    send_next_iblock();
}

static void send_wtx(void) {
    uint8_t off = 0;
    m_tx_buf[off++] = PCB_SBLOCK_WTX |
                      (m_cid_active ? PCB_CID_FOLLOWING : 0);
    if (m_cid_active) m_tx_buf[off++] = m_cid;
    m_tx_buf[off++] = m_wtxm;
    m_wtx_pending = true;
    send_frame(m_tx_buf, off);
}

/* ------------------------------------------------------------------ */
/*  State handler (called from NFCT ISR on each received frame)        */
/* ------------------------------------------------------------------ */

static void nfc_tag_14a_4_state_handler(uint8_t *data, uint16_t szBits) {
    /* The shared layer has already validated and removed the CRC. */
    uint16_t szBytes = szBits >> 3;
    if (szBytes == 0 || (szBits & 0x07) != 0) return;
    uint8_t pcb = data[0];

    /* ---- S-block ---- */
    if (is_sblock(pcb)) {
        if ((pcb & 0xF0u) == PCB_PPS) {
            if (pps_request_valid(data, szBytes, m_cid_available, m_cid)) {
                send_frame(data, 1u);
            }
            return;
        }
        uint8_t offset = 1;
        if (!parse_cid(pcb, data, szBytes, &offset)) return;
        if ((pcb & 0xF7) == PCB_SBLOCK_DESELECT) {
            if (offset != szBytes) return;
            send_frame(data, szBytes);
            nfc_tag_14a_4_reset_handler();
            nfc_tag_14a_set_state(NFC_TAG_STATE_14A_HALTED);
            return;
        }
        if ((pcb & 0xF7) == PCB_SBLOCK_WTX) {
            if (offset + 1 != szBytes || data[offset] == 0 ||
                    (data[offset] & 0xC0) != 0) {
                return;
            }
            if (m_wtx_pending) {
                if (data[offset] != m_wtxm) return;
                m_wtx_pending = false;
                if (m_response_ready) start_response();
                else send_wtx();
            } else {
                /* An unsolicited PCD WTX request is answered with the same WTXM. */
                send_frame(data, szBytes);
            }
            return;
        }
        return;
    }

    /* ---- R-block ---- */
    if (is_rblock(pcb)) {
        uint8_t offset = 1;
        if (!parse_cid(pcb, data, szBytes, &offset) || offset != szBytes ||
                !m_last_iblock_valid) {
            return;
        }
        uint8_t block_num = pcb & PCB_BLOCK_NUM;
        if ((pcb & PCB_RBLOCK_NAK) != 0) {
            if (block_num == m_last_iblock_num) {
                nfc_tag_14a_tx_bytes(m_last_iblock, m_last_iblock_len, true);
            }
        } else if (block_num == m_tx_block_num && m_last_iblock_chained) {
            send_next_iblock();
        }
        return;
    }

    /* ---- I-block ---- */
    if (is_iblock(pcb)) {
        uint8_t reader_blknum = pcb & PCB_BLOCK_NUM;
        bool    has_nad       = (pcb & PCB_NAD_FOLLOWING) != 0;
        bool    more_chain    = (pcb & PCB_CHAIN)         != 0;

        uint8_t offset = 1;
        if (!parse_cid(pcb, data, szBytes, &offset)) return;
        if (has_nad) {
            if (!m_nad_available || offset >= szBytes) {
                send_rblock(true, m_rx_block_num);
                return;
            }
            if (m_rx_chaining && (!m_nad_active || data[offset] != m_nad)) {
                send_rblock(true, m_rx_block_num);
                return;
            }
            m_nad = data[offset++];
            m_nad_active = true;
        } else if (m_rx_chaining && m_nad_active) {
            send_rblock(true, m_rx_block_num);
            return;
        }

        if (offset >= szBytes) {
            send_rblock(true, m_rx_block_num);
            return;
        }

        uint16_t apdu_len = szBytes - offset;

        m_dbg_iblocks_rx++;
        m_dbg_last_rx_pcb = pcb;
        NRF_LOG_INFO("14A4 I-block #%d: reader_blk=%d expected=%d inf_len=%d",
                     m_dbg_iblocks_rx, reader_blknum, m_rx_block_num, apdu_len);

        /* Block number check per ISO14443-4 §7.5.3.3:
         * If block number matches expected, process new APDU.
         * If block number does NOT match, it is a retransmit —
         * resend the last response without re-processing. */
        if (reader_blknum != m_rx_block_num) {
            if (m_last_reply_valid) resend_last_reply();
            else send_rblock(true, m_rx_block_num);
            return;
        }

        if (!m_rx_chaining) {
            m_apdu_len = 0;
            m_apdu_pending = false;
            m_response_ready = false;
            m_resp_len = 0;
            m_resp_offset = 0;
            m_last_iblock_valid = false;
            if (!has_nad) m_nad_active = false;
        }
        if (apdu_len > NFC_14A_4_MAX_APDU - m_apdu_len) {
            send_rblock(true, m_rx_block_num);
            return;
        }

        memcpy(&m_apdu_buf[m_apdu_len], &data[offset], apdu_len);
        m_apdu_len += apdu_len;
        m_rx_block_num ^= 1;

        if (more_chain) {
            m_rx_chaining = true;
            send_rblock(false, reader_blknum);
            return;
        }

        m_rx_chaining = false;
        m_apdu_pending = true;
        /* The first response repeats the final command I-block number. */
        m_tx_block_num = reader_blknum;

        /* APDU complete — check static table first, then WTX */
        {
            uint8_t  *static_resp = NULL;
            uint16_t  static_len  = 0;
            bool _found = find_static_response(m_apdu_buf, m_apdu_len,
                                               &static_resp, &static_len);
            m_dbg_last_match = _found ? 1 : 0;
            NRF_LOG_INFO("14A4 find_static: found=%d static_len=%d resp_count=%d",
                         _found, static_len, m_static_resp_count);
            if (_found) {
                memcpy(m_resp_buf, static_resp, static_len);
                m_resp_len = static_len;
                start_response();
            } else if (m_response_ready) {
                start_response();
            } else {
                /* No response ready — keep reader alive with WTX */
                send_wtx();
            }
        }
        return;
    }

    NRF_LOG_INFO("14A-4: unknown PCB 0x%02x", pcb);
}

/* ------------------------------------------------------------------ */
/*  Independent T=CL path for protocol-specific emulators             */
/* ------------------------------------------------------------------ */

static void base_send_frame(nfc_tag_14a_4_tcl_state_t *state,
                            const uint8_t *data, uint16_t len) {
    if (state == NULL || data == NULL || len == 0u ||
            len > sizeof(state->m_last_reply)) return;
    memcpy(state->m_last_reply, data, len);
    state->m_last_reply_len = len;
    state->m_last_reply_valid = true;
    nfc_tag_14a_tx_bytes(state->m_last_reply, len, true);
}

static bool base_parse_cid(nfc_tag_14a_4_tcl_state_t *state, uint8_t pcb,
                           const uint8_t *data, uint16_t len,
                           uint8_t *offset) {
    if (state == NULL || data == NULL || offset == NULL) return false;
    if ((pcb & PCB_CID_FOLLOWING) == 0u) return !state->m_cid_active;
    if (*offset >= len || !state->m_cid_available ||
            (data[*offset] & 0xF0u) != 0u ||
            (data[*offset] & 0x0Fu) != state->m_cid) {
        return false;
    }
    state->m_cid_active = true;
    (*offset)++;
    return true;
}

static void base_send_rblock(nfc_tag_14a_4_tcl_state_t *state, bool nak,
                             uint8_t block_num) {
    if (state == NULL) return;
    uint8_t frame[2];
    uint8_t off = 0;
    frame[off++] = PCB_RBLOCK_VAL |
                   (nak ? PCB_RBLOCK_NAK : 0u) |
                   (block_num & PCB_BLOCK_NUM) |
                   (state->m_cid_active ? PCB_CID_FOLLOWING : 0u);
    if (state->m_cid_active) frame[off++] = state->m_cid;
    base_send_frame(state, frame, off);
}

static uint16_t base_response_inf_capacity(const nfc_tag_14a_4_tcl_state_t *state) {
    if (state == NULL) return 0u;
    uint16_t overhead = 1u + NFC_TAG_14A_CRC_LENGTH;
    if (state->m_cid_active) overhead++;
    if (state->m_nad_active) overhead++;
    return state->m_pcd_frame_size > overhead ?
           state->m_pcd_frame_size - overhead : 0u;
}

static void base_send_next_iblock(nfc_tag_14a_4_tcl_state_t *state) {
    if (state == NULL || state->m_resp_len == 0u ||
            state->m_resp_len > NFC_14A_4_MAX_APDU ||
            state->m_resp_offset > state->m_resp_len) return;
    uint16_t capacity = base_response_inf_capacity(state);
    if (capacity == 0u) return;
    uint16_t remaining = state->m_resp_len - state->m_resp_offset;
    uint16_t chunk = remaining > capacity ? capacity : remaining;
    bool chained = chunk < remaining;
    uint8_t off = 0;
    uint8_t pcb = PCB_IBLOCK_VAL | (state->m_tx_block_num & PCB_BLOCK_NUM);
    if (chained) pcb |= PCB_CHAIN;
    if (state->m_cid_active) pcb |= PCB_CID_FOLLOWING;
    if (state->m_nad_active) pcb |= PCB_NAD_FOLLOWING;
    state->m_last_iblock[off++] = pcb;
    if (state->m_cid_active) state->m_last_iblock[off++] = state->m_cid;
    if (state->m_nad_active) state->m_last_iblock[off++] = state->m_nad;
    if ((uint16_t)off + chunk > sizeof(state->m_last_iblock)) return;
    memcpy(&state->m_last_iblock[off],
           &state->m_resp_buf[state->m_resp_offset], chunk);
    off += chunk;

    state->m_last_iblock_len = off;
    state->m_last_iblock_num = state->m_tx_block_num;
    state->m_last_iblock_chained = chained;
    state->m_last_iblock_valid = true;
    state->m_resp_offset += chunk;
    state->m_tx_block_num ^= 1u;
    if (!chained) state->m_rx_block_num = state->m_tx_block_num;
    m_dbg_iblocks_tx++;
    base_send_frame(state, state->m_last_iblock, off);
}

static void base_start_response(nfc_tag_14a_4_tcl_state_t *state) {
    if (state == NULL) return;
    state->m_resp_offset = 0u;
    state->m_response_ready = false;
    state->m_wtx_pending = false;
    base_send_next_iblock(state);
}

static void base_send_wtx(nfc_tag_14a_4_tcl_state_t *state) {
    if (state == NULL) return;
    uint8_t frame[3];
    uint8_t off = 0;
    frame[off++] = PCB_SBLOCK_WTX |
                   (state->m_cid_active ? PCB_CID_FOLLOWING : 0u);
    if (state->m_cid_active) frame[off++] = state->m_cid;
    frame[off++] = state->m_wtxm;
    state->m_wtx_pending = true;
    base_send_frame(state, frame, off);
}

void nfc_tag_14a_4_base_respond(nfc_tag_14a_4_tcl_state_t *state) {
    if (state == NULL) return;
    if (state->m_response_ready) {
        if (state->m_resp_len == 0u || state->m_resp_len > NFC_14A_4_MAX_APDU) {
            state->m_response_ready = false;
            return;
        }
        base_start_response(state);
    } else {
        base_send_wtx(state);
    }
}

bool nfc_tag_14a_4_base_handler(nfc_tag_14a_4_tcl_state_t *state,
                               const uint8_t *data, uint16_t szBytes) {
    if (state == NULL || data == NULL || szBytes == 0u) return false;
    uint8_t pcb = data[0];

    if (is_sblock(pcb)) {
        if ((pcb & 0xF0u) == PCB_PPS) {
            if (pps_request_valid(data, szBytes, state->m_cid_available,
                                  state->m_cid)) {
                base_send_frame(state, data, 1u);
            }
            return false;
        }
        uint8_t offset = 1u;
        if (!base_parse_cid(state, pcb, data, szBytes, &offset)) return false;
        if ((pcb & 0xF7u) == PCB_SBLOCK_DESELECT) {
            if (offset != szBytes) return false;
            base_send_frame(state, data, szBytes);
            nfc_tag_14a_4_reset_state(state);
            nfc_tag_14a_set_state(NFC_TAG_STATE_14A_HALTED);
            return false;
        }
        if ((pcb & 0xF7u) == PCB_SBLOCK_WTX) {
            if ((uint16_t)offset + 1u != szBytes || data[offset] == 0u ||
                    (data[offset] & 0xC0u) != 0u) return false;
            if (state->m_wtx_pending) {
                if (data[offset] != state->m_wtxm) return false;
                state->m_wtx_pending = false;
                if (state->m_response_ready) base_start_response(state);
                else base_send_wtx(state);
            } else {
                base_send_frame(state, data, szBytes);
            }
            return false;
        }
        return false;
    }

    if (is_rblock(pcb)) {
        uint8_t offset = 1u;
        if (!base_parse_cid(state, pcb, data, szBytes, &offset) ||
                offset != szBytes || !state->m_last_iblock_valid) {
            return false;
        }
        uint8_t block_num = pcb & PCB_BLOCK_NUM;
        if ((pcb & PCB_RBLOCK_NAK) != 0u) {
            if (block_num == state->m_last_iblock_num) {
                nfc_tag_14a_tx_bytes(state->m_last_iblock,
                                     state->m_last_iblock_len, true);
            }
        } else if (block_num == state->m_tx_block_num &&
                   state->m_last_iblock_chained) {
            base_send_next_iblock(state);
        }
        return false;
    }

    if (is_iblock(pcb)) {
        uint8_t reader_blknum = pcb & PCB_BLOCK_NUM;
        bool has_nad = (pcb & PCB_NAD_FOLLOWING) != 0u;
        bool more_chain = (pcb & PCB_CHAIN) != 0u;
        uint8_t offset = 1u;
        if (!base_parse_cid(state, pcb, data, szBytes, &offset)) return false;
        if (has_nad) {
            if (!state->m_nad_available || offset >= szBytes) {
                base_send_rblock(state, true, state->m_rx_block_num);
                return false;
            }
            if (state->m_rx_chaining &&
                    (!state->m_nad_active || data[offset] != state->m_nad)) {
                base_send_rblock(state, true, state->m_rx_block_num);
                return false;
            }
            state->m_nad = data[offset++];
            state->m_nad_active = true;
        } else if (state->m_rx_chaining && state->m_nad_active) {
            base_send_rblock(state, true, state->m_rx_block_num);
            return false;
        }
        if (offset >= szBytes) {
            base_send_rblock(state, true, state->m_rx_block_num);
            return false;
        }

        uint16_t inf_len = szBytes - offset;
        m_dbg_iblocks_rx++;
        m_dbg_last_rx_pcb = pcb;
        if (reader_blknum != state->m_rx_block_num) {
            if (state->m_last_reply_valid) {
                nfc_tag_14a_tx_bytes(state->m_last_reply,
                                     state->m_last_reply_len, true);
            } else {
                base_send_rblock(state, true, state->m_rx_block_num);
            }
            return false;
        }

        if (!state->m_rx_chaining) {
            state->m_apdu_len = 0u;
            state->m_apdu_pending = false;
            state->m_response_ready = false;
            state->m_resp_len = 0u;
            state->m_resp_offset = 0u;
            state->m_last_iblock_valid = false;
            if (!has_nad) state->m_nad_active = false;
        }
        if (state->m_apdu_len > NFC_14A_4_MAX_APDU ||
                inf_len > NFC_14A_4_MAX_APDU - state->m_apdu_len) {
            base_send_rblock(state, true, state->m_rx_block_num);
            return false;
        }
        memcpy(&state->m_apdu_buf[state->m_apdu_len], &data[offset], inf_len);
        state->m_apdu_len += inf_len;
        state->m_rx_block_num ^= 1u;
        if (more_chain) {
            state->m_rx_chaining = true;
            base_send_rblock(state, false, reader_blknum);
            return false;
        }

        state->m_rx_chaining = false;
        state->m_apdu_pending = true;
        state->m_tx_block_num = reader_blknum;
        return true;
    }

    return false;
}

void nfc_tag_14a_4_reset_state(nfc_tag_14a_4_tcl_state_t *state) {
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->m_pcd_frame_size = 256u;
    state->m_wtxm = WTX_VALUE;
}

void nfc_tag_14a_4_base_activate(nfc_tag_14a_4_tcl_state_t *state,
                                const nfc_14a_ats_t *ats,
                                uint8_t fsdi, uint8_t cid) {
    static const uint16_t fsd_table[] = {
        16, 24, 32, 40, 48, 64, 96, 128, 256
    };
    if (state == NULL) return;
    nfc_tag_14a_4_reset_state(state);
    state->m_pcd_frame_size = fsdi < ARRAY_SIZE(fsd_table) ?
                              fsd_table[fsdi] : 256u;
    bool ats_cid = false;
    bool ats_nad = false;
    if (ats != NULL && ats->length >= 2u) {
        uint8_t t0 = ats->data[1];
        uint16_t index = 2u;
        if ((t0 & 0x10u) != 0u) index++;
        if ((t0 & 0x20u) != 0u) index++;
        if ((t0 & 0x40u) != 0u && index < ats->length) {
            ats_nad = (ats->data[index] & 0x01u) != 0u;
            ats_cid = (ats->data[index] & 0x02u) != 0u;
        }
    }
    state->m_cid = cid & 0x0Fu;
    state->m_cid_available = ats_cid && state->m_cid != 0x0Fu;
    state->m_nad_available = ats_nad;
}


/* ------------------------------------------------------------------ */
/*  APDU relay API (for host-driven responses)                         */
/* ------------------------------------------------------------------ */

bool nfc_tag_14a_4_get_pending_apdu(uint8_t *buf, uint16_t *length) {
    if (buf == NULL || length == NULL) return false;
    bool pending = false;
    CRITICAL_REGION_ENTER();
    if (m_apdu_pending) {
        m_apdu_pending = false;
        *length = m_apdu_len;
        memcpy(buf, m_apdu_buf, m_apdu_len);
        pending = true;
    }
    CRITICAL_REGION_EXIT();
    return pending;
}

void nfc_tag_14a_4_set_response(const uint8_t *data, uint16_t length) {
    if (data == NULL || length < 2u || length > NFC_14A_4_MAX_APDU) return;
    CRITICAL_REGION_ENTER();
    if (m_resp_offset == 0u && !m_response_ready) {
        memcpy(m_resp_buf, data, length);
        m_resp_len = length;
        m_response_ready = true;
    }
    CRITICAL_REGION_EXIT();
}

/* ------------------------------------------------------------------ */
/*  Reset handler                                                       */
/* ------------------------------------------------------------------ */

void nfc_tag_14a_4_reset_handler(void) {
    m_rx_block_num       = 0;
    m_tx_block_num       = 0;
    m_pcd_frame_size     = 256;
    m_cid_available      = false;
    m_nad_available      = false;
    m_cid_active         = false;
    m_cid                = 0;
    m_nad_active         = false;
    m_nad                = 0;
    m_apdu_pending       = false;
    m_rx_chaining        = false;
    m_response_ready     = false;
    m_wtx_pending        = false;
    m_apdu_len           = 0;
    m_resp_len           = 0;
    m_resp_offset        = 0;
    m_last_reply_len     = 0;
    m_last_reply_valid   = false;
    m_last_iblock_len    = 0;
    m_last_iblock_valid  = false;
    m_last_iblock_chained = false;
}

void nfc_tag_14a_4_activate(uint8_t fsdi, uint8_t cid) {
    static const uint16_t fsd_table[] = {
        16, 24, 32, 40, 48, 64, 96, 128, 256
    };
    m_pcd_frame_size = fsdi < (sizeof(fsd_table) / sizeof(fsd_table[0]))
                       ? fsd_table[fsdi] : 256;
    bool ats_cid = false;
    bool ats_nad = false;
    if (m_tag_information != NULL && m_tag_information->res_coll.ats.length >= 2u) {
        const uint8_t *ats = m_tag_information->res_coll.ats.data;
        uint8_t t0 = ats[1];
        uint16_t index = 2u;
        if (t0 & 0x10u) index++;
        if (t0 & 0x20u) index++;
        if ((t0 & 0x40u) && index < m_tag_information->res_coll.ats.length) {
            ats_nad = (ats[index] & 0x01u) != 0u;
            ats_cid = (ats[index] & 0x02u) != 0u;
        }
    }
    m_cid = cid & 0x0F;
    m_cid_available = ats_cid && m_cid != 0x0F;
    m_nad_available = ats_nad;
}

void nfc_tag_14a_4_get_debug_counters(uint8_t *rx, uint8_t *tx,
                                      uint8_t *last_pcb, uint8_t *last_match) {
    if (rx == NULL || tx == NULL || last_pcb == NULL || last_match == NULL) return;
    *rx = m_dbg_iblocks_rx;
    *tx = m_dbg_iblocks_tx;
    *last_pcb = m_dbg_last_rx_pcb;
    *last_match = m_dbg_last_match;
}

/* ------------------------------------------------------------------ */
/*  Anti-collision resource                                             */
/* ------------------------------------------------------------------ */

nfc_tag_14a_coll_res_reference_t *nfc_tag_14a_4_get_coll_res(void) {
    if (m_tag_information == NULL) return NULL;
    m_shadow_coll_res.sak  = m_tag_information->res_coll.sak;
    m_shadow_coll_res.atqa = m_tag_information->res_coll.atqa;
    m_shadow_coll_res.uid  = m_tag_information->res_coll.uid;
    m_shadow_coll_res.size = &m_tag_information->res_coll.size;
    m_shadow_coll_res.ats  = &m_tag_information->res_coll.ats;
    return &m_shadow_coll_res;
}

/* ------------------------------------------------------------------ */
/*  Data load / save / factory callbacks                               */
/* ------------------------------------------------------------------ */

int nfc_tag_14a_4_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    int info_size = sizeof(nfc_tag_14a_4_information_t);
    uint16_t stored_size = (uint16_t)((info_size + 3) & ~3);
    if (buffer->actual_length != 0 && buffer->actual_length != stored_size) {
        NRF_LOG_ERROR("14A-4 loadcb: invalid record length (%d != %d)",
                      buffer->actual_length, stored_size);
        return 0;
    }
    if (buffer->length < info_size) {
        NRF_LOG_ERROR("14A-4 loadcb: buffer too small (%d < %d)",
                      buffer->length, info_size);
        return info_size;
    }
    m_tag_information = (nfc_tag_14a_4_information_t *)buffer->buffer;
    m_large_resp_count = 0;
    memset(m_large_resp, 0, sizeof(m_large_resp));

    /* Populate RAM static table from persisted slot data */
    m_static_resp_count = m_tag_information->static_resp_count;
    if (m_static_resp_count > NFC_14A_4_MAX_STATIC_RESPONSES)
        m_static_resp_count = NFC_14A_4_MAX_STATIC_RESPONSES;
    memcpy(m_static_resp, m_tag_information->static_resp,
           m_static_resp_count * sizeof(nfc_tag_14a_4_static_response_t));

    nfc_tag_14a_handler_t handler = {
        .get_coll_res = nfc_tag_14a_4_get_coll_res,
        .cb_state     = nfc_tag_14a_4_state_handler,
        .cb_reset     = nfc_tag_14a_4_reset_handler,
        .cb_activated = nfc_tag_14a_4_activate,
        .cb_state_crc_strip = true,
    };
    nfc_tag_14a_set_handler(&handler);
    NRF_LOG_INFO("14A-4 loadcb OK: SAK=%02x uid_sz=%d static_resp=%d",
                 m_tag_information->res_coll.sak[0],
                 m_tag_information->res_coll.size,
                 m_static_resp_count);
    return info_size;
}

int nfc_tag_14a_4_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    return sizeof(nfc_tag_14a_4_information_t);
}

bool nfc_tag_14a_4_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    if (tag_type != TAG_TYPE_HF14A_4) return false;

    /* Build factory defaults on stack and write directly to FDS
     * (same pattern as nfc_tag_mf1_data_factory). */
    nfc_tag_14a_4_information_t info;
    memset(&info, 0, sizeof(info));

    /* Placeholder 7-byte NXP-style UID */
    info.res_coll.size    = NFC_TAG_14A_UID_DOUBLE_SIZE;
    info.res_coll.atqa[0] = 0x04;
    info.res_coll.atqa[1] = 0x00;
    info.res_coll.sak[0]  = 0x20;   /* ISO14443-4 */
    info.res_coll.uid[0]  = 0x04;
    info.res_coll.uid[1]  = 0x01;
    info.res_coll.uid[2]  = 0x02;
    info.res_coll.uid[3]  = 0x03;
    info.res_coll.uid[4]  = 0x04;
    info.res_coll.uid[5]  = 0x05;
    info.res_coll.uid[6]  = 0x06;

    static const uint8_t default_ats[] = {
        0x10, 0x78, 0x80, 0x70, 0x02, 0x00,
        0x31, 0xC1, 0x64, 0x09, 0x97, 0x61,
        0x26, 0x00, 0x90, 0x00
    };
    info.res_coll.ats.length = sizeof(default_ats);
    memcpy(info.res_coll.ats.data, default_ats, sizeof(default_ats));
    info.static_resp_count = 0;

    fds_slot_record_map_t map_info;
    get_fds_map_by_slot_sense_type_for_dump(slot, TAG_SENSE_HF, &map_info);
    bool ret = fds_write_sync(map_info.id, map_info.key, sizeof(info), &info);
    NRF_LOG_INFO("14A-4 factory slot %d: %s", slot, ret ? "OK" : "FAIL");
    return ret;
}
