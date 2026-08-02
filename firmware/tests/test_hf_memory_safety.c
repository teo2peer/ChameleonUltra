#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rfid/nfctag/hf/nfc_14a_frame_internal.h"
#include "rfid/nfctag/hf/ntag_mirror_internal.h"
#include "rfid/reader/hf/emv_trace_internal.h"
#include "mf1_key_access_internal.h"
#include "rfid/nfctag/hf/mf1_auth_log_internal.h"

static void test_ntag_mirror_render(void) {
    static const uint8_t uid[7] = {0xde, 0xad, 0xbe, 0xef, 0x01, 0x02, 0x03};
    static const uint8_t counter[3] = {0x00, 0xa1, 0xff};
    uint8_t output[NTAG_MIRROR_UID_COUNTER_SIZE];

    memset(output, 0xa5, sizeof(output));
    ntag_mirror_render_uid(uid, output);
    assert(memcmp(output, "DEADBEEF010203", NTAG_MIRROR_UID_SIZE) == 0);
    for (size_t index = NTAG_MIRROR_UID_SIZE; index < sizeof(output); index++) {
        assert(output[index] == 0xa5);
    }

    memset(output, 0xa5, sizeof(output));
    ntag_mirror_render_counter(counter, output);
    assert(memcmp(output, "00A1FF", NTAG_MIRROR_COUNTER_SIZE) == 0);
    for (size_t index = NTAG_MIRROR_COUNTER_SIZE; index < sizeof(output); index++) {
        assert(output[index] == 0xa5);
    }

    memset(output, 0xa5, sizeof(output));
    ntag_mirror_render_uid_counter(uid, counter, output);
    assert(memcmp(output, "DEADBEEF010203x00A1FF", sizeof(output)) == 0);
}

static void test_ntag_mirror_overlay(void) {
    uint8_t mirror[NTAG_MIRROR_UID_COUNTER_SIZE];
    for (size_t index = 0; index < sizeof(mirror); index++) {
        mirror[index] = (uint8_t)(index + 1u);
    }

    for (uint8_t first_offset = 0; first_offset < NTAG_MIRROR_PAGE_SIZE; first_offset++) {
        uint8_t pages = (uint8_t)((first_offset + sizeof(mirror) + 3u) / 4u);
        for (uint8_t relative_page = 0; relative_page < pages; relative_page++) {
            struct {
                uint8_t before;
                uint8_t page[NTAG_MIRROR_PAGE_SIZE];
                uint8_t after;
            } guarded = {.before = 0x5a, .page = {0xee, 0xee, 0xee, 0xee}, .after = 0xa5};

            size_t copied = ntag_mirror_overlay_page(
                guarded.page,
                (uint8_t)(10u + relative_page),
                10u,
                first_offset,
                mirror,
                sizeof(mirror)
            );
            assert(copied <= NTAG_MIRROR_PAGE_SIZE);
            assert(guarded.before == 0x5a);
            assert(guarded.after == 0xa5);

            for (size_t destination = 0; destination < NTAG_MIRROR_PAGE_SIZE; destination++) {
                int source = (int)relative_page * (int)NTAG_MIRROR_PAGE_SIZE +
                             (int)destination - (int)first_offset;
                uint8_t expected = source >= 0 && (size_t)source < sizeof(mirror)
                                       ? mirror[source]
                                       : 0xee;
                assert(guarded.page[destination] == expected);
            }
        }
    }

    uint8_t untouched[NTAG_MIRROR_PAGE_SIZE] = {1, 2, 3, 4};
    assert(ntag_mirror_overlay_page(untouched, 9, 10, 0, mirror, sizeof(mirror)) == 0u);
    assert(memcmp(untouched, (uint8_t[]){1, 2, 3, 4}, sizeof(untouched)) == 0);
}

static void test_mf1_key_access(void) {
    uint8_t zero_key[MF1_KEY_ACCESS_SIZE] = {0};
    uint8_t destination[MF1_KEY_ACCESS_SIZE];
    memset(destination, 0xa5, sizeof(destination));
    assert(!mf1_copy_nonzero_key(destination, zero_key));
    assert(memcmp(destination, (uint8_t[]){0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5},
                  sizeof(destination)) == 0);

    for (size_t offset = 0; offset < 8u; offset++) {
        uint8_t storage[MF1_KEY_ACCESS_SIZE + 8u] = {0};
        uint8_t *source = &storage[offset];
        source[MF1_KEY_ACCESS_SIZE - 1u] = 0x42;
        struct {
            uint8_t before;
            uint8_t key[MF1_KEY_ACCESS_SIZE];
            uint8_t after;
        } guarded = {.before = 0x5a, .key = {0}, .after = 0xa5};

        assert(mf1_copy_nonzero_key(guarded.key, source));
        assert(guarded.before == 0x5a);
        assert(guarded.after == 0xa5);
        assert(memcmp(guarded.key, source, MF1_KEY_ACCESS_SIZE) == 0);
    }
}

