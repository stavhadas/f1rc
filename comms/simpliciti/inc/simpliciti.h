#ifndef SIMPLICITI_H
#define SIMPLICITI_H
#include "nwk.h"
#include "mfri.h"
#include "simplciti_common.h"

#define SIMPLICITI_MAX_FRAME_SIZE (sizeof(nwk_header_t) + sizeof(mfri_header_t) + NWK_MAX_PAYLOAD_SIZE)

typedef struct
{
    nwk_header_t nwk_header;
    mfri_header_t mfri_header;
    uint8_t payload[NWK_MAX_PAYLOAD_SIZE];
} simpliciti_frame_t;

simpliciti_status_t simpliciti_decode_frame(const uint8_t *data,
                                            size_t length,
                                            simpliciti_frame_t *frame);

simpliciti_status_t simpliciti_encode_frame(const simpliciti_frame_t *frame,
                                            uint8_t *buffer,
                                            size_t *length);

typedef struct __attribute__((packed))
{
    uint8_t request;
} simpliciti_ping_payload_t;

typedef enum
{
    SIMPLICITI_LINK_STATE_DISCONNECTED,
    SIMPLICITI_LINK_STATE_LINK_REQUESTED,
    SIMPLICITI_LINK_STATE_CONNECTED
} simpliciti_link_state_t;

typedef struct
{
    simpliciti_link_state_t link_state;
    uint32_t dest_address;
    uint32_t last_ping_timestamp;
} simpliciti_link_info_t;



typedef simpliciti_status_t (*simpliciti_send_msg_cb_t)(const uint8_t *buffer, size_t length);
typedef simpliciti_status_t (*simpliciti_handle_app_message_cb_t)(const simpliciti_frame_t *frame);
typedef simpliciti_status_t (*simpliciti_get_time_ms_cb_t)(uint32_t *timestamp);

typedef struct
{
    simpliciti_send_msg_cb_t send_msg;
    simpliciti_handle_app_message_cb_t handle_app_message;
    simpliciti_get_time_ms_cb_t get_time_ms;
} simpliciti_callbacks_t;

typedef struct
{
    bool in_use;
    simpliciti_frame_t frame;
    uint8_t retries;
    bool is_timeout_set;
    uint32_t timeout;
} simpliciti_outgoing_msg_t;

typedef struct
{
    simpliciti_callbacks_t callbacks;
    uint32_t device_address;
    simpliciti_link_info_t link_info;
    uint8_t last_tid;
} simpliciti_context_t;



#define SIMPLICITI_ACK_TIMEOUT_MS (1000)
#define NWK_PORT_PING_REQUEST (0x01)
#define NWK_PORT_PING_RESPONSE (0x80)
#define SIMPLICITI_MAX_RETRIES (3)

simpliciti_status_t simpliciti_init(simpliciti_context_t *context);

simpliciti_status_t simpliciti_check_outgoing_messages(simpliciti_context_t *context);

simpliciti_status_t simpliciti_send_msg(simpliciti_context_t *context, simpliciti_frame_t *frame, bool is_response, bool is_retransmission);

simpliciti_status_t simpliciti_receive_msg(simpliciti_context_t *context, const uint8_t *buffer, size_t length);

simpliciti_status_t simpliciti_send_ping(simpliciti_context_t *context, uint32_t dest_address);
#endif // SIMPLICITI_H