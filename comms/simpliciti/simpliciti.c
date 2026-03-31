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

static simpliciti_status_t simpliciti_handle_out_of_retransmissions(simpliciti_context_t *context, const simpliciti_frame_t *frame)
{
    switch (frame->nwk_header.port.app_port)
    {
    case NWK_PORT_PING_REQUEST:
        TRACE("Ping Failed after maximum retries, disconnecting\n");
        context->link_info.link_state = SIMPLICITI_LINK_STATE_DISCONNECTED;
        if (context->callbacks.link_state_change != NULL)
        {
            return context->callbacks.link_state_change(SIMPLICITI_LINK_STATE_DISCONNECTED);
        }
        break;
    case NWK_PORT_LINK:
        TRACE("Link message failed after maximum retries\n");
        context->link_info.link_state = SIMPLICITI_LINK_STATE_DISCONNECTED;
        if (context->callbacks.link_state_change != NULL)
        {
            return context->callbacks.link_state_change(SIMPLICITI_LINK_STATE_DISCONNECTED);
        }
        break;

    default:
        if (context->callbacks.out_of_retransmission != NULL)
        {
            return context->callbacks.out_of_retransmission(frame);
        }
        break;
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
                        simpliciti_handle_out_of_retransmissions(context, &g_pending_msgs[i].frame);
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
    return SIMPLICITI_SUCCESS;
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
                context->link_info.last_ping_timestamp = now;
                if (context->callbacks.successful_ping != NULL)
                {
                    context->callbacks.successful_ping();
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
    if (g_pending_msgs[frame->nwk_header.tractid].in_use)
    {
        if (context->link_info.link_state == SIMPLICITI_LINK_STATE_LINK_REQUESTED)
        {
            // Link message is a reply to our link request
            TRACE("Established link with 0x%08X\n", frame->mfri_header.srcaddr);
            context->link_info.link_state = SIMPLICITI_LINK_STATE_CONNECTED;
            context->link_info.dest_address = frame->mfri_header.srcaddr;
            if (context->callbacks.link_state_change != NULL)
            {
                context->callbacks.link_state_change(SIMPLICITI_LINK_STATE_CONNECTED);
            }
        }
        else
        {
            TRACE("Received unexpected ACK for link message with TID %u in link state %d\n", frame->nwk_header.tractid, context->link_info.link_state);
            return SIMPLICITI_ERROR_INVALID_PARAM;
        }
    }
    else
    {
        // Link message is a new link request from another device
        if (context->link_info.link_state == SIMPLICITI_LINK_STATE_WAITING_FOR_LINK)
        {
            TRACE("Received link request from 0x%08X while waiting for link\n", frame->mfri_header.srcaddr);
            
            // Reply with ACK
            simpliciti_frame_t response = {0};
            response.mfri_header.dstaddr = frame->mfri_header.srcaddr;
            response.mfri_header.srcaddr = context->device_address;
            response.mfri_header.length = (uint8_t)(sizeof(nwk_header_t)); // No payload for link messages
            response.nwk_header.port.app_port = NWK_PORT_LINK;
            response.nwk_header.device_info.ack_reply = 1;
            response.nwk_header.tractid = frame->nwk_header.tractid; // Echo back TID from request
            simpliciti_status_t status = simpliciti_send_msg(context, &response, true, false);
            if (status != SIMPLICITI_SUCCESS)
            {
                TRACE("Failed to send ACK for link request: %d\n", status);
                return status;
            }
            
            // Update link state to connected
            TRACE("Established link with 0x%08X\n", frame->mfri_header.srcaddr);
            context->link_info.link_state = SIMPLICITI_LINK_STATE_CONNECTED;
            context->link_info.dest_address = frame->mfri_header.srcaddr;
            if (context->callbacks.link_state_change != NULL)
            {
                context->callbacks.link_state_change(SIMPLICITI_LINK_STATE_CONNECTED);
            }
        }
        else
        {
            TRACE("Received unexpected link message from 0x%08X in link state %d\n", frame->mfri_header.srcaddr, context->link_info.link_state);
            return SIMPLICITI_ERROR_INVALID_PARAM;
        }
    }
    return SIMPLICITI_SUCCESS;
}

static simpliciti_status_t simpliciti_handle_app_message(simpliciti_context_t *context, const simpliciti_frame_t *frame)
{
    TRACE("Received message on port %u\n", frame->nwk_header.port.app_port);
    if (context->callbacks.handle_app_message == NULL)
    {
        TRACE("No handle_app_message callback registered in context\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    context->callbacks.handle_app_message(frame);
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

    // Route by port
    if (frame->nwk_header.port.app_port <= SIMPLICITI_MAX_RESERVED_PORT)
    {
        return simpliciti_handle_control_packet(context, frame);
    }
    else if (frame->nwk_header.port.app_port <= SIMPLICITI_MAX_APP_PORT)
    {
        return simpliciti_handle_app_message(context, frame);
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
    if (context->link_info.link_state != SIMPLICITI_LINK_STATE_CONNECTED &&
        context->link_info.link_state != SIMPLICITI_LINK_STATE_LINK_REQUESTED &&
        frame.mfri_header.dstaddr != SIMPLICITI_BROADCAST_ADDRESS)
    {
        TRACE("Cannot receive message, link is not connected\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    if (context->device_address != frame.mfri_header.dstaddr && frame.mfri_header.dstaddr != SIMPLICITI_BROADCAST_ADDRESS)
    {
        TRACE("Received message intended for 0x%08X, but device address is 0x%08X\n", frame.mfri_header.dstaddr, context->device_address);
        return SIMPLICITI_ERROR_INVALID_PARAM;
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
        if (g_pending_msgs[frame.nwk_header.tractid].in_use)
        {
            TRACE("Received ACK for TID %u, clearing pending message\n", frame.nwk_header.tractid);
        g_pending_msgs[frame.nwk_header.tractid].in_use = false;
        }
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

simpliciti_status_t simpliciti_send_link(simpliciti_context_t *context)
{
    if (!context)
    {
        TRACE("Null pointer provided for context\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    if (context->link_info.link_state != SIMPLICITI_LINK_STATE_DISCONNECTED)
    {
        TRACE("Cannot send link message: already in link state %d\n", context->link_info.link_state);
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }

    simpliciti_frame_t frame = {};
    frame.mfri_header.dstaddr = SIMPLICITI_BROADCAST_ADDRESS; // Broadcast link requests
    frame.mfri_header.srcaddr = context->device_address;
    frame.mfri_header.length = (uint8_t)(sizeof(nwk_header_t)); // No payload for link messages
    frame.nwk_header.port.app_port = NWK_PORT_LINK;
    frame.nwk_header.device_info.ack_req = true;
    simpliciti_status_t status = simpliciti_send_msg(context, &frame, false, false);
    if (status == SIMPLICITI_SUCCESS)
    {
        context->link_info.link_state = SIMPLICITI_LINK_STATE_LINK_REQUESTED;
        TRACE("Sent link request message, updated link state to LINK_REQUESTED\n");
        if (context->callbacks.link_state_change != NULL)
        {
            context->callbacks.link_state_change(SIMPLICITI_LINK_STATE_LINK_REQUESTED);
        }
    }
    else
    {
        TRACE("Failed to send link request message: %d\n", status);
    }
    return status;
}

simpliciti_status_t simpliciti_wait_for_link(simpliciti_context_t *context)
{
    if (!context)
    {
        TRACE("Null pointer provided for context\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    if (context->link_info.link_state != SIMPLICITI_LINK_STATE_DISCONNECTED)
    {
        TRACE("Cannot wait for link: already in link state %d\n", context->link_info.link_state);
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    context->link_info.link_state = SIMPLICITI_LINK_STATE_WAITING_FOR_LINK;
    TRACE("Set link state to WAITING_FOR_LINK, waiting for incoming link requests\n");
    if (context->callbacks.link_state_change != NULL)
    {
        context->callbacks.link_state_change(SIMPLICITI_LINK_STATE_WAITING_FOR_LINK);
    }
    return SIMPLICITI_SUCCESS;
}

simpliciti_status_t simpliciti_disconnect(simpliciti_context_t *context)
{
    if (!context)
    {
        TRACE("Null pointer provided for context\n");
        return SIMPLICITI_ERROR_INVALID_PARAM;
    }
    if (context->link_info.link_state != SIMPLICITI_LINK_STATE_CONNECTED)
    {
        TRACE("Not connected, no action taken\n");
        return SIMPLICITI_SUCCESS;
    }
    
    simpliciti_frame_t frame = {};
    frame.mfri_header.dstaddr = context->link_info.dest_address;
    frame.mfri_header.srcaddr = context->device_address;
    frame.nwk_header.port.app_port = NWK_PORT_UNLINK;
    frame.nwk_header.device_info.ack_req = false;
    frame.mfri_header.length = (uint8_t)(sizeof(nwk_header_t)); // No payload for unlink messages
    simpliciti_status_t status = simpliciti_send_msg(context, &frame, false, false);
    if (status != SIMPLICITI_SUCCESS)
    {
        TRACE("Failed to send unlink message: %d\n", status);
        return status;
    }

    context->link_info.link_state = SIMPLICITI_LINK_STATE_DISCONNECTED;
    context->link_info.dest_address = 0;
    TRACE("Set link state to DISCONNECTED\n");

    if (context->callbacks.link_state_change != NULL)
    {
        context->callbacks.link_state_change(SIMPLICITI_LINK_STATE_DISCONNECTED);
    }

    return SIMPLICITI_SUCCESS;

}