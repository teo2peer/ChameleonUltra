#ifndef EMV_TRACE_H
#define EMV_TRACE_H

#include <stdint.h>

#define EMV_TRACE_PROTOCOL_VERSION 1u
#define EMV_TRACE_START_REQUEST_SIZE 25u
#define EMV_TRACE_START_REQUEST_PROFILE_SIZE 30u
#define EMV_TRACE_START_REQUEST_BEHAVIOR_SIZE 35u

#define EMV_TRACE_OPT_MAX_PROCESSING 0x01u
#define EMV_TRACE_OPT_INCLUDE_RF     0x02u
#define EMV_TRACE_OPT_TIMING         0x04u
#define EMV_TRACE_OPT_RECORD_GRID    0x08u
#define EMV_TRACE_OPT_TRANSACTION_LOG 0x10u
#define EMV_TRACE_OPT_PDOL_FALLBACK  0x20u
#define EMV_TRACE_OPT_EXPRESS_TRANSIT 0x40u
#define EMV_TRACE_OPT_TERMINAL_PROFILE 0x80u

#define EMV_TERMINAL_PROFILE_AUTO             0x00u
#define EMV_TERMINAL_PROFILE_APPLE_TRANSIT    0x01u
#define EMV_TERMINAL_PROFILE_ONLINE_NO_ODA    0x02u
#define EMV_TERMINAL_PROFILE_BROAD_MOBILE     0x03u
#define EMV_TERMINAL_PROFILE_QVSDC_ONLINE     0x04u
#define EMV_TERMINAL_PROFILE_MINIMAL_ONLINE   0x05u
#define EMV_TERMINAL_PROFILE_MSD_QVSDC        0x06u
#define EMV_TERMINAL_PROFILE_CUSTOM           0xFEu
#define EMV_TERMINAL_PROFILE_SWEEP            0xFFu

#define EMV_TRACE_BEHAVIOR_DIRECT_AID_FALLBACK 0x01u
#define EMV_TRACE_BEHAVIOR_ADAPTIVE_PROFILES   0x02u
#define EMV_TRACE_BEHAVIOR_REACQUIRE_PROFILES  0x04u
#define EMV_TRACE_BEHAVIOR_ALL                 0x07u

#define EMV_POLLING_PROFILE_DEFAULT  0x00u
#define EMV_POLLING_PROFILE_FAST     0x01u
#define EMV_POLLING_PROFILE_BALANCED 0x02u
#define EMV_POLLING_PROFILE_PATIENT  0x03u

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
