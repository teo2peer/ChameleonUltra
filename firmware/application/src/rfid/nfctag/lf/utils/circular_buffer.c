#include "circular_buffer.h"

#include <stdlib.h>
#include <string.h>

static bool cb_configure(circular_buffer *cb, void *storage, size_t capacity,
                         size_t sz, bool owns_buffer) {
    if (cb == NULL || storage == NULL || capacity == 0 || sz == 0 ||
            capacity >= SIZE_MAX / sz) {
        return false;
    }
    size_t storage_capacity = capacity + 1;
    cb->buffer = storage;
    cb->buffer_end = (char *)storage + storage_capacity * sz;
    cb->capacity = capacity;
    cb->sz = sz;
    cb->head = storage;
    cb->tail = storage;
    cb->dropped = 0;
    cb->owns_buffer = owns_buffer;
    return true;
}

bool cb_init(circular_buffer *cb, size_t capacity, size_t sz) {
    if (cb == NULL || capacity == 0 || sz == 0 || capacity >= SIZE_MAX / sz) {
        return false;
    }
    size_t storage_capacity = capacity + 1;
    void *storage = malloc(storage_capacity * sz);
    cb->buffer = storage;
    if (cb->buffer == NULL) {
        return false;
    }
    return cb_configure(cb, storage, capacity, sz, true);
}

bool cb_init_static(circular_buffer *cb, void *storage, size_t capacity, size_t sz) {
    return cb_configure(cb, storage, capacity, sz, false);
}

void cb_free(circular_buffer *cb) {
    if (cb != NULL && cb->buffer != NULL) {
        if (cb->owns_buffer) free(cb->buffer);
        cb->buffer = NULL;
        cb->buffer_end = NULL;
        cb->head = NULL;
        cb->tail = NULL;
        cb->capacity = 0;
        cb->sz = 0;
        cb->owns_buffer = false;
    }
}

bool cb_push_back(circular_buffer *cb, const void *item) {
    if (cb == NULL || cb->buffer == NULL || item == NULL) {
        return false;
    }

    void *head = cb->head;
    void *next = (char *)head + cb->sz;
    if (next == cb->buffer_end) {
        next = cb->buffer;
    }
    if (next == cb->tail) {
        cb->dropped++;
        return false;
    }
    memcpy(head, item, cb->sz);
    cb->head = next;
    return true;
}

bool cb_pop_front(circular_buffer *cb, void *item) {
    if (cb == NULL || cb->buffer == NULL || item == NULL) {
        return false;
    }

    void *tail = cb->tail;
    if (tail == cb->head) {
        return false;
    }
    memcpy(item, tail, cb->sz);
    tail = (char *)tail + cb->sz;
    if (tail == cb->buffer_end) {
        tail = cb->buffer;
    }
    cb->tail = tail;
    return true;
}

size_t cb_dropped(const circular_buffer *cb) {
    return cb == NULL ? 0 : cb->dropped;
}
