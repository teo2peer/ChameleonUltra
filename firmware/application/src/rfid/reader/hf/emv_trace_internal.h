#ifndef EMV_TRACE_INTERNAL_H
#define EMV_TRACE_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static inline bool emv_trace_same_uid(
    const uint8_t *expected,
    uint8_t expected_length,
    const uint8_t *observed,
    uint8_t observed_length
) {
    return expected_length == observed_length &&
           memcmp(expected, observed, expected_length) == 0;
}

#endif
