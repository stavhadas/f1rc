#ifndef CC1101_THREAD_H
#define CC1101_THREAD_H

#include "cc1101.h"
#include "cmsis_os2.h"

/**
 * Starts a task that probes the CC1101, and on success arms CW and then
 * periodically tops the TX FIFO back up toward CC1101_TX_FIFO_SIZE with
 * 0xFF so the carrier is sustained indefinitely instead of stopping once
 * the initial FIFO fill drains. On probe failure, traces the failure and
 * the thread exits.
 *
 * Must be started after osKernelStart() (i.e. from another thread, not
 * from controller_init()) -- spi_interface_transfer() blocks on an RTOS
 * semaphore that only ever gets signaled by the scheduler resuming this
 * thread after the DMA ISR releases it, which requires the scheduler to
 * already be running.
 *
 * @param attr Thread attributes, or NULL to use a built-in default.
 * @return The created thread's ID, or NULL on failure.
 */
osThreadId_t cc1101_start_bringup_thread(cc1101_t *dev, const osThreadAttr_t *attr);

#endif /* CC1101_THREAD_H */
