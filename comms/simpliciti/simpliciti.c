#include <string.h>
#include "simpliciti.h"
#include "tracing.h"

static simpliciti_outgoing_msg_t g_pending_msgs[SIMPLICITI_MAX_TID] = {0};

simpliciti_status_t simpliciti_init(simpliciti_context_t *context)
{
    context->link_info.link_state = SIMPLICITI_LINK_STATE_DISCONNECTED;
    context->link_info.dest_address = 0;
    context->last_tid = 0;
    memset(g_pending_msgs, 0, sizeof(g_pending_msgs));
    return SIMPLICITI_SUCCESS;
}

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
        .payload_length = simplicity_get_payload_length(frame)};
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
        .payload_length = simplicity_get_payload_length(frame) + NWK_HEADER_SIZE};

    status = mfri_encode_frame(&mfri_frame, buffer, length);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to encode MFRI frame: %d\n", status);
        return status;
    }
    return SIMPLICITI_SUCCESS;
}

simpliciti_status_t simpliciti_send_msg(simpliciti_context_t *context, simpliciti_frame_t *frame, bool is_response, bool is_retransmission)
{
    simpliciti_status_t status;
    uint8_t buffer[SIMPLICITI_MAX_FRAME_SIZE];
    size_t length = sizeof(buffer);

    // Match tractid
    if (!is_response)
    {
        frame->nwk_header.tractid = context->last_tid + 1;
        context->last_tid = frame->nwk_header.tractid;
    }
    else
    {
        frame->nwk_header.device_info.ack_reply = 1; // Set ACK reply flag for responses
    }

    // Encode frame
    status = simpliciti_encode_frame(frame, buffer, &length);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to encode frame for sending: %d\n", status);
        return status;
    }

    // Send frame using callback
    if (context->callbacks.send_msg == NULL)
    {
        TRACE("No send_msg callback registered in context\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    status = context->callbacks.send_msg(buffer, length);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("send_msg callback failed: %d\n", status);
        return status;
    }

    // Save for retransmission if ACK is required
    if (frame->nwk_header.device_info.ack_req && !is_retransmission && !is_response)
    {
        TRACE("Message with TID %u requires ACK, saving for potential retransmission\n", frame->nwk_header.tractid);
        if (memcmp(&g_pending_msgs[frame->nwk_header.tractid].frame, &(simpliciti_frame_t){0}, sizeof(simpliciti_frame_t)) != 0)
        {
            TRACE("Warning: Overwriting pending message with TID %u\n", frame->nwk_header.tractid);
        }
        // Store pending message for potential retransmission if ACK is required
        memcpy(&g_pending_msgs[frame->nwk_header.tractid].frame, frame, sizeof(simpliciti_frame_t));
        g_pending_msgs[frame->nwk_header.tractid].retries = SIMPLICITI_MAX_RETRIES;
        g_pending_msgs[frame->nwk_header.tractid].in_use = true;
        if (context->callbacks.get_time_ms != NULL)
        {
            uint32_t now;
            if (context->callbacks.get_time_ms(&now) == SIMPLICITI_SUCCESS)
            {
                g_pending_msgs[frame->nwk_header.tractid].timeout = now + SIMPLICITI_ACK_TIMEOUT_MS;
                g_pending_msgs[frame->nwk_header.tractid].is_timeout_set = true;
            }
            else
            {
                TRACE("get_time_ms callback failed, cannot set ACK timeout\n");
                g_pending_msgs[frame->nwk_header.tractid].timeout = 0; // No timeout
                g_pending_msgs[frame->nwk_header.tractid].is_timeout_set = false;
            }
        }
        else
        {
            TRACE("No get_time_ms callback registered, cannot set ACK timeout\n");
            g_pending_msgs[frame->nwk_header.tractid].timeout = 0; // No timeout
            g_pending_msgs[frame->nwk_header.tractid].is_timeout_set = false;
        }
    }
    return SIMPLICITI_SUCCESS;
}

