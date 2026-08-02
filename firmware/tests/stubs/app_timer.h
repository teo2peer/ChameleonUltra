#ifndef APP_TIMER_H
#define APP_TIMER_H

#include <stdint.h>

#define APP_TIMER_MAX_CNT_VAL 0x00ffffffu
#define APP_TIMER_TICKS(milliseconds) ((uint32_t)(milliseconds))

uint32_t app_timer_cnt_get(void);
uint32_t app_timer_cnt_diff_compute(uint32_t ticks_to, uint32_t ticks_from);

#endif
