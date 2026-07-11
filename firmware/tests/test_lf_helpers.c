#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "diphase.h"
#include "jablotron.h"
#include "manchester.h"
#include "wiegand.h"

static uint8_t identity_period(uint8_t interval) {
    return interval;
}

static void test_line_decoders(void) {
    bool bits[2] = {false, false};
    int8_t length;
    manchester m = {.rp = identity_period};
    diphase d = {.rp = identity_period};

    manchester_reset(&m);
    manchester_feed(&m, 0, bits, &length);
    assert(length == 1 && !bits[0] && m.sync);
    manchester_feed(&m, 1, bits, &length);
    assert(length == 1 && bits[0] && !m.sync);
    manchester_feed(&m, 1, bits, &length);
    assert(length == 2 && bits[0] && !bits[1] && m.sync);
    manchester_feed(&m, 3, bits, &length);
    assert(length == -1 && m.sync);

    diphase_reset(&d);
    diphase_feed(&d, 1, bits, &length);
    assert(length == 2 && bits[0] && !bits[1] && !d.boundary);
    diphase_feed(&d, 1, bits, &length);
    assert(length == 1 && bits[0] && d.boundary);
    diphase_feed(&d, 2, bits, &length);
    assert(length == 2 && bits[0] && bits[1] && d.boundary);
    diphase_feed(&d, 3, bits, &length);
    assert(length == -1 && d.boundary);
}

static void check_wiegand_roundtrip(wiegand_card_t card, uint8_t bits) {
    uint64_t raw = pack(&card);
    assert(raw != 0);
    wiegand_card_t *decoded = unpack(card.format, bits, 0, raw);
    assert(decoded != NULL);
    assert(decoded->format == card.format);
    assert(decoded->facility_code == card.facility_code);
    assert(decoded->card_number == card.card_number);
    assert(decoded->issue_level == card.issue_level);
    if (card.format == C15001 && card.oem == 0) {
        assert(decoded->oem == 900);
    } else {
        assert(decoded->oem == card.oem);
    }
    free(decoded);
}

static void test_wiegand_formats(void) {
    check_wiegand_roundtrip((wiegand_card_t){.format = H10301, .facility_code = 42, .card_number = 0x3456}, 26);
    check_wiegand_roundtrip((wiegand_card_t){.format = INDASC27, .facility_code = 0x123, .card_number = 0x2345}, 27);
    check_wiegand_roundtrip((wiegand_card_t){.format = KASTLE, .facility_code = 7, .card_number = 1234, .issue_level = 3}, 32);
    check_wiegand_roundtrip((wiegand_card_t){.format = C15001, .facility_code = 8, .card_number = 4321}, 36);
    check_wiegand_roundtrip((wiegand_card_t){.format = H10320, .card_number = 12345678}, 37);

    wiegand_card_t card = {.format = H10301, .facility_code = 42, .card_number = 0x3456};
    assert(unpack(H10301, 26, 0, pack(&card) ^ 1u) == NULL);
}

static void test_jablotron_vectors_and_modulator(void) {
    uint8_t uid[] = {0x01, 0xb6, 0x69, 0x00, 0x00};
    uint8_t invalid_uid[] = {0x81, 0, 0, 0, 0};
    uint32_t blocks[3] = {0};

    assert(jablotron_data_valid(uid));
    assert(!jablotron_data_valid(invalid_uid));
    assert(!jablotron_data_valid(NULL));
    assert(jablotron_t55xx_writer(uid, blocks) == 3);
    assert(blocks[0] == 0x00158050u);
    assert(blocks[1] == 0xffff01b6u);
    assert(blocks[2] == 0x6900001au);

    void *codec = jablotron.alloc();
    assert(codec != NULL);
    nrf_pwm_sequence_t *sequence = jablotron.modulator(codec, uid);
    assert(sequence != NULL);
    assert(jablotron.modulator(codec, invalid_uid) == NULL);

    const uint64_t raw = UINT64_C(0xffff01b66900001a);
    bool level = false;
    size_t output = 0;
    for (int pass = 0; pass < 2; pass++) {
        for (int bit_index = 63; bit_index >= 0; bit_index--) {
            bool bit = ((raw >> bit_index) & 1u) != 0;
            level = !level;
            assert(sequence->values.p_wave_form[output].channel_0 == (level ? 33 : 0));
            assert(sequence->values.p_wave_form[output++].counter_top == 32);
            if (!bit) {
                level = !level;
            }
            assert(sequence->values.p_wave_form[output].channel_0 == (level ? 33 : 0));
            assert(sequence->values.p_wave_form[output++].counter_top == 32);
        }
    }
    assert(output == 256);
    assert(!level);
    jablotron.free(codec);
}

int main(void) {
    test_line_decoders();
    test_wiegand_formats();
    test_jablotron_vectors_and_modulator();
    return 0;
}
