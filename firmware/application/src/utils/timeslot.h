#ifndef TIME_SLOT_H__
#define TIME_SLOT_H__

#include <stdint.h>

#include "sdk_errors.h"

typedef void (*timeslot_callback_t)(void);

/* The callback runs at SoftDevice signal-callback priority and must finish
 * within time_us. It must not call SoftDevice APIs. */
ret_code_t request_timeslot_timeout(uint32_t time_us, timeslot_callback_t callback, uint32_t timeout_ms);
ret_code_t request_timeslot(uint32_t time_us, timeslot_callback_t callback);
ret_code_t timeslot_cancel(void);

/* Split start/stop cannot safely represent a SoftDevice timeslot. */
ret_code_t timeslot_start(uint32_t time_ms);
ret_code_t timeslot_stop(void);

#endif
