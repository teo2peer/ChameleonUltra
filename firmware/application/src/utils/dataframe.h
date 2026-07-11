#ifndef DATAFRAME_H
#define DATAFRAME_H

#include <stdint.h>
#include <stdbool.h>

// Data frame process callback
typedef void (*data_frame_cbk_t)(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);

// TX buffer
typedef struct {
    uint8_t *const buffer;
    uint16_t length;
} data_frame_tx_t;

typedef enum {
    DATA_FRAME_TRANSPORT_NONE = 0,
    DATA_FRAME_TRANSPORT_USB,
    DATA_FRAME_TRANSPORT_BLE,
    DATA_FRAME_TRANSPORT_COUNT,
} data_frame_transport_t;

void data_frame_receive(uint8_t *data, uint16_t length);
// Returns the number of input bytes consumed. A short result applies backpressure;
// the caller must retain the remainder and retry after its flow callback runs.
uint16_t data_frame_receive_from(const uint8_t *data, uint16_t length, data_frame_transport_t transport);
// Valid while the registered frame-complete callback is running.
data_frame_transport_t data_frame_get_transport(void);
void data_frame_reset_transport(data_frame_transport_t transport);
void data_frame_set_flow_callback(data_frame_transport_t transport, void (*callback)(void));
void data_frame_set_ready_callback(data_frame_transport_t transport, bool (*callback)(void));
void data_frame_process(void);
void on_data_frame_complete(data_frame_cbk_t callback);

data_frame_tx_t *data_frame_make(
    uint16_t cmd,
    uint16_t status,
    uint16_t length,
    uint8_t *data
);


#endif // DATAFRAME_H
