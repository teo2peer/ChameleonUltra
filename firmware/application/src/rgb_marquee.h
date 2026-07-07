#ifndef RGB_MARQUEE_H
#define RGB_MARQUEE_H

#include <stdint.h>
#include "nrf_drv_pwm.h"


void rgb_marquee_init(void);
void rgb_marquee_stop(void);
void rgb_marquee_reset(void);
bool rgb_marquee_is_enabled(void);
void rgb_marquee_usb_open_sweep(uint8_t color, uint8_t dir);
void rgb_marquee_usb_open_symmetric(uint8_t color);
void rgb_marquee_sweep_to(uint8_t color, uint8_t dir, uint8_t end);
void rgb_marquee_slot_switch(uint8_t led_down, uint8_t color_led_down, uint8_t led_up, uint8_t color_led_up);
void rgb_marquee_sweep_fade(uint8_t color, uint8_t dir, uint8_t end, uint8_t start_light, uint8_t stop_light);
void rgb_marquee_sweep_from_to(uint8_t color, uint8_t start, uint8_t stop);
void rgb_marquee_usb_idle(void);
void rgb_marquee_symmetric_out(uint8_t color, uint8_t slot);
void rgb_marquee_symmetric_in(uint8_t color, uint8_t slot);

// Reader-key capture animation: a rainbow that radiates from the center of the
// LED bar outward. Non-blocking; drive it once per main-loop iteration.
void rgb_marquee_reader_keys_loop(void);
void rgb_marquee_set_reader_keys_anim(bool enable);
bool rgb_marquee_is_reader_keys_anim(void);

// BLE-test animation: a bar filling from the outer edges toward the centre
// (outside -> center), used while a directed BLE test (fuzz) is running.
void rgb_marquee_ble_test_loop(void);
void rgb_marquee_set_ble_test_anim(bool enable);
bool rgb_marquee_is_ble_test_anim(void);

// BLE-active animation: solid BLUE bar filling from the outer edges toward the
// centre (outside -> center), used while ANY BLE attack / stress / broadcast
// tool is running (flood, scan-buffer-wide kick / flood, environment-wide
// 2.4 GHz broadcast, fuzz). Non-blocking; drive it from the main loop.
// Pressing either physical button while this animation is active cancels the
// in-progress attack and re-runs the BLE app so the host can reconnect —
// regardless of the configured button-press function.
void rgb_marquee_ble_active_loop(void);
void rgb_marquee_set_ble_active_anim(bool enable);
bool rgb_marquee_is_ble_active_anim(void);

#endif
