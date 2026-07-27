// #include "commander.h"
#include "simpliciti.h"
#include "tracing.h"
// #include "car_dispatcher.h"
#include "car_bsp.h"
#include "car.h"
#include "hldc.h"
#include "cmsis_os2.h"
uint32_t version[] = {1, 0, 0};

#define CAR_COMMAND_PORT 0x20

// static commander_context_t commander_context;

// static simpliciti_status_t handle_app_message(const simpliciti_frame_t *frame)
// {
//     if (frame->nwk_header.port.app_port == CAR_COMMAND_PORT)
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
//     .device_address = 0x78901234, // Set this to your device's address
//     .link_info = {0},
//     .last_tid = 0,
// };

// static commander_status_t send_response(const uint8_t *data, size_t length)
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
//     frame.nwk_header.port.app_port = CAR_COMMAND_PORT;
//     memcpy(frame.payload, data, length);
//     return simpliciti_send_msg(&simpliciti_context, &frame, true, false);
// }

static hldc_context_t cmd_uart_hldc_context;

static void on_cmd_uart_hldc_decode(hldc_frame_t *frame)
{
    while (1)
    {
    }
}

static void on_cmd_uart_recv(uint8_t *buffer, size_t length)
{
    hldc_push_bytes(&cmd_uart_hldc_context, buffer, length);
}
static void car_test_thread(void *arg)
{
    car_context_t *context = (car_context_t *)arg;
    uint8_t test_data[] = "Hello, World!\n";
    uint8_t encoded_frame[HLDC_MAX_FRAME_SIZE];
    size_t encoded_length;
    hldc_frame_t frame = {0};
    frame.header.address = 0x01;
    frame.header.control = 0x02;
    frame.payload_length = sizeof(test_data);
    memcpy(frame.payload, test_data, sizeof(test_data));
    hldc_encode(&frame, encoded_frame, &encoded_length);
    while (1)
    {
        uart_interface_send(&context->cmd_uart, encoded_frame, encoded_length);
        osDelay(1000);
    }
}

void car_init(car_context_t *context)
{
    context->cmd_uart.on_recv = on_cmd_uart_recv;
    cmd_uart_hldc_context.on_frame = on_cmd_uart_hldc_decode;

    osThreadNew(car_test_thread, context, NULL);

    if (!car_bsp_init(context))
    {
        while (1)
            ;
    }
}

void car_start(car_context_t *context)
{
    // Initialize commander context and register command handler
    // commander_context.handle_command = command_handle;
    // commander_context.send_response = send_response;

    // simpliciti_init(&simpliciti_context);
    // TRACE("Car started with device address 0x%08X\n", simpliciti_context.device_address);
}