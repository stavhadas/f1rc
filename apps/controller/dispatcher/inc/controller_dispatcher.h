#ifndef CONTROLLER_DISPATCHER_H
#define CONTROLLER_DISPATCHER_H

#include "commander.h"

commander_status_t command_handle(F1rc__Command * cmd, F1rc__Response *response, bool expected_response);

#endif