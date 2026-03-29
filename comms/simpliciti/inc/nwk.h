#ifndef NWK_H
#define NWK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "simplciti_common.h"

#define NWK_MAX_PAYLOAD_SIZE (111)

typedef enum
{
    NWK_PORT_PING = 1,
    NWK_PORT_LINK = 2
} nwk_app_port_t;

typedef struct __attribute__((packed))
{
    bool forwarded : 1;
    bool encrypted : 1;
    nwk_app_port_t app_port : 6;
} nwk_port_t;

typedef enum __attribute__((packed))
{
    NWK_RECEIVER_TYPE_CONTROLLED_LISTEN = 0,
    NWK_RECEIVER_TYPE_SLEEPS_POLL = 1
} nwk_receiver_type_t;

typedef enum __attribute__((packed))
{
    NWK_SENDER_TYPE_END_DEVICE = 0,
    NWK_SENDER_TYPE_RANGE_EXTENDER = 1,
    NWK_SENDER_TYPE_ACCESS_POINT = 2,
    NWK_SENDER_TYPE_RESERVED = 3
} nwk_sender_type_t;

typedef struct __attribute__((packed))
{
    bool ack_req : 1;
    nwk_receiver_type_t receiver_type : 1;
    nwk_sender_type_t sender_type : 2;
    bool ack_reply : 1;
    uint8_t hop_count : 3;

} nwk_device_info_t;

typedef struct __attribute__((packed))
{
    nwk_port_t port;
    nwk_device_info_t device_info;
    uint8_t tractid;
} nwk_header_t;

#define NWK_MAX_FRAME_SIZE (sizeof(nwk_header_t) + NWK_MAX_PAYLOAD_SIZE)

typedef struct __attribute__((packed))
{
    nwk_header_t header;
    uint8_t *payload;
    uint8_t payload_length;
} nwk_frame_t;


simpliciti_status_t nwk_decode_frame(const uint8_t *data, size_t length, nwk_frame_t *frame);
simpliciti_status_t nwk_encode_frame(const nwk_frame_t *frame, uint8_t *buffer, size_t *length);

#endif // NWK_H