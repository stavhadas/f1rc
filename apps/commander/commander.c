#include "commander.h"
#include "protobuf_static_allocator.h"
#include <protobuf-c/protobuf-c.h>

#include "main.pb-c.h"

#include "tracing.h"

static MemPool pool;

static ProtobufCAllocator pool_allocator = {
    .alloc = pool_alloc,
    .free = pool_free,
    .allocator_data = &pool,
};

commander_status_t commander_handle_command(commander_context_t *context, const uint8_t *data, size_t length, bool expected_response)
{
    pool_reset(&pool);
    F1rc__Command *cmd = f1rc__command__unpack(&pool_allocator, length, data);
    if (cmd == NULL)
    {
        TRACE("Failed to unpack Command\n");
        return COMMANDER_STATUS_PROTOBUF_ERROR;
    }

    TRACE("Received command with ID: %d\n", cmd->id);
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