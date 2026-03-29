#ifndef NWK_H
#define NWK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

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


#define NWK_PORT_FORWARED_MASK (0x80)
#define NWK_PORT_FORWARDED_SHIFT (7)
#define NWK_PORT_ENCRYPTED_MASK (0x40)
#define NWK_PORT_ENCRYPTED_SHIFT (6)
#define NWK_PORT_APP_PORT_MASK (0x3F)

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

#define NWK_DEVICE_INFO_ACK_REQ_MASK (0x80)
#define NWK_DEVICE_INFO_RECEIVER_TYPE_MASK (0x40)
#define NWK_DEVICE_INFO_SENDER_TYPE_MASK (0x30)
#define NWK_DEVICE_INFO_ACK_REPLY_MASK (0x08)
#define NWK_DEVICE_INFO_HOP_COUNT_MASK (0x07)

#define NWK_DEVICE_INFO_ACK_REQ_SHIFT (7)
#define NWK_DEVICE_INFO_RECEIVER_TYPE_SHIFT (6)
#define NWK_DEVICE_INFO_SENDER_TYPE_SHIFT (4)
#define NWK_DEVICE_INFO_ACK_REPLY_SHIFT (3)

#define NWK_HEADER_SIZE (sizeof(nwk_header_t))


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