#ifndef CRC_H
#define CRC_H

#include <stdint.h>

uint16_t crc16_calculate(const uint8_t *data, size_t length, uint16_t initial_crc);

#endif /* CRC_H_ */