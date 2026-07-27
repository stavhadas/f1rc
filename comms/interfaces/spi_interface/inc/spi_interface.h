#ifndef SPI_INTERFACE_H
#define SPI_INTERFACE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

typedef struct
{
    SPI_HandleTypeDef hspi;
    DMA_HandleTypeDef hdma_tx;
    DMA_HandleTypeDef hdma_rx;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *miso_port;
    uint16_t miso_pin;
    // Signaled from HAL_SPI_TxRxCpltCallback (ISR context) via
    // osSemaphoreRelease (ISR-safe); spi_interface_transfer() blocks on it.
    osSemaphoreId_t xfer_done;
} spi_interface_t;

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
                         uint16_t miso_pin);

void spi_interface_cs_low(spi_interface_t *interface);
void spi_interface_cs_high(spi_interface_t *interface);

/**
 * Busy-waits (plain GPIO read, not RTOS-blocking -- the timing this is used
 * for is sub-millisecond, finer than the RTOS tick) for the MISO line to go
 * low. Several SPI slaves (e.g. CC1101) hold MISO high after CSn goes low
 * until they're actually ready to clock a transaction.
 *
 * @param timeout_iterations Max polling iterations before giving up.
 * @return true if MISO went low before the timeout, false otherwise.
 */
bool spi_interface_wait_miso_low(spi_interface_t *interface, uint32_t timeout_iterations);

/**
 * Blocking, DMA-driven full-duplex SPI transfer. CS must already be low
 * (see spi_interface_cs_low). Blocks the calling task (not a busy-wait) on
 * an RTOS semaphore released from the DMA/SPI completion ISR.
 *
 * @param tx     Bytes to transmit.
 * @param rx     Buffer to receive into, or NULL to discard received bytes.
 * @param length Number of bytes to transfer.
 */
void spi_interface_transfer(spi_interface_t *interface, const uint8_t *tx, uint8_t *rx, size_t length);

// Generic ISR dispatch, called from the board's interrupt vector file. Looks
// up which registered spi_interface_t owns the given peripheral/stream and
// forwards to the matching HAL IRQ handler, so adding a new SPI interface
// elsewhere only ever needs a one-line vector stub -- never a copy of any
// driver logic.
void spi_interface_dma_irq_handler(DMA_Stream_TypeDef *stream);
void spi_interface_spi_irq_handler(SPI_TypeDef *instance);

#endif /* SPI_INTERFACE_H */
