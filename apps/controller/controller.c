#include "commander.h"
// #include "simpliciti.h"
#include "tracing.h"
#include "controller_dispatcher.h"
#include "controller_bsp.h"
#include "controller.h"
#include "hldc.h"
#include "hldc_thread.h"
#include "cc1101_thread.h"
#include <string.h>

uint32_t version[] = {1, 0, 0};

// #define CONTROLLER_COMMAND_PORT 0x20

static commander_context_t commander_context;

// static simpliciti_status_t handle_app_message(const simpliciti_frame_t *frame)
// {
//     if (frame->nwk_header.port.app_port == CONTROLLER_COMMAND_PORT)
//     {
//         commander_status_t status = commander_handle_command(&commander_context, frame->payload, simpliciti_get_payload_length(frame), frame->nwk_header.device_info.ack_req);
//         if (status != COMMANDER_STATUS_OK)
//         {
//             TRACE("Failed to handle command: %d\n", status);
//             return SIMPLICITI_ERROR_DECODING;
//         }
//     }
//     TRACE("Received message on unsupported port %u\n", frame->nwk_header.port.app_port);
//     return SIMPLICITI_SUCCESS;
// }

// static simpliciti_context_t simpliciti_context = {
//     .callbacks = {
//         .send_msg = NULL,                         // Set this to your actual send message function
//         .get_time_ms = NULL,                      // Set this to your actual get time function
//         .handle_app_message = handle_app_message, // Set this to your actual app message handler
//         .link_state_change = NULL,                // Set this to your actual link state change handler
//         .out_of_retransmission = NULL,            // Set this to your actual out of retransmission handler
//         .successful_ping = NULL                   // Set this to your actual successful ping handler
//     },
//     .device_address = 0x12345678, // Set this to your device's address
//     .link_info = {0},
//     .last_tid = 0,
// };

// static commander_status_t simpliciti_send_response(const uint8_t *data, size_t length)
// {
//     if (simpliciti_context.link_info.link_state != SIMPLICITI_LINK_STATE_CONNECTED)
//     {
//         TRACE("Cannot send response, link is not connected\n");
//         return COMMANDER_STATUS_INVALID_COMMAND;
//     }
//     simpliciti_frame_t frame = {};
//     frame.mfri_header.dstaddr = simpliciti_context.link_info.dest_address;
//     frame.mfri_header.srcaddr = simpliciti_context.device_address;
//     frame.mfri_header.length = (uint8_t)(sizeof(nwk_header_t) + length);
//     frame.nwk_header.port.app_port = CONTROLLER_COMMAND_PORT;
//     memcpy(frame.payload, data, length);
//     return simpliciti_send_msg(&simpliciti_context, &frame, true, false);
// }

static hldc_context_t cmd_uart_hldc_context;
static controller_context_t *g_controller_context;

static commander_status_t send_response(const uint8_t *data, size_t length)
{
    uint8_t encoded_frame[HLDC_MAX_FRAME_SIZE];
    size_t encoded_length;
    hldc_frame_t frame = {0};
    frame.header.address = 0x01;
    frame.header.control = 0x02;
    frame.payload_length = length;
    memcpy(frame.payload, data, length);
    if (hldc_encode(&frame, encoded_frame, &encoded_length) != HLDC_STATUS_OK)
    {
        return COMMANDER_STATUS_INVALID_COMMAND;
    }
    uart_interface_send(&g_controller_context->cmd_uart, encoded_frame, encoded_length);
    return COMMANDER_STATUS_OK;
}

static void on_cmd_uart_hldc_decode(hldc_frame_t *frame)
{
    // Runs on the HLDC module's own decode thread (see hldc_start_decode_thread),
    // NOT ISR context -- but just hands off to the commander's queue rather
    // than dispatching here directly, so the (unbounded) protobuf work runs
    // on the commander's own thread instead of blocking further HLDC decode.
    commander_post_command(&commander_context, frame->payload, frame->payload_length, true);
}

static void on_cmd_uart_recv(uint8_t *buffer, size_t length)
{
    // ISR context -- bounded, non-blocking, matches car.c's equivalent.
    hldc_push_bytes(&cmd_uart_hldc_context, buffer, length);
}

void controller_init(controller_context_t *context)
{
    g_controller_context = context;
    context->cmd_uart.on_recv = on_cmd_uart_recv;
    cmd_uart_hldc_context.on_frame = on_cmd_uart_hldc_decode;

    commander_context.handle_command = command_handle;
    commander_context.send_response = send_response;

    if (!controller_bsp_init(context))
    {
        while (1)
            ;
    }

    // Both must come after controller_bsp_init(), which calls
    // osKernelInitialize() -- osThreadNew() before the kernel is initialized
    // is undefined behavior. Commander starts first so its queue exists
    // before the decode thread can ever call on_cmd_uart_hldc_decode.
    if (!commander_init(&commander_context, NULL))
    {
        while (1)
            ;
    }
    if (hldc_start_decode_thread(&cmd_uart_hldc_context, NULL) == NULL)
    {
        while (1)
            ;
    }

    // CC1101 bring-up (probe, then CW + FIFO refill on success) runs on its
    // own thread, not here: spi_interface_transfer() blocks on an RTOS
    // semaphore, and that can only ever be woken by the scheduler, which
    // isn't running yet at this point in controller_init() -- calling it
    // here would hang forever. A radio bring-up failure doesn't halt the
    // rest of controller startup either way.
    cc1101_start_bringup_thread(&context->cc1101, NULL);
}

void controller_start(controller_context_t *context)
{
    (void)context;
    controller_bsp_start();
}
