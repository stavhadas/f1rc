#ifndef UART_INTERFACE_H
#define UART_INTERFACE_H

#include <stdint.h>
#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include <stddef.h>

// Size of the circular DMA RX ring buffer, in bytes, shared by all
// uart_interface_t instances. Does not need to hold a whole protocol frame --
// bytes are streamed out via on_recv as they arrive -- it only bounds how
// much data can accumulate between two DMA/IDLE events.
#define UART_INTERFACE_RX_BUFFER_SIZE 256

/**
 * Callback invoked with newly received bytes.
 *
 * ISR-CONTEXT CONTRACT (hard constraint, applies to every implementation of
 * this callback, now and in the future):
 *   - This fires SYNCHRONOUSLY from interrupt context: either from
 *     HAL_UART_IRQHandler (USARTx global IRQ, IDLE-line event) or from
 *     HAL_DMA_IRQHandler (DMAx_Streamy IRQ, half/full-transfer event),
 *     both invoked at NVIC preemption priority 5.
 *   - Do NOT call any blocking API (HAL timeout loops, any polling wait).
 *   - Do NOT call any FreeRTOS API that is not an explicit ...FromISR
 *     variant (this repo currently has zero FromISR usage anywhere in
 *     app code -- do not be the first without re-reading this contract).
 *   - Keep work bounded and short: this handler runs with interrupts at
 *     priority 5 and above masked out (configLIBRARY_MAX_SYSCALL_
 *     INTERRUPT_PRIORITY = 5), so excessive work here delays every other
 *     interrupt at or below that priority, including this same UART's
 *     next IDLE/HT/TC event.
 *   - `buffer` points into uart_interface_t's internal static ring buffer
 *     and is only valid for the duration of this call -- copy out
 *     anything you need to keep (see comms/hldc/hldc.c hldc_push_bytes
 *     for the existing pattern: bounds-checked memcpy, no blocking).
 *
 * @param buffer Pointer to a contiguous run of newly received bytes.
 * @param length Number of valid bytes at buffer.
 */
typedef void (*on_uart_if_recv_t)(uint8_t *buffer, size_t length);

typedef struct
{
    UART_HandleTypeDef huart;
    DMA_HandleTypeDef hdma_rx;
    on_uart_if_recv_t on_recv;

    // Static (no-malloc) circular DMA RX buffer and the position-tracking
    // state used to turn HAL_UARTEx_RxEventCallback's absolute buffer
    // positions into contiguous on_recv(buffer, length) ranges. Written
    // only from ISR context (USARTx IRQ / DMAx_Streamy IRQ, both at NVIC
    // priority 5 -- equal priority means the NVIC never nests one inside
    // the other, so this is not a concurrent-access race).
    uint8_t rx_buffer[UART_INTERFACE_RX_BUFFER_SIZE];
    uint16_t rx_old_pos;
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
                         IRQn_Type dma_rx_irq,
                         IRQn_Type uart_irq);

void uart_interface_send(uart_interface_t *interface, uint8_t *data, size_t length);

// Generic ISR dispatch, called from the board's interrupt vector file
// (e.g. stm32f4xx_it.c). Looks up which registered uart_interface_t owns the
// given peripheral and forwards to the matching HAL IRQ handler, so adding a
// new uart_interface_t elsewhere only ever needs a one-line vector stub --
// never a copy of any driver logic.
void uart_interface_usart_irq_handler(USART_TypeDef *instance);
void uart_interface_dma_rx_irq_handler(DMA_Stream_TypeDef *stream);

#endif /* UART_INTERFACE_H */
