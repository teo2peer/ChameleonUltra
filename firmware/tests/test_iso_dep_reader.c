#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_status.h"
#include "iso_dep_reader.h"

typedef uint8_t (*exchange_handler_t)(const uint8_t *, uint8_t, uint8_t *, uint16_t *);

static exchange_handler_t m_handler;
static uint8_t m_step;
static uint16_t m_timeout = 100;

void crc_14a_calculate(uint8_t *data, size_t length, uint8_t *crc) {
    uint16_t value = 0x6363u;
    for (size_t i = 0; i < length; i++) value = (uint16_t)((value << 1) ^ data[i]);
    crc[0] = (uint8_t)value;
    crc[1] = (uint8_t)(value >> 8);
}

void crc_14a_append(uint8_t *data, size_t length) {
    crc_14a_calculate(data, length, &data[length]);
}

void write_register_single(uint8_t address, uint8_t value) {
    (void)address;
    (void)value;
}

uint16_t pcd_14a_reader_timeout_get(void) {
    return m_timeout;
}

void pcd_14a_reader_timeout_set(uint16_t timeout_ms) {
    m_timeout = timeout_ms;
}

uint8_t pcd_14a_reader_bytes_transfer(uint8_t command, uint8_t *tx,
                                      uint8_t tx_len, uint8_t *rx,
                                      uint16_t *rx_bits, uint16_t max_rx_bits) {
    assert(command == PCD_TRANSCEIVE);
    assert(max_rx_bits >= 64u * 8u);
    uint16_t rx_len = 0;
    uint8_t status = m_handler(tx, tx_len, rx, &rx_len);
    *rx_bits = rx_len * 8u;
    return status;
}

static void make_frame(uint8_t *out, uint16_t *length,
                       const uint8_t *body, uint8_t body_len) {
    memcpy(out, body, body_len);
    crc_14a_append(out, body_len);
    *length = body_len + 2u;
}

static uint8_t chained_command_handler(const uint8_t *tx, uint8_t tx_len,
                                       uint8_t *rx, uint16_t *rx_len) {
    if (m_step++ == 0u) {
        assert(tx[0] == 0x12u); /* chained PCD I(0) */
        assert(tx_len == 16u);
        const uint8_t rack[] = {0xA2}; /* PICC acknowledges received BN=0 */
        make_frame(rx, rx_len, rack, sizeof(rack));
    } else {
        assert(tx[0] == 0x03u); /* final PCD I(1) */
        const uint8_t response[] = {0x03, 0x90, 0x00}; /* PICC response I(1) */
        make_frame(rx, rx_len, response, sizeof(response));
    }
    return STATUS_HF_TAG_OK;
}

static uint8_t chained_response_handler(const uint8_t *tx, uint8_t tx_len,
                                        uint8_t *rx, uint16_t *rx_len) {
    (void)tx_len;
    if (m_step == 0u) {
        assert(tx[0] == 0x02u); /* command I(0) */
        const uint8_t first[] = {0x12, 0xAA}; /* chained response I(0) */
        make_frame(rx, rx_len, first, sizeof(first));
    } else if (m_step == 1u) {
        assert(tx[0] == 0xA3u); /* PCD expects response I(1) next */
        const uint8_t second[] = {0x03, 0xBB, 0x90, 0x00};
        make_frame(rx, rx_len, second, sizeof(second));
    } else {
        assert(tx[0] == 0x02u); /* final I(1) made the next command I(0) */
        const uint8_t response[] = {0x02, 0x90, 0x00};
        make_frame(rx, rx_len, response, sizeof(response));
    }
    m_step++;
    return STATUS_HF_TAG_OK;
}

static uint8_t wtx_handler(const uint8_t *tx, uint8_t tx_len,
                           uint8_t *rx, uint16_t *rx_len) {
    (void)tx_len;
    if (m_step++ == 0u) {
        assert(tx[0] == 0x02u);
        const uint8_t wtx[] = {0xF2, 0x02};
        make_frame(rx, rx_len, wtx, sizeof(wtx));
    } else {
        assert(tx[0] == 0xF2u && tx[1] == 0x02u);
        const uint8_t response[] = {0x02, 0x90, 0x00};
        make_frame(rx, rx_len, response, sizeof(response));
    }
    return STATUS_HF_TAG_OK;
}

