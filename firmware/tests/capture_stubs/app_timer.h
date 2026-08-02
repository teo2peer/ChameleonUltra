#pragma once

#include <stdint.h>

#define APP_TIMER_TICKS(value) (value)

uint32_t app_timer_cnt_get(void);
uint32_t app_timer_cnt_diff_compute(uint32_t ticks_to, uint32_t ticks_from);
