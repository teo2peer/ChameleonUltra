#ifndef HF_CAPTURE_H
#define HF_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "dataframe.h"

#define HF_CAPTURE_PROTOCOL_VERSION 2u
#define HF_CAPTURE_RING_CAPACITY 8192u
#define HF_CAPTURE_META_SIZE 48u
#define HF_CAPTURE_PAGE_HEADER_SIZE 72u
#define HF_CAPTURE_MAX_RECORD_DATA_SIZE 511u
#define HF_CAPTURE_MIN_PAGE_SIZE \
    (HF_CAPTURE_PAGE_HEADER_SIZE + 22u + HF_CAPTURE_MAX_RECORD_DATA_SIZE)

typedef enum {
    HF_CAPTURE_MODE_EMULATION = 0,
    HF_CAPTURE_MODE_PASSIVE = 1,
    HF_CAPTURE_MODE_READER = 2,
} hf_capture_mode_t;

typedef enum {
    HF_CAPTURE_RESULT_OK = 0,
    HF_CAPTURE_RESULT_INVALID,
    HF_CAPTURE_RESULT_BUSY,
    HF_CAPTURE_RESULT_SESSION,
} hf_capture_result_t;

typedef enum {
    HF_CAPTURE_DIRECTION_READER_TO_CARD = 0,
    HF_CAPTURE_DIRECTION_CARD_TO_READER = 1,
    HF_CAPTURE_DIRECTION_EVENT = 2,
} hf_capture_direction_t;

#define HF_CAPTURE_FRAME_FLAG_PARITY_PACKED 0x01u
#define HF_CAPTURE_FRAME_FLAG_CRC_AUTO       0x02u
#define HF_CAPTURE_FRAME_FLAG_RF_ERROR       0x04u

hf_capture_result_t hf_capture_start(hf_capture_mode_t mode,
                                     data_frame_transport_t owner,
                                     uint32_t start_token,
                                     uint32_t *session_id);
hf_capture_result_t hf_capture_resume(uint32_t session_id,
                                      uint32_t start_token,
                                      data_frame_transport_t owner);
void hf_capture_owner_disconnected(data_frame_transport_t owner);
hf_capture_result_t hf_capture_stop(uint32_t session_id,
                                    data_frame_transport_t owner);
hf_capture_result_t hf_capture_get(uint32_t session_id, bool acknowledge_present,
                                   uint32_t ack_sequence,
                                   uint64_t ack_delivery_token,
                                   data_frame_transport_t owner,
                                   uint16_t requested_bytes, uint8_t *response,
                                   uint16_t response_capacity,
                                   uint16_t *response_length);

uint16_t hf_capture_build_meta(uint32_t session_id, uint8_t *response,
                               uint16_t response_capacity);
uint32_t hf_capture_session_id(void);
data_frame_transport_t hf_capture_owner(void);
bool hf_capture_is_active(void);
bool hf_capture_prevents_system_off(void);
bool hf_capture_notification_due(void);
void hf_capture_notification_sent(void);
void hf_capture_clock_process(void);

#if defined(HF_CAPTURE_TEST)
void hf_capture_test_set_next_sequence(uint32_t sequence);
#endif

#endif
