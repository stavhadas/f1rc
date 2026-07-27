#include "uart_interface.h"
#include <stddef.h>

// Registry of all initialized instances, used only by the generic ISR
// dispatch functions below (uart_interface_usart_irq_handler /
// uart_interface_dma_rx_irq_handler). STM32F411 has 3 USARTs (1/2/6).
#define UART_INTERFACE_MAX_INSTANCES 3

static uart_interface_t *g_uart_interfaces[UART_INTERFACE_MAX_INSTANCES];
static size_t g_uart_interface_count = 0;

static void uart_interface_register(uart_interface_t *interface)
{
    if (g_uart_interface_count >= UART_INTERFACE_MAX_INSTANCES)
    {
        while (1) { /* raise UART_INTERFACE_MAX_INSTANCES */ }
    }
    g_uart_interfaces[g_uart_interface_count++] = interface;
}

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
                         IRQn_Type uart_irq)
{
    // Initialize the UART peripheral
    interface->huart.Instance = instance;
    interface->huart.Init.BaudRate = baudrate;
    interface->huart.Init.WordLength = word_length;
    interface->huart.Init.StopBits = stop_bits;
    interface->huart.Init.Parity = parity;
    interface->huart.Init.Mode = mode;
    interface->huart.Init.HwFlowCtl = hw_flow_ctl;
    interface->huart.Init.OverSampling = oversampling;

    if (HAL_UART_Init(&interface->huart) != HAL_OK)
    {
        while (1)
        {
            // TODO: Add error handling
        }
    }

    // Configure DMA for UART RX
    __HAL_RCC_DMA2_CLK_ENABLE();
    interface->hdma_rx.Instance = dma_stream;
    interface->hdma_rx.Init.Channel = dma_channel;
    interface->hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    interface->hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    interface->hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
    interface->hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    interface->hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    interface->hdma_rx.Init.Mode = DMA_CIRCULAR;
    interface->hdma_rx.Init.Priority = DMA_PRIORITY_LOW;
    interface->hdma_rx.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    interface->hdma_rx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL;
    interface->hdma_rx.Init.MemBurst = DMA_MBURST_SINGLE;
    interface->hdma_rx.Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&interface->hdma_rx) != HAL_OK)
    {
        while (1) {}
    }

    __HAL_LINKDMA(&interface->huart, hdmarx, interface->hdma_rx);

    // Register before enabling any IRQ, so an interrupt can never fire before
    // the dispatch functions know how to route it to this instance.
    uart_interface_register(interface);

    // NOTE: on a parity/framing/overrun error, HAL aborts DMA reception with
    // no auto-restart, and the default HAL_UART_ErrorCallback is a no-op --
    // line noise can permanently stop RX. Not handled here; flagged for
    // whoever picks this up next.

    HAL_NVIC_SetPriority(dma_rx_irq, 5, 0);
    HAL_NVIC_EnableIRQ(dma_rx_irq);

    // IDLE-line detection is delivered on the USARTx global IRQ, not the DMA
    // IRQ -- must be enabled at the same priority (5) so it and the DMA IRQ
    // can never preempt/nest each other, since both touch interface->rx_old_pos.
    HAL_NVIC_SetPriority(uart_irq, 5, 0);
    HAL_NVIC_EnableIRQ(uart_irq);

    interface->rx_old_pos = 0;

    if (HAL_UARTEx_ReceiveToIdle_DMA(&interface->huart, interface->rx_buffer,
                                     UART_INTERFACE_RX_BUFFER_SIZE) != HAL_OK)
    {
        while (1) {}
    }
}

void uart_interface_send(uart_interface_t *interface, uint8_t *data, size_t length)
{
    HAL_UART_Transmit(&interface->huart, data, length, HAL_MAX_DELAY);
}

void uart_interface_usart_irq_handler(USART_TypeDef *instance)
{
    for (size_t i = 0; i < g_uart_interface_count; i++)
    {
        if (g_uart_interfaces[i]->huart.Instance == instance)
        {
            HAL_UART_IRQHandler(&g_uart_interfaces[i]->huart);
            return;
        }
    }
}

void uart_interface_dma_rx_irq_handler(DMA_Stream_TypeDef *stream)
{
    for (size_t i = 0; i < g_uart_interface_count; i++)
    {
        if (g_uart_interfaces[i]->hdma_rx.Instance == stream)
        {
            HAL_DMA_IRQHandler(&g_uart_interfaces[i]->hdma_rx);
            return;
        }
    }
}

// Recovers the owning uart_interface_t* from the UART_HandleTypeDef* HAL
// hands back, via the struct's first member -- works generically for any
// number of registered instances.
static uart_interface_t *uart_interface_from_huart(UART_HandleTypeDef *huart)
{
    return (uart_interface_t *)((uint8_t *)huart - offsetof(uart_interface_t, huart));
}

// Called from ISR context -- see uart_interface.h's on_uart_if_recv_t doc
// comment for the full contract. Converts an absolute ring-buffer position
// (Size, as delivered on HT/TC/IDLE events) into one or two contiguous
// on_recv() calls.
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    uart_interface_t *interface = uart_interface_from_huart(huart);
    uint16_t pos = Size;
    uint16_t old_pos = interface->rx_old_pos;

    if (pos == old_pos)
    {
        return; // No new data (e.g. spurious/duplicate event).
    }

    if (pos > old_pos)
    {
        // Single contiguous run: [old_pos, pos).
        if (interface->on_recv)
        {
            interface->on_recv(&interface->rx_buffer[old_pos], (size_t)(pos - old_pos));
        }
    }
    else // pos < old_pos: new data wrapped past the end of the ring buffer.
    {
        uint16_t tail_len = (uint16_t)(UART_INTERFACE_RX_BUFFER_SIZE - old_pos);
        if (interface->on_recv)
        {
            interface->on_recv(&interface->rx_buffer[old_pos], tail_len);
        }
        if (pos > 0 && interface->on_recv)
        {
            interface->on_recv(&interface->rx_buffer[0], pos);
        }
    }

    // Normalize: a TC event reports pos == UART_INTERFACE_RX_BUFFER_SIZE
    // (one-past-the-end), but the DMA's NDTR register has already reloaded
    // and the next byte physically lands at index 0.
    interface->rx_old_pos = (pos == UART_INTERFACE_RX_BUFFER_SIZE) ? 0 : pos;
}
