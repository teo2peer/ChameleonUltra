#include "iso_dep_reader.h"

#include <string.h>

#include "app_status.h"
#include "bsp_wdt.h"

#define ISO_DEP_PCB_I_BASE       0x02u
#define ISO_DEP_PCB_I_CHAIN      0x10u
#define ISO_DEP_PCB_CID          0x08u
#define ISO_DEP_PCB_BLOCK_NUM    0x01u
#define ISO_DEP_PCB_R_ACK        0xA2u
#define ISO_DEP_PCB_R_NAK        0xB2u
#define ISO_DEP_PCB_S_DESELECT   0xC2u
#define ISO_DEP_PCB_S_WTX        0xF2u

#define ISO_DEP_FRAME_MAX        64u
#define ISO_DEP_RESPONSE_RETRIES 2u

static const uint16_t m_frame_sizes[] = {16, 24, 32, 40, 48, 64, 96, 128, 256};

static uint16_t fwt_ms_from_ats(const picc_14a_tag_t *tag) {
    uint8_t fwi = 4;
    if (tag != NULL && tag->ats_len >= 3) {
        uint8_t t0 = tag->ats[1];
        uint8_t index = 2;
        if (t0 & 0x10u) index++;
        if ((t0 & 0x20u) && index < tag->ats_len) {
            fwi = (uint8_t)(tag->ats[index] >> 4);
            if (fwi > 14u) fwi = 14u;
        }
    }

    /* FWT = 302 us * 2^FWI. Keep a practical floor for RC522/phone jitter. */
    uint32_t us = 302u << fwi;
    uint32_t ms = (us + 999u) / 1000u;
    if (ms < 50u) ms = 50u;
    if (ms > 5000u) ms = 5000u;
    return (uint16_t)ms;
}

void iso_dep_reader_init(iso_dep_reader_t *reader, const picc_14a_tag_t *tag) {
    if (reader == NULL) return;
    memset(reader, 0, sizeof(*reader));
    reader->picc_fsc = 16u;
    reader->frame_timeout_ms = fwt_ms_from_ats(tag);

    if (tag != NULL && tag->ats_len >= 2) {
        uint8_t fsci = tag->ats[1] & 0x0Fu;
        if (fsci < (sizeof(m_frame_sizes) / sizeof(m_frame_sizes[0]))) {
            uint16_t fsc = m_frame_sizes[fsci];
            reader->picc_fsc = (uint8_t)(fsc > ISO_DEP_FRAME_MAX ? ISO_DEP_FRAME_MAX : fsc);
        }
    }
}

static bool crc_valid(const uint8_t *frame, uint16_t length) {
    if (length < 3u) return false;
    uint8_t crc[2];
    crc_14a_calculate((uint8_t *)frame, length - 2u, crc);
    return frame[length - 2u] == crc[0] && frame[length - 1u] == crc[1];
}

static bool is_i_block(uint8_t pcb) {
    return (pcb & 0xE2u) == ISO_DEP_PCB_I_BASE;
}

static bool is_r_ack(uint8_t pcb) {
    return (pcb & 0xF6u) == ISO_DEP_PCB_R_ACK;
}

static bool is_r_nak(uint8_t pcb) {
    return (pcb & 0xF6u) == ISO_DEP_PCB_R_NAK;
}

static bool is_wtx(uint8_t pcb) {
    return (pcb & 0xF7u) == ISO_DEP_PCB_S_WTX;
}

static uint8_t transceive_frame(const uint8_t *tx, uint8_t tx_len,
                                 uint8_t *rx, uint16_t *rx_len) {
    if (tx == NULL || tx_len == 0 || tx_len > ISO_DEP_FRAME_MAX ||
            rx == NULL || rx_len == NULL) {
        return STATUS_PAR_ERR;
    }
    uint16_t rx_bits = 0;
    write_register_single(ComIrqReg, 0x7F);
    uint8_t status = pcd_14a_reader_bytes_transfer(
                         PCD_TRANSCEIVE, (uint8_t *)tx, tx_len,
                         rx, &rx_bits, ISO_DEP_FRAME_MAX * 8u);
    if ((rx_bits & 7u) != 0) {
        *rx_len = 0;
        return STATUS_HF_ERR_STAT;
    }
    *rx_len = rx_bits / 8u;
    return status;
}

