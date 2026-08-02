#include "dataframe.h"
#include "netdata.h"

#include <stddef.h>
#include <string.h>
#include "app_util_platform.h"

#define NRF_LOG_MODULE_NAME data_frame
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

#define DATA_FRAME_REQUEST_QUEUE_DEPTH 2

typedef struct {
    netdata_frame_raw_t frame;
    uint16_t length;
    uint16_t payload_length;
    bool complete;
    volatile bool receiving;
    volatile uint32_t generation;
} data_frame_decoder_t;

typedef struct {
    volatile bool reserved;
    volatile bool valid;
    volatile bool publishing;
    uint32_t sequence;
    uint32_t generation;
    data_frame_transport_t transport;
    uint16_t cmd;
    uint16_t status;
    uint16_t length;
    uint8_t data[NETDATA_MAX_DATA_LENGTH];
} data_frame_request_t;

static data_frame_decoder_t m_decoders[DATA_FRAME_TRANSPORT_COUNT - 1];
static data_frame_request_t m_requests[DATA_FRAME_REQUEST_QUEUE_DEPTH];
static uint32_t m_request_sequence;
static data_frame_transport_t m_current_transport = DATA_FRAME_TRANSPORT_NONE;
static data_frame_request_t *m_processing_request;
static void (*m_flow_callbacks[DATA_FRAME_TRANSPORT_COUNT])(void);
static bool (*m_ready_callbacks[DATA_FRAME_TRANSPORT_COUNT])(void);

static netdata_frame_raw_t m_netdata_frame_tx_buf;
static data_frame_tx_t m_frame_tx_buf_info = {
    .buffer = (uint8_t *)&m_netdata_frame_tx_buf,
};
static data_frame_cbk_t m_frame_process_cbk;

static uint8_t compute_lrc(const uint8_t *buf, uint16_t bufsize) {
    uint8_t lrc = 0;
    for (uint16_t i = 0; i < bufsize; i++) {
        lrc += buf[i];
    }
    return (uint8_t)(0x100 - lrc);
}

static data_frame_decoder_t *decoder_for_transport(data_frame_transport_t transport) {
    if (transport <= DATA_FRAME_TRANSPORT_NONE || transport >= DATA_FRAME_TRANSPORT_COUNT) {
        return NULL;
    }
    return &m_decoders[transport - 1];
}

static data_frame_request_t *reserve_request(data_frame_transport_t transport,
                                             uint32_t generation) {
    data_frame_request_t *request = NULL;
    CRITICAL_REGION_ENTER();
    for (uint8_t i = 0; i < DATA_FRAME_REQUEST_QUEUE_DEPTH; i++) {
        if (!m_requests[i].reserved) {
            request = &m_requests[i];
            request->reserved = true;
            request->valid = false;
            request->publishing = true;
            request->transport = transport;
            request->generation = generation;
            request->sequence = m_request_sequence++;
            break;
        }
    }
    CRITICAL_REGION_EXIT();
    return request;
}

static bool queue_decoder(data_frame_transport_t transport, data_frame_decoder_t *decoder,
                          uint32_t generation) {
    data_frame_request_t *request = reserve_request(transport, generation);
    if (request == NULL) {
        CRITICAL_REGION_ENTER();
        if (decoder->generation == generation) {
            decoder->complete = true;
        }
        CRITICAL_REGION_EXIT();
        return false;
    }

    request->cmd = U16NTOHS(decoder->frame.pre.cmd);
    request->status = U16NTOHS(decoder->frame.pre.status);
    request->length = decoder->payload_length;
    if (request->length != 0) {
        memcpy(request->data, decoder->frame.data, request->length);
    }
    bool current;
    CRITICAL_REGION_ENTER();
    current = decoder->generation == generation;
    if (current && request->reserved && request->publishing &&
            request->generation == generation) {
        request->publishing = false;
        __DMB();
        request->valid = true;
    } else {
        request->valid = false;
        request->publishing = false;
        request->reserved = false;
    }
    if (current) {
        decoder->length = 0;
        decoder->payload_length = 0;
        decoder->complete = false;
    }
    CRITICAL_REGION_EXIT();
    return current;
}

static uint16_t receive_finish(data_frame_decoder_t *decoder, uint32_t generation,
                               uint16_t consumed, uint16_t input_length) {
    CRITICAL_REGION_ENTER();
    if (decoder->generation != generation) {
        decoder->length = 0;
        decoder->payload_length = 0;
        decoder->complete = false;
        consumed = input_length;
    }
    decoder->receiving = false;
    CRITICAL_REGION_EXIT();
    return consumed;
}

