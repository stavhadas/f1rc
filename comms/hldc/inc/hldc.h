#ifndef HLDC_H_
#define HLDC_H_

#define MAX_PAYLOAD_SIZE (1024)
#define HLDC_START_FLAG (0x7E)
#define HLDC_END_FLAG (0x7E)
#define HLDC_START_FLAG_SIZE (1)
#define HLDC_END_FLAG_SIZE (1)
#define HLDC_ESCAPE_CHAR (0x7D)
#define HLDC_ESCAPE_XOR (0x20)

typedef struct __attribute__((packed))
{
    uint8_t address;
    uint8_t control;
} hldc_header_t;

typedef struct __attribute__((packed))
{
    uint16_t crc;
} hldc_trailer_t;

typedef struct __attribute__((packed))
{
    hldc_header_t header;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    size_t payload_length;
    hldc_trailer_t trailer;
} hldc_frame_t;

#define HLDC_HEADER_SIZE (sizeof(hldc_header_t))
#define HLDC_TRAILER_SIZE (sizeof(hldc_trailer_t))
#define HLDC_PRE_PAYLOAD_SIZE (HLDC_START_FLAG_SIZE + HLDC_HEADER_SIZE)
#define HLDC_POST_PAYLOAD_SIZE (HLDC_TRAILER_SIZE + HLDC_END_FLAG_SIZE)
#define HLDC_FRAME_OVERHEAD (HLDC_PRE_PAYLOAD_SIZE + HLDC_POST_PAYLOAD_SIZE)
#define HLDC_MAX_FRAME_SIZE (HLDC_FRAME_OVERHEAD + MAX_PAYLOAD_SIZE)

typedef void (*on_frame_cb_t)(hldc_frame_t *frame);

typedef enum
{
    HLDC_STATE_START_FLAG,
    HLDC_STATE_ADDRESS,
    HLDC_STATE_CONTROL,
    HLDC_STATE_PAYLOAD,
    HLDC_STATE_ESCAPED_CHAR
} hldc_state_t;

typedef enum
{
    HLDC_STATUS_OK,
    HLDC_STATUS_DECODE_ERROR,
    HLDC_STATUS_CRC_ERROR,
    HLDC_STATUS_INPUT_PARAMS_ERROR,
} hldc_status_t;

typedef void (*on_error_cb_t)(hldc_status_t status);

typedef struct
{
    hldc_state_t state;
    hldc_frame_t current_frame;
    on_frame_cb_t on_frame;
    on_error_cb_t on_error;
    uint8_t decode_buffer[HLDC_MAX_FRAME_SIZE];
    size_t decode_buffer_index;
    size_t bytes_left_to_decode;
} hldc_context_t;

/**
 * Encodes an HLDC frame into a byte buffer.
 * @param frame The HLDC frame to encode.
 * @param buffer The output buffer to hold the encoded frame.
 * @param length On input, the size of the given buffer. on output - the actual length of the encoded frame.
 * @return HLDC_STATUS_OK on success, or an appropriate error code on failure.
 */
hldc_status_t hldc_encode(hldc_frame_t *frame, uint8_t *buffer, size_t *length);

/**
 * Pushes bytes into the decoder context.
 * @note For actually decoding the pushed bytes into frames, call hldc_decode_pushed_bytes() after pushing.
 * @param ctx The HLDC decoder context.
 * @param data The byte buffer to push into the decoder.
 * @param length The number of bytes to push from the data buffer.
 * @return HLDC_STATUS_OK on success, or an appropriate error code on failure.
 */
hldc_status_t hldc_push_bytes(hldc_context_t *ctx, const uint8_t *data, size_t length);

/**
 * Decodes the bytes that have been pushed into the decoder context through hldc_push_bytes(). This function should be called after pushing bytes to actually process them and extract frames.
 * @param ctx The HLDC decoder context containing the pushed bytes to decode.
 * @return HLDC_STATUS_OK on success, or an appropriate error code on failure.
 */
hldc_status_t hldc_decode_pushed_bytes(hldc_context_t *ctx);

/**
 * Resets the decoder context to its initial state
 */
void hldc_reset_context(hldc_context_t *ctx);

#endif /* HLDC_H_ */