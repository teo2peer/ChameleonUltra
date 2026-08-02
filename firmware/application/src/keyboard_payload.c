#include "keyboard_payload.h"

#include <string.h>

#include "app_timer.h"
#include "ble_main.h"
#include "keyboard_hid.h"
#include "nrf_error.h"
#include "syssleep.h"
#include "usb_main.h"

#define KEYBOARD_OPCODE_END              0x00u
#define KEYBOARD_OPCODE_DELAY            0x01u
#define KEYBOARD_OPCODE_TAP              0x02u
#define KEYBOARD_MIN_USAGE               0x04u
#define KEYBOARD_MAX_USAGE               0x65u
#define KEYBOARD_MAX_DELAY_MS            10000u
#define KEYBOARD_TRANSITION_MS           10u
#define KEYBOARD_RUN_START_DELAY_MS      100u
#define KEYBOARD_READY_TIMEOUT_MS        1000u

typedef enum {
    INTERPRETER_FETCH = 0,
    INTERPRETER_RELEASE,
    INTERPRETER_DELAY,
} interpreter_phase_t;

static uint8_t m_buffers[2][KEYBOARD_PAYLOAD_MAX_LENGTH];
static uint8_t m_upload_buffer;
static uint8_t m_committed_buffer;
static bool m_upload_active;
static bool m_commit_valid;
static data_frame_transport_t m_upload_transport;
static uint16_t m_expected;
static uint16_t m_received;
static uint16_t m_length;
static uint16_t m_pc;
static uint32_t m_expected_crc;
static uint32_t m_committed_crc;
static uint32_t m_upload_id;
static uint32_t m_commit_id;
static uint32_t m_run_id;
static uint32_t m_next_id = 1u;
static uint32_t m_delay_deadline;
static uint32_t m_transition_deadline;
static uint32_t m_not_ready_since;
static uint8_t m_outputs;
static data_frame_transport_t m_run_transport;
static uint8_t m_tap_modifier;
static uint8_t m_tap_usage;
static keyboard_payload_state_t m_state = KEYBOARD_PAYLOAD_STATE_EMPTY;
static keyboard_payload_error_t m_error = KEYBOARD_PAYLOAD_ERROR_NONE;
static interpreter_phase_t m_phase;
static bool m_not_ready;
static bool m_waiting_run_response;
static bool m_auto_run;
static volatile bool m_cancel_requested;
static volatile uint8_t m_cancel_error;

static uint16_t read_u16be(const uint8_t *data) {
    return ((uint16_t)data[0] << 8) | data[1];
}

