#ifndef COMMANDER_H
#define COMMANDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "main.pb-c.h"

typedef enum
{
    COMMANDER_STATUS_OK = 0,
    COMMANDER_STATUS_INVALID_COMMAND,
    COMMANDER_STATUS_PROTOBUF_ERROR
} commander_status_t;

typedef commander_status_t (*commander_command_handler_t)(F1rc__Command * cmd, F1rc__Response *response, bool expected_response);
typedef commander_status_t (*commander_send_response)(const uint8_t *data, size_t length);

typedef struct
{
    commander_command_handler_t handle_command;
    commander_send_response send_response;
} commander_context_t;

commander_status_t commander_handle_command(commander_context_t* context, const uint8_t *data, size_t length, bool expected_response);

#endif /* COMMANDER_H */