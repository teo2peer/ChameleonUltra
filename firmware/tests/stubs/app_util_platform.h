#pragma once

#define CRITICAL_REGION_ENTER() do { } while (0)
#define CRITICAL_REGION_EXIT() do { } while (0)
#define __DMB() __asm__ __volatile__("" ::: "memory")
