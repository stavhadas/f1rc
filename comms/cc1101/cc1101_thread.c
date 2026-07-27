#include "cc1101_thread.h"
#include "tracing.h"
#include <string.h>

// At the CC1101's uncontrolled reset-default data rate (~115kBaud), a full
// 64-byte TX FIFO drains in ~4.5ms -- close enough to a coarser poll
// interval that ordinary scheduling jitter can underflow it. 1ms (this
// project's FreeRTOS tick period) is the finest osDelay granularity
// available; underflow recovery below is the real safety net regardless.
#define CC1101_REFILL_POLL_INTERVAL_MS 1
#define CC1101_TXBYTES_UNDERFLOW_FLAG 0x80
#define CC1101_BRINGUP_DEFAULT_STACK_SIZE 2048

static void cc1101_bringup_thread_entry(void *arg)
{
    cc1101_t *dev = (cc1101_t *)arg;

    if (!cc1101_probe(dev))
    {
        TRACE("CC1101 not responding: PARTNUM=0x%02X VERSION=0x%02X\n",
              dev->partnum, dev->version);
        return;
    }
    TRACE("CC1101 responsive: PARTNUM=0x%02X VERSION=0x%02X\n",
          dev->partnum, dev->version);
    cc1101_set_cw(dev);

    while (1)
    {
        uint8_t txbytes_raw = cc1101_read_status(dev, CC1101_STATUS_TXBYTES);
        if (txbytes_raw & CC1101_TXBYTES_UNDERFLOW_FLAG)
        {
            // TXFIFO_UNDERFLOW is terminal until explicitly cleared: flush
            // (only valid from IDLE/TXFIFO_UNDERFLOW, which this is),
            // refill, and re-arm TX. Without this, one missed refill
            // window silently and permanently kills the carrier.
            uint8_t fill[CC1101_TX_FIFO_SIZE];
            memset(fill, 0xFF, sizeof(fill));
            cc1101_strobe(dev, CC1101_STROBE_SFTX);
            cc1101_write_burst(dev, CC1101_REG_FIFO, fill, sizeof(fill));
            cc1101_strobe(dev, CC1101_STROBE_STX);
        }
        else
        {
            uint8_t txbytes = txbytes_raw & 0x7F;
            if (txbytes < CC1101_TX_FIFO_SIZE)
            {
                uint8_t fill[CC1101_TX_FIFO_SIZE];
                size_t free_space = (size_t)(CC1101_TX_FIFO_SIZE - txbytes);
                memset(fill, 0xFF, free_space);
                cc1101_write_burst(dev, CC1101_REG_FIFO, fill, free_space);
            }
        }
        osDelay(CC1101_REFILL_POLL_INTERVAL_MS);
    }
}

osThreadId_t cc1101_start_bringup_thread(cc1101_t *dev, const osThreadAttr_t *attr)
{
    static const osThreadAttr_t default_attr = {
        .name = "cc1101_bringup",
        .stack_size = CC1101_BRINGUP_DEFAULT_STACK_SIZE,
    };
    if (attr == NULL)
    {
        attr = &default_attr;
    }
    return osThreadNew(cc1101_bringup_thread_entry, dev, attr);
}
