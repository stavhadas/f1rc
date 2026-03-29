#include <string.h>
#include "simpliciti.h"
#include "tracing.h"

simpliciti_status_t simpliciti_decode_frame(const uint8_t *data,
                                            size_t length,
                                            simpliciti_frame_t *frame)
{
    simpliciti_status_t status = SIMPLICITI_SUCCESS;

    // Decode MFRI frame
    mfri_frame_t mfri_frame;
    status = mfri_decode_frame(data, length, &mfri_frame);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to decode MFRI frame: %d\n", status);
        return status;
    }

    // Decode NWK frame
    nwk_frame_t nwk_frame;
    status = nwk_decode_frame(data, length, &nwk_frame);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to decode NWK frame: %d\n", status);
        return status;
    }

    // Populate simpliciti frame
    memcpy(frame->payload, &nwk_frame.payload, nwk_frame.payload_length);
    memcpy(&frame->nwk_header, &nwk_frame.header, sizeof(nwk_header_t));
    memcpy(&frame->mfri_header, &mfri_frame.header, sizeof(mfri_header_t));

    return SIMPLICITI_SUCCESS;
}

simpliciti_status_t simpliciti_encode_frame(const simpliciti_frame_t *frame,
                                            uint8_t *buffer,
                                            size_t *length)
{
    // Encode NWK frame
    nwk_frame_t nwk_frame;
    memcpy(&nwk_frame.header, &frame->nwk_header, sizeof(nwk_header_t));
    nwk_frame.payload = (uint8_t *)frame->payload;
    nwk_frame.payload_length = frame->nwk_header.port.app_port; // Assuming app_port indicates payload length for encoding
    simpliciti_status_t status = nwk_encode_frame(&nwk_frame, buffer, length);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to encode NWK frame: %d\n", status);
        return status;
    }

    // Encode MFRI frame
    mfri_frame_t mfri_frame;
    memcpy(&mfri_frame.header, &frame->mfri_header, sizeof(mfri_header_t));
    mfri_frame.payload = (uint8_t *)frame->payload;
    mfri_frame.payload_length = frame->mfri_header.length; // Assuming length field indicates payload length for encoding
    status = mfri_encode_frame(&mfri_frame, buffer, length);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to encode MFRI frame: %d\n", status);
        return status;
    }

    return SIMPLICITI_SUCCESS;
}