static uint8_t request_response_retransmission(const iso_dep_reader_t *reader,
                                               uint8_t block_num,
                                               uint8_t *rx, uint16_t *rx_len) {
    uint8_t rnak[4];
    uint8_t rnak_len = 0u;
    rnak[rnak_len++] = ISO_DEP_PCB_R_NAK | block_num |
                       (reader->cid_enabled ? ISO_DEP_PCB_CID : 0u);
    if (reader->cid_enabled) rnak[rnak_len++] = reader->cid;
    crc_14a_append(rnak, rnak_len);
    rnak_len += 2u;
    pcd_14a_reader_timeout_set(reader->frame_timeout_ms);
    return transceive_frame(rnak, rnak_len, rx, rx_len);
}

static bool parse_cid(const iso_dep_reader_t *reader, const uint8_t *frame,
                      uint16_t frame_len, uint8_t *offset) {
    bool has_cid = (frame[0] & ISO_DEP_PCB_CID) != 0;
    *offset = 1;
    if (!has_cid) return !reader->cid_enabled;
    if (!reader->cid_enabled || frame_len < 4u) return false;
    if ((frame[1] & 0x0Fu) != reader->cid) return false;
    *offset = 2;
    return true;
}

static bool iso_dep_reader_transceive_impl(iso_dep_reader_t *reader,
                                           const uint8_t *apdu, uint16_t apdu_len,
                                           uint8_t *response, uint16_t response_capacity,
                                           iso_dep_result_t *result) {
    iso_dep_result_t local = {0};
    if (result == NULL) result = &local;
    memset(result, 0, sizeof(*result));

    if (reader == NULL || apdu == NULL || apdu_len == 0u ||
            response == NULL || response_capacity < 2u) {
        result->error = ISO_DEP_ERR_PARAM;
        return false;
    }

    uint8_t tx[ISO_DEP_FRAME_MAX];
    uint8_t rx[ISO_DEP_FRAME_MAX];
    uint16_t apdu_offset = 0;
    uint8_t max_inf = reader->picc_fsc;
    uint8_t overhead = (uint8_t)(3u + (reader->cid_enabled ? 1u : 0u));
    if (max_inf <= overhead) {
        result->error = ISO_DEP_ERR_PARAM;
        return false;
    }
    max_inf = (uint8_t)(max_inf - overhead);

    uint8_t tx_block_num = reader->block_num;
    uint16_t rx_len = 0;
    uint8_t chain_guard = 0;
    uint8_t wtx_guard = 0;
    uint32_t wtx_total_ms = 0;

    while (apdu_offset < apdu_len) {
        uint16_t remaining = apdu_len - apdu_offset;
        uint8_t chunk = (uint8_t)(remaining > max_inf ? max_inf : remaining);
        bool more = remaining > chunk;
        uint8_t tx_len = 0;
        tx[tx_len++] = ISO_DEP_PCB_I_BASE | tx_block_num |
                       (more ? ISO_DEP_PCB_I_CHAIN : 0u) |
                       (reader->cid_enabled ? ISO_DEP_PCB_CID : 0u);
        if (reader->cid_enabled) tx[tx_len++] = reader->cid;
        memcpy(&tx[tx_len], &apdu[apdu_offset], chunk);
        tx_len += chunk;
        crc_14a_append(tx, tx_len);
        tx_len += 2;

        uint8_t retries = 0;
        while (true) {
            bsp_wdt_feed();
            pcd_14a_reader_timeout_set(reader->frame_timeout_ms);
            result->rf_status = transceive_frame(tx, tx_len, rx, &rx_len);
            uint8_t response_retries = 0u;
            while (!more && result->rf_status == STATUS_HF_TAG_NO &&
                    response_retries++ < ISO_DEP_RESPONSE_RETRIES) {
                uint16_t recovery_timeout = reader->frame_timeout_ms;
                if (wtx_total_ms > ISO_DEP_READER_MAX_WTX_TOTAL_MS - recovery_timeout) {
                    result->error = ISO_DEP_ERR_TIMEOUT;
                    return false;
                }
                wtx_total_ms += recovery_timeout;
                result->rf_status = request_response_retransmission(
                    reader, tx_block_num, rx, &rx_len);
            }
            if (result->rf_status != STATUS_HF_TAG_OK || rx_len < 3u) {
                result->error = result->rf_status == STATUS_HF_TAG_NO ?
                                ISO_DEP_ERR_TIMEOUT : ISO_DEP_ERR_TRANSPORT;
                return false;
            }
            if (!crc_valid(rx, rx_len)) {
                result->error = ISO_DEP_ERR_CRC;
                return false;
            }
            if (is_r_nak(rx[0]) && retries++ < 2u) continue;
            break;
        }

        apdu_offset += chunk;
        if (more) {
            uint8_t cid_offset;
            if (!is_r_ack(rx[0]) ||
                    (rx[0] & ISO_DEP_PCB_BLOCK_NUM) != tx_block_num ||
                    !parse_cid(reader, rx, rx_len, &cid_offset) ||
                    rx_len != (uint16_t)(cid_offset + 2u)) {
                result->error = ISO_DEP_ERR_SEQUENCE;
                return false;
            }
            tx_block_num ^= 1u;
            if (++chain_guard > ISO_DEP_READER_MAX_CHAIN_BLOCKS) {
                result->error = ISO_DEP_ERR_BLOCK;
                return false;
            }
            continue;
        }
        break;
    }

    /* The first response I-block repeats the final command I-block number. */
    reader->rx_block_num = tx_block_num;
    uint16_t response_len = 0;
    while (true) {
        bsp_wdt_feed();
        uint8_t pcb = rx[0];

        if (is_wtx(pcb)) {
            uint8_t offset;
            if (!parse_cid(reader, rx, rx_len, &offset) ||
                    rx_len != (uint16_t)(offset + 3u) ||
                    rx[offset] == 0u || rx[offset] > 59u) {
                result->error = ISO_DEP_ERR_BLOCK;
                return false;
            }
            if (++wtx_guard > ISO_DEP_READER_MAX_WTX) {
                result->wtx_count = wtx_guard;
                result->error = ISO_DEP_ERR_BLOCK;
                return false;
            }
            result->wtx_count = wtx_guard;
            uint32_t requested_timeout = (uint32_t)reader->frame_timeout_ms * rx[offset];
            uint16_t effective_timeout = (uint16_t)(requested_timeout > 5000u ?
                                                    5000u : requested_timeout);
            if (wtx_total_ms > ISO_DEP_READER_MAX_WTX_TOTAL_MS - effective_timeout) {
                result->error = ISO_DEP_ERR_TIMEOUT;
                return false;
            }
            wtx_total_ms += effective_timeout;
            uint8_t wtx[5];
            uint8_t wtx_len = 0;
            wtx[wtx_len++] = pcb;
            if (reader->cid_enabled) wtx[wtx_len++] = reader->cid;
            wtx[wtx_len++] = rx[offset];
            crc_14a_append(wtx, wtx_len);
            wtx_len += 2;
            pcd_14a_reader_timeout_set(effective_timeout);
            result->rf_status = transceive_frame(wtx, wtx_len, rx, &rx_len);
            uint8_t recovery_retries = 0u;
            while (result->rf_status == STATUS_HF_TAG_NO &&
                    recovery_retries++ < ISO_DEP_RESPONSE_RETRIES) {
                uint16_t recovery_timeout = reader->frame_timeout_ms;
                if (wtx_total_ms > ISO_DEP_READER_MAX_WTX_TOTAL_MS - recovery_timeout) {
                    result->error = ISO_DEP_ERR_TIMEOUT;
                    return false;
                }
                wtx_total_ms += recovery_timeout;
                result->rf_status = request_response_retransmission(
                    reader, reader->rx_block_num, rx, &rx_len);
            }
            if (result->rf_status != STATUS_HF_TAG_OK || rx_len < 3u) {
                result->error = ISO_DEP_ERR_TIMEOUT;
                return false;
            }
            if (!crc_valid(rx, rx_len)) {
                result->error = ISO_DEP_ERR_CRC;
                return false;
            }
            continue;
        }

        if (!is_i_block(pcb)) {
            result->error = ISO_DEP_ERR_BLOCK;
            return false;
        }
        if ((pcb & 0x04u) != 0u) { /* NAD was not negotiated by this reader. */
            result->error = ISO_DEP_ERR_BLOCK;
            return false;
        }
        if ((pcb & ISO_DEP_PCB_BLOCK_NUM) != reader->rx_block_num) {
            result->error = ISO_DEP_ERR_SEQUENCE;
            return false;
        }

        uint8_t offset;
        if (!parse_cid(reader, rx, rx_len, &offset) || rx_len < (uint16_t)(offset + 2u)) {
            result->error = ISO_DEP_ERR_BLOCK;
            return false;
        }
        uint16_t inf_len = rx_len - offset - 2u;
        if (inf_len > response_capacity - response_len) {
            result->error = ISO_DEP_ERR_OVERFLOW;
            return false;
        }
        memcpy(&response[response_len], &rx[offset], inf_len);
        response_len += inf_len;
        result->chain_blocks++;
        reader->rx_block_num ^= 1u;

        if ((pcb & ISO_DEP_PCB_I_CHAIN) == 0u) {
            reader->block_num = reader->rx_block_num;
            result->response_len = response_len;
            result->wtx_count = wtx_guard;
            result->error = ISO_DEP_OK;
            return response_len >= 2u;
        }
        if (result->chain_blocks >= ISO_DEP_READER_MAX_CHAIN_BLOCKS) {
            result->error = ISO_DEP_ERR_BLOCK;
            return false;
        }

        uint8_t rack[4];
        uint8_t rack_len = 0;
        rack[rack_len++] = ISO_DEP_PCB_R_ACK | reader->rx_block_num |
                           (reader->cid_enabled ? ISO_DEP_PCB_CID : 0u);
        if (reader->cid_enabled) rack[rack_len++] = reader->cid;
        crc_14a_append(rack, rack_len);
        rack_len += 2;
        pcd_14a_reader_timeout_set(reader->frame_timeout_ms);
        result->rf_status = transceive_frame(rack, rack_len, rx, &rx_len);
        if (result->rf_status != STATUS_HF_TAG_OK || rx_len < 3u) {
            result->error = ISO_DEP_ERR_TIMEOUT;
            return false;
        }
        if (!crc_valid(rx, rx_len)) {
            result->error = ISO_DEP_ERR_CRC;
            return false;
        }
    }
}

