#pragma once

#define CRITICAL_REGION_ENTER() do {
#define CRITICAL_REGION_EXIT() } while (0)
#define __DMB() __asm__ __volatile__("" ::: "memory")
