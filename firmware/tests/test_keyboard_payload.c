#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_timer.h"
#include "keyboard_payload.h"
#include "nrf_error.h"

typedef struct {
    uint8_t outputs;
    uint8_t modifiers;
    uint8_t usage;
} report_t;

static uint32_t now_ticks;
static bool usb_working;
static bool usb_tx_idle;
static bool ble_authorized;
static bool nus_tx_idle;
static bool hid_outputs_ready;
static bool ble_hid_ready;
static bool cancel_during_ble_ready;
static uint32_t hid_send_result;
static report_t reports[8];
static size_t report_count;
static size_t abort_count;
static size_t name_restore_count;

uint32_t app_timer_cnt_get(void) {
    return now_ticks;
}

uint32_t app_timer_cnt_diff_compute(uint32_t ticks_to, uint32_t ticks_from) {
    return (ticks_to - ticks_from) & APP_TIMER_MAX_CNT_VAL;
}

bool is_usb_working(void) {
    return usb_working;
}

bool is_usb_tx_idle(void) {
    return usb_tx_idle;
}

bool is_nus_tx_idle(void) {
    return nus_tx_idle;
}

bool ble_keyboard_link_authorized(void) {
    return ble_authorized;
}

bool keyboard_hid_ble_ready(void) {
    if (cancel_during_ble_ready) {
        cancel_during_ble_ready = false;
        assert(keyboard_payload_cancel_from_button());
    }
    return ble_hid_ready;
}

uint32_t ble_peripheral_name_set_temporary(const uint8_t *name, uint8_t length) {
    if (name == NULL && length == 0u) name_restore_count++;
    return NRF_SUCCESS;
}

void sleep_timer_stop(void) {
}

bool keyboard_hid_outputs_ready(uint8_t outputs) {
    (void)outputs;
    return hid_outputs_ready;
}

uint32_t keyboard_hid_send_report(uint8_t outputs, uint8_t modifiers,
                                  uint8_t usage) {
    assert(report_count < sizeof(reports) / sizeof(reports[0]));
    if (hid_send_result == NRF_SUCCESS) {
        reports[report_count++] = (report_t){outputs, modifiers, usage};
    }
    return hid_send_result;
}

void keyboard_hid_abort(void) {
    abort_count++;
}

static uint32_t crc32_ieee(const uint8_t *data, size_t length) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static void reset_test(void) {
    if (keyboard_payload_is_running()) {
        keyboard_payload_cancel();
        keyboard_payload_process();
    }
    assert(keyboard_payload_clear() == KEYBOARD_PAYLOAD_RESULT_OK);
    now_ticks = 0;
    usb_working = true;
    usb_tx_idle = true;
    ble_authorized = true;
    nus_tx_idle = true;
    hid_outputs_ready = true;
    ble_hid_ready = false;
    cancel_during_ble_ready = false;
    hid_send_result = NRF_SUCCESS;
    report_count = 0;
    abort_count = 0;
    name_restore_count = 0;
}

