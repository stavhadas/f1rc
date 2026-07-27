#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>
#include "uart_interface.h"

extern uint32_t version[];

typedef struct {
    uart_interface_t cmd_uart;
} controller_context_t;

void controller_init(controller_context_t *context);

void controller_start(controller_context_t *context);

#endif /* CONTROLLER_H */
