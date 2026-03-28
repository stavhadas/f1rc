#include <stdint.h>
#include <stddef.h>

#include "crc.h"

uint16_t crc16_calculate(const uint8_t *data, size_t length, uint16_t initial_crc)
{
    uint16_t crc = initial_crc;

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }

    return crc;
}