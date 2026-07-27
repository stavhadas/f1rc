#include "spi_interface.h"
#include <stddef.h>

// Registry of all initialized instances, used only by the generic ISR
// dispatch functions below. This board exposes at most SPI1/SPI2 on
// accessible pins.
#define SPI_INTERFACE_MAX_INSTANCES 2

static spi_interface_t *g_spi_interfaces[SPI_INTERFACE_MAX_INSTANCES];
static size_t g_spi_interface_count = 0;

static void spi_interface_register(spi_interface_t *interface)
{
    if (g_spi_interface_count >= SPI_INTERFACE_MAX_INSTANCES)
    {
        while (1) { /* raise SPI_INTERFACE_MAX_INSTANCES */ }
    }
    g_spi_interfaces[g_spi_interface_count++] = interface;
}

// HAL_GPIO_Init() on a port silently no-ops until that port's AHB1 clock is
// enabled -- and CS's port isn't guaranteed to already be clocked this early
// (HAL_SPI_Init()/MspInit() below is what enables the SCK/MISO/MOSI port's
// clock, but CS may be on a different port, and even when it's the same
// port, that enable happens after this point). Without this, CS silently
// stays in its post-reset floating input state and is never actually driven
// low, so the peripheral it's meant to select never sees itself selected.
static void spi_interface_enable_gpio_clock(GPIO_TypeDef *port)
{
    if (port == GPIOA) { __HAL_RCC_GPIOA_CLK_ENABLE(); }
    else if (port == GPIOB) { __HAL_RCC_GPIOB_CLK_ENABLE(); }
    else if (port == GPIOC) { __HAL_RCC_GPIOC_CLK_ENABLE(); }
    else if (port == GPIOH) { __HAL_RCC_GPIOH_CLK_ENABLE(); }
}

void spi_interface_init(spi_interface_t *interface,
                         SPI_TypeDef *instance,
                         uint32_t baudrate_prescaler,
                         DMA_Stream_TypeDef *tx_dma_stream,
                         uint32_t tx_dma_channel,
                         IRQn_Type tx_dma_irq,
                         DMA_Stream_TypeDef *rx_dma_stream,
                         uint32_t rx_dma_channel,
                         IRQn_Type rx_dma_irq,
                         IRQn_Type spi_irq,
                         GPIO_TypeDef *cs_port,
                         uint16_t cs_pin,
                         GPIO_TypeDef *miso_port,
                         uint16_t miso_pin)
{
    interface->cs_port = cs_port;
    interface->cs_pin = cs_pin;
    interface->miso_port = miso_port;
    interface->miso_pin = miso_pin;

    // CS is a plain software-controlled GPIO (SPI_NSS_SOFT below), not the
    // peripheral's hardware NSS -- idle high (deselected) before anything
    // else touches the bus.
    spi_interface_enable_gpio_clock(cs_port);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
    GPIO_InitTypeDef cs_gpio = {0};
    cs_gpio.Pin = cs_pin;
    cs_gpio.Mode = GPIO_MODE_OUTPUT_PP;
    cs_gpio.Pull = GPIO_NOPULL;
    cs_gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(cs_port, &cs_gpio);

    // Initialize the SPI peripheral (triggers HAL_SPI_MspInit: peripheral
    // clock + SCLK/MISO/MOSI GPIO AF config). MISO stays readable as a plain
    // GPIO input via HAL_GPIO_ReadPin regardless of its AF mode -- the pin's
    // input state (IDR) path is independent of the alternate function.
    interface->hspi.Instance = instance;
    interface->hspi.Init.Mode = SPI_MODE_MASTER;
    interface->hspi.Init.Direction = SPI_DIRECTION_2LINES;
    interface->hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    interface->hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
    interface->hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
    interface->hspi.Init.NSS = SPI_NSS_SOFT;
    interface->hspi.Init.BaudRatePrescaler = baudrate_prescaler;
    interface->hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    interface->hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    interface->hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    interface->hspi.Init.CRCPolynomial = 7;

    if (HAL_SPI_Init(&interface->hspi) != HAL_OK)
    {
        while (1) {}
    }

    // Configure RX/TX DMA. Normal (not circular) mode -- SPI transfers here
    // are one-shot bursts (a register access or a FIFO refill chunk), not a
    // continuous stream like UART RX.
    __HAL_RCC_DMA1_CLK_ENABLE();

    interface->hdma_tx.Instance = tx_dma_stream;
    interface->hdma_tx.Init.Channel = tx_dma_channel;
    interface->hdma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    interface->hdma_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    interface->hdma_tx.Init.MemInc = DMA_MINC_ENABLE;
    interface->hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    interface->hdma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    interface->hdma_tx.Init.Mode = DMA_NORMAL;
    interface->hdma_tx.Init.Priority = DMA_PRIORITY_MEDIUM;
    interface->hdma_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&interface->hdma_tx) != HAL_OK)
    {
        while (1) {}
    }
    __HAL_LINKDMA(&interface->hspi, hdmatx, interface->hdma_tx);

    interface->hdma_rx.Instance = rx_dma_stream;
    interface->hdma_rx.Init.Channel = rx_dma_channel;
    interface->hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    interface->hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    interface->hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
    interface->hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    interface->hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    interface->hdma_rx.Init.Mode = DMA_NORMAL;
    interface->hdma_rx.Init.Priority = DMA_PRIORITY_LOW;
    interface->hdma_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&interface->hdma_rx) != HAL_OK)
    {
        while (1) {}
    }
    __HAL_LINKDMA(&interface->hspi, hdmarx, interface->hdma_rx);

    // Register before enabling any IRQ, so an interrupt can never fire
    // before the dispatch functions know how to route it to this instance.
    spi_interface_register(interface);

    HAL_NVIC_SetPriority(tx_dma_irq, 5, 0);
    HAL_NVIC_EnableIRQ(tx_dma_irq);
    HAL_NVIC_SetPriority(rx_dma_irq, 5, 0);
    HAL_NVIC_EnableIRQ(rx_dma_irq);
    HAL_NVIC_SetPriority(spi_irq, 5, 0);
    HAL_NVIC_EnableIRQ(spi_irq);

    interface->xfer_done = osSemaphoreNew(1, 0, NULL);
    if (interface->xfer_done == NULL)
    {
        while (1) {}
    }
}