simpliciti_status_t simpliciti_check_outgoing_messages(simpliciti_context_t *context)
{
    for (size_t i = 0; i < SIMPLICITI_MAX_TID; i++)
    {
        if (g_pending_msgs[i].in_use && g_pending_msgs[i].is_timeout_set)
        {
            uint32_t now;
            if (context->callbacks.get_time_ms != NULL && context->callbacks.get_time_ms(&now) == SIMPLICITI_SUCCESS)
            {
                if (now >= g_pending_msgs[i].timeout)
                {
                    if (g_pending_msgs[i].retries > 0)
                    {
                        TRACE("ACK timeout for TID %zu, retransmitting message\n", i);
                        simpliciti_send_msg(context, &g_pending_msgs[i].frame, false, true);
                        g_pending_msgs[i].retries--;
                        g_pending_msgs[i].timeout = now + SIMPLICITI_ACK_TIMEOUT_MS;
                    }
                    else
                    {
                        TRACE("Message with TID %zu failed after maximum retries\n", i);
                        // TODO: Notify application layer of failure if desired
                        memset(&g_pending_msgs[i], 0, sizeof(simpliciti_outgoing_msg_t));
                    }
                }
            }
            else
            {
                TRACE("get_time_ms callback failed, cannot check ACK timeouts\n");
                return SIMPLICITI_ERROR_INVALID_PARAM;
            }
        }
    }
}

static simpliciti_status_t simpliciti_handle_ping(simpliciti_context_t *context, const simpliciti_frame_t *frame)
{
    if (simplicity_get_payload_length(frame) < sizeof(simpliciti_ping_payload_t))
    {
        TRACE("Ping payload too short: %zu bytes\n", simplicity_get_payload_length(frame));
        return SIMPLICITI_ERROR_DECODING;
    }
    simpliciti_ping_payload_t *ping_payload = (simpliciti_ping_payload_t *)frame->payload;
    switch (ping_payload->request)
    {
    case NWK_PORT_PING_REQUEST:
        simpliciti_frame_t response = {0};
        TRACE("Received ping message from 0x%08X with TID %u\n", frame->mfri_header.srcaddr, frame->nwk_header.tractid);

        // Copy request frame to response, then modify fields for reply
        memcpy(&response, frame, sizeof(simpliciti_frame_t));

        // Swap addresses
        uint32_t me = context->device_address;
        response.mfri_header.dstaddr = frame->mfri_header.srcaddr;
        response.mfri_header.srcaddr = me;

        // Set response payload
        simpliciti_ping_payload_t response_payload = {
            .request = NWK_PORT_PING_RESPONSE};
        memcpy(response.payload, &response_payload, sizeof(simpliciti_ping_payload_t));

        // Send response
        return simpliciti_send_msg(context, &response, true, false);
        break;
    case NWK_PORT_PING_RESPONSE:
        if (g_pending_msgs[frame->nwk_header.tractid].in_use)
        {
            TRACE("Received ping response from 0x%08X with TID %u\n", frame->mfri_header.srcaddr, frame->nwk_header.tractid);
            uint32_t now = 0;
            if (context->callbacks.get_time_ms && context->callbacks.get_time_ms(&now) == SIMPLICITI_SUCCESS)
            {
                uint32_t rtt = now - context->last_ping_timestamp;
                TRACE("Ping RTT: %u ms\n", rtt);
                if (context->callbacks.successful_ping != NULL)
                {
                    context->callbacks.successful_ping(frame->mfri_header.srcaddr, rtt);
                }
            }
            else
            {
                TRACE("get_time_ms callback failed, cannot calculate ping RTT\n");
            }
        }
        else
        {
            TRACE("Received unexpected ping response from 0x%08X with TID %u\n", frame->mfri_header.srcaddr, frame->nwk_header.tractid);
        }
        // TODO: Implement handling of received ping response, e.g. notify application layer or update state
        break;
    default:
        TRACE("Unknown ping request type: %u\n", ping_payload->request);
        return SIMPLICITI_ERROR_INVALID_PARAM;
        break;
    }
    return SIMPLICITI_SUCCESS;
}

