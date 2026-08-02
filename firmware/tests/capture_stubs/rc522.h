#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*pcd_14a_trace_cb_t)(bool, const uint8_t *, uint16_t, uint8_t);

void pcd_14a_reader_capture_set(pcd_14a_trace_cb_t callback);
void pcd_14a_reader_capture_clear(void);
