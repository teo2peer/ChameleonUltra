#pragma once

#include "protocols.h"

extern const protocol jablotron;
bool jablotron_data_valid(const uint8_t *uid);

uint8_t jablotron_t55xx_writer(uint8_t* uid, uint32_t* blks);