static void test_mf1_auth_log_latch(void) {
    assert(sizeof(mf1_auth_log_record_t) == MF1_AUTH_LOG_RECORD_SIZE);
    assert(mf1_auth_log_flags(false, false) == 0u);
    assert(mf1_auth_log_flags(true, true) ==
           (MF1_AUTH_LOG_FLAG_KEY_B | MF1_AUTH_LOG_FLAG_NESTED));

    mf1_auth_log_latch_t latch = {0};
    assert(mf1_auth_log_latch_begin(&latch, true, 7u));
    assert(mf1_auth_log_latch_active(&latch, true, 7u));
    assert(!mf1_auth_log_latch_active(&latch, true, 8u));
    assert(!mf1_auth_log_latch_finish(&latch, true, 8u));
    assert(!latch.pending);

    assert(mf1_auth_log_latch_begin(&latch, true, 9u));
    assert(mf1_auth_log_latch_finish(&latch, true, 9u));
    assert(!mf1_auth_log_latch_begin(&latch, false, 10u));
}

static void set_frame_bit(uint8_t *frame, size_t bit, uint8_t value) {
    if (value != 0u) frame[bit / 8u] |= 1u << (bit % 8u);
}

static void test_nfc_14a_unwrap_exact_groups(void) {
    uint8_t short_frame = 0x5au;
    uint8_t short_output = 0u;
    assert(nfc_tag_14a_unwrap_frame_checked(
               &short_frame, 7u, &short_output, NULL) == 7u);
    assert(short_output == short_frame);

    uint8_t one_group[2] = {0};
    uint8_t one_output = 0xa5u;
    uint8_t one_parity = 0xa5u;
    assert(nfc_tag_14a_unwrap_frame_checked(
               one_group, 9u, &one_output, &one_parity) == 8u);
    assert(one_output == 0u);
    assert(one_parity == 0u);

    uint8_t partial[2] = {0xa5u, 0u};
    uint8_t partial_output[2] = {0};
    set_frame_bit(partial, 8u, 1u);  // parity
    set_frame_bit(partial, 9u, 1u);  // trailing data
    assert(nfc_tag_14a_unwrap_frame_checked(
               partial, 10u, partial_output, NULL) == 9u);
    assert(partial_output[0] == 0xa5u);
    assert(partial_output[1] == 0x01u);

    uint8_t in_place[3] = {0x3cu, 0u, 0u};
    const uint8_t trailing_byte = 0xa6u;
    set_frame_bit(in_place, 8u, 1u);  // parity
    for (uint8_t bit = 0u; bit < 8u; bit++) {
        set_frame_bit(in_place, 9u + bit, trailing_byte & (1u << bit));
    }
    assert(nfc_tag_14a_unwrap_frame_checked(
               in_place, 17u, in_place, NULL) == 16u);
    assert(in_place[0] == 0x3cu);
    assert(in_place[1] == trailing_byte);

    uint8_t eight_groups[9] = {0};
    uint8_t output[8];
    uint8_t parity[8];
    memset(output, 0xa5, sizeof(output));
    memset(parity, 0xa5, sizeof(parity));
    assert(nfc_tag_14a_unwrap_frame_checked(
               eight_groups, 72u, output, parity) == 64u);
    assert(memcmp(output, (uint8_t[8]){0}, sizeof(output)) == 0);
    assert(memcmp(parity, (uint8_t[8]){0}, sizeof(parity)) == 0);

    uint8_t eight_groups_partial[10] = {0};
    set_frame_bit(eight_groups_partial, 72u, 1u);
    assert(nfc_tag_14a_unwrap_frame_checked(
               eight_groups_partial, 73u, eight_groups_partial, NULL) == 65u);
    assert(eight_groups_partial[8] == 0x01u);
}

static void test_emv_uid_continuity(void) {
    static const uint8_t initial[] = {0xde, 0xad, 0xbe, 0xef};
    static const uint8_t same[] = {0xde, 0xad, 0xbe, 0xef};
    static const uint8_t changed[] = {0xde, 0xad, 0xbe, 0x00};

    assert(emv_trace_same_uid(initial, sizeof(initial), same, sizeof(same)));
    assert(!emv_trace_same_uid(initial, sizeof(initial), changed, sizeof(changed)));
    assert(!emv_trace_same_uid(initial, sizeof(initial), same, sizeof(same) - 1u));
}

int main(void) {
    test_ntag_mirror_render();
    test_ntag_mirror_overlay();
    test_mf1_key_access();
    test_mf1_auth_log_latch();
    test_nfc_14a_unwrap_exact_groups();
    test_emv_uid_continuity();
    return 0;
}
