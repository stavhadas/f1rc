#include <stdint.h>
#include <string.h>

#include "hldc.h"
#include "hldc_internal.h"
#include "crc.h"
#include "tracing.h"

void hldc_reset_context(hldc_context_t *ctx)
{
    if (!ctx)
    {
        TRACE("Invalid input parameter: ctx=%p", (void *)ctx);
    }
    ctx->state = HLDC_STATE_START_FLAG;
    ctx->current_frame.payload_length = 0;
    ctx->current_frame.trailer.crc = 0;
    ctx->current_frame.header.address = 0;
    ctx->current_frame.header.control = 0;
    ctx->decode_buffer_index = 0;
    ctx->bytes_left_to_decode = 0;
}

void call_on_error(hldc_context_t *ctx, hldc_status_t status)
{
    if (ctx->on_error)
    {
        ctx->on_error(status);
    }
}

void call_on_frame(hldc_context_t *ctx)
{
    if (ctx->on_frame)
    {
        ctx->on_frame(&ctx->current_frame);
    }
}

void hldc_restart_buffer(hldc_context_t *ctx, bool is_last_packet_successfull)
{

    ctx->state = HLDC_STATE_START_FLAG;
    if (is_last_packet_successfull)
    {
        // Reset to the end flag of the last successfully decoded frame, so that if the next packet starts immediately after, we can detect it without waiting for the next start flag
        memmove(ctx->decode_buffer, ctx->decode_buffer + ctx->decode_buffer_index - 1, ctx->bytes_left_to_decode + 1);
        ctx->decode_buffer_index = 0;
        ctx->bytes_left_to_decode += 1; // Account for the moved end flag byte
    }
    else
    {
        // Move the decode buffer one byte left and start scanning again for start of packet
        size_t bytes_left_to_decode = ctx->bytes_left_to_decode + ctx->decode_buffer_index - 1;
        if (bytes_left_to_decode > 0)
        {
            memmove(ctx->decode_buffer, ctx->decode_buffer + 1, bytes_left_to_decode);
        }
        ctx->decode_buffer_index = 0;
        ctx->bytes_left_to_decode = bytes_left_to_decode;
    }
    ctx->current_frame.payload_length = 0;
    ctx->current_frame.trailer.crc = 0;
    ctx->current_frame.header.address = 0;
    ctx->current_frame.header.control = 0;
}

hldc_status_t hldc_handle_end_byte(hldc_context_t *ctx)
{
    uint16_t received_crc = (ctx->current_frame.payload[ctx->current_frame.payload_length - 2]) |
                            (ctx->current_frame.payload[ctx->current_frame.payload_length - 1] << 8);
    if (ctx->current_frame.payload_length < HLDC_TRAILER_SIZE)
    {
        TRACE("Not enough bytes for CRC in payload: payload_length=%zu", ctx->current_frame.payload_length);
        hldc_restart_buffer(ctx, false); // Reset context on decode error
        return HLDC_STATUS_DECODE_ERROR;
    }
    bool has_payload = ctx->current_frame.payload_length > HLDC_TRAILER_SIZE;
    if (has_payload)
    {
        ctx->current_frame.payload_length -= HLDC_TRAILER_SIZE; // Remove CRC from payload length
    }
    else
    {
        ctx->current_frame.payload_length = 0; // No payload, only CRC
    }

    // Calculate CRC over [address, control, payload] and compare with received CRC
    uint16_t crc_input_length = HLDC_HEADER_SIZE + ctx->current_frame.payload_length;
    uint8_t crc_input[crc_input_length];
    memcpy(crc_input, &ctx->current_frame.header, HLDC_HEADER_SIZE);
    if (has_payload)
    {
        memcpy(crc_input + HLDC_HEADER_SIZE, ctx->current_frame.payload, ctx->current_frame.payload_length);
    }
    uint16_t calculated_crc = crc16_calculate(crc_input, crc_input_length, 0xFFFF);
    if (received_crc != calculated_crc)
    {
        TRACE("CRC error: received 0x%04X, calculated 0x%04X", received_crc, calculated_crc);
        hldc_restart_buffer(ctx, false); // Reset context on CRC error
        return HLDC_STATUS_CRC_ERROR;
    }
    call_on_frame(ctx);             // Call the frame handler callback
    hldc_restart_buffer(ctx, true); // Reset context for next frame
    return HLDC_STATUS_OK;
}

