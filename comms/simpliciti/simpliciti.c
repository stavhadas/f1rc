#include <string.h>
#include "simpliciti.h"
#include "tracing.h"

size_t simplicity_get_payload_length(const simpliciti_frame_t *frame)
{
    return frame->mfri_header.length - NWK_HEADER_SIZE;
}

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
    memcpy(&frame->mfri_header, &mfri_frame.header, sizeof(mfri_header_t));

    // Decode NWK frame
    nwk_frame_t nwk_frame;
    status = nwk_decode_frame(mfri_frame.payload, mfri_frame.payload_length, &nwk_frame);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to decode NWK frame: %d\n", status);
        return status;
    }
    memcpy(&frame->nwk_header, &nwk_frame.header, sizeof(nwk_header_t));
    
    // Populate simpliciti frame
    memcpy(frame->payload, nwk_frame.payload, simplicity_get_payload_length(frame));
    return SIMPLICITI_SUCCESS;
}

simpliciti_status_t simpliciti_encode_frame(const simpliciti_frame_t *frame,
                                            uint8_t *buffer,
                                            size_t *length)
{
    // Encode NWK frame
    uint8_t nwk_buffer[NWK_MAX_FRAME_SIZE];
    nwk_frame_t nwk_frame = {
        .header = frame->nwk_header,
        .payload = (uint8_t *)frame->payload,
        .payload_length = simplicity_get_payload_length(frame)
    };
    size_t nwk_length = sizeof(nwk_buffer);
    simpliciti_status_t status = nwk_encode_frame(&nwk_frame, nwk_buffer, &nwk_length);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to encode NWK frame: %d\n", status);
        return status;
    }

    // Encode MFRI frame
    mfri_frame_t mfri_frame = {
        .header = frame->mfri_header,
        .payload = nwk_buffer,
        .payload_length = simplicity_get_payload_length(frame) + NWK_HEADER_SIZE
    };

    status = mfri_encode_frame(&mfri_frame, buffer, length);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to encode MFRI frame: %d\n", status);
        return status;
    }
    
    return SIMPLICITI_SUCCESS;
}