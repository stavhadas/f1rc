#include "uart_interface.h"
#include "cmsis_os2.h"

static void uart_recv_thread(void *arg)
{
    uart_interface_t *interface = (uart_interface_t *)arg;
    while (1)
    {
        uint8_t recv;
        if (HAL_UART_Receive(&interface->huart, &recv, 1, HAL_MAX_DELAY) == HAL_OK)
        {
            if (interface->on_recv)
            {
                interface->on_recv(&recv, sizeof(recv));
            }
        }
    }
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
                         IRQn_Type dma_rx_irq)
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
    HAL_NVIC_SetPriority(dma_rx_irq, 5, 0);
    HAL_NVIC_EnableIRQ(dma_rx_irq);
    if (HAL_DMA_Init(&interface->hdma_rx) != HAL_OK)
    {
        while (1) {}
    }

    __HAL_LINKDMA(&interface->huart, hdmarx, interface->hdma_rx);
    // Start recv thread
    osThreadNew(uart_recv_thread, interface, NULL);
}

void uart_interface_send(uart_interface_t *interface, uint8_t *data, size_t length)
{
    HAL_UART_Transmit(&interface->huart, data, length, HAL_MAX_DELAY);
}
