#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct circular_buffer {
    void *buffer;      // data buffer
    void *buffer_end;  // end of data buffer
    size_t capacity;   // maximum number of items in the buffer
    size_t sz;         // size of each item in the buffer
    void * volatile head; // producer-owned published pointer
    void * volatile tail; // consumer-owned published pointer
    volatile size_t dropped;
    bool owns_buffer;
} circular_buffer;

extern bool cb_init(circular_buffer *cb, size_t capacity, size_t sz);
extern bool cb_init_static(circular_buffer *cb, void *storage, size_t capacity, size_t sz);
extern void cb_free(circular_buffer *cb);
extern bool cb_push_back(circular_buffer *cb, const void *item);
extern bool cb_pop_front(circular_buffer *cb, void *item);
extern size_t cb_dropped(const circular_buffer *cb);

#ifdef __cplusplus
}
#endif
