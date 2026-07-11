#include <assert.h>
#include <stdint.h>

#include "circular_buffer.h"

static void test_invalid_initialization(void) {
    circular_buffer cb = {0};

    assert(!cb_init(NULL, 1, 1));
    assert(!cb_init(&cb, 0, 1));
    assert(!cb_init(&cb, 1, 0));
    assert(!cb_init(&cb, SIZE_MAX, 1));
}

static void test_fifo_wrap_and_drop_count(void) {
    circular_buffer cb = {0};
    uint32_t value;
    const uint32_t one = 1;
    const uint32_t two = 2;
    const uint32_t three = 3;
    const uint32_t four = 4;

    assert(cb_init(&cb, 3, sizeof(uint32_t)));
    assert(cb_push_back(&cb, &one));
    assert(cb_push_back(&cb, &two));
    assert(cb_push_back(&cb, &three));
    assert(!cb_push_back(&cb, &four));
    assert(cb_dropped(&cb) == 1);

    assert(cb_pop_front(&cb, &value) && value == one);
    assert(cb_push_back(&cb, &four));
    assert(cb_pop_front(&cb, &value) && value == two);
    assert(cb_pop_front(&cb, &value) && value == three);
    assert(cb_pop_front(&cb, &value) && value == four);
    assert(!cb_pop_front(&cb, &value));

    assert(!cb_push_back(&cb, NULL));
    assert(!cb_pop_front(&cb, NULL));
    cb_free(&cb);
    cb_free(&cb);
    assert(cb.buffer == NULL);
}

static void test_static_storage(void) {
    circular_buffer cb = {0};
    uint16_t storage[5] = {0};
    uint16_t value = 0;
    const uint16_t expected = 0x1234;

    assert(cb_init_static(&cb, storage, 4, sizeof(storage[0])));
    assert(cb_push_back(&cb, &expected));
    assert(cb_pop_front(&cb, &value));
    assert(value == expected);
    cb_free(&cb);
    assert(cb.buffer == NULL);
}

int main(void) {
    test_invalid_initialization();
    test_fifo_wrap_and_drop_count();
    test_static_storage();
    return 0;
}
