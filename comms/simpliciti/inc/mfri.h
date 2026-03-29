#ifndef MFRI_H
#define MFRI_H

#include <stdint.h>
#include <stddef.h>

#include "simplciti_common.h"

#define MFRI_MAX_PAYLOAD_SIZE (114)

typedef struct __attribute__((packed))
{
    uint8_t length;
    uint32_t dstaddr;
    uint32_t srcaddr;
} mfri_header_t;

#define MFRI_HEADER_SIZE (sizeof(mfri_header_t))

typedef struct
{
    mfri_header_t header;
    uint8_t *payload;
    size_t payload_length;
} mfri_frame_t;

#define MFRI_MAX_FRAME_SIZE (MFRI_HEADER_SIZE + MFRI_MAX_PAYLOAD_SIZE)

/** 
 * @brief Decodes a raw byte array into an MFRI frame structure.
 * @note The buffer must be synced to the start of an MFRI frame.
 *       The function will validate the length and structure of the frame.
 * @param data Pointer to the input byte array containing the raw frame data.
 * @param length Length of the input byte array.
 * @param frame Pointer to the output MFRI frame structure to populate.
 * @return SIMPLICITI_SUCCESS if decoding is successful, or an appropriate error code on failure
 */
simpliciti_status_t mfri_decode_frame(const uint8_t *data, size_t length, mfri_frame_t *frame);

simpliciti_status_t mfri_encode_frame(const mfri_frame_t *frame, uint8_t *buffer, size_t *length);

#endif // MFRI_H