#ifndef CAR_H
#define CAR_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

extern uint32_t version[];

typedef struct {
    UART_HandleTypeDef cmd_uart;
} car_context_t;

void car_init(car_context_t *context);

void car_start(car_context_t *context);

#endif /* CAR_H */