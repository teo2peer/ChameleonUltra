#pragma once

#include <stdint.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define IS_SET(value, bit) (((uint64_t)(value) & (UINT64_C(1) << (bit))) != 0)
