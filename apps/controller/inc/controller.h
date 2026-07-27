#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>
#include "uart_interface.h"
#include "spi_interface.h"
#include "cc1101.h"

extern uint32_t version[];

typedef struct {
    uart_interface_t cmd_uart;
    spi_interface_t cc1101_spi;
    cc1101_t cc1101;
} controller_context_t;

void controller_init(controller_context_t *context);

void controller_start(controller_context_t *context);

#endif /* CONTROLLER_H */
