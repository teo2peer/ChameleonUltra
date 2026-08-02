#pragma once

#include <assert.h>

#define STATIC_ASSERT(condition) _Static_assert(condition, #condition)
#define APP_ERROR_CHECK(error) assert((error) == 0u)
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
