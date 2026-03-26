#ifndef CRC_H
#define CRC_H

#include <stdint.h>

int crc16_calculate(const uint8_t *data, size_t length);

#endif /* CRC_H_ */