hldc_status_t hldc_insert_escape_char(uint8_t *buffer, size_t index, uint8_t byte, size_t buffer_length)
{
    if (index + 1 > buffer_length)
    {
        TRACE("Cannot insert escape character: index %zu out of bounds for buffer length %zu", index, buffer_length);
        return HLDC_STATUS_INPUT_PARAMS_ERROR; // Index out of bounds
    }
    uint8_t escape_char = byte ^ HLDC_ESCAPE_XOR;
    buffer[index] = HLDC_ESCAPE_CHAR;
    buffer[index + 1] = escape_char;
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
    uint16_t crc = crc16_calculate((uint8_t *)&frame->header, HLDC_HEADER_SIZE, 0xFFFF);
    crc = crc16_calculate(frame->payload, frame->payload_length, crc);

    size_t idx = 0;

    buffer[idx++] = HLDC_START_FLAG;
    buffer[idx++] = frame->header.address;
    buffer[idx++] = frame->header.control;

    // Byte-stuff payload into output buffer
    if (frame->payload_length > 0)
    {
        size_t payload_index = 0;
        while (idx < HLDC_MAX_FRAME_SIZE && payload_index < frame->payload_length)
        {
            if (frame->payload[payload_index] == HLDC_ESCAPE_CHAR || frame->payload[payload_index] == HLDC_END_FLAG)
            {
                if (hldc_insert_escape_char(buffer, idx, frame->payload[payload_index], *length) != HLDC_STATUS_OK)
                {
                    return HLDC_STATUS_INPUT_PARAMS_ERROR; // Failed to insert escape character
                }
                idx++;
            }
            else
            {
                buffer[idx] = frame->payload[payload_index];
            }
            payload_index++;
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
        ctx->state = HLDC_STATE_ESCAPED_CHAR;
        return HLDC_STATUS_OK; // Next byte will be handled in the next call to hldc_handle_byte
        break;
    default:
        if (ctx->current_frame.payload_length < MAX_PAYLOAD_SIZE + HLDC_TRAILER_SIZE)
        {
            ctx->current_frame.payload[ctx->current_frame.payload_length] = byte;
            ctx->current_frame.payload_length++;
            return HLDC_STATUS_OK;
        }
        else
        {
            TRACE("Payload overflow: payload length %zu exceeds maximum %zu", ctx->current_frame.payload_length, MAX_PAYLOAD_SIZE + HLDC_TRAILER_SIZE);
            hldc_restart_buffer(ctx, false); // Reset context on overflow
            return HLDC_STATUS_DECODE_ERROR;
        }
        break;
    }
}

hldc_status_t hldc_handle_byte(hldc_context_t *ctx, uint8_t byte)
{
    switch (ctx->state)
    {
    case HLDC_STATE_START_FLAG:
        if (byte == HLDC_START_FLAG)
        {
            ctx->state = HLDC_STATE_ADDRESS;
            ctx->current_frame.payload_length = 0; // Reset payload length for new frame
        }
        break;
    case HLDC_STATE_ADDRESS:
        ctx->current_frame.header.address = byte;
        ctx->state = HLDC_STATE_CONTROL;
        break;
    case HLDC_STATE_CONTROL:
        ctx->current_frame.header.control = byte;
        ctx->current_frame.payload_length = 0;
        ctx->state = HLDC_STATE_PAYLOAD;
        break;
    case HLDC_STATE_PAYLOAD:
        hldc_status_t status = hldc_handle_payload(ctx, byte);
        if (status != HLDC_STATUS_OK)
        {
            call_on_error(ctx, status); // Call error callback on payload handling error
            return status;              // Payload handling error (e.g., overflow or CRC error)
        }
        break;
    case HLDC_STATE_ESCAPED_CHAR:
        uint8_t escaped_char = byte ^ HLDC_ESCAPE_XOR;

        if (ctx->current_frame.payload_length < MAX_PAYLOAD_SIZE)
        {
            ctx->current_frame.payload[ctx->current_frame.payload_length] = escaped_char;
            ctx->current_frame.payload_length++;
        }
        else
        {
            hldc_restart_buffer(ctx, false);              // Reset context on overflow
            call_on_error(ctx, HLDC_STATUS_DECODE_ERROR); // Call error callback on overflow
            return HLDC_STATUS_DECODE_ERROR;
        }
        ctx->state = HLDC_STATE_PAYLOAD; // Return to payload state after handling escaped
        break;
    default:
        break;
    }
    return HLDC_STATUS_OK;
}

hldc_status_t hldc_push_bytes(hldc_context_t *ctx, const uint8_t *data, size_t length)
{
    hldc_status_t status = HLDC_STATUS_OK;
    if (!ctx || !data || length == 0)
    {
        TRACE("Invalid input parameters: ctx=%p, data=%p, length=%zu", (void *)ctx, (void *)data, length);
        return HLDC_STATUS_INPUT_PARAMS_ERROR;
    }
    if (ctx->decode_buffer_index + length > sizeof(ctx->decode_buffer))
    {
        TRACE("Decode buffer overflow: current index %zu, incoming length %zu, buffer size %zu", ctx->decode_buffer_index, length, sizeof(ctx->decode_buffer));
        return HLDC_STATUS_DECODE_ERROR; // Not enough space in decode buffer
    }
    memcpy(ctx->decode_buffer + ctx->decode_buffer_index, data, length);
    ctx->bytes_left_to_decode += length;
    return HLDC_STATUS_OK;
}

hldc_status_t hldc_decode_pushed_bytes(hldc_context_t *ctx)
{
    while (ctx->bytes_left_to_decode > 0)
    {
        hldc_status_t status = HLDC_STATUS_OK;
        status = hldc_handle_byte(ctx, ctx->decode_buffer[ctx->decode_buffer_index]);
        if (status == HLDC_STATUS_OK)
        {
            ctx->decode_buffer_index++;
            ctx->bytes_left_to_decode--;
        }
    }
    return HLDC_STATUS_OK;
}