bool iso_dep_reader_transceive(iso_dep_reader_t *reader,
                               const uint8_t *apdu, uint16_t apdu_len,
                               uint8_t *response, uint16_t response_capacity,
                               iso_dep_result_t *result) {
    uint16_t previous_timeout = pcd_14a_reader_timeout_get();
    bool ok = iso_dep_reader_transceive_impl(reader, apdu, apdu_len,
                                             response, response_capacity, result);
    pcd_14a_reader_timeout_set(previous_timeout);
    return ok;
}

void iso_dep_reader_deselect(iso_dep_reader_t *reader) {
    if (reader == NULL) return;
    uint8_t tx[4];
    uint8_t rx[ISO_DEP_FRAME_MAX];
    uint8_t tx_len = 0;
    uint16_t rx_len = 0;
    tx[tx_len++] = ISO_DEP_PCB_S_DESELECT |
                   (reader->cid_enabled ? ISO_DEP_PCB_CID : 0u);
    if (reader->cid_enabled) tx[tx_len++] = reader->cid;
    crc_14a_append(tx, tx_len);
    tx_len += 2;
    uint16_t previous_timeout = pcd_14a_reader_timeout_get();
    pcd_14a_reader_timeout_set(reader->frame_timeout_ms);
    (void)transceive_frame(tx, tx_len, rx, &rx_len);
    pcd_14a_reader_timeout_set(previous_timeout);
}