static bool decoder_prefix_valid(data_frame_decoder_t *decoder, bool *frame_complete) {
    uint8_t *raw = (uint8_t *)&decoder->frame;
    *frame_complete = false;

    if (raw[0] != NETDATA_FRAME_SOF) {
        return false;
    }
    if (decoder->length >= 2 &&
            decoder->frame.pre.lrc1 != compute_lrc(raw, offsetof(netdata_frame_preamble_t, lrc1))) {
        return false;
    }
    if (decoder->length < sizeof(netdata_frame_preamble_t)) {
        return true;
    }
    if (decoder->frame.pre.lrc2 != compute_lrc(raw, offsetof(netdata_frame_preamble_t, lrc2))) {
        return false;
    }

    decoder->payload_length = U16NTOHS(decoder->frame.pre.len);
    if (decoder->payload_length > NETDATA_MAX_DATA_LENGTH) {
        return false;
    }

    uint16_t frame_length = (uint16_t)(NETDATA_FRAME_OVERHEAD + decoder->payload_length);
    if (decoder->length > frame_length) {
        return false;
    }
    if (decoder->length == frame_length) {
        netdata_frame_postamble_t *post = (netdata_frame_postamble_t *)(raw +
                                                sizeof(netdata_frame_preamble_t) +
                                                decoder->payload_length);
        if (post->lrc3 != compute_lrc(decoder->frame.data, decoder->payload_length)) {
            return false;
        }
        *frame_complete = true;
    }
    return true;
}

static void decoder_resynchronize(data_frame_decoder_t *decoder) {
    uint8_t *raw = (uint8_t *)&decoder->frame;

    while (decoder->length != 0) {
        uint16_t next = 1;
        while (next < decoder->length && raw[next] != NETDATA_FRAME_SOF) {
            next++;
        }
        if (next == decoder->length) {
            decoder->length = 0;
            decoder->payload_length = 0;
            decoder->complete = false;
            return;
        }

        decoder->length -= next;
        memmove(raw, raw + next, decoder->length);
        bool complete;
        if (decoder_prefix_valid(decoder, &complete)) {
            decoder->complete = complete;
            return;
        }
    }
}

static uint16_t receive_from_generation(const uint8_t *data, uint16_t length,
                                        data_frame_transport_t transport,
                                        bool require_generation,
                                        uint32_t expected_generation) {
    data_frame_decoder_t *decoder = decoder_for_transport(transport);
    if (decoder == NULL || (data == NULL && length != 0)) {
        return 0;
    }
    uint32_t generation = 0;
    uint16_t consumed = 0;
    bool can_receive = false;
    CRITICAL_REGION_ENTER();
    if (!decoder->receiving) {
        if (require_generation && decoder->generation != expected_generation) {
            consumed = length;
        } else {
            decoder->receiving = true;
            generation = decoder->generation;
            can_receive = true;
        }
    }
    CRITICAL_REGION_EXIT();
    if (!can_receive) {
        return consumed;
    }

    if (decoder->complete && !queue_decoder(transport, decoder, generation)) {
        return receive_finish(decoder, generation, 0, length);
    }

    while (consumed < length) {
        if (decoder->length >= sizeof(decoder->frame)) {
            decoder_resynchronize(decoder);
            if (decoder->length >= sizeof(decoder->frame)) {
                decoder->length = 0;
            }
        }

        ((uint8_t *)&decoder->frame)[decoder->length++] = data[consumed++];
        bool complete;
        if (!decoder_prefix_valid(decoder, &complete)) {
            NRF_LOG_DEBUG("Malformed frame bytes on transport %u", transport);
            decoder_resynchronize(decoder);
            if (decoder->complete && !queue_decoder(transport, decoder, generation)) {
                break;
            }
            continue;
        }
        if (complete && !queue_decoder(transport, decoder, generation)) {
            break;
        }
    }
    return receive_finish(decoder, generation, consumed, length);
}

uint16_t data_frame_receive_from(const uint8_t *data, uint16_t length,
                                 data_frame_transport_t transport) {
    return receive_from_generation(data, length, transport, false, 0);
}

uint16_t data_frame_receive_from_generation(const uint8_t *data, uint16_t length,
                                            data_frame_transport_t transport,
                                            uint32_t generation) {
    return receive_from_generation(data, length, transport, true, generation);
}

void data_frame_receive(uint8_t *data, uint16_t length) {
    uint16_t consumed = data_frame_receive_from(data, length, DATA_FRAME_TRANSPORT_USB);
    if (consumed != length) {
        NRF_LOG_WARNING("Legacy receive backpressured at %u/%u", consumed, length);
    }
}

void data_frame_reset_transport(data_frame_transport_t transport) {
    data_frame_decoder_t *decoder = decoder_for_transport(transport);
    if (decoder == NULL) {
        return;
    }
    CRITICAL_REGION_ENTER();
    decoder->generation++;
    if (!decoder->receiving) {
        decoder->length = 0;
        decoder->payload_length = 0;
        decoder->complete = false;
    }
    for (uint8_t i = 0; i < DATA_FRAME_REQUEST_QUEUE_DEPTH; i++) {
        if (&m_requests[i] != m_processing_request && m_requests[i].reserved &&
                m_requests[i].transport == transport) {
            m_requests[i].valid = false;
            if (!m_requests[i].publishing) {
                m_requests[i].reserved = false;
            }
        }
    }
    CRITICAL_REGION_EXIT();
}

