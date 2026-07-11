#include "timeslot.h"

#include <stdbool.h>

#include "app_timer.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_soc.h"

#define NRF_LOG_MODULE_NAME timeslot
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

#define TIMESLOT_DEFAULT_TIMEOUT_MS 1000u
#define TIMESLOT_MAX_TIMEOUT_MS 127000u

typedef enum {
    TIMESLOT_IDLE,
    TIMESLOT_WAITING,
    TIMESLOT_RUNNING,
    TIMESLOT_DONE,
    TIMESLOT_FAILED,
} timeslot_state_t;

static nrf_radio_request_t m_request;
static nrf_radio_signal_callback_return_param_t m_return_param;
static timeslot_callback_t m_callback;
static volatile timeslot_state_t m_state = TIMESLOT_IDLE;
static volatile ret_code_t m_result = NRF_SUCCESS;
static volatile bool m_session_open;
static volatile bool m_cancel_requested;

static bool deadline_expired(uint32_t started, uint32_t timeout_ms) {
    return app_timer_cnt_diff_compute(app_timer_cnt_get(), started) >= APP_TIMER_TICKS(timeout_ms);
}

static void timeslot_soc_evt_handler(uint32_t evt_id, void *p_context) {
    (void)p_context;

    switch (evt_id) {
        case NRF_EVT_RADIO_SIGNAL_CALLBACK_INVALID_RETURN:
            m_result = NRF_ERROR_INTERNAL;
            m_state = TIMESLOT_FAILED;
            break;
        case NRF_EVT_RADIO_BLOCKED:
        case NRF_EVT_RADIO_CANCELED:
            m_result = NRF_ERROR_BUSY;
            m_state = TIMESLOT_FAILED;
            break;
        case NRF_EVT_RADIO_SESSION_CLOSED:
            m_session_open = false;
            if (m_state == TIMESLOT_FAILED && m_callback == NULL) {
                m_state = TIMESLOT_IDLE;
            }
            break;
        default:
            break;
    }
}
NRF_SDH_SOC_OBSERVER(m_sys_obs, 0, timeslot_soc_evt_handler, NULL);

static nrf_radio_signal_callback_return_param_t *radio_callback(uint8_t signal_type) {
    m_return_param.params.request.p_next = NULL;
    m_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE;

    if (signal_type != NRF_RADIO_CALLBACK_SIGNAL_TYPE_START) {
        return &m_return_param;
    }

    m_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_END;
    if (m_state != TIMESLOT_WAITING || m_cancel_requested) {
        m_result = NRF_ERROR_TIMEOUT;
        m_state = TIMESLOT_FAILED;
        return &m_return_param;
    }

    m_state = TIMESLOT_RUNNING;
    timeslot_callback_t callback = m_callback;
    if (callback != NULL) {
        callback();
    }
    m_callback = NULL;
    m_result = NRF_SUCCESS;
    m_state = TIMESLOT_DONE;
    return &m_return_param;
}

ret_code_t request_timeslot_timeout(uint32_t time_us, timeslot_callback_t callback, uint32_t timeout_ms) {
    if (callback == NULL || timeout_ms == 0 || timeout_ms > TIMESLOT_MAX_TIMEOUT_MS ||
            time_us < NRF_RADIO_LENGTH_MIN_US || time_us > NRF_RADIO_LENGTH_MAX_US) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (m_state != TIMESLOT_IDLE) {
        return NRF_ERROR_BUSY;
    }

    m_request.request_type = NRF_RADIO_REQ_TYPE_EARLIEST;
    m_request.params.earliest.hfclk = NRF_RADIO_HFCLK_CFG_NO_GUARANTEE;
    m_request.params.earliest.priority = NRF_RADIO_PRIORITY_HIGH;
    m_request.params.earliest.length_us = time_us;
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000u;
    m_request.params.earliest.timeout_us = timeout_us > NRF_RADIO_EARLIEST_TIMEOUT_MAX_US
                                           ? NRF_RADIO_EARLIEST_TIMEOUT_MAX_US
                                           : (uint32_t)timeout_us;

    m_callback = callback;
    m_cancel_requested = false;
    m_result = NRF_SUCCESS;
    m_state = TIMESLOT_WAITING;

    ret_code_t err = sd_radio_session_open(radio_callback);
    if (err != NRF_SUCCESS) {
        m_callback = NULL;
        m_state = TIMESLOT_IDLE;
        return err;
    }
    m_session_open = true;

    err = sd_radio_request(&m_request);
    if (err != NRF_SUCCESS) {
        m_result = err;
        m_state = TIMESLOT_FAILED;
    }

    uint32_t started = app_timer_cnt_get();
    while (m_state == TIMESLOT_WAITING) {
        if (deadline_expired(started, timeout_ms)) {
            m_cancel_requested = true;
            m_result = NRF_ERROR_TIMEOUT;
            m_state = TIMESLOT_FAILED;
            break;
        }
        NRF_LOG_PROCESS();
    }

    ret_code_t result = m_result;
    err = sd_radio_session_close();
    if (err != NRF_SUCCESS && result == NRF_SUCCESS) {
        result = err;
    }

    started = app_timer_cnt_get();
    while (m_session_open && !deadline_expired(started, timeout_ms)) {
        NRF_LOG_PROCESS();
    }
    if (m_session_open) {
        m_callback = NULL;
        m_cancel_requested = true;
        m_state = TIMESLOT_FAILED;
        return result == NRF_SUCCESS ? NRF_ERROR_TIMEOUT : result;
    }

    m_callback = NULL;
    m_cancel_requested = false;
    m_state = TIMESLOT_IDLE;
    return result;
}

ret_code_t request_timeslot(uint32_t time_us, timeslot_callback_t callback) {
    return request_timeslot_timeout(time_us, callback, TIMESLOT_DEFAULT_TIMEOUT_MS);
}

ret_code_t timeslot_cancel(void) {
    if (m_state != TIMESLOT_WAITING) {
        return NRF_ERROR_INVALID_STATE;
    }
    m_cancel_requested = true;
    m_result = NRF_ERROR_TIMEOUT;
    m_state = TIMESLOT_FAILED;
    return NRF_SUCCESS;
}

ret_code_t timeslot_start(uint32_t time_ms) {
    (void)time_ms;
    return NRF_ERROR_NOT_SUPPORTED;
}

ret_code_t timeslot_stop(void) {
    return timeslot_cancel();
}
