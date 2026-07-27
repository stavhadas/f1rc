#include "commander.h"
#include "tracing.h"
#include "controller_dispatcher.h"
#include "controller.h"

commander_status_t command_handle(F1rc__Command * cmd, F1rc__Response *response, bool expected_response)
{
    switch (cmd->command_case)
    {
    case F1RC__COMMAND__COMMAND_GET_VERSION:
        TRACE("Handling GET_VERSION command\n");
        if (expected_response)
        {
            // response->get_version is a NULL oneof union pointer until
            // pointed at real storage; synchronous/single-threaded command
            // handling, so a static local is fine (mirrors commander.c's
            // own static MemPool pattern).
            static F1rc__GetVersionResponse get_version_response;
            f1rc__get_version_response__init(&get_version_response);
            get_version_response.major = version[0];
            get_version_response.minor = version[1];
            get_version_response.patch = version[2];
            response->response_case = F1RC__RESPONSE__RESPONSE_GET_VERSION;
            response->get_version = &get_version_response;
        }
        break;
    default:
        return COMMANDER_STATUS_INVALID_COMMAND;
    }
    return COMMANDER_STATUS_OK;
}