static uint8_t repeated_wtx_handler(const uint8_t *tx, uint8_t tx_len,
                                    uint8_t *rx, uint16_t *rx_len) {
    (void)tx_len;
    if (m_step == 0u) {
        assert(tx[0] == 0x02u);
    } else {
        assert(tx[0] == 0xF2u && tx[1] == 0x01u);
    }
    if (m_step++ < 9u) {
        const uint8_t wtx[] = {0xF2, 0x01};
        make_frame(rx, rx_len, wtx, sizeof(wtx));
    } else {
        const uint8_t response[] = {0x02, 0x90, 0x00};
        make_frame(rx, rx_len, response, sizeof(response));
    }
    return STATUS_HF_TAG_OK;
}

static uint8_t endless_wtx_handler(const uint8_t *tx, uint8_t tx_len,
                                   uint8_t *rx, uint16_t *rx_len) {
    (void)tx_len;
    if (m_step++ == 0u) {
        assert(tx[0] == 0x02u);
    } else {
        assert(tx[0] == 0xF2u && tx[1] == 0x02u);
    }
    const uint8_t wtx[] = {0xF2, 0x02};
    make_frame(rx, rx_len, wtx, sizeof(wtx));
    return STATUS_HF_TAG_OK;
}

static uint8_t wtx_timeout_recovery_handler(const uint8_t *tx, uint8_t tx_len,
                                            uint8_t *rx, uint16_t *rx_len) {
    (void)tx_len;
    if (m_step == 0u) {
        assert(tx[0] == 0x02u);
        const uint8_t wtx[] = {0xF2, 0x01};
        make_frame(rx, rx_len, wtx, sizeof(wtx));
        m_step++;
        return STATUS_HF_TAG_OK;
    }
    if (m_step == 1u) {
        assert(tx[0] == 0xF2u && tx[1] == 0x01u);
        *rx_len = 0u;
        m_step++;
        return STATUS_HF_TAG_NO;
    }
    assert(tx[0] == 0xB2u);
    const uint8_t response[] = {0x02, 0x90, 0x00};
    make_frame(rx, rx_len, response, sizeof(response));
    m_step++;
    return STATUS_HF_TAG_OK;
}

static uint8_t initial_timeout_recovery_handler(const uint8_t *tx, uint8_t tx_len,
                                                uint8_t *rx, uint16_t *rx_len) {
    (void)tx_len;
    if (m_step++ == 0u) {
        assert(tx[0] == 0x02u);
        *rx_len = 0u;
        return STATUS_HF_TAG_NO;
    }
    assert(tx[0] == 0xB2u);
    const uint8_t response[] = {0x02, 0x90, 0x00};
    make_frame(rx, rx_len, response, sizeof(response));
    return STATUS_HF_TAG_OK;
}

static uint8_t bad_crc_handler(const uint8_t *tx, uint8_t tx_len,
                               uint8_t *rx, uint16_t *rx_len) {
    (void)tx;
    (void)tx_len;
    const uint8_t invalid[] = {0x02, 0x90, 0x00, 0x00, 0x00};
    memcpy(rx, invalid, sizeof(invalid));
    *rx_len = sizeof(invalid);
    return STATUS_HF_TAG_OK;
}

static uint8_t final_nak_handler(const uint8_t *tx, uint8_t tx_len,
                                 uint8_t *rx, uint16_t *rx_len) {
    (void)tx_len;
    assert(tx[0] == 0x02u);
    if (m_step++ == 0u) {
        const uint8_t nak[] = {0xB2};
        make_frame(rx, rx_len, nak, sizeof(nak));
    } else {
        const uint8_t response[] = {0x02, 0x90, 0x00};
        make_frame(rx, rx_len, response, sizeof(response));
    }
    return STATUS_HF_TAG_OK;
}

static uint8_t deselect_handler(const uint8_t *tx, uint8_t tx_len,
                                uint8_t *rx, uint16_t *rx_len) {
    assert(tx[0] == 0xC2u);
    assert(tx_len == 3u);
    const uint8_t response[] = {0xC2};
    make_frame(rx, rx_len, response, sizeof(response));
    m_step++;
    return STATUS_HF_TAG_OK;
}

static iso_dep_reader_t new_reader(uint8_t fsci) {
    picc_14a_tag_t tag = {0};
    tag.ats_len = 2;
    tag.ats[0] = 2;
    tag.ats[1] = fsci;
    iso_dep_reader_t reader;
    iso_dep_reader_init(&reader, &tag);
    return reader;
}

