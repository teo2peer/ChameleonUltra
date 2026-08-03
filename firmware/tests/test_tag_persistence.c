#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fds_util.h"
#include "nfc_mf1.h"
#include "rfid/nfctag/tag_emulation.h"
#include "rfid/nfctag/tag_persistence.h"

static bool write_results[8];
static size_t write_result_count;
static size_t write_result_index;
static unsigned write_count;
static ret_code_t last_fds_error = NRF_SUCCESS;
static int delete_count;
static unsigned mf1_save_calls;
static bool shadow_prepare;
static bool shadow_prepared;
static uint8_t last_write[4500];
static uint16_t last_write_length;
static bool hf_sense_enabled;
static bool lf_sense_enabled;
static unsigned hf_sense_changes;
static unsigned lf_sense_changes;
static nfc_tag_mf1_write_mode_t write_mode = NFC_TAG_MF1_WRITE_NORMAL;

static void set_write_results(const bool *results, size_t count) {
    assert(count <= sizeof(write_results) / sizeof(write_results[0]));
    memcpy(write_results, results, count * sizeof(results[0]));
    write_result_count = count;
    write_result_index = 0u;
}

bool fds_write_sync(uint16_t id, uint16_t key, uint16_t length, void *buffer) {
    (void)id;
    (void)key;
    assert(length <= sizeof(last_write));
    memcpy(last_write, buffer, length);
    last_write_length = length;
    write_count++;
    bool result = write_result_index < write_result_count
                  ? write_results[write_result_index++] : true;
    last_fds_error = result ? NRF_SUCCESS : NRF_ERROR_BUSY;
    return result;
}

bool fds_read_sync(uint16_t id, uint16_t key, uint16_t *length, uint8_t *buffer) {
    (void)id;
    (void)key;
    (void)length;
    (void)buffer;
    last_fds_error = NRF_ERROR_BUSY;
    return false;
}

int fds_delete_sync(uint16_t id, uint16_t key) {
    (void)id;
    (void)key;
    return delete_count;
}

bool fds_is_exists(uint16_t id, uint16_t key) {
    (void)id;
    (void)key;
    return false;
}

ret_code_t fds_util_last_error(void) {
    return last_fds_error;
}

void get_fds_map_by_slot_sense_type_for_dump(
    uint8_t slot,
    tag_sense_type_t sense_type,
    fds_slot_record_map_t *map
) {
    map->id = (uint16_t)(0x100u + slot);
    map->key = (uint16_t)(0x200u + sense_type);
}

void get_fds_map_by_slot_sense_type_for_nick(
    uint8_t slot,
    tag_sense_type_t sense_type,
    fds_slot_record_map_t *map
) {
    get_fds_map_by_slot_sense_type_for_dump(slot, sense_type, map);
}

void nfc_tag_14a_sense_switch(bool enable) {
    hf_sense_enabled = enable;
    hf_sense_changes++;
}

void lf_tag_125khz_sense_switch(bool enable) {
    lf_sense_enabled = enable;
    lf_sense_changes++;
}

void rgb_marquee_reset(void) {}

int nfc_tag_mf1_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    (void)type;
    (void)buffer;
    return 16;
}

int nfc_tag_mf1_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    (void)type;
    mf1_save_calls++;
    if (shadow_prepare && !shadow_prepared) {
        buffer->buffer[1] ^= 0x5au;
        shadow_prepared = true;
        return 16;
    }
    if (shadow_prepare) return 0;
    return 16;
}

void nfc_tag_mf1_data_save_failcb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    (void)type;
    (void)buffer;
    shadow_prepared = false;
}

#define DEFINE_TAG_CALLBACKS(prefix, length_value)                                      \
    int prefix##_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer) {     \
        (void)type;                                                                      \
        (void)buffer;                                                                    \
        return (length_value);                                                           \
    }                                                                                   \
    int prefix##_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {     \
        (void)type;                                                                      \
        (void)buffer;                                                                    \
        return (length_value);                                                           \
    }                                                                                   \
    bool prefix##_data_factory(uint8_t slot, tag_specific_type_t type) {                \
        (void)slot;                                                                      \
        (void)type;                                                                      \
        return true;                                                                     \
    }

DEFINE_TAG_CALLBACKS(nfc_tag_mf0_ntag, 16)
DEFINE_TAG_CALLBACKS(nfc_tag_14a_4, 16)
DEFINE_TAG_CALLBACKS(nfc_tag_seos, 16)

void nfc_tag_mf0_ntag_data_save_failcb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    (void)type;
    (void)buffer;
}

bool nfc_tag_mf1_data_factory(uint8_t slot, tag_specific_type_t type) {
    (void)slot;
    (void)type;
    return true;
}

nfc_tag_mf1_write_mode_t nfc_tag_mf1_get_write_mode(void) {
    return write_mode;
}

