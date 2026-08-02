#ifndef ISO_DEP_SESSION_H
#define ISO_DEP_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "dataframe.h"
#include "iso_dep_reader.h"

typedef enum {
    ISO_DEP_SESSION_EXCHANGE_OK = 0,
    ISO_DEP_SESSION_EXCHANGE_INVALID,
    ISO_DEP_SESSION_EXCHANGE_RF_ERROR,
} iso_dep_session_exchange_status_t;

uint8_t iso_dep_session_start(picc_14a_tag_t *tag, uint32_t *session_id);
uint8_t iso_dep_session_start_apple_transit(picc_14a_tag_t *tag,
                                            uint32_t *session_id);
iso_dep_session_exchange_status_t iso_dep_session_exchange(
    uint32_t session_id, const uint8_t *apdu, uint16_t apdu_len,
    uint8_t *response, uint16_t response_capacity, iso_dep_result_t *result);
bool iso_dep_session_stop(uint32_t session_id);
void iso_dep_session_abort(void);
void iso_dep_session_owner_disconnected(data_frame_transport_t transport);
void iso_dep_session_process(void);
bool iso_dep_session_is_active(void);
bool iso_dep_session_is_current_transport_owner(void);

#endif
