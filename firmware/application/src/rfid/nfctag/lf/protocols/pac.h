#pragma once

#include "protocols.h"

extern const protocol pac;
bool pac_data_valid(const uint8_t *data);
uint8_t pac_t55xx_writer(uint8_t *data, uint32_t *blks);
