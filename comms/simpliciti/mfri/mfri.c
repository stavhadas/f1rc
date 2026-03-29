#include <string.h>

#include "mfri.h"
#include "tracing.h"

simpliciti_status_t mfri_decode_frame(const uint8_t *data, size_t length, mfri_frame_t *frame)
{
    if (length > MFRI_MAX_FRAME_SIZE || length < MFRI_HEADER_SIZE)
    {
        TRACE("Invalid frame length: %zu\n", length);
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    if (frame == NULL || data == NULL)
    {
        TRACE("Null pointer provided for frame or data\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }

    // Decode header
    frame->header.length = data[0];
    if (frame->header.length != length - MFRI_HEADER_SIZE)
    {
        TRACE("Length mismatch: header length %u does not match actual payload length %zu\n", frame->header.length, length - MFRI_HEADER_SIZE);
        return SIMPLICITI_ERROR_DECODING;
    }

    frame->header.dstaddr = *(uint32_t *)(data + 1);
    frame->header.srcaddr = *(uint32_t *)(data + 5);

    // Change ptr to payload location in data buffer and set length
    frame->payload = (uint8_t *)(data + MFRI_HEADER_SIZE);
    frame->payload_length = frame->header.length;
    
    return SIMPLICITI_SUCCESS;
}

simpliciti_status_t mfri_encode_frame(const mfri_frame_t *frame, uint8_t *buffer, size_t *length)
{
    if (frame == NULL || buffer == NULL || length == NULL)
    {
        TRACE("Null pointer provided for frame, buffer, or length\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    if (frame->header.length > MFRI_MAX_PAYLOAD_SIZE)
    {
        TRACE("Payload length exceeds maximum: %u\n", frame->header.length);
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }

    // Encode header
    buffer[0] = frame->header.length;
    *(uint32_t *)(buffer + 1) = frame->header.dstaddr;
    *(uint32_t *)(buffer + 5) = frame->header.srcaddr;

    // Copy payload
    memcpy(buffer + MFRI_HEADER_SIZE, frame->payload, frame->header.length);
    
    *length = MFRI_HEADER_SIZE + frame->header.length;

    return SIMPLICITI_SUCCESS;
}