void data_frame_set_flow_callback(data_frame_transport_t transport, void (*callback)(void)) {
    if (transport > DATA_FRAME_TRANSPORT_NONE && transport < DATA_FRAME_TRANSPORT_COUNT) {
        m_flow_callbacks[transport] = callback;
    }
}

void data_frame_set_ready_callback(data_frame_transport_t transport, bool (*callback)(void)) {
    if (transport > DATA_FRAME_TRANSPORT_NONE && transport < DATA_FRAME_TRANSPORT_COUNT) {
        m_ready_callbacks[transport] = callback;
    }
}

data_frame_transport_t data_frame_get_transport(void) {
    return m_current_transport;
}

uint32_t data_frame_get_transport_generation(data_frame_transport_t transport) {
    data_frame_decoder_t *decoder = decoder_for_transport(transport);
    if (decoder == NULL) {
        return 0;
    }
    uint32_t generation;
    CRITICAL_REGION_ENTER();
    generation = decoder->generation;
    CRITICAL_REGION_EXIT();
    return generation;
}

bool data_frame_current_transport_generation_valid(void) {
    bool valid = false;
    CRITICAL_REGION_ENTER();
    data_frame_request_t *request = m_processing_request;
    if (request != NULL) {
        data_frame_decoder_t *decoder = decoder_for_transport(request->transport);
        valid = decoder != NULL && request->generation == decoder->generation;
    }
    CRITICAL_REGION_EXIT();
    return valid;
}

static data_frame_request_t *oldest_request(void) {
    data_frame_request_t *oldest = NULL;
    for (uint8_t i = 0; i < DATA_FRAME_REQUEST_QUEUE_DEPTH; i++) {
        if (!m_requests[i].valid) {
            continue;
        }
        __DMB();
        data_frame_transport_t transport = m_requests[i].transport;
        bool ready = m_ready_callbacks[transport] == NULL || m_ready_callbacks[transport]();
        if (ready && (oldest == NULL || m_requests[i].sequence < oldest->sequence)) {
            oldest = &m_requests[i];
        }
    }
    return oldest;
}

static void resume_transports(void) {
    for (data_frame_transport_t transport = DATA_FRAME_TRANSPORT_USB;
            transport < DATA_FRAME_TRANSPORT_COUNT; transport++) {
        (void)data_frame_receive_from(NULL, 0, transport);
        if (m_flow_callbacks[transport] != NULL) {
            m_flow_callbacks[transport]();
        }
    }
}

void data_frame_process(void) {
    resume_transports();
    data_frame_request_t *request = oldest_request();
    CRITICAL_REGION_ENTER();
    if (request != NULL && request->valid) {
        m_processing_request = request;
    } else {
        request = NULL;
    }
    CRITICAL_REGION_EXIT();
    if (request == NULL) {
        return;
    }

    m_current_transport = request->transport;
    if (m_frame_process_cbk != NULL) {
        m_frame_process_cbk(request->cmd, request->status, request->length,
                            request->length ? request->data : NULL);
    }
    m_current_transport = DATA_FRAME_TRANSPORT_NONE;
    CRITICAL_REGION_ENTER();
    request->valid = false;
    request->reserved = false;
    m_processing_request = NULL;
    CRITICAL_REGION_EXIT();

    resume_transports();
}

void on_data_frame_complete(data_frame_cbk_t callback) {
    m_frame_process_cbk = callback;
}

data_frame_tx_t *data_frame_make(uint16_t cmd, uint16_t status, uint16_t data_length, uint8_t *data) {
    if ((data_length != 0 && data == NULL) || data_length > NETDATA_MAX_DATA_LENGTH) {
        NRF_LOG_ERROR("Invalid response payload: %u bytes", data_length);
        return NULL;
    }

    netdata_frame_postamble_t *post = (netdata_frame_postamble_t *)((uint8_t *)&m_netdata_frame_tx_buf +
                                             sizeof(netdata_frame_preamble_t) + data_length);
    m_netdata_frame_tx_buf.pre.sof = NETDATA_FRAME_SOF;
    m_netdata_frame_tx_buf.pre.lrc1 = compute_lrc((uint8_t *)&m_netdata_frame_tx_buf.pre,
                                                  offsetof(netdata_frame_preamble_t, lrc1));
    m_netdata_frame_tx_buf.pre.cmd = U16HTONS(cmd);
    m_netdata_frame_tx_buf.pre.status = U16HTONS(status);
    m_netdata_frame_tx_buf.pre.len = U16HTONS(data_length);
    m_netdata_frame_tx_buf.pre.lrc2 = compute_lrc((uint8_t *)&m_netdata_frame_tx_buf.pre,
                                                  offsetof(netdata_frame_preamble_t, lrc2));
    if (data_length != 0) {
        memcpy(m_netdata_frame_tx_buf.data, data, data_length);
    }
    post->lrc3 = compute_lrc(m_netdata_frame_tx_buf.data, data_length);
    m_frame_tx_buf_info.length = (uint16_t)(NETDATA_FRAME_OVERHEAD + data_length);
    return &m_frame_tx_buf_info;
}
