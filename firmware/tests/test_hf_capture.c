#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "hf_capture.h"
#include "nfc_14a.h"
#include "rc522.h"

static uint32_t ticks;
static nfc_tag_14a_sniff_cb_t rx_callback;
static nfc_tag_14a_sniff_cb_t tx_callback;
static nfc_tag_14a_field_sniff_cb_t field_callback;
static pcd_14a_trace_cb_t reader_callback;

uint32_t app_timer_cnt_get(void) { return ++ticks; }
uint32_t app_timer_cnt_diff_compute(uint32_t to, uint32_t from) { return to - from; }
void reader_mode_enter(void) {}
void tag_mode_enter(void) {}
void tag_emulation_sense_run(void) {}
void nfc_tag_14a_sense_switch(bool enable) { (void)enable; }
void nfc_tag_14a_set_sniff_cb(nfc_tag_14a_sniff_cb_t callback) { rx_callback = callback; }
void nfc_tag_14a_clear_sniff_cb(void) { rx_callback = NULL; }
void nfc_tag_14a_set_tx_sniff_cb(nfc_tag_14a_sniff_cb_t callback) { tx_callback = callback; }
void nfc_tag_14a_clear_tx_sniff_cb(void) { tx_callback = NULL; }
void nfc_tag_14a_set_field_sniff_cb(nfc_tag_14a_field_sniff_cb_t callback) { field_callback = callback; }
void nfc_tag_14a_clear_field_sniff_cb(void) { field_callback = NULL; }
void nfc_tag_14a_set_sniff_passive(bool passive) { (void)passive; }
void pcd_14a_reader_capture_set(pcd_14a_trace_cb_t callback) { reader_callback = callback; }
void pcd_14a_reader_capture_clear(void) { reader_callback = NULL; }

static uint16_t read_u16(const uint8_t *data) {
    return ((uint16_t)data[0] << 8) | data[1];
}

