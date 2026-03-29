#include "nwk.h"
#include "tracing.h"

simpliciti_status_t nwk_decode_frame(const uint8_t *data, size_t length, nwk_frame_t *frame)
{
    if (length > NWK_MAX_FRAME_SIZE || length < sizeof(nwk_header_t))
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
    memcpy(&frame->header, data, sizeof(nwk_header_t));

    // Validate payload length
    if (frame->header.port.app_port > NWK_MAX_PAYLOAD_SIZE)
    {
        TRACE("Payload length exceeds maximum: %u\n", frame->header.port.app_port);
        return SIMPLICITI_ERROR_DECODING;
    }

    // Copy payload
    frame->payload = (uint8_t *)(data + sizeof(nwk_header_t));
    frame->payload_length = length - sizeof(nwk_header_t);

    return SIMPLICITI_SUCCESS;
}

simpliciti_status_t nwk_encode_frame(const nwk_frame_t *frame, uint8_t *buffer, size_t *length)
{
    if (frame == NULL || buffer == NULL || length == NULL)
    {
        TRACE("Null pointer provided for frame, buffer, or length\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    if (frame->payload_length > NWK_MAX_PAYLOAD_SIZE)
    {
        TRACE("Payload length exceeds maximum: %u\n", frame->payload_length);
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }

    // Encode header
    memcpy(buffer, &frame->header, sizeof(nwk_header_t));

    // Copy payload
    memcpy(buffer + sizeof(nwk_header_t), frame->payload, frame->payload_length);
    *length = sizeof(nwk_header_t) + frame->payload_length;

    return SIMPLICITI_SUCCESS;
}