int lf_tag_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    (void)type;
    (void)buffer;
    return 8;
}

#define DEFINE_LF_CALLBACKS(prefix)                                                      \
    int prefix##_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {     \
        (void)type;                                                                      \
        (void)buffer;                                                                    \
        return 8;                                                                        \
    }                                                                                   \
    bool prefix##_data_factory(uint8_t slot, tag_specific_type_t type) {                \
        (void)slot;                                                                      \
        (void)type;                                                                      \
        return true;                                                                     \
    }

DEFINE_LF_CALLBACKS(lf_tag_em410x)
DEFINE_LF_CALLBACKS(lf_tag_hidprox)
DEFINE_LF_CALLBACKS(lf_tag_ioprox)
DEFINE_LF_CALLBACKS(lf_tag_viking)
DEFINE_LF_CALLBACKS(lf_tag_pac)
DEFINE_LF_CALLBACKS(lf_tag_jablotron)
DEFINE_LF_CALLBACKS(lf_tag_idteck)

static tag_data_buffer_t *prepare_active_mf1(void) {
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_MIFARE_1024);
    assert(buffer != NULL);
    memset(buffer->buffer, 0, buffer->length);
    buffer->actual_length = 0u;
    assert(tag_emulation_load_by_buffer(TAG_TYPE_MIFARE_1024, false));
    assert(tag_emulation_is_active_type_loaded(TAG_TYPE_MIFARE_1024));
    assert(!tag_emulation_is_active_type_loaded(TAG_TYPE_NTAG_213));
    return buffer;
}

static void test_failed_save_retries_and_commits_crc(void) {
    tag_data_buffer_t *buffer = prepare_active_mf1();
    const bool baseline_results[] = {true, true};
    set_write_results(baseline_results, 2u);
    assert(tag_emulation_save());
    assert(write_count == 2u);

    buffer->buffer[0] = 0x42u;
    const bool failed[] = {false};
    set_write_results(failed, 1u);
    assert(!tag_emulation_save());
    assert(write_count == 3u);
    unsigned callbacks_after_failure = mf1_save_calls;

    const bool retried[] = {true};
    set_write_results(retried, 1u);
    assert(tag_emulation_save());
    assert(write_count == 4u);
    assert(mf1_save_calls == callbacks_after_failure + 1u);

    assert(tag_emulation_save());
    assert(write_count == 4u);
}

static void test_failed_shadow_prepare_is_retried(void) {
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_MIFARE_1024);
    shadow_prepare = true;
    shadow_prepared = false;
    buffer->buffer[2] ^= 0xa5u;

    const bool failed[] = {false};
    set_write_results(failed, 1u);
    assert(!tag_emulation_save());
    unsigned callbacks_after_failure = mf1_save_calls;
    assert(!shadow_prepared);
    assert(last_write_length == 16u);
    uint8_t failed_payload[16];
    memcpy(failed_payload, last_write, sizeof(failed_payload));

    // The failed transition rolled back, so writes before the successful retry
    // are still normal-mode writes and belong in the new persisted baseline.
    buffer->buffer[4] ^= 0x96u;

    const bool retried[] = {true};
    set_write_results(retried, 1u);
    assert(tag_emulation_save());
    assert(mf1_save_calls == callbacks_after_failure + 1u);
    assert(shadow_prepared);
    assert(last_write_length == 16u);
    assert(memcmp(failed_payload, last_write, sizeof(failed_payload)) != 0);
    shadow_prepare = false;
}

static void test_factory_data_requires_configured_type(void) {
    tag_slot_specific_type_t types;
    tag_emulation_get_specific_types_by_slot(0u, &types);
    assert(types.tag_hf == TAG_TYPE_MIFARE_1024);
    assert(!tag_emulation_factory_data(0u, TAG_TYPE_NTAG_213));
}

static void test_invalid_crc_baseline_forces_write(void) {
    tag_data_buffer_t *buffer = prepare_active_mf1();
    unsigned writes_before = write_count;
    buffer->crc_valid = false;
    const bool succeeded[] = {true};
    set_write_results(succeeded, 1u);
    assert(tag_emulation_save());
    assert(write_count == writes_before + 1u);
    assert(buffer->crc_valid);
}

