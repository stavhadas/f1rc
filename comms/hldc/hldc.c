#include <stdint.h>
#include <string.h>

#include "hldc.h"
#include "hldc_internal.h"
#include "crc.h"
#include "tracing.h"
void hldc_reset_context(hldc_context_t *ctx)
{
    ctx->state = HLDC_STATE_START_FLAG;
    ctx->length = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
    memset(&ctx->current_frame, 0, sizeof(ctx->current_frame));
}

hldc_status_t hldc_handle_end_byte(hldc_context_t *ctx)
{
    uint16_t received_crc = *(uint16_t *)(ctx->current_frame.payload + ctx->current_frame.payload_length - HLDC_TRAILER_SIZE);
    ctx->current_frame.payload_length -= HLDC_TRAILER_SIZE;               // Remove CRC from payload length
    ctx->current_frame.payload[ctx->current_frame.payload_length] = '\0'; // Null-terminate payload for safety
    ctx->current_frame.trailer.crc = crc16_calculate((uint8_t *)&ctx->current_frame.header, HLDC_HEADER_SIZE + ctx->current_frame.payload_length);
    if (received_crc != ctx->current_frame.trailer.crc)
    {
        hldc_reset_context(ctx); // Reset context on CRC error
        return HLDC_STATUS_CRC_ERROR;
    }
    ctx->on_frame(&ctx->current_frame); // Call the frame handler callback
    hldc_reset_context(ctx);            // Reset context for next frame
    return HLDC_STATUS_OK;
}

hldc_status_t hldc_insert_escape_char(uint8_t *buffer, size_t index, size_t buffer_length)
{
    if (buffer_length + 1 >= HLDC_MAX_FRAME_SIZE)
    {
        return HLDC_STATUS_INPUT_PARAMS_ERROR; // Not enough space to insert escape character
    }
    buffer[index] = HLDC_ESCAPE_CHAR;
    return HLDC_STATUS_OK;
}
hldc_status_t hldc_encode(hldc_frame_t *frame, uint8_t *buffer, size_t *length)
{
    if (!frame || !buffer || !length)
    {
        TRACE("Invalid input parameters: frame=%p, buffer=%p, length=%p", (void *)frame, (void *)buffer, (void *)length);
        return HLDC_STATUS_INPUT_PARAMS_ERROR;
    }

    if (*length < HLDC_FRAME_OVERHEAD + frame->payload_length)
    {
        TRACE("Output buffer too small: required=%zu, provided=%zu", HLDC_FRAME_OVERHEAD + frame->payload_length, *length);
        return HLDC_STATUS_INPUT_PARAMS_ERROR;
    }

    // CRC covers [address, control, payload] before byte-stuffing
    uint16_t crc = (uint16_t)crc16_calculate((uint8_t *)&frame->header,
                                             HLDC_HEADER_SIZE + frame->payload_length);

    size_t idx = 0;

    buffer[idx++] = HLDC_START_FLAG;
    buffer[idx++] = frame->header.address;
    buffer[idx++] = frame->header.control;

    // Byte-stuff payload into output buffer
    if (frame->payload_length > 0)
    {
        while (idx < HLDC_MAX_FRAME_SIZE && (idx - HLDC_PRE_PAYLOAD_SIZE) < frame->payload_length)
        {
            if (frame->payload[idx] == HLDC_ESCAPE_CHAR)
            {
                hldc_insert_escape_char(buffer, idx, *length);
                idx++;
            }
            buffer[idx] = frame->payload[idx - HLDC_PRE_PAYLOAD_SIZE];
            idx++;
        }
    }

    if (idx + HLDC_TRAILER_SIZE + HLDC_END_FLAG_SIZE > *length)
    {
        TRACE("Not enough space for trailer and end flag after payload: required=%zu, available=%zu", HLDC_TRAILER_SIZE + HLDC_END_FLAG_SIZE, *length - idx);
        return HLDC_STATUS_INPUT_PARAMS_ERROR; // Not enough space for trailer and end flag
    }

    // Append CRC (little-endian)
    buffer[idx++] = crc & 0xFF;
    buffer[idx++] = (crc >> 8) & 0xFF;
    buffer[idx++] = HLDC_END_FLAG;

    *length = idx;
    return HLDC_STATUS_OK;
}

hldc_status_t hldc_handle_payload(hldc_context_t *ctx, uint8_t byte)
{
    switch (byte)
    {
    case HLDC_END_FLAG:
        return hldc_handle_end_byte(ctx);
        break;
    case HLDC_ESCAPE_CHAR:
        return HLDC_STATUS_OK; // Next byte will be handled in the next call to hldc_push_byte
        break;
    default:
        if (ctx->length < MAX_PAYLOAD_SIZE)
        {
            ctx->current_frame.payload[ctx->length++] = byte;
            return HLDC_STATUS_OK;
        }
        else
        {
            hldc_reset_context(ctx); // Reset context on overflow
            return HLDC_STATUS_OVERFLOW_ERROR;
        }
        break;
    }
}

hldc_status_t hldc_push_byte(hldc_context_t *ctx, uint8_t byte)
{
    switch (ctx->state)
    {
    case HLDC_START_FLAG:
        if (byte == HLDC_START_FLAG)
        {
            ctx->state = HLDC_STATE_ADDRESS;
            ctx->length = 0;
        }
        break;
    case HLDC_STATE_ADDRESS:
        ctx->current_frame.header.address = byte;
        ctx->state = HLDC_STATE_CONTROL;
        break;
    case HLDC_STATE_CONTROL:
        ctx->current_frame.header.control = byte;
        ctx->state = HLDC_STATE_PAYLOAD;
        break;
    case HLDC_STATE_PAYLOAD:
        hldc_handle_payload(ctx, byte);
        break;
    default:
        break;
    }
}

hldc_status_t push_data(hldc_context_t *ctx, uint8_t *data, size_t length)
{
    hldc_status_t status = HLDC_STATUS_OK;
    for (size_t i = 0; i < length; i++)
    {
        hldc_push_byte(ctx, data[i]);
    }
    return HLDC_STATUS_OK;
}