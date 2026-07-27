#include "hldc_thread.h"

#define HLDC_DECODE_POLL_INTERVAL_MS 10
#define HLDC_DECODE_DEFAULT_STACK_SIZE 2048

static void hldc_decode_thread_entry(void *arg)
{
    hldc_context_t *ctx = (hldc_context_t *)arg;
    while (1)
    {
        hldc_decode_pushed_bytes(ctx);
        osDelay(HLDC_DECODE_POLL_INTERVAL_MS);
    }
}

osThreadId_t hldc_start_decode_thread(hldc_context_t *ctx, const osThreadAttr_t *attr)
{
    // 2048 bytes covers hldc_handle_end_byte's crc_input VLA (up to
    // HLDC_HEADER_SIZE + MAX_PAYLOAD_SIZE for a maximum-size payload) plus
    // the rest of the decode call chain -- configMINIMAL_STACK_SIZE (512
    // bytes, CMSIS-OS2's default when attr is NULL) is not enough headroom.
    static const osThreadAttr_t default_attr = {
        .name = "hldc_decode",
        .stack_size = HLDC_DECODE_DEFAULT_STACK_SIZE,
    };
    if (attr == NULL)
    {
        attr = &default_attr;
    }
    return osThreadNew(hldc_decode_thread_entry, ctx, attr);
}
