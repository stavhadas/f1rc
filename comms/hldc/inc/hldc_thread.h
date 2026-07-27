#ifndef HLDC_THREAD_H
#define HLDC_THREAD_H

#include "hldc.h"
#include "cmsis_os2.h"

/**
 * Starts a dedicated task that repeatedly calls hldc_decode_pushed_bytes(ctx)
 * on a fixed poll interval, draining bytes pushed via hldc_push_bytes (e.g.
 * from an ISR) and dispatching ctx->on_frame from that task's context.
 *
 * ctx->on_frame runs on this thread, not ISR context -- but it still runs on
 * a shared decode thread, so it should stay fast (e.g. hand the frame off to
 * another queue/thread) rather than doing unbounded work itself.
 *
 * @param ctx  HLDC decoder context to poll. Must outlive the thread.
 * @param attr Thread attributes (name/stack/priority), or NULL to use a
 *             built-in default sized for typical HLDC frame decoding.
 * @return The created thread's ID, or NULL on failure.
 */
osThreadId_t hldc_start_decode_thread(hldc_context_t *ctx, const osThreadAttr_t *attr);

#endif /* HLDC_THREAD_H */
