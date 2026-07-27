#ifndef UART_INTERFACE_H
#define UART_INTERFACE_H

#include <stdint.h>
#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include <stddef.h>

typedef void (*on_uart_if_recv_t)(uint8_t *buffer, size_t length);

typedef struct
{
    UART_HandleTypeDef huart;
    DMA_HandleTypeDef hdma_rx;
    on_uart_if_recv_t on_recv;
} uart_interface_t;

void uart_interface_init(uart_interface_t *interface,
                         USART_TypeDef *instance,
                         uint32_t baudrate,
                         uint32_t word_length,
                         uint32_t stop_bits,
                         uint32_t parity,
                         uint32_t mode,
                         uint32_t hw_flow_ctl,
                         uint32_t oversampling,
                         DMA_Stream_TypeDef *dma_stream,
                         uint32_t dma_channel,
                         IRQn_Type dma_rx_irq);

void uart_interface_send(uart_interface_t *interface, uint8_t *data, size_t length);

#endif /* UART_INTERFACE_H */