static void test_atomic_snapshot_force_write_retry_and_restore(void) {
    tag_data_buffer_t *buffer = prepare_active_mf1();
    tag_emulation_sense_run();
    assert(hf_sense_enabled);
    assert(!lf_sense_enabled);

    uint8_t slot = UINT8_MAX;
    tag_specific_type_t type = TAG_TYPE_UNDEFINED;
    uint32_t owner_generation = 0u;
    assert(tag_emulation_snapshot_begin(&slot, &type, &owner_generation));
    assert(slot == 0u);
    assert(type == TAG_TYPE_MIFARE_1024);
    assert(owner_generation != 0u);
    assert(tag_emulation_snapshot_is_active());
    assert(!hf_sense_enabled);
    assert(!lf_sense_enabled);

    tag_slot_specific_type_t configured;
    tag_emulation_change_type(0u, TAG_TYPE_NTAG_213);
    tag_emulation_get_specific_types_by_slot(0u, &configured);
    assert(configured.tag_hf == TAG_TYPE_MIFARE_1024);
    assert(!tag_emulation_change_slot(1u, true));
    assert(!tag_emulation_factory_data(0u, TAG_TYPE_MIFARE_1024));
    assert(!tag_emulation_load_by_buffer(TAG_TYPE_MIFARE_1024, false));
    assert(!tag_emulation_save());

    buffer->buffer[7] ^= 0x7eu;
    unsigned writes_before = write_count;
    const bool failed[] = {false};
    set_write_results(failed, 1u);
    assert(tag_emulation_snapshot_save(slot, type) ==
           TAG_SNAPSHOT_SAVE_FLASH_FAIL);
    assert(write_count == writes_before + 1u);
    assert(tag_emulation_snapshot_is_active());
    assert(!hf_sense_enabled);

    const bool retried[] = {true};
    set_write_results(retried, 1u);
    assert(tag_emulation_snapshot_save(slot, type) == TAG_SNAPSHOT_SAVE_OK);
    assert(write_count == writes_before + 2u);
    assert(tag_emulation_snapshot_is_active());
    tag_emulation_snapshot_release();
    assert(!tag_emulation_snapshot_is_active());
    assert(hf_sense_enabled);
    assert(!lf_sense_enabled);

    // SAVE_RELEASE must write even when the CRC baseline is unchanged.
    writes_before = write_count;
    uint32_t same_owner_generation = 0u;
    assert(tag_emulation_snapshot_begin(
               &slot, &type, &same_owner_generation));
    assert(same_owner_generation == owner_generation);
    const bool forced[] = {true};
    set_write_results(forced, 1u);
    assert(tag_emulation_snapshot_save(slot, type) == TAG_SNAPSHOT_SAVE_OK);
    assert(write_count == writes_before + 1u);
    tag_emulation_snapshot_release();

    assert(tag_emulation_load_by_buffer(TAG_TYPE_MIFARE_1024, false));
    uint32_t reloaded_owner_generation = 0u;
    write_mode = NFC_TAG_MF1_WRITE_SHADOW;
    assert(tag_emulation_snapshot_begin(
               &slot, &type, &reloaded_owner_generation));
    assert(reloaded_owner_generation != owner_generation);
    writes_before = write_count;
    assert(tag_emulation_snapshot_save(slot, type) ==
           TAG_SNAPSHOT_SAVE_WRITE_MODE);
    assert(write_count == writes_before);
    assert(tag_emulation_snapshot_is_active());
    tag_emulation_snapshot_release();
    write_mode = NFC_TAG_MF1_WRITE_NORMAL;
    assert(hf_sense_changes > 0u);
    assert(lf_sense_changes > 0u);
}

static void test_failed_switch_and_delete_keep_owner(void) {
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_MIFARE_1024);
    buffer->buffer[3] ^= 0x3cu;
    const bool failed[] = {false};
    set_write_results(failed, 1u);
    g_is_tag_emulating = true;
    assert(!tag_emulation_change_slot(1u, true));
    assert(tag_emulation_get_slot() == 0u);
    assert(tag_emulation_is_active_type_loaded(TAG_TYPE_MIFARE_1024));
    assert(!g_is_tag_emulating);

    tag_slot_specific_type_t types;
    last_fds_error = NRF_ERROR_BUSY;
    delete_count = 0;
    assert(!tag_emulation_delete_data(0u, TAG_SENSE_HF));
    tag_emulation_get_specific_types_by_slot(0u, &types);
    assert(types.tag_hf == TAG_TYPE_MIFARE_1024);
    assert(tag_emulation_is_active_type_loaded(TAG_TYPE_MIFARE_1024));

    last_fds_error = NRF_SUCCESS;
    assert(tag_emulation_delete_data(0u, TAG_SENSE_HF));
    tag_emulation_get_specific_types_by_slot(0u, &types);
    assert(types.tag_hf == TAG_TYPE_UNDEFINED);
    assert(!tag_emulation_is_active_type_loaded(TAG_TYPE_MIFARE_1024));
}

int main(void) {
    test_failed_save_retries_and_commits_crc();
    test_failed_shadow_prepare_is_retried();
    test_factory_data_requires_configured_type();
    test_invalid_crc_baseline_forces_write();
    test_atomic_snapshot_force_write_retry_and_restore();
    test_failed_switch_and_delete_keep_owner();
    return 0;
}
