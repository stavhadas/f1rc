#include "commander.h"
#include "protobuf_static_allocator.h"
#include <protobuf-c/protobuf-c.h>
#include <string.h>

#include "main.pb-c.h"

#include "tracing.h"

static MemPool pool;

static ProtobufCAllocator pool_allocator = {
    .alloc = pool_alloc,
    .free = pool_free,
    .allocator_data = &pool,
};

typedef struct
{
    uint8_t data[COMMANDER_MAX_MESSAGE_SIZE];
    size_t length;
    bool expected_response;
} commander_message_t;

commander_status_t commander_handle_command(commander_context_t *context, const uint8_t *data, size_t length, bool expected_response)
{
    pool_reset(&pool);
    F1rc__Command *cmd = f1rc__command__unpack(&pool_allocator, length, data);
    if (cmd == NULL)
    {
        TRACE("Failed to unpack Command\n");
        return COMMANDER_STATUS_PROTOBUF_ERROR;
    }

    TRACE("Received command with case: %d\n", cmd->command_case);
    if (context->handle_command == NULL)
    {
        TRACE("No command handler registered in context\n");
        return COMMANDER_STATUS_INVALID_COMMAND;
    }
    F1rc__Response response;
    f1rc__response__init(&response);
    commander_status_t status = context->handle_command(cmd, &response, expected_response);
    if (status != COMMANDER_STATUS_OK)
    {
        TRACE("Command handler returned error status: %d\n", status);
        return status;
    }
    if (expected_response)
    {
        size_t response_size = f1rc__response__get_packed_size(&response);
        uint8_t response_buffer[response_size];
        f1rc__response__pack(&response, response_buffer);
        if (context->send_response != NULL)
        {
            context->send_response(response_buffer, response_size);
        }
        else
        {
            TRACE("No send_response function registered in context\n");
        }
        f1rc__response__free_unpacked(&response, &pool_allocator); // Free any dynamically allocated fields in the response
    }
    // Free the unpacked message (no-op in our allocator)
    f1rc__command__free_unpacked(cmd, &pool_allocator);
    return COMMANDER_STATUS_OK;
}

static void commander_thread_entry(void *arg)
{
    commander_context_t *context = (commander_context_t *)arg;
    commander_message_t message;
    while (1)
    {
        if (osMessageQueueGet(context->queue, &message, NULL, osWaitForever) == osOK)
        {
            commander_handle_command(context, message.data, message.length, message.expected_response);
        }
    }
}

bool commander_init(commander_context_t *context, const osThreadAttr_t *thread_attr)
{
    // 4096 bytes covers protobuf unpack/pack, the caller's send_response
    // (which HLDC-encodes into an HLDC_MAX_FRAME_SIZE/1030-byte buffer) and
    // a blocking UART transmit -- well past configMINIMAL_STACK_SIZE.
    static const osThreadAttr_t default_attr = {
        .name = "commander",
        .stack_size = 4096,
    };
    if (thread_attr == NULL)
    {
        thread_attr = &default_attr;
    }

    context->queue = osMessageQueueNew(COMMANDER_QUEUE_DEPTH, sizeof(commander_message_t), NULL);
    if (context->queue == NULL)
    {
        return false;
    }
    return osThreadNew(commander_thread_entry, context, thread_attr) != NULL;
}

bool commander_post_command(commander_context_t *context, const uint8_t *data, size_t length, bool expected_response)
{
    if (length > COMMANDER_MAX_MESSAGE_SIZE)
    {
        TRACE("Command too large to queue: %zu > %d\n", length, COMMANDER_MAX_MESSAGE_SIZE);
        return false;
    }
    commander_message_t message;
    memcpy(message.data, data, length);
    message.length = length;
    message.expected_response = expected_response;
    return osMessageQueuePut(context->queue, &message, 0, 0) == osOK;
}
