#ifndef __CRC_UTILS_H
#define __CRC_UTILS_H

#include <stdint.h>
#include <stddef.h>

void calc_14a_crc_lut(const uint8_t *data, size_t length, uint8_t *output);

#endif