static simpliciti_status_t simpliciti_handle_link(simpliciti_context_t *context, const simpliciti_frame_t *frame)
{
    TRACE("Received link message on port %u\n", frame->nwk_header.port.app_port);
    // Handle link messages
    return SIMPLICITI_SUCCESS;
}

static simpliciti_status_t simpliciti_handle_app_message(simpliciti_context_t *context, const simpliciti_frame_t *frame)
{
    TRACE("Received message on port %u\n", frame->nwk_header.port.app_port);
    // Handle application-level messages
    return SIMPLICITI_SUCCESS;
}

static simpliciti_status_t simpliciti_handle_control_packet(simpliciti_context_t *context, const simpliciti_frame_t *frame)
{
    switch (frame->nwk_header.port.app_port)
    {
    case NWK_PORT_PING:
        return simpliciti_handle_ping(context, frame);
    case NWK_PORT_LINK:
        return simpliciti_handle_link(context, frame);
    default:
        TRACE("Received control packet on unsupported port %u\n", frame->nwk_header.port.app_port);
        return SIMPLICITI_ERROR_INVALID_PARAM;
        break;
    }
}

static simpliciti_status_t simpliciti_handle_packet(simpliciti_context_t *context, const simpliciti_frame_t *frame)
{
    if (context == NULL || frame == NULL)
    {
        TRACE("Null pointer provided for context or frame\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }

    // Check dstaddr in MFRI header matches device address in context
    if ((frame->mfri_header.dstaddr != context->device_address) &&
        (frame->mfri_header.dstaddr != SIMPLICITI_BROADCAST_PORT)) // Allow broadcast address
    {
        TRACE("Received frame with destination address 0x%08X does not match device address 0x%08X\n",
              frame->mfri_header.dstaddr, context->device_address);
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }

    // Route by port
    if (frame->nwk_header.port.app_port <= SIMPLICITI_MAX_RESERVED_PORT)
    {
        return simpliciti_handle_control_packet(context, frame);
    }
    else if (frame->nwk_header.port.app_port <= SIMPLICITI_MAX_APP_PORT)
    {
        return simpliciti_handle_app_message(context, frame);
    }
    else if (frame->nwk_header.port.app_port == SIMPLICITI_BROADCAST_PORT)
    {
        TRACE("Received broadcast message on port %u\n", frame->nwk_header.port.app_port);
        // Handle broadcast messages
    }
    else
    {
        TRACE("Received message on unsupported port %u\n", frame->nwk_header.port.app_port);
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    return SIMPLICITI_SUCCESS;
}

simpliciti_status_t simpliciti_receive_msg(simpliciti_context_t *context, const uint8_t *buffer, size_t length)
{
    simpliciti_frame_t frame;

    simpliciti_status_t status = simpliciti_decode_frame(buffer, length, &frame);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to decode received frame: %d\n", status);
        return status;
    }

    status = simpliciti_handle_packet(context, &frame);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to handle received packet: %d\n", status);
        return status;
    }

    // Message was a reply to a message we sent that requires ACK; clear pending message
    if (frame.nwk_header.device_info.ack_reply)
    {
        g_pending_msgs[frame.nwk_header.tractid].in_use = false;
    }
    return SIMPLICITI_SUCCESS;
}

simpliciti_status_t simpliciti_send_ping(simpliciti_context_t *context, uint32_t dest_address)
{
    simpliciti_frame_t frame = {};
    frame.mfri_header.dstaddr = dest_address;
    frame.mfri_header.srcaddr = context->device_address;
    frame.mfri_header.length = (uint8_t)(sizeof(nwk_header_t) + sizeof(simpliciti_ping_payload_t));
    frame.nwk_header.device_info.ack_req = true; // Request ACK for ping messages
    frame.nwk_header.port.app_port = NWK_PORT_PING;
    simpliciti_ping_payload_t ping_payload = {
        .request = NWK_PORT_PING_REQUEST};
    memcpy(frame.payload, &ping_payload, sizeof(simpliciti_ping_payload_t));

    return simpliciti_send_msg(context, &frame, false, false);
}