void spi_interface_cs_low(spi_interface_t *interface)
{
    HAL_GPIO_WritePin(interface->cs_port, interface->cs_pin, GPIO_PIN_RESET);
}

void spi_interface_cs_high(spi_interface_t *interface)
{
    HAL_GPIO_WritePin(interface->cs_port, interface->cs_pin, GPIO_PIN_SET);
}

bool spi_interface_wait_miso_low(spi_interface_t *interface, uint32_t timeout_iterations)
{
    for (uint32_t i = 0; i < timeout_iterations; i++)
    {
        if (HAL_GPIO_ReadPin(interface->miso_port, interface->miso_pin) == GPIO_PIN_RESET)
        {
            return true;
        }
    }
    return false;
}

void spi_interface_transfer(spi_interface_t *interface, const uint8_t *tx, uint8_t *rx, size_t length)
{
    // HAL_SPI_TransmitReceive_DMA requires a non-NULL rx buffer of the full
    // transfer length even when the caller doesn't care about the received
    // bytes -- DMA writes `length` bytes into it regardless of whether
    // anyone reads them, so this must be sized to match or DMA writes past
    // the end of it.
    uint8_t discard[length];
    uint8_t *rx_buf = rx;
    if (rx_buf == NULL)
    {
        rx_buf = discard;
    }
    HAL_SPI_TransmitReceive_DMA(&interface->hspi, (uint8_t *)tx, rx_buf, (uint16_t)length);
    osSemaphoreAcquire(interface->xfer_done, osWaitForever);
}

void spi_interface_dma_irq_handler(DMA_Stream_TypeDef *stream)
{
    for (size_t i = 0; i < g_spi_interface_count; i++)
    {
        if (g_spi_interfaces[i]->hdma_tx.Instance == stream)
        {
            HAL_DMA_IRQHandler(&g_spi_interfaces[i]->hdma_tx);
            return;
        }
        if (g_spi_interfaces[i]->hdma_rx.Instance == stream)
        {
            HAL_DMA_IRQHandler(&g_spi_interfaces[i]->hdma_rx);
            return;
        }
    }
}

void spi_interface_spi_irq_handler(SPI_TypeDef *instance)
{
    for (size_t i = 0; i < g_spi_interface_count; i++)
    {
        if (g_spi_interfaces[i]->hspi.Instance == instance)
        {
            HAL_SPI_IRQHandler(&g_spi_interfaces[i]->hspi);
            return;
        }
    }
}

// Recovers the owning spi_interface_t* from the SPI_HandleTypeDef* HAL hands
// back, via the struct's first member -- works generically for any number
// of registered instances (same trick as uart_interface_from_huart).
static spi_interface_t *spi_interface_from_hspi(SPI_HandleTypeDef *hspi)
{
    return (spi_interface_t *)((uint8_t *)hspi - offsetof(spi_interface_t, hspi));
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    spi_interface_t *interface = spi_interface_from_hspi(hspi);
    osSemaphoreRelease(interface->xfer_done);
}
