#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "dataframe.h"
#include "netdata.h"

typedef struct {
    uint16_t cmd;
    uint16_t status;
    uint16_t length;
    uint8_t data[16];
    data_frame_transport_t transport;
} received_frame_t;

static received_frame_t received[8];
static size_t received_count;

static uint8_t lrc(const uint8_t *data, size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum = (uint8_t)(sum + data[i]);
    }
    return (uint8_t)(0u - sum);
}

static size_t make_frame(uint8_t *out, uint16_t cmd, uint16_t status,
                         const uint8_t *payload, uint16_t payload_length) {
    out[0] = NETDATA_FRAME_SOF;
    out[1] = lrc(out, 1);
    out[2] = (uint8_t)(cmd >> 8);
    out[3] = (uint8_t)cmd;
    out[4] = (uint8_t)(status >> 8);
    out[5] = (uint8_t)status;
    out[6] = (uint8_t)(payload_length >> 8);
    out[7] = (uint8_t)payload_length;
    out[8] = lrc(out, 8);
    if (payload_length != 0) {
        memcpy(&out[9], payload, payload_length);
    }
    out[9 + payload_length] = lrc(payload, payload_length);
    return NETDATA_FRAME_OVERHEAD + payload_length;
}

static void on_frame(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    assert(received_count < sizeof(received) / sizeof(received[0]));
    assert(length <= sizeof(received[0].data));
    received_frame_t *frame = &received[received_count++];
    frame->cmd = cmd;
    frame->status = status;
    frame->length = length;
    frame->transport = data_frame_get_transport();
    if (length != 0) {
        memcpy(frame->data, data, length);
    }
}

static void reset_state(void) {
    data_frame_reset_transport(DATA_FRAME_TRANSPORT_USB);
    data_frame_reset_transport(DATA_FRAME_TRANSPORT_BLE);
    data_frame_set_ready_callback(DATA_FRAME_TRANSPORT_USB, NULL);
    data_frame_set_ready_callback(DATA_FRAME_TRANSPORT_BLE, NULL);
    received_count = 0;
    memset(received, 0, sizeof(received));
    on_data_frame_complete(on_frame);
}

static void test_response_encoding(void) {
    const uint8_t payload[] = {0x10, 0x20, 0x30};
    data_frame_tx_t *tx = data_frame_make(0x1234, 0x0068, sizeof(payload),
                                          (uint8_t *)payload);
    assert(tx != NULL);
    assert(tx->length == NETDATA_FRAME_OVERHEAD + sizeof(payload));
    const uint8_t expected[] = {
        0x11, 0xef, 0x12, 0x34, 0x00, 0x68, 0x00, 0x03, 0x4f,
        0x10, 0x20, 0x30, 0xa0,
    };
    assert(memcmp(tx->buffer, expected, sizeof(expected)) == 0);
    assert(data_frame_make(1, 2, 1, NULL) == NULL);
    assert(data_frame_make(1, 2, NETDATA_MAX_DATA_LENGTH + 1u, (uint8_t *)payload) == NULL);
}

static void test_fragmented_and_interleaved_frames(void) {
    uint8_t usb[32];
    uint8_t ble[32];
    const uint8_t usb_data[] = {1, 2, 3};
    const uint8_t ble_data[] = {4, 5};
    size_t usb_length = make_frame(usb, 0x1001, 0x0068, usb_data, sizeof(usb_data));
    size_t ble_length = make_frame(ble, 0x7001, 0x0060, ble_data, sizeof(ble_data));

    reset_state();
    assert(data_frame_receive_from(usb, 4, DATA_FRAME_TRANSPORT_USB) == 4);
    assert(data_frame_receive_from(ble, ble_length, DATA_FRAME_TRANSPORT_BLE) == ble_length);
    assert(data_frame_receive_from(usb + 4, usb_length - 4, DATA_FRAME_TRANSPORT_USB) == usb_length - 4);
    data_frame_process();
    data_frame_process();

    assert(received_count == 2);
    assert(received[0].cmd == 0x7001);
    assert(received[0].transport == DATA_FRAME_TRANSPORT_BLE);
    assert(received[1].cmd == 0x1001);
    assert(received[1].transport == DATA_FRAME_TRANSPORT_USB);
    assert(memcmp(received[1].data, usb_data, sizeof(usb_data)) == 0);
}

static void test_malformed_input_resynchronizes(void) {
    uint8_t bad[32];
    uint8_t good[32];
    uint8_t stream[80];
    const uint8_t payload = 0x5a;
    size_t bad_length = make_frame(bad, 1, 2, &payload, 1);
    size_t good_length = make_frame(good, 3, 4, &payload, 1);
    bad[8] ^= 1;

    stream[0] = 0x99;
    memcpy(stream + 1, bad, bad_length);
    memcpy(stream + 1 + bad_length, good, good_length);

    reset_state();
    size_t stream_length = 1 + bad_length + good_length;
    assert(data_frame_receive_from(stream, stream_length, DATA_FRAME_TRANSPORT_USB) == stream_length);
    data_frame_process();
    assert(received_count == 1);
    assert(received[0].cmd == 3);
    assert(received[0].data[0] == payload);
}

static void test_queue_backpressure_preserves_order(void) {
    uint8_t frames[4][NETDATA_FRAME_OVERHEAD];
    for (uint16_t i = 0; i < 4; i++) {
        assert(make_frame(frames[i], (uint16_t)(10 + i), 0, NULL, 0) == NETDATA_FRAME_OVERHEAD);
    }

    reset_state();
    assert(data_frame_receive_from(frames[0], sizeof(frames[0]), DATA_FRAME_TRANSPORT_USB) == sizeof(frames[0]));
    assert(data_frame_receive_from(frames[1], sizeof(frames[1]), DATA_FRAME_TRANSPORT_USB) == sizeof(frames[1]));
    assert(data_frame_receive_from(frames[2], sizeof(frames[2]), DATA_FRAME_TRANSPORT_USB) == sizeof(frames[2]));
    assert(data_frame_receive_from(frames[3], sizeof(frames[3]), DATA_FRAME_TRANSPORT_USB) == 0);

    data_frame_process();
    assert(data_frame_receive_from(frames[3], sizeof(frames[3]), DATA_FRAME_TRANSPORT_USB) == sizeof(frames[3]));
    data_frame_process();
    data_frame_process();
    data_frame_process();

    assert(received_count == 4);
    for (uint16_t i = 0; i < 4; i++) {
        assert(received[i].cmd == (uint16_t)(10 + i));
    }
}

int main(void) {
    test_response_encoding();
    test_fragmented_and_interleaved_frames();
    test_malformed_input_resynchronizes();
    test_queue_backpressure_preserves_order();
    return 0;
}
