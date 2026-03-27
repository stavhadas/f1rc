#ifndef HLDC_H_
#define HLDC_H_

#define MAX_PAYLOAD_SIZE (1024)
#define HLDC_START_FLAG (0x7E)
#define HLDC_END_FLAG (0x7E)
#define HLDC_START_FLAG_SIZE (1)
#define HLDC_END_FLAG_SIZE (1)
#define HLDC_ESCAPE_CHAR (0x5E)

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

enum hldc_state
{
    HLDC_STATE_START_FLAG,
    HLDC_STATE_ADDRESS,
    HLDC_STATE_CONTROL,
    HLDC_STATE_PAYLOAD
};

typedef enum
{
    HLDC_STATUS_OK,
    HLDC_STATUS_DECODE_ERROR,
    HLDC_STATUS_OVERFLOW_ERROR,
    HLDC_STATUS_CRC_ERROR,
    HLDC_STATUS_INPUT_PARAMS_ERROR,
} hldc_status_t;

typedef struct
{
    enum hldc_state state;
    uint8_t buffer[HLDC_MAX_FRAME_SIZE];
    size_t length;
    hldc_frame_t current_frame;
    on_frame_cb_t on_frame;
} hldc_context_t;

/**
 * Encodes an HLDC frame into a byte buffer.
 * @param frame The HLDC frame to encode.
 * @param buffer The output buffer to hold the encoded frame.
 * @param length On input, the size of the given buffer. on output - the actual length of the encoded frame.
 * @return HLDC_STATUS_OK on success, or an appropriate error code on failure.
 */
hldc_status_t hldc_encode(hldc_frame_t *frame, uint8_t *buffer, size_t *length);
hldc_status_t push_data(hldc_context_t *ctx, uint8_t *data, size_t length);

#endif /* HLDC_H_ */