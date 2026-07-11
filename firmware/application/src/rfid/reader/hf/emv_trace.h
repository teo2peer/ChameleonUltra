#ifndef EMV_TRACE_H
#define EMV_TRACE_H

#include <stdint.h>

#define EMV_TRACE_PROTOCOL_VERSION 1u
#define EMV_TRACE_START_REQUEST_SIZE 25u

#define EMV_TRACE_OPT_MAX_PROCESSING 0x01u
#define EMV_TRACE_OPT_INCLUDE_RF     0x02u
#define EMV_TRACE_OPT_TIMING         0x04u
#define EMV_TRACE_OPT_RECORD_GRID    0x08u
#define EMV_TRACE_OPT_TRANSACTION_LOG 0x10u
#define EMV_TRACE_OPT_PDOL_FALLBACK  0x20u

uint16_t emv_trace_start(const uint8_t *request, uint16_t request_length,
                         uint8_t *response, uint16_t response_capacity,
                         uint16_t *status);
uint16_t emv_trace_meta(const uint8_t *request, uint16_t request_length,
                        uint8_t *response, uint16_t response_capacity,
                        uint16_t *status);
uint16_t emv_trace_get(const uint8_t *request, uint16_t request_length,
                       uint8_t *response, uint16_t response_capacity,
                       uint16_t *status);

#endif
