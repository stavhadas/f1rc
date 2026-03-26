#include <stdint.h>
#include <string.h>

#include "hldc.h"
#include "hldc_internal.h"

void hldc_reset_context(hldc_context_t *ctx)
{
    ctx->state = HLDC_STATE_START_FLAG;
    ctx->length = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
    memset(&ctx->current_frame, 0, sizeof(ctx->current_frame));
}

enum hldc_status hldc_handle_end_byte(hldc_context_t *ctx)
{
    uint16_t received_crc = *(uint16_t *)(ctx->current_frame.payload + ctx->current_frame.payload_length - HLDC_TRAILER_SIZE);
            ctx->current_frame.payload_length -= HLDC_TRAILER_SIZE;               // Remove CRC from payload length
            ctx->current_frame.payload[ctx->current_frame.payload_length] = '\0'; // Null-terminate payload for safety
            ctx->current_frame.trailer.crc = crc16_calculate((uint8_t *)&ctx->current_frame.header, HLDC_HEADER_SIZE + ctx->current_frame.payload_length);
            if (received_crc != ctx->current_frame.trailer.crc)
            {
                return HLDC_STATUS_CRC_ERROR;
            }
            ctx->on_frame(&ctx->current_frame); // Call the frame handler callback
            hldc_reset_context(ctx); // Reset context for next frame
    
}

enum hldc_status hldc_encode(struct hldc_frame *frame, uint8_t *buffer, size_t *length)
{
    // Validate input parameters
    if (!frame || !buffer || !length)
    {
        return HLDC_STATUS_ERROR;
    }

    if (*length < HLDC_FRAME_OVERHEAD + frame->payload_length)
    {
        return HLDC_STATUS_ERROR;
    }

    buffer[0] = HLDC_START_FLAG;
    *(uint16_t *)(buffer + HLDC_START_FLAG_SIZE) = frame->header;
    memcpy(&buffer[HLDC_PRE_PAYLOAD_SIZE], frame->payload, frame->payload_length);
    uint16_t crc = calculate_crc(frame);
    buffer[HLDC_PRE_PAYLOAD_SIZE + frame->payload_length] = (crc >> 8) & 0xFF;
    buffer[HLDC_PRE_PAYLOAD_SIZE + frame->payload_length + 1] = crc & 0xFF;
    buffer[HLDC_PRE_PAYLOAD_SIZE + frame->payload_length + 2] = HLDC_END_FLAG;

    *length = HLDC_FRAME_OVERHEAD + frame->payload_length;
    return HLDC_STATUS_OK;
}

enum hldc_status hldc_push_byte(hldc_context_t *ctx, uint8_t byte)
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
        if (byte == HLDC_END_FLAG)
        {
            if (ctx->length < HLDC_TRAILER_SIZE)
            {
                return HLDC_STATUS_DECODE_ERROR;
            }
            
        }
        else
        {
            if (ctx->length < MAX_PAYLOAD_SIZE)
            {
                ctx->current_frame.payload[ctx->length++] = byte;
            }
            else
            {
                return HLDC_STATUS_OVERFLOW_ERROR;
            }
        }
        break;
    default:
        break;
    }
}

enum hldc_status push_data(hldc_context_t *ctx, uint8_t *data, size_t length)
{
    enum hldc_status status = HLDC_STATUS_OK;
    for (size_t i = 0; i < length; i++)
    {
        /* code */
    }

    // This function would handle incoming data and process it according to the HLDC protocol.
    // For now, it's just a placeholder.
    return HLDC_STATUS_OK;
}