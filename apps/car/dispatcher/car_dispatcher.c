#include "commander.h"
#include "tracing.h"
#include "car_dispatcher.h"
#include "car.h"

commander_status_t command_handle(F1rc__Command * cmd, F1rc__Response *response, bool expected_response)
{
    switch (cmd->id)
    {
    case F1RC__COMMAND__COMMAND_GET_VERSION:
        TRACE("Handling GET_VERSION command\n");
        if (expected_response)
        {
            response->id = F1RC__RESPONSE_ID__RESPONSE_ID_GET_VERSION;
            response->response_case = F1RC__RESPONSE__RESPONSE_GET_VERSION;
            f1rc__get_version_response__init(response->get_version);
            response->get_version->major = version[0];
            response->get_version->minor = version[1];
            response->get_version->patch = version[2];
        }
        // Handle get version command
        break;
    default:
        return COMMANDER_STATUS_INVALID_COMMAND;
    }
    return COMMANDER_STATUS_OK;
}