static void test_chained_command(void) {
    iso_dep_reader_t reader = new_reader(0);
    uint8_t command[20] = {0};
    uint8_t response[32];
    iso_dep_result_t result;
    m_step = 0;
    m_handler = chained_command_handler;
    assert(iso_dep_reader_transceive(&reader, command, sizeof(command),
                                     response, sizeof(response), &result));
    assert(result.response_len == 2u);
    assert(reader.block_num == 0u);
}

static void test_chained_response_and_next_command(void) {
    iso_dep_reader_t reader = new_reader(8);
    const uint8_t command[] = {0x00, 0xA4, 0x04, 0x00, 0x00};
    uint8_t response[32];
    iso_dep_result_t result;
    m_step = 0;
    m_handler = chained_response_handler;
    assert(iso_dep_reader_transceive(&reader, command, sizeof(command),
                                     response, sizeof(response), &result));
    assert(result.response_len == 4u);
    assert(memcmp(response, "\xAA\xBB\x90\x00", 4) == 0);
    assert(reader.block_num == 0u);
    assert(iso_dep_reader_transceive(&reader, command, sizeof(command),
                                     response, sizeof(response), &result));
}

static void test_wtx_and_crc_error(void) {
    iso_dep_reader_t reader = new_reader(8);
    const uint8_t command[] = {0x00, 0x84, 0x00, 0x00, 0x08};
    uint8_t response[32];
    iso_dep_result_t result;
    m_step = 0;
    m_handler = wtx_handler;
    assert(iso_dep_reader_transceive(&reader, command, sizeof(command),
                                     response, sizeof(response), &result));
    assert(result.wtx_count == 1u);

    reader = new_reader(8);
    m_handler = bad_crc_handler;
    assert(!iso_dep_reader_transceive(&reader, command, sizeof(command),
                                      response, sizeof(response), &result));
    assert(result.error == ISO_DEP_ERR_CRC);

    reader = new_reader(8);
    m_step = 0;
    m_handler = final_nak_handler;
    assert(iso_dep_reader_transceive(&reader, command, sizeof(command),
                                     response, sizeof(response), &result));
    assert(m_step == 2u);
}

static void test_repeated_wtx_and_total_budget(void) {
    iso_dep_reader_t reader = new_reader(8);
    const uint8_t command[] = {0x00, 0xA4, 0x04, 0x00, 0x00};
    uint8_t response[32];
    iso_dep_result_t result;

    m_step = 0u;
    m_handler = repeated_wtx_handler;
    assert(iso_dep_reader_transceive(&reader, command, sizeof(command),
                                     response, sizeof(response), &result));
    assert(result.wtx_count == 9u);
    assert(result.response_len == 2u);

    reader = new_reader(8);
    reader.frame_timeout_ms = 1000u;
    m_step = 0u;
    m_handler = endless_wtx_handler;
    assert(!iso_dep_reader_transceive(&reader, command, sizeof(command),
                                      response, sizeof(response), &result));
    assert(result.error == ISO_DEP_ERR_TIMEOUT);
    assert(result.wtx_count == 3u);
}

static void test_wtx_timeout_response_recovery(void) {
    iso_dep_reader_t reader = new_reader(8);
    const uint8_t command[] = {0x80, 0xCA, 0x9F, 0x36, 0x00};
    uint8_t response[32];
    iso_dep_result_t result;

    m_step = 0u;
    m_handler = wtx_timeout_recovery_handler;
    assert(iso_dep_reader_transceive(&reader, command, sizeof(command),
                                     response, sizeof(response), &result));
    assert(m_step == 3u);
    assert(result.wtx_count == 1u);
    assert(result.response_len == 2u);

    reader = new_reader(8);
    m_step = 0u;
    m_handler = initial_timeout_recovery_handler;
    assert(iso_dep_reader_transceive(&reader, command, sizeof(command),
                                     response, sizeof(response), &result));
    assert(m_step == 2u);
    assert(result.wtx_count == 0u);
    assert(result.response_len == 2u);
}

static void test_deselect(void) {
    iso_dep_reader_t reader = new_reader(8);
    m_step = 0u;
    m_timeout = 123u;
    m_handler = deselect_handler;
    iso_dep_reader_deselect(&reader);
    assert(m_step == 1u);
    assert(m_timeout == 123u);
}

int main(void) {
    test_chained_command();
    test_chained_response_and_next_command();
    test_wtx_and_crc_error();
    test_repeated_wtx_and_total_budget();
    test_wtx_timeout_response_recovery();
    test_deselect();
    puts("iso_dep_reader tests passed");
    return 0;
}