static uint32_t crc32_ieee(const uint8_t *data, uint16_t length) {
    uint32_t crc = 0xffffffffu;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static uint32_t allocate_id(void) {
    uint32_t id = m_next_id++;
    if (id == 0u) {
        id = m_next_id++;
    }
    return id;
}

static bool deadline_reached(uint32_t now, uint32_t deadline) {
    return app_timer_cnt_diff_compute(now, deadline) <
           ((APP_TIMER_MAX_CNT_VAL + 1u) / 2u);
}

static void finish(keyboard_payload_state_t state, keyboard_payload_error_t error) {
    keyboard_hid_abort();
    m_state = state;
    m_error = error;
    m_phase = INTERPRETER_FETCH;
    m_not_ready = false;
    m_waiting_run_response = false;
    m_auto_run = false;
    m_cancel_requested = false;
}

static bool finish_cancel_request(void) {
    if (!m_cancel_requested) {
        return false;
    }
    if (m_state == KEYBOARD_PAYLOAD_STATE_ARMED) {
        (void)ble_peripheral_name_set_temporary(NULL, 0u);
    }
    if (m_state == KEYBOARD_PAYLOAD_STATE_ARMED ||
        m_state == KEYBOARD_PAYLOAD_STATE_RUNNING) {
        finish(KEYBOARD_PAYLOAD_STATE_CANCELLED,
               (keyboard_payload_error_t)m_cancel_error);
        return true;
    }
    m_cancel_requested = false;
    return false;
}

static bool send_report(uint8_t modifiers, uint8_t usage) {
    if (finish_cancel_request()) {
        return false;
    }
    uint32_t error = keyboard_hid_send_report(m_outputs, modifiers, usage);
    if (error == NRF_SUCCESS) {
        return true;
    }
    if (error == NRF_ERROR_RESOURCES || error == NRF_ERROR_BUSY) {
        return false;
    }
    finish(KEYBOARD_PAYLOAD_STATE_ERROR, KEYBOARD_PAYLOAD_ERROR_HID_SEND);
    return false;
}

static keyboard_payload_error_t validate(const uint8_t *payload, uint16_t length) {
    uint16_t pc = 0u;
    uint16_t taps = 0u;
    uint32_t delays = 0u;

    while (pc < length) {
        uint8_t opcode = payload[pc];
        if (opcode == KEYBOARD_OPCODE_END) {
            return pc == length - 1u ? KEYBOARD_PAYLOAD_ERROR_NONE
                                     : KEYBOARD_PAYLOAD_ERROR_TRAILING_DATA;
        }
        if (opcode == KEYBOARD_OPCODE_DELAY) {
            if ((uint32_t)pc + 3u > length) {
                return KEYBOARD_PAYLOAD_ERROR_TRUNCATED_OPCODE;
            }
            uint16_t delay = read_u16be(&payload[pc + 1u]);
            if (delay == 0u || delay > KEYBOARD_MAX_DELAY_MS) {
                return KEYBOARD_PAYLOAD_ERROR_DELAY_RANGE;
            }
            delays += delay;
            if (delays > KEYBOARD_PAYLOAD_MAX_DELAY_MS) {
                return KEYBOARD_PAYLOAD_ERROR_DELAY_LIMIT;
            }
            pc += 3u;
            continue;
        }
        if (opcode == KEYBOARD_OPCODE_TAP) {
            if ((uint32_t)pc + 3u > length) {
                return KEYBOARD_PAYLOAD_ERROR_TRUNCATED_OPCODE;
            }
            if (payload[pc + 2u] < KEYBOARD_MIN_USAGE ||
                    payload[pc + 2u] > KEYBOARD_MAX_USAGE) {
                return KEYBOARD_PAYLOAD_ERROR_USAGE_RANGE;
            }
            if (++taps > KEYBOARD_PAYLOAD_MAX_TAPS) {
                return KEYBOARD_PAYLOAD_ERROR_TAP_LIMIT;
            }
            pc += 3u;
            continue;
        }
        return KEYBOARD_PAYLOAD_ERROR_UNKNOWN_OPCODE;
    }
    return KEYBOARD_PAYLOAD_ERROR_MISSING_END;
}

static bool outputs_ready(uint32_t now) {
    if (keyboard_hid_outputs_ready(m_outputs)) {
        m_not_ready = false;
        return true;
    }
    if (!m_not_ready) {
        m_not_ready = true;
        m_not_ready_since = now;
    } else if (app_timer_cnt_diff_compute(now, m_not_ready_since) >=
               APP_TIMER_TICKS(KEYBOARD_READY_TIMEOUT_MS)) {
        finish(KEYBOARD_PAYLOAD_STATE_ERROR,
               KEYBOARD_PAYLOAD_ERROR_OUTPUT_UNAVAILABLE);
    }
    return false;
}

keyboard_payload_result_t keyboard_payload_upload_begin(
    uint16_t total, uint32_t crc32, data_frame_transport_t transport,
    uint32_t *upload_id, uint16_t *next_offset) {
    if (total == 0u || total > KEYBOARD_PAYLOAD_MAX_LENGTH ||
            transport <= DATA_FRAME_TRANSPORT_NONE ||
            transport >= DATA_FRAME_TRANSPORT_COUNT || upload_id == NULL ||
            next_offset == NULL) {
        return KEYBOARD_PAYLOAD_RESULT_PARAMETER;
    }
    if (m_state == KEYBOARD_PAYLOAD_STATE_RUNNING ||
            m_state == KEYBOARD_PAYLOAD_STATE_ARMED) {
        return KEYBOARD_PAYLOAD_RESULT_STATE;
    }

    m_upload_buffer = m_commit_valid ? (uint8_t)(m_committed_buffer ^ 1u) : 0u;
    m_upload_transport = transport;
    m_expected = total;
    m_received = 0u;
    m_expected_crc = crc32;
    m_upload_id = allocate_id();
    m_upload_active = true;
    m_state = KEYBOARD_PAYLOAD_STATE_UPLOADING;
    m_error = KEYBOARD_PAYLOAD_ERROR_NONE;
    *upload_id = m_upload_id;
    *next_offset = 0u;
    return KEYBOARD_PAYLOAD_RESULT_OK;
}

keyboard_payload_result_t keyboard_payload_upload_chunk(
    uint32_t upload_id, uint16_t offset, const uint8_t *data, uint16_t length,
    data_frame_transport_t transport, uint16_t *next_offset) {
    if (data == NULL || length == 0u || next_offset == NULL) {
        return KEYBOARD_PAYLOAD_RESULT_PARAMETER;
    }
    if (!m_upload_active || upload_id != m_upload_id) {
        return KEYBOARD_PAYLOAD_RESULT_STATE;
    }
    if (transport != m_upload_transport) {
        return KEYBOARD_PAYLOAD_RESULT_TRANSPORT;
    }
    if ((uint32_t)offset + length > m_expected) {
        return KEYBOARD_PAYLOAD_RESULT_PARAMETER;
    }

    if (offset == m_received) {
        memcpy(&m_buffers[m_upload_buffer][offset], data, length);
        m_received += length;
    } else if (offset > m_received || (uint32_t)offset + length > m_received ||
               memcmp(&m_buffers[m_upload_buffer][offset], data, length) != 0) {
        return KEYBOARD_PAYLOAD_RESULT_PARAMETER;
    }

    *next_offset = m_received;
    return KEYBOARD_PAYLOAD_RESULT_OK;
}

keyboard_payload_result_t keyboard_payload_upload_commit(
    uint32_t upload_id, data_frame_transport_t transport,
    uint32_t *commit_id, uint16_t *length, uint32_t *crc32) {
    if (commit_id == NULL || length == NULL || crc32 == NULL) {
        return KEYBOARD_PAYLOAD_RESULT_PARAMETER;
    }
    if (!m_upload_active || upload_id != m_upload_id) {
        return KEYBOARD_PAYLOAD_RESULT_STATE;
    }
    if (transport != m_upload_transport) {
        return KEYBOARD_PAYLOAD_RESULT_TRANSPORT;
    }
    if (m_received != m_expected) {
        return KEYBOARD_PAYLOAD_RESULT_STATE;
    }

    uint32_t actual_crc = crc32_ieee(m_buffers[m_upload_buffer], m_expected);
    if (actual_crc != m_expected_crc) {
        m_upload_active = false;
        m_state = KEYBOARD_PAYLOAD_STATE_ERROR;
        m_error = KEYBOARD_PAYLOAD_ERROR_CRC;
        return KEYBOARD_PAYLOAD_RESULT_CRC;
    }
    keyboard_payload_error_t error = validate(m_buffers[m_upload_buffer], m_expected);
    if (error != KEYBOARD_PAYLOAD_ERROR_NONE) {
        m_upload_active = false;
        m_state = KEYBOARD_PAYLOAD_STATE_ERROR;
        m_error = error;
        return KEYBOARD_PAYLOAD_RESULT_FORMAT;
    }

    m_committed_buffer = m_upload_buffer;
    m_length = m_expected;
    m_committed_crc = actual_crc;
    m_commit_id = allocate_id();
    m_commit_valid = true;
    m_upload_active = false;
    m_pc = 0u;
    m_run_id = 0u;
    m_outputs = 0u;
    m_run_transport = DATA_FRAME_TRANSPORT_NONE;
    m_auto_run = false;
    m_state = KEYBOARD_PAYLOAD_STATE_READY;
    m_error = KEYBOARD_PAYLOAD_ERROR_NONE;
    *commit_id = m_commit_id;
    *length = m_length;
    *crc32 = m_committed_crc;
    return KEYBOARD_PAYLOAD_RESULT_OK;
}

keyboard_payload_result_t keyboard_payload_run(
    uint32_t commit_id, uint8_t outputs, data_frame_transport_t transport,
    uint32_t *run_id) {
    if (run_id == NULL || outputs < KEYBOARD_OUTPUT_USB ||
            outputs > KEYBOARD_OUTPUT_BOTH ||
            (transport != DATA_FRAME_TRANSPORT_USB &&
             transport != DATA_FRAME_TRANSPORT_BLE)) {
        return KEYBOARD_PAYLOAD_RESULT_PARAMETER;
    }
    if (!m_commit_valid || m_upload_active ||
            m_state == KEYBOARD_PAYLOAD_STATE_RUNNING ||
            m_state == KEYBOARD_PAYLOAD_STATE_ARMED ||
            commit_id != m_commit_id) {
        return KEYBOARD_PAYLOAD_RESULT_STATE;
    }
    if (!keyboard_hid_outputs_ready(outputs)) {
        return KEYBOARD_PAYLOAD_RESULT_UNAVAILABLE;
    }

    m_outputs = outputs;
    m_run_transport = transport;
    m_pc = 0u;
    m_run_id = allocate_id();
    m_phase = INTERPRETER_FETCH;
    m_error = KEYBOARD_PAYLOAD_ERROR_NONE;
    m_cancel_requested = false;
    m_not_ready = false;
    m_waiting_run_response = true;
    m_auto_run = false;
    m_state = KEYBOARD_PAYLOAD_STATE_RUNNING;
    *run_id = m_run_id;
    return KEYBOARD_PAYLOAD_RESULT_OK;
}

keyboard_payload_result_t keyboard_payload_arm_ble(
    uint32_t commit_id, uint32_t *run_id) {
    if (run_id == NULL) return KEYBOARD_PAYLOAD_RESULT_PARAMETER;
    if (!m_commit_valid || m_upload_active ||
            m_state == KEYBOARD_PAYLOAD_STATE_RUNNING ||
            m_state == KEYBOARD_PAYLOAD_STATE_ARMED ||
            commit_id != m_commit_id) {
        return KEYBOARD_PAYLOAD_RESULT_STATE;
    }

    m_outputs = KEYBOARD_OUTPUT_BLE;
    m_run_transport = DATA_FRAME_TRANSPORT_NONE;
    m_pc = 0u;
    m_run_id = allocate_id();
    m_phase = INTERPRETER_FETCH;
    m_error = KEYBOARD_PAYLOAD_ERROR_NONE;
    m_cancel_requested = false;
    m_not_ready = false;
    m_waiting_run_response = false;
    m_auto_run = false;
    m_state = KEYBOARD_PAYLOAD_STATE_ARMED;
    *run_id = m_run_id;
    return KEYBOARD_PAYLOAD_RESULT_OK;
}

void keyboard_payload_cancel(void) {
    if (m_state == KEYBOARD_PAYLOAD_STATE_RUNNING) {
        m_cancel_error = KEYBOARD_PAYLOAD_ERROR_CANCELLED;
        m_cancel_requested = true;
    } else if (m_state == KEYBOARD_PAYLOAD_STATE_ARMED) {
        (void)ble_peripheral_name_set_temporary(NULL, 0u);
        finish(KEYBOARD_PAYLOAD_STATE_CANCELLED,
               KEYBOARD_PAYLOAD_ERROR_CANCELLED);
    } else if (m_upload_active) {
        m_upload_active = false;
        m_received = 0u;
        m_expected = 0u;
        m_state = m_commit_valid ? KEYBOARD_PAYLOAD_STATE_READY
                                 : KEYBOARD_PAYLOAD_STATE_CANCELLED;
        m_error = KEYBOARD_PAYLOAD_ERROR_CANCELLED;
    }
}

void keyboard_payload_cancel_command_link(void) {
    if (m_state == KEYBOARD_PAYLOAD_STATE_RUNNING) {
        m_cancel_error = KEYBOARD_PAYLOAD_ERROR_COMMAND_LINK_LOST;
        m_cancel_requested = true;
    }
}

bool keyboard_payload_command_link_alive(void) {
    if (m_state == KEYBOARD_PAYLOAD_STATE_ARMED || m_auto_run) {
        return true;
    }
    if (m_state != KEYBOARD_PAYLOAD_STATE_RUNNING) {
        return true;
    }
    if (m_run_transport == DATA_FRAME_TRANSPORT_USB) {
        return is_usb_working();
    }
    if (m_run_transport == DATA_FRAME_TRANSPORT_BLE) {
        return ble_keyboard_link_authorized();
    }
    return false;
}

bool keyboard_payload_cancel_from_button(void) {
    if (m_state != KEYBOARD_PAYLOAD_STATE_ARMED &&
        m_state != KEYBOARD_PAYLOAD_STATE_RUNNING) {
        return false;
    }
    m_cancel_error = KEYBOARD_PAYLOAD_ERROR_CANCELLED;
    m_cancel_requested = true;
    return true;
}

keyboard_payload_result_t keyboard_payload_clear(void) {
    if (m_state == KEYBOARD_PAYLOAD_STATE_RUNNING) {
        return KEYBOARD_PAYLOAD_RESULT_STATE;
    }
    if (m_state == KEYBOARD_PAYLOAD_STATE_ARMED) {
        (void)ble_peripheral_name_set_temporary(NULL, 0u);
    }
    keyboard_hid_abort();
    memset(m_buffers, 0, sizeof(m_buffers));
    m_upload_active = false;
    m_commit_valid = false;
    m_upload_transport = DATA_FRAME_TRANSPORT_NONE;
    m_expected = 0u;
    m_received = 0u;
    m_length = 0u;
    m_pc = 0u;
    m_expected_crc = 0u;
    m_committed_crc = 0u;
    m_upload_id = 0u;
    m_commit_id = 0u;
    m_run_id = 0u;
    m_outputs = 0u;
    m_run_transport = DATA_FRAME_TRANSPORT_NONE;
    m_state = KEYBOARD_PAYLOAD_STATE_EMPTY;
    m_error = KEYBOARD_PAYLOAD_ERROR_NONE;
    m_waiting_run_response = false;
    m_auto_run = false;
    m_cancel_requested = false;
    return KEYBOARD_PAYLOAD_RESULT_OK;
}

void keyboard_payload_get_status(keyboard_payload_status_t *status) {
    if (status == NULL) {
        return;
    }
    status->state = m_state;
    status->error = m_error;
    status->outputs = m_outputs;
    status->upload_id = m_upload_id;
    status->commit_id = m_commit_id;
    status->run_id = m_run_id;
    status->expected = m_expected;
    status->received = m_received;
    status->pc = m_pc;
    status->length = m_length;
    status->crc32 = m_committed_crc;
}

bool keyboard_payload_is_running(void) {
    return m_state == KEYBOARD_PAYLOAD_STATE_RUNNING ||
           m_state == KEYBOARD_PAYLOAD_STATE_ARMED;
}

bool keyboard_payload_is_armed(void) {
    return m_state == KEYBOARD_PAYLOAD_STATE_ARMED;
}

void keyboard_payload_process(void) {
    if (finish_cancel_request()) {
        return;
    }
    if (m_state == KEYBOARD_PAYLOAD_STATE_ARMED) {
        sleep_timer_stop();
        if (!keyboard_hid_ble_ready()) return;
        if (finish_cancel_request()) return;
        (void)ble_peripheral_name_set_temporary(NULL, 0u);
        uint32_t now = app_timer_cnt_get();
        m_state = KEYBOARD_PAYLOAD_STATE_RUNNING;
        m_auto_run = true;
        m_transition_deadline = now + APP_TIMER_TICKS(KEYBOARD_RUN_START_DELAY_MS);
        return;
    }
    if (m_state != KEYBOARD_PAYLOAD_STATE_RUNNING) {
        return;
    }
    uint32_t now = app_timer_cnt_get();
    const uint8_t *payload = m_buffers[m_committed_buffer];
    if (!outputs_ready(now)) {
        return;
    }
    if (m_waiting_run_response) {
        bool response_sent = m_run_transport == DATA_FRAME_TRANSPORT_USB
                                 ? is_usb_tx_idle()
                                 : is_nus_tx_idle();
        if (!response_sent) {
            return;
        }
        m_waiting_run_response = false;
        m_transition_deadline = now + APP_TIMER_TICKS(KEYBOARD_RUN_START_DELAY_MS);
        return;
    }

    if (m_phase == INTERPRETER_DELAY) {
        if (!deadline_reached(now, m_delay_deadline)) {
            return;
        }
        m_phase = INTERPRETER_FETCH;
    }

    if (m_phase == INTERPRETER_RELEASE) {
        if (!deadline_reached(now, m_transition_deadline)) {
            return;
        }
        if (!send_report(0u, 0u)) {
            return;
        }
        m_pc += 3u;
        m_transition_deadline = now + APP_TIMER_TICKS(KEYBOARD_TRANSITION_MS);
        m_phase = INTERPRETER_FETCH;
        return;
    }

    uint8_t opcode = payload[m_pc];
    if (opcode == KEYBOARD_OPCODE_END) {
        finish(KEYBOARD_PAYLOAD_STATE_COMPLETE, KEYBOARD_PAYLOAD_ERROR_NONE);
        return;
    }
    if (opcode == KEYBOARD_OPCODE_DELAY) {
        uint16_t delay = read_u16be(&payload[m_pc + 1u]);
        m_pc += 3u;
        m_delay_deadline = now + APP_TIMER_TICKS(delay);
        m_phase = INTERPRETER_DELAY;
        return;
    }

    if (!deadline_reached(now, m_transition_deadline)) {
        return;
    }
    m_tap_modifier = payload[m_pc + 1u];
    m_tap_usage = payload[m_pc + 2u];
    if (!send_report(m_tap_modifier, m_tap_usage)) {
        return;
    }
    m_transition_deadline = now + APP_TIMER_TICKS(KEYBOARD_TRANSITION_MS);
    m_phase = INTERPRETER_RELEASE;
}
