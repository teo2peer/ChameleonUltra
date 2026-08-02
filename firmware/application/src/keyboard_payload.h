#ifndef KEYBOARD_PAYLOAD_H
#define KEYBOARD_PAYLOAD_H

#include <stdbool.h>
#include <stdint.h>

#include "dataframe.h"

#define KEYBOARD_PAYLOAD_VERSION          1u
#define KEYBOARD_PAYLOAD_MAX_LENGTH       4096u
#define KEYBOARD_PAYLOAD_MAX_TAPS         1024u
#define KEYBOARD_PAYLOAD_MAX_DELAY_MS     60000u

#define KEYBOARD_OUTPUT_USB               0x01u
#define KEYBOARD_OUTPUT_BLE               0x02u
#define KEYBOARD_OUTPUT_BOTH              (KEYBOARD_OUTPUT_USB | KEYBOARD_OUTPUT_BLE)

typedef enum {
    KEYBOARD_PAYLOAD_STATE_EMPTY = 0,
    KEYBOARD_PAYLOAD_STATE_UPLOADING = 1,
    KEYBOARD_PAYLOAD_STATE_READY = 2,
    KEYBOARD_PAYLOAD_STATE_RUNNING = 3,
    KEYBOARD_PAYLOAD_STATE_COMPLETE = 4,
    KEYBOARD_PAYLOAD_STATE_CANCELLED = 5,
    KEYBOARD_PAYLOAD_STATE_ERROR = 6,
    KEYBOARD_PAYLOAD_STATE_ARMED = 7,
} keyboard_payload_state_t;

typedef enum {
    KEYBOARD_PAYLOAD_ERROR_NONE = 0,
    KEYBOARD_PAYLOAD_ERROR_CANCELLED = 1,
    KEYBOARD_PAYLOAD_ERROR_COMMAND_LINK_LOST = 2,
    KEYBOARD_PAYLOAD_ERROR_OUTPUT_UNAVAILABLE = 3,
    KEYBOARD_PAYLOAD_ERROR_CRC = 4,
    KEYBOARD_PAYLOAD_ERROR_MISSING_END = 5,
    KEYBOARD_PAYLOAD_ERROR_TRAILING_DATA = 6,
    KEYBOARD_PAYLOAD_ERROR_TRUNCATED_OPCODE = 7,
    KEYBOARD_PAYLOAD_ERROR_UNKNOWN_OPCODE = 8,
    KEYBOARD_PAYLOAD_ERROR_DELAY_RANGE = 9,
    KEYBOARD_PAYLOAD_ERROR_DELAY_LIMIT = 10,
    KEYBOARD_PAYLOAD_ERROR_USAGE_RANGE = 11,
    KEYBOARD_PAYLOAD_ERROR_TAP_LIMIT = 12,
    KEYBOARD_PAYLOAD_ERROR_HID_SEND = 13,
} keyboard_payload_error_t;

typedef enum {
    KEYBOARD_PAYLOAD_RESULT_OK = 0,
    KEYBOARD_PAYLOAD_RESULT_PARAMETER,
    KEYBOARD_PAYLOAD_RESULT_STATE,
    KEYBOARD_PAYLOAD_RESULT_TRANSPORT,
    KEYBOARD_PAYLOAD_RESULT_CRC,
    KEYBOARD_PAYLOAD_RESULT_FORMAT,
    KEYBOARD_PAYLOAD_RESULT_UNAVAILABLE,
} keyboard_payload_result_t;

/* Fixed 28-byte GET_STATUS payload, with all multi-byte fields big-endian:
 * version[1] | state[1] | error[1] | outputs[1] |
 * upload_id[4] | commit_id[4] | run_id[4] |
 * expected[2] | received[2] | pc[2] | length[2] | crc32[4]. */
typedef struct {
    uint8_t state;
    uint8_t error;
    uint8_t outputs;
    uint32_t upload_id;
    uint32_t commit_id;
    uint32_t run_id;
    uint16_t expected;
    uint16_t received;
    uint16_t pc;
    uint16_t length;
    uint32_t crc32;
} keyboard_payload_status_t;

keyboard_payload_result_t keyboard_payload_upload_begin(
    uint16_t total, uint32_t crc32, data_frame_transport_t transport,
    uint32_t *upload_id, uint16_t *next_offset);
keyboard_payload_result_t keyboard_payload_upload_chunk(
    uint32_t upload_id, uint16_t offset, const uint8_t *data, uint16_t length,
    data_frame_transport_t transport, uint16_t *next_offset);
keyboard_payload_result_t keyboard_payload_upload_commit(
    uint32_t upload_id, data_frame_transport_t transport,
    uint32_t *commit_id, uint16_t *length, uint32_t *crc32);
keyboard_payload_result_t keyboard_payload_run(
    uint32_t commit_id, uint8_t outputs, data_frame_transport_t transport,
    uint32_t *run_id);
keyboard_payload_result_t keyboard_payload_arm_ble(
    uint32_t commit_id, uint32_t *run_id);
void keyboard_payload_cancel(void);
void keyboard_payload_cancel_command_link(void);
bool keyboard_payload_command_link_alive(void);
bool keyboard_payload_cancel_from_button(void);
keyboard_payload_result_t keyboard_payload_clear(void);
void keyboard_payload_get_status(keyboard_payload_status_t *status);
bool keyboard_payload_is_running(void);
bool keyboard_payload_is_armed(void);
void keyboard_payload_process(void);

#endif