static uint32_t upload_program(const uint8_t *program, uint16_t length,
                               data_frame_transport_t transport) {
    uint32_t upload_id;
    uint32_t commit_id;
    uint32_t crc;
    uint16_t offset;
    uint16_t committed_length;

    assert(keyboard_payload_upload_begin(length, crc32_ieee(program, length),
                                         transport, &upload_id, &offset) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    assert(offset == 0);
    assert(keyboard_payload_upload_chunk(upload_id, 0, program, length,
                                         transport, &offset) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    assert(offset == length);
    assert(keyboard_payload_upload_commit(upload_id, transport, &commit_id,
                                          &committed_length, &crc) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    assert(committed_length == length);
    assert(crc == crc32_ieee(program, length));
    return commit_id;
}

static void test_usb_run_waits_for_command_response(void) {
    static const uint8_t program[] = {0x02, 0x01, 0x04, 0x00};
    reset_test();
    uint32_t commit_id = upload_program(program, sizeof(program),
                                        DATA_FRAME_TRANSPORT_USB);
    uint32_t run_id;
    assert(keyboard_payload_run(commit_id, KEYBOARD_OUTPUT_USB,
                                DATA_FRAME_TRANSPORT_USB, &run_id) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    assert(run_id != 0);
    assert(keyboard_payload_command_link_alive());

    usb_tx_idle = false;
    keyboard_payload_process();
    assert(report_count == 0);

    usb_tx_idle = true;
    keyboard_payload_process();
    now_ticks = 99;
    keyboard_payload_process();
    assert(report_count == 0);

    now_ticks = 100;
    keyboard_payload_process();
    assert(report_count == 1);
    assert(reports[0].outputs == KEYBOARD_OUTPUT_USB);
    assert(reports[0].modifiers == 0x01);
    assert(reports[0].usage == 0x04);

    now_ticks = 110;
    keyboard_payload_process();
    assert(report_count == 2);
    assert(reports[1].modifiers == 0);
    assert(reports[1].usage == 0);
    keyboard_payload_process();

    keyboard_payload_status_t status;
    keyboard_payload_get_status(&status);
    assert(status.state == KEYBOARD_PAYLOAD_STATE_COMPLETE);
    assert(status.error == KEYBOARD_PAYLOAD_ERROR_NONE);
    assert(status.pc == 3);
    assert(abort_count == 1);
}

static void test_run_tracks_its_command_transport(void) {
    static const uint8_t program[] = {0x00};
    reset_test();
    uint32_t commit_id = upload_program(program, sizeof(program),
                                        DATA_FRAME_TRANSPORT_USB);
    uint32_t run_id;

    ble_authorized = false;
    assert(keyboard_payload_run(commit_id, KEYBOARD_OUTPUT_USB,
                                DATA_FRAME_TRANSPORT_USB, &run_id) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    assert(keyboard_payload_command_link_alive());
    usb_working = false;
    assert(!keyboard_payload_command_link_alive());
    keyboard_payload_cancel_command_link();
    keyboard_payload_process();

    usb_working = true;
    ble_authorized = true;
    assert(keyboard_payload_run(commit_id, KEYBOARD_OUTPUT_USB,
                                DATA_FRAME_TRANSPORT_BLE, &run_id) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    assert(keyboard_payload_command_link_alive());
    ble_authorized = false;
    assert(!keyboard_payload_command_link_alive());
}

static void test_upload_transport_crc_and_atomic_commit(void) {
    static const uint8_t original[] = {0x00};
    static const uint8_t replacement[] = {0x01, 0x00, 0x01, 0x00};
    reset_test();
    uint32_t original_commit = upload_program(original, sizeof(original),
                                              DATA_FRAME_TRANSPORT_USB);

    uint32_t upload_id;
    uint16_t offset;
    assert(keyboard_payload_upload_begin(sizeof(replacement), 0,
                                         DATA_FRAME_TRANSPORT_USB,
                                         &upload_id, &offset) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    assert(keyboard_payload_upload_chunk(upload_id, 0, replacement,
                                         sizeof(replacement),
                                         DATA_FRAME_TRANSPORT_BLE, &offset) ==
           KEYBOARD_PAYLOAD_RESULT_TRANSPORT);
    assert(keyboard_payload_upload_chunk(upload_id, 0, replacement,
                                         sizeof(replacement),
                                         DATA_FRAME_TRANSPORT_USB, &offset) ==
           KEYBOARD_PAYLOAD_RESULT_OK);

    uint32_t replacement_commit;
    uint32_t crc;
    uint16_t length;
    assert(keyboard_payload_upload_commit(upload_id, DATA_FRAME_TRANSPORT_USB,
                                          &replacement_commit, &length, &crc) ==
           KEYBOARD_PAYLOAD_RESULT_CRC);

    uint32_t run_id;
    assert(keyboard_payload_run(original_commit, KEYBOARD_OUTPUT_USB,
                                DATA_FRAME_TRANSPORT_USB, &run_id) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
}

static void test_run_rejects_unavailable_output_and_bad_transport(void) {
    static const uint8_t program[] = {0x00};
    reset_test();
    uint32_t commit_id = upload_program(program, sizeof(program),
                                        DATA_FRAME_TRANSPORT_USB);
    uint32_t run_id;
    hid_outputs_ready = false;
    assert(keyboard_payload_run(commit_id, KEYBOARD_OUTPUT_USB,
                                DATA_FRAME_TRANSPORT_USB, &run_id) ==
           KEYBOARD_PAYLOAD_RESULT_UNAVAILABLE);
    hid_outputs_ready = true;
    assert(keyboard_payload_run(commit_id, KEYBOARD_OUTPUT_USB,
                                DATA_FRAME_TRANSPORT_NONE, &run_id) ==
           KEYBOARD_PAYLOAD_RESULT_PARAMETER);
}

static void test_shorter_commit_resets_execution_status(void) {
    static const uint8_t longer[] = {0x01, 0x00, 0x01, 0x00};
    static const uint8_t shorter[] = {0x00};
    reset_test();
    uint32_t commit_id = upload_program(longer, sizeof(longer),
                                        DATA_FRAME_TRANSPORT_USB);
    uint32_t run_id;
    assert(keyboard_payload_run(commit_id, KEYBOARD_OUTPUT_USB,
                                DATA_FRAME_TRANSPORT_USB, &run_id) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    keyboard_payload_process();
    now_ticks = 100;
    keyboard_payload_process();
    now_ticks = 101;
    keyboard_payload_process();

    keyboard_payload_status_t status;
    keyboard_payload_get_status(&status);
    assert(status.state == KEYBOARD_PAYLOAD_STATE_COMPLETE);
    assert(status.pc == 3);

    upload_program(shorter, sizeof(shorter), DATA_FRAME_TRANSPORT_USB);
    keyboard_payload_get_status(&status);
    assert(status.state == KEYBOARD_PAYLOAD_STATE_READY);
    assert(status.pc == 0);
    assert(status.length == sizeof(shorter));
    assert(status.outputs == 0);
    assert(status.run_id == 0);
}

static void test_ble_arm_waits_for_hid_and_runs_without_command_link(void) {
    static const uint8_t program[] = {0x02, 0x00, 0x04, 0x00};
    reset_test();
    uint32_t commit_id = upload_program(program, sizeof(program),
                                        DATA_FRAME_TRANSPORT_USB);
    uint32_t run_id;
    assert(keyboard_payload_arm_ble(commit_id, &run_id) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    assert(run_id != 0u);
    keyboard_payload_status_t status;
    keyboard_payload_get_status(&status);
    assert(status.state == KEYBOARD_PAYLOAD_STATE_ARMED);
    assert(keyboard_payload_is_armed());
    assert(status.outputs == KEYBOARD_OUTPUT_BLE);

    ble_authorized = false;
    assert(keyboard_payload_command_link_alive());
    keyboard_payload_process();
    assert(report_count == 0u);
    assert(name_restore_count == 0u);

    ble_hid_ready = true;
    keyboard_payload_process();
    assert(name_restore_count == 1u);
    keyboard_payload_get_status(&status);
    assert(status.state == KEYBOARD_PAYLOAD_STATE_RUNNING);
    assert(!keyboard_payload_is_armed());
    assert(keyboard_payload_command_link_alive());

    now_ticks = 100u;
    keyboard_payload_process();
    assert(report_count == 1u);
    assert(reports[0].outputs == KEYBOARD_OUTPUT_BLE);
}

static void test_cancel_disarms_and_restores_name(void) {
    static const uint8_t program[] = {0x00};
    reset_test();
    uint32_t commit_id = upload_program(program, sizeof(program),
                                        DATA_FRAME_TRANSPORT_USB);
    uint32_t run_id;
    assert(keyboard_payload_arm_ble(commit_id, &run_id) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    keyboard_payload_cancel();

    keyboard_payload_status_t status;
    keyboard_payload_get_status(&status);
    assert(status.state == KEYBOARD_PAYLOAD_STATE_CANCELLED);
    assert(!keyboard_payload_is_armed());
    assert(name_restore_count == 1u);
}

static void test_button_cancel_cannot_be_overwritten_by_armed_transition(void) {
    static const uint8_t program[] = {0x02, 0x00, 0x04, 0x00};
    reset_test();
    uint32_t commit_id = upload_program(program, sizeof(program),
                                        DATA_FRAME_TRANSPORT_USB);
    uint32_t run_id;
    assert(keyboard_payload_arm_ble(commit_id, &run_id) ==
           KEYBOARD_PAYLOAD_RESULT_OK);
    ble_hid_ready = true;
    cancel_during_ble_ready = true;

    keyboard_payload_process();

    keyboard_payload_status_t status;
    keyboard_payload_get_status(&status);
    assert(status.state == KEYBOARD_PAYLOAD_STATE_CANCELLED);
    assert(status.error == KEYBOARD_PAYLOAD_ERROR_CANCELLED);
    assert(report_count == 0u);
    assert(name_restore_count == 1u);
}

int main(void) {
    test_usb_run_waits_for_command_response();
    test_run_tracks_its_command_transport();
    test_upload_transport_crc_and_atomic_commit();
    test_run_rejects_unavailable_output_and_bad_transport();
    test_shorter_commit_resets_execution_status();
    test_ble_arm_waits_for_hid_and_runs_without_command_link();
    test_cancel_disarms_and_restores_name();
    test_button_cancel_cannot_be_overwritten_by_armed_transition();
    return 0;
}
