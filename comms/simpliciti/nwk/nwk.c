#include "nwk.h"
#include "tracing.h"

static void nwk_decode_port(uint8_t byte, nwk_port_t *port)
{
    port->forwarded = (byte & NWK_PORT_FORWARED_MASK) >> NWK_PORT_FORWARDED_SHIFT;
    port->encrypted = (byte & NWK_PORT_ENCRYPTED_MASK) >> NWK_PORT_ENCRYPTED_SHIFT;
    port->app_port = byte & NWK_PORT_APP_PORT_MASK;
}

static void nwk_decode_device_info(uint8_t byte, nwk_device_info_t *device_info)
{
    device_info->ack_req = (byte & NWK_DEVICE_INFO_ACK_REQ_MASK) >> NWK_DEVICE_INFO_ACK_REQ_SHIFT;
    device_info->receiver_type = (byte & NWK_DEVICE_INFO_RECEIVER_TYPE_MASK) >> NWK_DEVICE_INFO_RECEIVER_TYPE_SHIFT;
    device_info->sender_type = (byte & NWK_DEVICE_INFO_SENDER_TYPE_MASK) >> NWK_DEVICE_INFO_SENDER_TYPE_SHIFT;
    device_info->ack_reply = (byte & NWK_DEVICE_INFO_ACK_REPLY_MASK) >> NWK_DEVICE_INFO_ACK_REPLY_SHIFT;
    device_info->hop_count = byte & NWK_DEVICE_INFO_HOP_COUNT_MASK;
}

simpliciti_status_t nwk_decode_frame(const uint8_t *data, size_t length, nwk_frame_t *frame)
{
    if (length > NWK_MAX_FRAME_SIZE || length < NWK_HEADER_SIZE)
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
    nwk_decode_port(data[0], &frame->header.port);
    nwk_decode_device_info(data[1], &frame->header.device_info);
    frame->header.tractid = data[2];

    // Validate payload length
    if (frame->header.port.app_port > NWK_MAX_PAYLOAD_SIZE)
    {
        TRACE("Payload length exceeds maximum: %u\n", frame->header.port.app_port);
        return SIMPLICITI_ERROR_DECODING;
    }

    // Copy payload
    frame->payload = (uint8_t *)(data + NWK_HEADER_SIZE);
    frame->payload_length = length - NWK_HEADER_SIZE;

    return SIMPLICITI_SUCCESS;
}

static uint8_t nwk_encode_port(const nwk_port_t *port)
{
    return (port->forwarded << NWK_PORT_FORWARDED_SHIFT) |
           (port->encrypted << NWK_PORT_ENCRYPTED_SHIFT) |
           (port->app_port);
}

static uint8_t nwk_encode_device_info(const nwk_device_info_t *device_info)
{
    return (device_info->ack_req << NWK_DEVICE_INFO_ACK_REQ_SHIFT) |
           (device_info->receiver_type << NWK_DEVICE_INFO_RECEIVER_TYPE_SHIFT) |
           (device_info->sender_type << NWK_DEVICE_INFO_SENDER_TYPE_SHIFT) |
           (device_info->ack_reply << NWK_DEVICE_INFO_ACK_REPLY_SHIFT) |
           (device_info->hop_count);
}

simpliciti_status_t nwk_encode_frame(const nwk_frame_t *frame, uint8_t *buffer, size_t *length)
{
    if (frame == NULL || buffer == NULL || length == NULL)
    {
        TRACE("Null pointer provided for frame, buffer or length\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    if (frame->payload_length > NWK_MAX_PAYLOAD_SIZE)
    {
        TRACE("Payload length exceeds maximum: %u\n", frame->payload_length);
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }

    // Encode header
    buffer[0] = nwk_encode_port(&frame->header.port);
    buffer[1] = nwk_encode_device_info(&frame->header.device_info);
    buffer[2] = frame->header.tractid;

    // Copy payload
    if (frame->payload != NULL)
    {
        memcpy(buffer + NWK_HEADER_SIZE, frame->payload, frame->payload_length);
    }

    *length = NWK_HEADER_SIZE + frame->payload_length;
    return SIMPLICITI_SUCCESS;
}