static uint32_t read_u32(const uint8_t *data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

static uint64_t read_u64(const uint8_t *data) {
    return ((uint64_t)read_u32(data) << 32) | read_u32(&data[4]);
}

int main(void) {
    const uint32_t first_token = 0x01020304u;
    uint32_t session_id = 0;
    assert(hf_capture_start(HF_CAPTURE_MODE_EMULATION,
                            DATA_FRAME_TRANSPORT_USB,
                            first_token,
                            &session_id) == HF_CAPTURE_RESULT_OK);
    assert(session_id != 0u);
    assert(rx_callback != NULL && tx_callback != NULL && field_callback != NULL);
    assert(reader_callback == NULL);
    uint32_t retried_session_id = 0;
    assert(hf_capture_start(HF_CAPTURE_MODE_EMULATION,
                            DATA_FRAME_TRANSPORT_USB,
                            first_token,
                            &retried_session_id) == HF_CAPTURE_RESULT_OK);
    assert(retried_session_id == session_id);
    assert(hf_capture_start(HF_CAPTURE_MODE_EMULATION,
                            DATA_FRAME_TRANSPORT_USB,
                            first_token + 1u,
                            &retried_session_id) == HF_CAPTURE_RESULT_BUSY);

    const uint8_t request[] = {0x60, 0x04};
    rx_callback(request, 16u, 0u);
    field_callback(true);

    uint8_t page[4096] = {0};
    uint16_t page_length = 0;
    assert(hf_capture_get(session_id, false, 0u, 0u, DATA_FRAME_TRANSPORT_USB,
                          HF_CAPTURE_MIN_PAGE_SIZE - 1u, page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_INVALID);
    assert(hf_capture_get(session_id, false, 0u, 0u, DATA_FRAME_TRANSPORT_USB,
                          sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    assert(page_length > HF_CAPTURE_PAGE_HEADER_SIZE);
    assert(read_u16(&page[56]) == 2u);
    assert(read_u32(&page[44]) == first_token);
    assert(read_u32(&page[48]) == 0u);
    assert(read_u32(&page[52]) == 2u);
    uint64_t delivery_token = read_u64(&page[64]);
    assert(delivery_token != 0u);
    assert(hf_capture_get(session_id, true, 2u, delivery_token,
                          DATA_FRAME_TRANSPORT_USB,
                          sizeof(page), page, sizeof(page), &page_length) ==
           HF_CAPTURE_RESULT_INVALID);
    assert(hf_capture_resume(0u, first_token, DATA_FRAME_TRANSPORT_BLE) ==
            HF_CAPTURE_RESULT_SESSION);
    hf_capture_owner_disconnected(DATA_FRAME_TRANSPORT_USB);
    assert(hf_capture_get(session_id, false, 0u, 0u, DATA_FRAME_TRANSPORT_USB,
                          sizeof(page), page, sizeof(page), &page_length) ==
           HF_CAPTURE_RESULT_SESSION);
    assert(hf_capture_resume(0u, first_token + 1u,
                             DATA_FRAME_TRANSPORT_USB) ==
            HF_CAPTURE_RESULT_SESSION);
    assert(hf_capture_resume(0u, first_token, DATA_FRAME_TRANSPORT_USB) ==
            HF_CAPTURE_RESULT_OK);
    assert(hf_capture_resume(session_id, first_token,
                             DATA_FRAME_TRANSPORT_BLE) ==
            HF_CAPTURE_RESULT_OK);
    assert(hf_capture_get(session_id, true, 1u, delivery_token,
                          DATA_FRAME_TRANSPORT_USB,
                          sizeof(page), page, sizeof(page), &page_length) ==
           HF_CAPTURE_RESULT_SESSION);

    assert(hf_capture_get(session_id, true, 1u, delivery_token,
                          DATA_FRAME_TRANSPORT_BLE,
                          sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    assert(read_u16(&page[56]) == 0u);
    assert(read_u16(&page[58]) == 0u);

    assert(hf_capture_stop(session_id, DATA_FRAME_TRANSPORT_USB) ==
           HF_CAPTURE_RESULT_SESSION);
    assert(hf_capture_stop(session_id, DATA_FRAME_TRANSPORT_BLE) ==
           HF_CAPTURE_RESULT_OK);
    assert(!hf_capture_is_active());
    assert(rx_callback == NULL && tx_callback == NULL && field_callback == NULL);

    uint32_t second_session = 0;
    assert(hf_capture_start(HF_CAPTURE_MODE_EMULATION,
                            DATA_FRAME_TRANSPORT_USB,
                            0x11121314u,
                            &second_session) == HF_CAPTURE_RESULT_OK);
    rx_callback(request, 16u, 0u);
    assert(hf_capture_stop(second_session, DATA_FRAME_TRANSPORT_USB) ==
           HF_CAPTURE_RESULT_OK);
    assert(hf_capture_start(HF_CAPTURE_MODE_EMULATION,
                            DATA_FRAME_TRANSPORT_USB,
                            0x21222324u,
                            &session_id) == HF_CAPTURE_RESULT_BUSY);

    assert(hf_capture_get(second_session, false, 0u, 0u,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    delivery_token = read_u64(&page[64]);
    assert(hf_capture_get(second_session, true, 0u, delivery_token,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);

    uint32_t gap_session = 0;
    assert(hf_capture_start(HF_CAPTURE_MODE_EMULATION,
                            DATA_FRAME_TRANSPORT_USB,
                            0x31323334u,
                            &gap_session) == HF_CAPTURE_RESULT_OK);
    rx_callback(request, 16u, 0u);
    assert(hf_capture_get(gap_session, false, 0u, 0u,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    delivery_token = read_u64(&page[64]);
    hf_capture_test_set_next_sequence(0x80000001u);
    rx_callback(request, 16u, 0u);
    assert(hf_capture_get(gap_session, true, 0u, delivery_token,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    assert(read_u16(&page[56]) == 1u);
    assert(read_u32(&page[48]) == 0x80000001u);
    assert(read_u32(&page[HF_CAPTURE_PAGE_HEADER_SIZE + 4u]) == 0x80000001u);
    delivery_token = read_u64(&page[64]);
    assert(hf_capture_get(gap_session, true, 0x80000001u, delivery_token,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    assert(hf_capture_get(gap_session, true, 0x80000001u, delivery_token,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    hf_capture_test_set_next_sequence(0x80000001u);
    rx_callback(request, 16u, 0u);
    assert(hf_capture_get(gap_session, false, 0u, 0u,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    uint64_t wrapped_delivery_token = read_u64(&page[64]);
    assert(wrapped_delivery_token != delivery_token);
    assert(hf_capture_get(gap_session, true, 0x80000001u,
                          wrapped_delivery_token, DATA_FRAME_TRANSPORT_USB,
                          sizeof(page), page, sizeof(page), &page_length) ==
           HF_CAPTURE_RESULT_OK);
    assert(hf_capture_stop(gap_session, DATA_FRAME_TRANSPORT_USB) ==
           HF_CAPTURE_RESULT_OK);

    uint32_t overflow_session = 0;
    assert(hf_capture_start(HF_CAPTURE_MODE_EMULATION,
                            DATA_FRAME_TRANSPORT_USB,
                            0x41424344u,
                            &overflow_session) == HF_CAPTURE_RESULT_OK);
    for (uint16_t i = 0; i < 341u; i++) rx_callback(request, 16u, 0u);
    rx_callback(request, 16u, 0u);  /* sequence 341 is explicitly dropped */

    assert(hf_capture_get(overflow_session, false, 0u, 0u,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    assert(read_u32(&page[52]) == 167u);
    delivery_token = read_u64(&page[64]);
    assert(hf_capture_get(overflow_session, true, 166u, delivery_token,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    assert(read_u32(&page[52]) == 334u);
    delivery_token = read_u64(&page[64]);
    assert(hf_capture_get(overflow_session, true, 333u, delivery_token,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    assert(read_u32(&page[52]) == 341u);
    delivery_token = read_u64(&page[64]);
    assert(hf_capture_get(overflow_session, true, 339u, delivery_token,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    rx_callback(request, 16u, 0u);  /* sequence 342 fits after the partial ACK */
    assert(hf_capture_get(overflow_session, false, 0u, 0u,
                          DATA_FRAME_TRANSPORT_USB, sizeof(page), page,
                          sizeof(page), &page_length) == HF_CAPTURE_RESULT_OK);
    assert(read_u32(&page[24]) == 1u);
    assert(read_u32(&page[48]) == 340u);
    assert(read_u32(&page[52]) == 343u);
    assert(read_u16(&page[56]) == 2u);
    assert(read_u32(&page[HF_CAPTURE_PAGE_HEADER_SIZE + 4u]) == 340u);
    assert(read_u32(&page[HF_CAPTURE_PAGE_HEADER_SIZE + 28u]) == 342u);
    return 0;
}
