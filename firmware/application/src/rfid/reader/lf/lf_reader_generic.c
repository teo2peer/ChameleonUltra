#include "lf_reader_generic.h"
#include "lf_reader_data.h"

#include "bsp_delay.h"
#include "bsp_wdt.h"
#include "bsp_time.h"
#include "circular_buffer.h"
#include "lf_125khz_radio.h"
#include "lf_reader_data.h"
#include "protocols/protocols.h"

#define NRF_LOG_MODULE_NAME lfgeneric
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

/*
 * Circular buffer for SAADC samples.
 * One SAADC completion contains 2048 samples. The queue must hold complete
 * callback batches or every callback creates a discontinuity.
 */
#define CIRCULAR_BUFFER_SIZE (6144)
static circular_buffer cb;
static nrf_saadc_value_t m_sample_storage[CIRCULAR_BUFFER_SIZE + 1];

static void saadc_cb(nrf_saadc_value_t *vals, size_t size) {
    for (int i = 0; i < size; i++) {
        nrf_saadc_value_t val = vals[i];
        if (!cb_push_back(&cb, &val)) {
            return;
        }
    }
}

static void init_saadc_hw(void) {
    lf_125khz_radio_saadc_enable(saadc_cb);
}

static void uninit_saadc_hw(void) {
    lf_125khz_radio_saadc_disable();
}

bool raw_read_to_buffer(uint8_t *data, size_t maxlen, uint32_t timeout_ms, size_t *outlen) {
    if (data == NULL || outlen == NULL || maxlen == 0) {
        return false;
    }
    *outlen = 0;
    if (!cb_init_static(&cb, m_sample_storage, CIRCULAR_BUFFER_SIZE,
                        sizeof(nrf_saadc_value_t))) {
        return false;
    }
    init_saadc_hw();
    start_lf_125khz_radio();

    /* Wait for antenna to settle before capturing.
     * The LC circuit rings for ~400µs on field startup, then takes
     * another ~800µs to reach steady state. Skip 2ms to be safe. */
    bsp_delay_ms(2);

    autotimer *p_at = bsp_obtain_timer(0);
    while (NO_TIMEOUT_1MS(p_at, timeout_ms) && *outlen < maxlen) {
        nrf_saadc_value_t val = 0;
        while (cb_pop_front(&cb, &val) && *outlen < maxlen) {
            int32_t scaled = val > 0 ? ((int32_t)val >> 5) : 0;
            data[*outlen] = scaled > 0xff ? 0xff : (uint8_t)scaled;
            ++(*outlen);
        }
        bsp_wdt_feed();  /* prevent watchdog reset during long captures */
    }

    bsp_return_timer(p_at);
    stop_lf_125khz_radio();
    uninit_saadc_hw();
    bool complete = cb_dropped(&cb) == 0;
    cb_free(&cb);

    return complete;
}
