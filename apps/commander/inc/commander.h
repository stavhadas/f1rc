#ifndef COMMANDER_H
#define COMMANDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "main.pb-c.h"
#include "cmsis_os2.h"

typedef enum
{
    COMMANDER_STATUS_OK = 0,
    COMMANDER_STATUS_INVALID_COMMAND,
    COMMANDER_STATUS_PROTOBUF_ERROR
} commander_status_t;

typedef commander_status_t (*commander_command_handler_t)(F1rc__Command * cmd, F1rc__Response *response, bool expected_response);
typedef commander_status_t (*commander_send_response)(const uint8_t *data, size_t length);

// Max size of a single queued command's encoded protobuf bytes. Real
// commands in this project are a handful of bytes; this is sized generously
// above that, not to HLDC's full MAX_PAYLOAD_SIZE (1024), to keep the
// queue's static footprint (COMMANDER_QUEUE_DEPTH * this) reasonable.
#define COMMANDER_MAX_MESSAGE_SIZE 256
#define COMMANDER_QUEUE_DEPTH 4

typedef struct
{
    commander_command_handler_t handle_command;
    commander_send_response send_response;
    osMessageQueueId_t queue;
} commander_context_t;

/**
 * Starts the commander's worker thread and its message queue. Must be
 * called after osKernelInitialize() (osThreadNew() before the kernel is
 * initialized is undefined behavior), and before commander_post_command().
 *
 * @param context     Commander context (handle_command/send_response should
 *                     already be set).
 * @param thread_attr  Thread attributes, or NULL to use a built-in default
 *                     sized for protobuf unpack/pack + HLDC encode + a
 *                     blocking UART transmit.
 * @return true on success, false if queue or thread creation failed.
 */
bool commander_init(commander_context_t *context, const osThreadAttr_t *thread_attr);

/**
 * Enqueues a command for the commander thread to unpack and dispatch
 * asynchronously. Safe to call from any task context (not ISR -- this does
 * a blocking, non-ISR queue put).
 *
 * @return true if queued; false if the queue is full or length exceeds
 *         COMMANDER_MAX_MESSAGE_SIZE.
 */
bool commander_post_command(commander_context_t *context, const uint8_t *data, size_t length, bool expected_response);

/**
 * Synchronously unpacks and dispatches one command on the calling thread.
 * Called internally by the commander thread for each queued message;
 * exposed directly for callers that genuinely need synchronous dispatch
 * (e.g. host-side testing) instead of commander_post_command().
 */
commander_status_t commander_handle_command(commander_context_t* context, const uint8_t *data, size_t length, bool expected_response);

#endif /* COMMANDER_H */
