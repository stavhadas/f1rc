#include <gtest/gtest.h>

extern "C"
{
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "simpliciti.h"
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Builds a raw MFRI wire frame: [length(1)][dstaddr(4 LE)][srcaddr(4 LE)][payload...]
 * `length` field = plen (bytes after the 9-byte header).
 */
static size_t build_mfri_frame(uint32_t dst, uint32_t src,
                               const uint8_t *payload, size_t plen,
                               uint8_t *out)
{
    out[0] = (uint8_t)plen;
    memcpy(out + 1, &dst, 4);
    memcpy(out + 5, &src, 4);
    if (plen > 0 && payload != nullptr)
        memcpy(out + MFRI_HEADER_SIZE, payload, plen);
    return MFRI_HEADER_SIZE + plen;
}

/**
 * Builds a raw NWK wire frame: [port_byte(1)][device_info_byte(1)][tractid(1)][payload...]
 */
static size_t build_nwk_frame(uint8_t port_byte, uint8_t di_byte,
                              uint8_t tractid,
                              const uint8_t *payload, size_t plen,
                              uint8_t *out)
{
    out[0] = port_byte;
    out[1] = di_byte;
    out[2] = tractid;
    if (plen > 0 && payload != nullptr)
        memcpy(out + sizeof(nwk_header_t), payload, plen);
    return sizeof(nwk_header_t) + plen;
}

/**
 * Builds a full SimpliciTI wire frame:
 * [mfri_length(1)][dstaddr(4)][srcaddr(4)][nwk_port(1)][nwk_di(1)][tractid(1)][payload...]
 * mfri_length = sizeof(nwk_header_t) + plen (everything after the MFRI header).
 */
static size_t build_simpliciti_frame(uint32_t dst, uint32_t src,
                                     uint8_t port_byte, uint8_t di_byte,
                                     uint8_t tractid,
                                     const uint8_t *payload, size_t plen,
                                     uint8_t *out)
{
    out[0] = (uint8_t)(sizeof(nwk_header_t) + plen);
    memcpy(out + 1, &dst, 4);
    memcpy(out + 5, &src, 4);
    out[9]  = port_byte;
    out[10] = di_byte;
    out[11] = tractid;
    if (plen > 0 && payload != nullptr)
        memcpy(out + 12, payload, plen);
    return 12 + plen;
}

// ===========================================================================
// MFRI Layer — Decode
// ===========================================================================

TEST(MfriDecode, BasicFrame)
{
    const uint8_t payload[] = {0x11, 0x22, 0x33};
    uint8_t raw[MFRI_MAX_FRAME_SIZE];
    size_t len = build_mfri_frame(0xAABBCCDD, 0x11223344, payload, sizeof(payload), raw);

    mfri_frame_t frame;
    ASSERT_EQ(mfri_decode_frame(raw, len, &frame), SIMPLICITI_SUCCESS);
    EXPECT_EQ(frame.header.length, (uint8_t)sizeof(payload));
    EXPECT_EQ(frame.header.dstaddr, 0xAABBCCDDu);
    EXPECT_EQ(frame.header.srcaddr, 0x11223344u);
    EXPECT_EQ(frame.payload_length, sizeof(payload));
    EXPECT_EQ(memcmp(frame.payload, payload, sizeof(payload)), 0);
}

TEST(MfriDecode, EmptyPayload)
{
    uint8_t raw[MFRI_MAX_FRAME_SIZE];
    size_t len = build_mfri_frame(0x01020304, 0x05060708, nullptr, 0, raw);

    mfri_frame_t frame;
    ASSERT_EQ(mfri_decode_frame(raw, len, &frame), SIMPLICITI_SUCCESS);
    EXPECT_EQ(frame.header.length, 0u);
    EXPECT_EQ(frame.payload_length, 0u);
}

TEST(MfriDecode, LittleEndianAddresses)
{
    // 0x12345678 in LE: 78 56 34 12
    uint8_t raw[MFRI_MAX_FRAME_SIZE];
    size_t len = build_mfri_frame(0x12345678, 0xDEADBEEF, nullptr, 0, raw);

    EXPECT_EQ(raw[1], 0x78u) << "dstaddr LE byte 0";
    EXPECT_EQ(raw[2], 0x56u) << "dstaddr LE byte 1";
    EXPECT_EQ(raw[3], 0x34u) << "dstaddr LE byte 2";
    EXPECT_EQ(raw[4], 0x12u) << "dstaddr LE byte 3";

    mfri_frame_t frame;
    ASSERT_EQ(mfri_decode_frame(raw, len, &frame), SIMPLICITI_SUCCESS);
    EXPECT_EQ(frame.header.dstaddr, 0x12345678u);
    EXPECT_EQ(frame.header.srcaddr, 0xDEADBEEFu);
}

TEST(MfriDecode, LengthMismatch)
{
    // Build a valid frame then corrupt the length field
    uint8_t raw[MFRI_MAX_FRAME_SIZE];
    size_t len = build_mfri_frame(0, 0, nullptr, 0, raw);
    raw[0] = 5; // claims 5 payload bytes but actual data has 0

    mfri_frame_t frame;
    EXPECT_EQ(mfri_decode_frame(raw, len, &frame), SIMPLICITI_ERROR_DECODING);
}

TEST(MfriDecode, TooShort)
{
    uint8_t raw[MFRI_MAX_FRAME_SIZE] = {};
    mfri_frame_t frame;
    EXPECT_EQ(mfri_decode_frame(raw, MFRI_HEADER_SIZE - 1, &frame), SIMPLICITI_ERROR_INVALID_PARAM);
}

TEST(MfriDecode, TooLong)
{
    uint8_t raw[MFRI_MAX_FRAME_SIZE + 1] = {};
    mfri_frame_t frame;
    EXPECT_EQ(mfri_decode_frame(raw, MFRI_MAX_FRAME_SIZE + 1, &frame), SIMPLICITI_ERROR_INVALID_PARAM);
}

TEST(MfriDecode, NullData)
{
    mfri_frame_t frame;
    EXPECT_EQ(mfri_decode_frame(nullptr, MFRI_HEADER_SIZE, &frame), SIMPLICITI_ERROR_INVALID_PARAM);
}

TEST(MfriDecode, NullFrame)
{
    uint8_t raw[MFRI_MAX_FRAME_SIZE] = {};
    EXPECT_EQ(mfri_decode_frame(raw, MFRI_HEADER_SIZE, nullptr), SIMPLICITI_ERROR_INVALID_PARAM);
}

// ===========================================================================
// MFRI Layer — Encode
// ===========================================================================

TEST(MfriEncode, BasicFrame)
{
    const uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    // Pre-build expected wire frame for comparison
    uint8_t expected[MFRI_MAX_FRAME_SIZE];
    size_t expected_len = build_mfri_frame(0x11223344, 0x55667788, payload, sizeof(payload), expected);

    mfri_frame_t frame = {};
    frame.header.dstaddr    = 0x11223344;
    frame.header.srcaddr    = 0x55667788;
    frame.header.length     = sizeof(payload);
    frame.payload           = const_cast<uint8_t *>(payload);

    uint8_t buf[MFRI_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(mfri_encode_frame(&frame, buf, &len), SIMPLICITI_SUCCESS);
    EXPECT_EQ(len, expected_len);
    EXPECT_EQ(memcmp(buf, expected, len), 0);
}

TEST(MfriEncode, EmptyPayload)
{
    mfri_frame_t frame = {};
    frame.header.dstaddr = 0xDEAD;
    frame.header.srcaddr = 0xBEEF;
    frame.header.length  = 0;
    frame.payload        = nullptr;

    uint8_t buf[MFRI_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(mfri_encode_frame(&frame, buf, &len), SIMPLICITI_SUCCESS);
    EXPECT_EQ(len, (size_t)MFRI_HEADER_SIZE);
    EXPECT_EQ(buf[0], 0u); // length field = 0
}

TEST(MfriEncode, LittleEndianAddresses)
{
    mfri_frame_t frame = {};
    frame.header.dstaddr = 0xDEADBEEF;
    frame.header.srcaddr = 0x12345678;
    frame.header.length  = 0;
    frame.payload        = nullptr;

    uint8_t buf[MFRI_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(mfri_encode_frame(&frame, buf, &len), SIMPLICITI_SUCCESS);
    // dstaddr at bytes 1-4, little-endian
    EXPECT_EQ(buf[1], 0xEFu);
    EXPECT_EQ(buf[2], 0xBEu);
    EXPECT_EQ(buf[3], 0xADu);
    EXPECT_EQ(buf[4], 0xDEu);
    // srcaddr at bytes 5-8, little-endian
    EXPECT_EQ(buf[5], 0x78u);
    EXPECT_EQ(buf[6], 0x56u);
    EXPECT_EQ(buf[7], 0x34u);
    EXPECT_EQ(buf[8], 0x12u);
}

TEST(MfriEncode, NullParams)
{
    mfri_frame_t frame = {};
    uint8_t buf[MFRI_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    EXPECT_EQ(mfri_encode_frame(nullptr, buf, &len),  SIMPLICITI_ERROR_INVALID_PARAM);
    EXPECT_EQ(mfri_encode_frame(&frame, nullptr, &len), SIMPLICITI_ERROR_INVALID_PARAM);
    EXPECT_EQ(mfri_encode_frame(&frame, buf, nullptr),  SIMPLICITI_ERROR_INVALID_PARAM);
}

TEST(MfriEncode, PayloadTooLarge)
{
    mfri_frame_t frame = {};
    frame.header.length = MFRI_MAX_PAYLOAD_SIZE + 1;

    uint8_t buf[MFRI_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    EXPECT_EQ(mfri_encode_frame(&frame, buf, &len), SIMPLICITI_ERROR_INVALID_PARAM);
}

// ===========================================================================
// MFRI Layer — Round-trip
// ===========================================================================

TEST(MfriRoundTrip, BasicFrame)
{
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    mfri_frame_t tx = {};
    tx.header.dstaddr = 0xAAAA5555;
    tx.header.srcaddr = 0x5555AAAA;
    tx.header.length  = sizeof(payload);
    tx.payload        = const_cast<uint8_t *>(payload);

    uint8_t buf[MFRI_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(mfri_encode_frame(&tx, buf, &len), SIMPLICITI_SUCCESS);

    mfri_frame_t rx;
    ASSERT_EQ(mfri_decode_frame(buf, len, &rx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(rx.header.dstaddr,     tx.header.dstaddr);
    EXPECT_EQ(rx.header.srcaddr,     tx.header.srcaddr);
    EXPECT_EQ(rx.header.length,      tx.header.length);
    EXPECT_EQ(rx.payload_length,     (size_t)sizeof(payload));
    EXPECT_EQ(memcmp(rx.payload, payload, sizeof(payload)), 0);
}

// ===========================================================================
// NWK Layer — Decode
// ===========================================================================

// Wire bit layout per spec (bit 0 = LSB, bit 7 = MSB):
//
// nwk_port_t:
//   bits 5-0: app_port
//   bit 6:    encrypted
//   bit 7:    forwarded
// Wire byte formula: (forwarded<<7) | (encrypted<<6) | (app_port & 0x3F)
//
// nwk_device_info_t:
//   bits 2-0: hop_count
//   bit 3:    ack_reply
//   bits 5-4: sender_type
//   bit 6:    receiver_type
//   bit 7:    ack_req
// Wire byte formula: (ack_req<<7) | (receiver_type<<6) | (sender_type<<4) | (ack_reply<<3) | (hop_count & 7)
//
// NOTE: The current struct declarations list forwarded/ack_req first, which
// in GCC little-endian maps them to bit 0 (LSB) — the opposite of the spec.
// Tests below use spec-correct wire bytes; tests that check wire byte values
// or specific field decoding will FAIL until the struct field order is fixed.

TEST(NwkDecode, BasicFrame)
{
    const uint8_t payload[] = {0xAA, 0xBB};
    // port_byte: forwarded=0(bit7), encrypted=0(bit6), app_port=NWK_PORT_PING=1(bits5-0)
    // → (0<<7) | (0<<6) | 1 = 0x01
    uint8_t raw[NWK_MAX_FRAME_SIZE];
    size_t len = build_nwk_frame(0x01, 0x00, 0x42, payload, sizeof(payload), raw);

    nwk_frame_t frame;
    ASSERT_EQ(nwk_decode_frame(raw, len, &frame), SIMPLICITI_SUCCESS);
    EXPECT_EQ((int)frame.header.port.app_port, (int)NWK_PORT_PING);
    EXPECT_EQ(frame.header.port.forwarded, false);
    EXPECT_EQ(frame.header.port.encrypted, false);
    EXPECT_EQ(frame.header.tractid, 0x42u);
    EXPECT_EQ((size_t)frame.payload_length, sizeof(payload));
    EXPECT_EQ(memcmp(frame.payload, payload, sizeof(payload)), 0);
}

TEST(NwkDecode, EmptyPayload)
{
    // port_byte: forwarded=0(bit7), encrypted=0(bit6), app_port=NWK_PORT_PING=1(bits5-0)
    // → (0<<7) | (0<<6) | 1 = 0x01
    uint8_t raw[sizeof(nwk_header_t)];
    size_t len = build_nwk_frame(0x01, 0x00, 0x01, nullptr, 0, raw);

    nwk_frame_t frame;
    ASSERT_EQ(nwk_decode_frame(raw, len, &frame), SIMPLICITI_SUCCESS);
    EXPECT_EQ(frame.payload_length, 0u);
}

TEST(NwkDecode, PortBitFields)
{
    // forwarded=0(bit7), encrypted=1(bit6), app_port=1(bits5-0)
    // port_byte = (0<<7) | (1<<6) | 1 = 0x41
    uint8_t raw[sizeof(nwk_header_t)];
    size_t len = build_nwk_frame(0x41, 0x00, 0x00, nullptr, 0, raw);

    nwk_frame_t frame;
    ASSERT_EQ(nwk_decode_frame(raw, len, &frame), SIMPLICITI_SUCCESS);
    EXPECT_EQ(frame.header.port.forwarded, false);
    EXPECT_EQ(frame.header.port.encrypted, true);
    EXPECT_EQ((int)frame.header.port.app_port, 1);
}

TEST(NwkDecode, DeviceInfoBitFields)
{
    // ack_req=1(bit7), receiver_type=1(bit6), sender_type=END_DEVICE=0(bits5-4),
    // ack_reply=1(bit3), hop_count=3(bits2-0)
    // di_byte = (1<<7) | (1<<6) | (0<<4) | (1<<3) | 3 = 128|64|0|8|3 = 203 = 0xCB
    // port_byte: forwarded=0, encrypted=0, app_port=1 → 0x01
    uint8_t raw[sizeof(nwk_header_t)];
    size_t len = build_nwk_frame(0x01, 0xCB, 0x00, nullptr, 0, raw);

    nwk_frame_t frame;
    ASSERT_EQ(nwk_decode_frame(raw, len, &frame), SIMPLICITI_SUCCESS);
    EXPECT_EQ(frame.header.device_info.ack_req,       true);
    EXPECT_EQ(frame.header.device_info.receiver_type, NWK_RECEIVER_TYPE_SLEEPS_POLL);
    EXPECT_EQ(frame.header.device_info.sender_type,   NWK_SENDER_TYPE_END_DEVICE);
    EXPECT_EQ(frame.header.device_info.ack_reply,     true);
    EXPECT_EQ(frame.header.device_info.hop_count,     3u);
}

TEST(NwkDecode, TooShort)
{
    uint8_t raw[sizeof(nwk_header_t)] = {};
    nwk_frame_t frame;
    EXPECT_EQ(nwk_decode_frame(raw, sizeof(nwk_header_t) - 1, &frame), SIMPLICITI_ERROR_INVALID_PARAM);
}

TEST(NwkDecode, TooLong)
{
    uint8_t raw[NWK_MAX_FRAME_SIZE + 1] = {};
    nwk_frame_t frame;
    EXPECT_EQ(nwk_decode_frame(raw, NWK_MAX_FRAME_SIZE + 1, &frame), SIMPLICITI_ERROR_INVALID_PARAM);
}

TEST(NwkDecode, NullParams)
{
    uint8_t raw[sizeof(nwk_header_t)] = {};
    raw[0] = 0x01; // valid spec-correct port byte (app_port=1)
    nwk_frame_t frame;
    EXPECT_EQ(nwk_decode_frame(nullptr, sizeof(nwk_header_t), &frame), SIMPLICITI_ERROR_INVALID_PARAM);
    EXPECT_EQ(nwk_decode_frame(raw, sizeof(nwk_header_t), nullptr),    SIMPLICITI_ERROR_INVALID_PARAM);
}

// ===========================================================================
// NWK Layer — Encode
// ===========================================================================

TEST(NwkEncode, BasicFrame)
{
    const uint8_t payload[] = {0xDE, 0xAD};

    nwk_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.header.port.app_port = NWK_PORT_PING;
    frame.header.tractid       = 0x77;
    frame.payload              = const_cast<uint8_t *>(payload);
    frame.payload_length       = sizeof(payload);

    uint8_t buf[NWK_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(nwk_encode_frame(&frame, buf, &len), SIMPLICITI_SUCCESS);
    EXPECT_EQ(len, sizeof(nwk_header_t) + sizeof(payload));
    EXPECT_EQ(buf[2], 0x77u) << "tractid at wire byte 2";
    EXPECT_EQ(buf[sizeof(nwk_header_t) + 0], 0xDEu);
    EXPECT_EQ(buf[sizeof(nwk_header_t) + 1], 0xADu);
}

TEST(NwkEncode, PortBitFields)
{
    // forwarded=false(bit7), encrypted=false(bit6), app_port=NWK_PORT_PING=1(bits5-0)
    // Spec-correct expected port byte: (0<<7) | (0<<6) | 1 = 0x01
    nwk_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.header.port.forwarded = false;
    frame.header.port.encrypted = false;
    frame.header.port.app_port  = NWK_PORT_PING;
    frame.payload               = nullptr;
    frame.payload_length        = 0;

    uint8_t buf[NWK_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(nwk_encode_frame(&frame, buf, &len), SIMPLICITI_SUCCESS);
    EXPECT_EQ(buf[0], 0x01u) << "port byte: forwarded=0, encrypted=0, app_port=1";
}

TEST(NwkEncode, PayloadTooLarge)
{
    nwk_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.payload_length = NWK_MAX_PAYLOAD_SIZE + 1;

    uint8_t buf[NWK_MAX_FRAME_SIZE + 10];
    size_t len = sizeof(buf);
    EXPECT_EQ(nwk_encode_frame(&frame, buf, &len), SIMPLICITI_ERROR_INVALID_PARAM);
}

TEST(NwkEncode, NullParams)
{
    nwk_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    uint8_t buf[NWK_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    EXPECT_EQ(nwk_encode_frame(nullptr, buf, &len),    SIMPLICITI_ERROR_INVALID_PARAM);
    EXPECT_EQ(nwk_encode_frame(&frame, nullptr, &len), SIMPLICITI_ERROR_INVALID_PARAM);
    EXPECT_EQ(nwk_encode_frame(&frame, buf, nullptr),  SIMPLICITI_ERROR_INVALID_PARAM);
}

// ===========================================================================
// NWK Layer — Round-trip
// ===========================================================================

TEST(NwkRoundTrip, BasicFrame)
{
    const uint8_t payload[] = {0x10, 0x20, 0x30};

    nwk_frame_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.header.port.app_port         = NWK_PORT_LINK;
    tx.header.port.encrypted        = false;
    tx.header.port.forwarded        = false;
    tx.header.device_info.ack_req   = true;
    tx.header.device_info.hop_count = 5;
    tx.header.tractid               = 0xAB;
    tx.payload                      = const_cast<uint8_t *>(payload);
    tx.payload_length               = sizeof(payload);

    uint8_t buf[NWK_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(nwk_encode_frame(&tx, buf, &len), SIMPLICITI_SUCCESS);

    nwk_frame_t rx;
    ASSERT_EQ(nwk_decode_frame(buf, len, &rx), SIMPLICITI_SUCCESS);
    EXPECT_EQ((int)rx.header.port.app_port,         (int)tx.header.port.app_port);
    EXPECT_EQ(rx.header.port.encrypted,             tx.header.port.encrypted);
    EXPECT_EQ(rx.header.port.forwarded,             tx.header.port.forwarded);
    EXPECT_EQ(rx.header.device_info.ack_req,        tx.header.device_info.ack_req);
    EXPECT_EQ(rx.header.device_info.hop_count,      tx.header.device_info.hop_count);
    EXPECT_EQ(rx.header.tractid,                    tx.header.tractid);
    EXPECT_EQ((size_t)rx.payload_length,            (size_t)tx.payload_length);
    EXPECT_EQ(memcmp(rx.payload, payload, sizeof(payload)), 0);
}

// ===========================================================================
// SimpliciTI Top-Level — Decode
// ===========================================================================

TEST(SimplictiDecode, BasicFrame)
{
    const uint8_t payload[] = {0xCA, 0xFE};
    // port_byte: forwarded=0(bit7), encrypted=0(bit6), app_port=NWK_PORT_PING=1(bits5-0)
    // → (0<<7) | (0<<6) | 1 = 0x01
    uint8_t raw[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(0xDEAD1234, 0xBEEF5678,
                                        0x01, 0x00, 0x55,
                                        payload, sizeof(payload), raw);

    simpliciti_frame_t frame;
    ASSERT_EQ(simpliciti_decode_frame(raw, len, &frame), SIMPLICITI_SUCCESS);
    EXPECT_EQ(frame.mfri_header.dstaddr,               0xDEAD1234u);
    EXPECT_EQ(frame.mfri_header.srcaddr,               0xBEEF5678u);
    EXPECT_EQ((int)frame.nwk_header.port.app_port,     (int)NWK_PORT_PING);
    EXPECT_EQ(frame.nwk_header.tractid,                0x55u);
    EXPECT_EQ(memcmp(frame.payload, payload, sizeof(payload)), 0);
}

TEST(SimplictiDecode, NullParams)
{
    // null data propagates into mfri_decode_frame which validates it
    uint8_t raw[SIMPLICITI_MAX_FRAME_SIZE] = {};
    simpliciti_frame_t frame;
    EXPECT_EQ(simpliciti_decode_frame(nullptr, MFRI_HEADER_SIZE, &frame), SIMPLICITI_ERROR_INVALID_PARAM);
}

// ===========================================================================
// SimpliciTI Top-Level — Encode
// ===========================================================================

TEST(SimplictiEncode, BasicFrame)
{
    const uint8_t payload[] = {0x01, 0x02, 0x03};

    simpliciti_frame_t frame = {};
    frame.mfri_header.dstaddr      = 0x11223344;
    frame.mfri_header.srcaddr      = 0x55667788;
    frame.mfri_header.length       = (uint8_t)(sizeof(nwk_header_t) + sizeof(payload));
    frame.nwk_header.port.app_port = NWK_PORT_PING;
    frame.nwk_header.tractid       = 0xAB;
    memcpy(frame.payload, payload, sizeof(payload));

    uint8_t buf[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(simpliciti_encode_frame(&frame, buf, &len), SIMPLICITI_SUCCESS);

    // Check MFRI header: dstaddr at bytes 1-4, srcaddr at bytes 5-8
    uint32_t dst, src;
    memcpy(&dst, buf + 1, 4);
    memcpy(&src, buf + 5, 4);
    EXPECT_EQ(dst, 0x11223344u);
    EXPECT_EQ(src, 0x55667788u);
    // NWK header follows MFRI header: tractid at wire byte 11
    EXPECT_EQ(buf[11], 0xABu) << "tractid at wire offset 11";
    // Payload follows NWK header: first byte at offset 12
    EXPECT_EQ(buf[12], 0x01u);
}

TEST(SimplictiEncode, NullParams)
{
    // null buffer propagates into nwk_encode_frame which validates it
    simpliciti_frame_t frame = {};
    size_t len = SIMPLICITI_MAX_FRAME_SIZE;
    EXPECT_EQ(simpliciti_encode_frame(&frame, nullptr, &len), SIMPLICITI_ERROR_INVALID_PARAM);
}

// ===========================================================================
// SimpliciTI Top-Level — Round-trip
// ===========================================================================

TEST(SimplictiRoundTrip, BasicFrame)
{
    const uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD};

    simpliciti_frame_t tx = {};
    tx.mfri_header.dstaddr       = 0x12345678;
    tx.mfri_header.srcaddr       = 0x87654321;
    tx.mfri_header.length        = (uint8_t)(sizeof(nwk_header_t) + sizeof(payload));
    tx.nwk_header.port.app_port  = NWK_PORT_LINK;
    tx.nwk_header.port.encrypted = false;
    tx.nwk_header.port.forwarded = false;
    tx.nwk_header.tractid        = 0x99;
    memcpy(tx.payload, payload, sizeof(payload));

    uint8_t buf[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(simpliciti_encode_frame(&tx, buf, &len), SIMPLICITI_SUCCESS);

    simpliciti_frame_t rx;
    ASSERT_EQ(simpliciti_decode_frame(buf, len, &rx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(rx.mfri_header.dstaddr,               tx.mfri_header.dstaddr);
    EXPECT_EQ(rx.mfri_header.srcaddr,               tx.mfri_header.srcaddr);
    EXPECT_EQ((int)rx.nwk_header.port.app_port,     (int)tx.nwk_header.port.app_port);
    EXPECT_EQ(rx.nwk_header.tractid,                tx.nwk_header.tractid);
    EXPECT_EQ(memcmp(rx.payload, payload, sizeof(payload)), 0);
}

// ===========================================================================
// Context API — shared test helpers
// ===========================================================================
//
// Ping payload layout (spec section 4.1):
//   Byte 0: app_info  — NWK_PORT_PING_REQUEST (0x01) or NWK_PORT_PING_RESPONSE (0x80)
//   Byte 1: TID       — echoed by responder
//
// Full ping wire layout:
//   [0]    : mfri_length = NWK_HEADER_SIZE + sizeof(simpliciti_ping_payload_t)
//   [1-4]  : dstaddr (LE)
//   [5-8]  : srcaddr (LE)
//   [9]    : NWK port byte = 0x01
//   [10]   : device_info
//   [11]   : tractid  (auto-assigned by send_msg; sole TID — no duplicate in payload)
//   [12]   : app_info (request / response)

#define PING_WIRE_PORT_OFFSET     (9)
#define PING_WIRE_DI_OFFSET       (10)
#define PING_WIRE_TRACTID_OFFSET  (11)
#define PING_WIRE_APP_INFO_OFFSET (12)

// device_info wire byte: ack_reply=1 → (0<<7)|(0<<6)|(0<<4)|(1<<3)|0 = 0x08
#define DI_ACK_REPLY_WIRE (0x08u)
// device_info wire byte: ack_req=1  → (1<<7)|(0<<6)|(0<<4)|(0<<3)|0 = 0x80
#define DI_ACK_REQ_WIRE   (0x80u)

static uint8_t  g_sent_buf[SIMPLICITI_MAX_FRAME_SIZE];
static size_t   g_sent_len   = 0;
static int      g_send_count = 0;
static uint32_t g_mock_time  = 0;

static simpliciti_status_t test_send_cb(const uint8_t *buf, size_t len)
{
    memcpy(g_sent_buf, buf, len);
    g_sent_len = len;
    g_send_count++;
    return SIMPLICITI_SUCCESS;
}

static simpliciti_status_t test_get_time_cb(uint32_t *ts)
{
    *ts = g_mock_time;
    return SIMPLICITI_SUCCESS;
}

static simpliciti_status_t test_get_time_fail_cb(uint32_t *ts)
{
    (void)ts;
    return SIMPLICITI_ERROR_INVALID_PARAM;
}

// Sets context to CONNECTED with the given peer (simulates a pre-established link).
static void set_connected(simpliciti_context_t *ctx, uint32_t peer_addr)
{
    ctx->link_info.link_state   = SIMPLICITI_LINK_STATE_CONNECTED;
    ctx->link_info.dest_address = peer_addr;
}

// Resets all shared state, inits the context, and returns it ready to use.
// Every test using send_msg/receive_msg must call this to isolate g_pending_msgs.
static simpliciti_context_t make_test_context(uint32_t addr)
{
    g_sent_len   = 0;
    g_send_count = 0;
    g_mock_time  = 0;
    simpliciti_context_t ctx = {};
    simpliciti_init(&ctx);
    ctx.device_address        = addr;
    ctx.callbacks.send_msg    = test_send_cb;
    ctx.callbacks.get_time_ms = test_get_time_cb;
    return ctx;
}

// Build a ping reply frame. tractid is NOT set — send_msg auto-assigns it.
// For sending a ping REQUEST, use simpliciti_send_ping() instead.
static simpliciti_frame_t make_ping_frame(uint32_t dst, uint32_t src, uint8_t app_info)
{
    simpliciti_frame_t frame = {};
    frame.mfri_header.dstaddr      = dst;
    frame.mfri_header.srcaddr      = src;
    frame.mfri_header.length       = (uint8_t)(sizeof(nwk_header_t) +
                                               sizeof(simpliciti_ping_payload_t));
    frame.nwk_header.port.app_port = NWK_PORT_PING;
    ((simpliciti_ping_payload_t *)frame.payload)->request = app_info;
    return frame;
}

// ===========================================================================
// SimpliciTI — Ping
// ===========================================================================

// Initiator sends a ping request via the send_ping API
TEST(SimplictiPing, SendRequest)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);

    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);
    ASSERT_GT(g_sent_len, (size_t)0);

    EXPECT_EQ(g_sent_buf[PING_WIRE_PORT_OFFSET],     0x01u);
    EXPECT_EQ(g_sent_buf[PING_WIRE_APP_INFO_OFFSET], NWK_PORT_PING_REQUEST);
}

// Responder sends a ping reply; app_info MSB must be set, payload TID echoed
TEST(SimplictiPing, SendReply)
{
    simpliciti_context_t ctx = make_test_context(0x11223344);
    simpliciti_frame_t frame = make_ping_frame(0xAABBCCDD, ctx.device_address,
                                               NWK_PORT_PING_RESPONSE);

    ASSERT_EQ(simpliciti_send_msg(&ctx, &frame, true, false), SIMPLICITI_SUCCESS);
    ASSERT_GT(g_sent_len, (size_t)0);

    EXPECT_EQ(g_sent_buf[PING_WIRE_PORT_OFFSET],     0x01u);
    EXPECT_EQ(g_sent_buf[PING_WIRE_APP_INFO_OFFSET], NWK_PORT_PING_RESPONSE) << "MSB set = reply";
}

// Request and reply are distinguishable by app_info byte alone; port byte is the same
TEST(SimplictiPing, RequestAndReplyDifferentiated)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);

    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);
    uint8_t req_port     = g_sent_buf[PING_WIRE_PORT_OFFSET];
    uint8_t req_app_info = g_sent_buf[PING_WIRE_APP_INFO_OFFSET];

    simpliciti_frame_t rep = make_ping_frame(0x11223344, ctx.device_address, NWK_PORT_PING_RESPONSE);
    ASSERT_EQ(simpliciti_send_msg(&ctx, &rep, true, false), SIMPLICITI_SUCCESS);
    uint8_t rep_port     = g_sent_buf[PING_WIRE_PORT_OFFSET];
    uint8_t rep_app_info = g_sent_buf[PING_WIRE_APP_INFO_OFFSET];

    EXPECT_EQ(req_port, rep_port)          << "same NWK port byte";
    EXPECT_EQ(req_app_info & 0x80u, 0x00u) << "request: MSB clear";
    EXPECT_EQ(rep_app_info & 0x80u, 0x80u) << "reply: MSB set";
}

// Receiving a ping request triggers an auto-reply via handle_ping
TEST(SimplictiPing, ReceiveRequestTriggersReply)
{
    const uint32_t my_addr   = 0xAABBCCDD;
    const uint32_t peer_addr = 0x11223344;
    simpliciti_context_t ctx = make_test_context(my_addr);
    set_connected(&ctx, peer_addr);

    uint8_t ping_payload[] = {NWK_PORT_PING_REQUEST};
    uint8_t wire[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(my_addr, peer_addr, 0x01, 0x00, 0x01,
                                        ping_payload, sizeof(ping_payload), wire);

    ASSERT_EQ(simpliciti_receive_msg(&ctx, wire, len), SIMPLICITI_SUCCESS);

    EXPECT_EQ(g_send_count, 1) << "one reply sent by handle_ping";
    EXPECT_EQ(g_sent_buf[PING_WIRE_APP_INFO_OFFSET], NWK_PORT_PING_RESPONSE);
    uint32_t reply_dst;
    memcpy(&reply_dst, g_sent_buf + 1, 4);
    EXPECT_EQ(reply_dst, peer_addr) << "reply addressed back to original sender";
}

// Receiving a ping response does not trigger a further send
TEST(SimplictiPing, ReceiveResponseNoExtraSend)
{
    const uint32_t my_addr   = 0xAABBCCDD;
    const uint32_t peer_addr = 0x11223344;
    simpliciti_context_t ctx = make_test_context(my_addr);
    set_connected(&ctx, peer_addr);

    uint8_t ping_payload[] = {NWK_PORT_PING_RESPONSE};
    uint8_t wire[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(my_addr, peer_addr, 0x01, 0x00, 0x01,
                                        ping_payload, sizeof(ping_payload), wire);

    ASSERT_EQ(simpliciti_receive_msg(&ctx, wire, len), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, 0) << "no extra send on response";
}

// ===========================================================================
// SimpliciTI — TID Management
// ===========================================================================

// After init, last_tid=0; first send_ping assigns tractid=1
TEST(TidManagement, AutoAssignedOnFirstSend)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);

    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);

    EXPECT_EQ(ctx.last_tid, 1u) << "tractid = last_tid+1 = 1";
    EXPECT_EQ(g_sent_buf[PING_WIRE_TRACTID_OFFSET], 1u);
}

// Each successive send increments the TID by one
TEST(TidManagement, IncrementsBetweenSends)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);

    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);
    uint8_t tid1 = g_sent_buf[PING_WIRE_TRACTID_OFFSET];
    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);
    uint8_t tid2 = g_sent_buf[PING_WIRE_TRACTID_OFFSET];

    EXPECT_EQ(tid2, tid1 + 1u);
    EXPECT_EQ(ctx.last_tid, tid2);
}

// tractid in the wire matches ctx.last_tid after send_ping
TEST(TidManagement, FrameTractidMatchesWire)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);

    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_sent_buf[PING_WIRE_TRACTID_OFFSET], ctx.last_tid);
}

// ===========================================================================
// SimpliciTI — Pending Messages
// ===========================================================================

// send_ping sets ack_req=1 implicitly; check_outgoing retransmits after timeout
TEST(PendingMsg, StoredAndRetransmittedOnTimeout)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);

    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, 1);

    g_mock_time = SIMPLICITI_ACK_TIMEOUT_MS + 1;
    ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, 2) << "retransmitted once after timeout";
}

// Message sent with ack_req=0 is not stored; no retransmit occurs
TEST(PendingMsg, NotStoredWithoutAckReq)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);
    simpliciti_frame_t frame = make_ping_frame(0x11223344, ctx.device_address,
                                               NWK_PORT_PING_REQUEST);
    frame.nwk_header.device_info.ack_req = false;

    ASSERT_EQ(simpliciti_send_msg(&ctx, &frame, false, false), SIMPLICITI_SUCCESS);

    g_mock_time = SIMPLICITI_ACK_TIMEOUT_MS + 1;
    ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, 1) << "no retransmit when ack_req=0";
}

// Receiving a reply with ack_reply=1 clears the pending message; no further retransmit
TEST(PendingMsg, ClearedOnAckReply)
{
    const uint32_t my_addr   = 0xAABBCCDD;
    const uint32_t peer_addr = 0x11223344;
    simpliciti_context_t ctx = make_test_context(my_addr);
    set_connected(&ctx, peer_addr);

    ASSERT_EQ(simpliciti_send_ping(&ctx, peer_addr), SIMPLICITI_SUCCESS);
    uint8_t assigned_tid = g_sent_buf[PING_WIRE_TRACTID_OFFSET];

    // Receive a reply with ack_reply=1 and the matching tractid
    uint8_t reply_payload[] = {NWK_PORT_PING_RESPONSE};
    uint8_t wire[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(my_addr, peer_addr,
                                        0x01, DI_ACK_REPLY_WIRE, assigned_tid,
                                        reply_payload, sizeof(reply_payload), wire);
    ASSERT_EQ(simpliciti_receive_msg(&ctx, wire, len), SIMPLICITI_SUCCESS);

    // Advance past timeout — no retransmit since pending msg was cleared
    g_mock_time = SIMPLICITI_ACK_TIMEOUT_MS + 1;
    int count_before = g_send_count;
    ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, count_before) << "cleared; no retransmit";
}

// Without get_time_ms, is_timeout_set=false; check_outgoing never retransmits
TEST(PendingMsg, NoTimeoutWithoutTimeCallback)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);
    ctx.callbacks.get_time_ms = nullptr;

    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);

    g_mock_time = SIMPLICITI_ACK_TIMEOUT_MS + 1;
    ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, 1) << "no retransmit without time callback";
}

// ===========================================================================
// SimpliciTI — Timeout and Retry
// ===========================================================================

// Each timeout triggers one retransmit with a rescheduled window
TEST(Retry, RetransmitsOnEachTimeout)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);

    g_mock_time = SIMPLICITI_ACK_TIMEOUT_MS + 1;
    ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, 2) << "first retransmit";

    g_mock_time += SIMPLICITI_ACK_TIMEOUT_MS;
    ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, 3) << "second retransmit";
}

// Total sends = 1 original + SIMPLICITI_MAX_RETRIES retransmissions
TEST(Retry, TotalSendCountMatchesMaxRetries)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);

    for (int i = 0; i < SIMPLICITI_MAX_RETRIES; i++)
    {
        g_mock_time += SIMPLICITI_ACK_TIMEOUT_MS + 1;
        ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    }

    EXPECT_EQ(g_send_count, 1 + SIMPLICITI_MAX_RETRIES)
        << "1 original + " << SIMPLICITI_MAX_RETRIES << " retransmits";
}

// After max retries are exhausted, the message is dropped — no further sends
TEST(Retry, StopsAfterMaxRetries)
{
    simpliciti_context_t ctx = make_test_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_send_ping(&ctx, 0x11223344), SIMPLICITI_SUCCESS);

    for (int i = 0; i < SIMPLICITI_MAX_RETRIES; i++)
    {
        g_mock_time += SIMPLICITI_ACK_TIMEOUT_MS + 1;
        ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    }
    int count_at_exhaustion = g_send_count;

    // One additional timeout — must be silent
    g_mock_time += SIMPLICITI_ACK_TIMEOUT_MS + 1;
    ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, count_at_exhaustion) << "dropped after max retries";
}

// ===========================================================================
// SimpliciTI — Link
// ===========================================================================

#define LINK_WIRE_PORT_OFFSET    (9)
#define LINK_WIRE_DI_OFFSET      (10)
#define LINK_WIRE_TRACTID_OFFSET (11)

static simpliciti_link_state_t g_last_link_state = SIMPLICITI_LINK_STATE_DISCONNECTED;
static int                     g_link_cb_count   = 0;

static simpliciti_status_t test_link_state_cb(simpliciti_link_state_t state)
{
    g_last_link_state = state;
    g_link_cb_count++;
    return SIMPLICITI_SUCCESS;
}

// Returns a DISCONNECTED context with the link_state_change callback wired up.
static simpliciti_context_t make_link_context(uint32_t addr)
{
    g_last_link_state = SIMPLICITI_LINK_STATE_DISCONNECTED;
    g_link_cb_count   = 0;
    simpliciti_context_t ctx = make_test_context(addr);
    ctx.callbacks.link_state_change = test_link_state_cb;
    return ctx;
}

// --- send_link ---

// send_link broadcasts a link request and transitions state to LINK_REQUESTED
TEST(SimplictiLink, SendLink_StateBecomesRequested)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_send_link(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(ctx.link_info.link_state, SIMPLICITI_LINK_STATE_LINK_REQUESTED);
}

// send_link wire: dst=broadcast, port=NWK_PORT_LINK, ack_req set
TEST(SimplictiLink, SendLink_WireFrame)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_send_link(&ctx), SIMPLICITI_SUCCESS);

    uint32_t dst;
    memcpy(&dst, g_sent_buf + 1, 4);
    EXPECT_EQ(dst, SIMPLICITI_BROADCAST_ADDRESS)                               << "link request must be broadcast";
    EXPECT_EQ(g_sent_buf[LINK_WIRE_PORT_OFFSET], (uint8_t)NWK_PORT_LINK)      << "port=LINK";
    EXPECT_EQ(g_sent_buf[LINK_WIRE_DI_OFFSET] & DI_ACK_REQ_WIRE, DI_ACK_REQ_WIRE) << "ack_req set";
}

// send_link fires link_state_change callback with LINK_REQUESTED
TEST(SimplictiLink, SendLink_FiresCallback)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_send_link(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_last_link_state, SIMPLICITI_LINK_STATE_LINK_REQUESTED);
    EXPECT_EQ(g_link_cb_count, 1);
}

// send_link fails if already in any non-DISCONNECTED state
TEST(SimplictiLink, SendLink_FailsIfNotDisconnected)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_send_link(&ctx), SIMPLICITI_SUCCESS); // → LINK_REQUESTED
    EXPECT_NE(simpliciti_send_link(&ctx), SIMPLICITI_SUCCESS)  << "reject when LINK_REQUESTED";

    ctx = make_link_context(0xAABBCCDD);
    set_connected(&ctx, 0x11223344);
    EXPECT_NE(simpliciti_send_link(&ctx), SIMPLICITI_SUCCESS)  << "reject when CONNECTED";
}

// --- wait_for_link ---

// wait_for_link transitions to WAITING_FOR_LINK without sending any frame
TEST(SimplictiLink, WaitForLink_StateBecomesWaiting)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_wait_for_link(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(ctx.link_info.link_state, SIMPLICITI_LINK_STATE_WAITING_FOR_LINK);
    EXPECT_EQ(g_send_count, 0) << "wait_for_link must not send a frame";
}

// wait_for_link fires link_state_change callback with WAITING_FOR_LINK
TEST(SimplictiLink, WaitForLink_FiresCallback)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_wait_for_link(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_last_link_state, SIMPLICITI_LINK_STATE_WAITING_FOR_LINK);
    EXPECT_EQ(g_link_cb_count, 1);
}

// wait_for_link fails if not DISCONNECTED
TEST(SimplictiLink, WaitForLink_FailsIfNotDisconnected)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_wait_for_link(&ctx), SIMPLICITI_SUCCESS); // → WAITING
    EXPECT_NE(simpliciti_wait_for_link(&ctx), SIMPLICITI_SUCCESS)  << "reject when WAITING_FOR_LINK";

    ctx = make_link_context(0xAABBCCDD);
    set_connected(&ctx, 0x11223344);
    EXPECT_NE(simpliciti_wait_for_link(&ctx), SIMPLICITI_SUCCESS)  << "reject when CONNECTED";
}

// --- Server-side: receive link request while WAITING_FOR_LINK ---

// Server receives a broadcast link request → sends ACK, becomes CONNECTED
TEST(SimplictiLink, Server_ReceivesLinkRequest_BecomesConnected)
{
    const uint32_t server_addr = 0xAABBCCDD;
    const uint32_t client_addr = 0x11223344;
    simpliciti_context_t ctx = make_link_context(server_addr);
    ASSERT_EQ(simpliciti_wait_for_link(&ctx), SIMPLICITI_SUCCESS);
    g_send_count = 0;

    uint8_t wire[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(SIMPLICITI_BROADCAST_ADDRESS, client_addr,
                                        NWK_PORT_LINK, DI_ACK_REQ_WIRE, 0x01,
                                        nullptr, 0, wire);
    ASSERT_EQ(simpliciti_receive_msg(&ctx, wire, len), SIMPLICITI_SUCCESS);

    EXPECT_EQ(ctx.link_info.link_state,   SIMPLICITI_LINK_STATE_CONNECTED);
    EXPECT_EQ(ctx.link_info.dest_address, client_addr);
}

// Server's ACK: ack_reply=1, tractid echoed, unicast back to client
TEST(SimplictiLink, Server_ReceivesLinkRequest_AckCorrect)
{
    const uint32_t server_addr = 0xAABBCCDD;
    const uint32_t client_addr = 0x11223344;
    const uint8_t  req_tid     = 0x07;
    simpliciti_context_t ctx = make_link_context(server_addr);
    ASSERT_EQ(simpliciti_wait_for_link(&ctx), SIMPLICITI_SUCCESS);
    g_send_count = 0;

    uint8_t wire[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(SIMPLICITI_BROADCAST_ADDRESS, client_addr,
                                        NWK_PORT_LINK, DI_ACK_REQ_WIRE, req_tid,
                                        nullptr, 0, wire);
    ASSERT_EQ(simpliciti_receive_msg(&ctx, wire, len), SIMPLICITI_SUCCESS);

    EXPECT_EQ(g_send_count, 1)                                                      << "server sends one ACK";
    uint32_t reply_dst;
    memcpy(&reply_dst, g_sent_buf + 1, 4);
    EXPECT_EQ(reply_dst, client_addr)                                               << "ACK sent to client";
    EXPECT_EQ(g_sent_buf[LINK_WIRE_PORT_OFFSET], (uint8_t)NWK_PORT_LINK)           << "port=LINK";
    EXPECT_EQ(g_sent_buf[LINK_WIRE_DI_OFFSET] & DI_ACK_REPLY_WIRE, DI_ACK_REPLY_WIRE) << "ack_reply set";
    EXPECT_EQ(g_sent_buf[LINK_WIRE_TRACTID_OFFSET], req_tid)                       << "tractid echoed";
}

// Server fires link_state_change callback with CONNECTED
TEST(SimplictiLink, Server_ReceivesLinkRequest_FiresCallback)
{
    const uint32_t server_addr = 0xAABBCCDD;
    const uint32_t client_addr = 0x11223344;
    simpliciti_context_t ctx = make_link_context(server_addr);
    ASSERT_EQ(simpliciti_wait_for_link(&ctx), SIMPLICITI_SUCCESS);
    g_link_cb_count = 0;

    uint8_t wire[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(SIMPLICITI_BROADCAST_ADDRESS, client_addr,
                                        NWK_PORT_LINK, DI_ACK_REQ_WIRE, 0x01,
                                        nullptr, 0, wire);
    ASSERT_EQ(simpliciti_receive_msg(&ctx, wire, len), SIMPLICITI_SUCCESS);

    EXPECT_EQ(g_last_link_state, SIMPLICITI_LINK_STATE_CONNECTED);
    EXPECT_GE(g_link_cb_count, 1);
}

// Link request while not WAITING_FOR_LINK is rejected by handle_link
TEST(SimplictiLink, Server_ReceivesLinkRequest_WhileDisconnected_Rejected)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    // DISCONNECTED + broadcast passes receive_msg gate but handle_link rejects it

    uint8_t wire[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(SIMPLICITI_BROADCAST_ADDRESS, 0x11223344,
                                        NWK_PORT_LINK, DI_ACK_REQ_WIRE, 0x01,
                                        nullptr, 0, wire);
    EXPECT_NE(simpliciti_receive_msg(&ctx, wire, len), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, 0);
}

// --- Client-side: receive link ACK while LINK_REQUESTED ---

// Client sends link, receives ACK → CONNECTED, dest_address set
TEST(SimplictiLink, Client_ReceivesLinkAck_BecomesConnected)
{
    const uint32_t client_addr = 0xAABBCCDD;
    const uint32_t server_addr = 0x11223344;
    simpliciti_context_t ctx = make_link_context(client_addr);
    ASSERT_EQ(simpliciti_send_link(&ctx), SIMPLICITI_SUCCESS);
    uint8_t sent_tid = g_sent_buf[LINK_WIRE_TRACTID_OFFSET];
    g_send_count = 0;

    uint8_t wire[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(client_addr, server_addr,
                                        NWK_PORT_LINK, DI_ACK_REPLY_WIRE, sent_tid,
                                        nullptr, 0, wire);
    ASSERT_EQ(simpliciti_receive_msg(&ctx, wire, len), SIMPLICITI_SUCCESS);

    EXPECT_EQ(ctx.link_info.link_state,   SIMPLICITI_LINK_STATE_CONNECTED);
    EXPECT_EQ(ctx.link_info.dest_address, server_addr);
    EXPECT_EQ(g_send_count, 0) << "no additional frame sent on ACK receipt";
}

// Client fires link_state_change callback with CONNECTED after ACK
TEST(SimplictiLink, Client_ReceivesLinkAck_FiresCallback)
{
    const uint32_t client_addr = 0xAABBCCDD;
    const uint32_t server_addr = 0x11223344;
    simpliciti_context_t ctx = make_link_context(client_addr);
    ASSERT_EQ(simpliciti_send_link(&ctx), SIMPLICITI_SUCCESS);
    uint8_t sent_tid = g_sent_buf[LINK_WIRE_TRACTID_OFFSET];
    g_link_cb_count = 0;

    uint8_t wire[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(client_addr, server_addr,
                                        NWK_PORT_LINK, DI_ACK_REPLY_WIRE, sent_tid,
                                        nullptr, 0, wire);
    ASSERT_EQ(simpliciti_receive_msg(&ctx, wire, len), SIMPLICITI_SUCCESS);

    EXPECT_EQ(g_last_link_state, SIMPLICITI_LINK_STATE_CONNECTED);
    EXPECT_GE(g_link_cb_count, 1);
}

// Pending link message is cleared after ACK — no retransmit occurs
TEST(SimplictiLink, Client_ReceivesLinkAck_ClearsPendingMsg)
{
    const uint32_t client_addr = 0xAABBCCDD;
    const uint32_t server_addr = 0x11223344;
    simpliciti_context_t ctx = make_link_context(client_addr);
    ASSERT_EQ(simpliciti_send_link(&ctx), SIMPLICITI_SUCCESS);
    uint8_t sent_tid = g_sent_buf[LINK_WIRE_TRACTID_OFFSET];

    uint8_t wire[SIMPLICITI_MAX_FRAME_SIZE];
    size_t len = build_simpliciti_frame(client_addr, server_addr,
                                        NWK_PORT_LINK, DI_ACK_REPLY_WIRE, sent_tid,
                                        nullptr, 0, wire);
    ASSERT_EQ(simpliciti_receive_msg(&ctx, wire, len), SIMPLICITI_SUCCESS);

    int count_before = g_send_count;
    g_mock_time = SIMPLICITI_ACK_TIMEOUT_MS + 1;
    ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, count_before) << "pending msg cleared, no retransmit";
}

// --- Disconnect ---

// disconnect sends unlink frame to peer and transitions to DISCONNECTED
TEST(SimplictiLink, Disconnect_SendsUnlinkAndBecomesDisconnected)
{
    const uint32_t my_addr   = 0xAABBCCDD;
    const uint32_t peer_addr = 0x11223344;
    simpliciti_context_t ctx = make_link_context(my_addr);
    set_connected(&ctx, peer_addr);

    ASSERT_EQ(simpliciti_disconnect(&ctx), SIMPLICITI_SUCCESS);

    EXPECT_EQ(ctx.link_info.link_state,   SIMPLICITI_LINK_STATE_DISCONNECTED);
    EXPECT_EQ(ctx.link_info.dest_address, 0u);
    EXPECT_EQ(g_send_count, 1) << "one unlink frame sent";
    uint32_t unlink_dst;
    memcpy(&unlink_dst, g_sent_buf + 1, 4);
    EXPECT_EQ(unlink_dst, peer_addr)                                             << "unlink addressed to peer";
    EXPECT_EQ(g_sent_buf[LINK_WIRE_PORT_OFFSET], (uint8_t)NWK_PORT_UNLINK)      << "port=UNLINK";
}

// disconnect fires link_state_change callback with DISCONNECTED
TEST(SimplictiLink, Disconnect_FiresCallback)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    set_connected(&ctx, 0x11223344);
    g_link_cb_count = 0;

    ASSERT_EQ(simpliciti_disconnect(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_last_link_state, SIMPLICITI_LINK_STATE_DISCONNECTED);
    EXPECT_EQ(g_link_cb_count, 1);
}

// disconnect when not connected is a no-op — no frame sent, returns SUCCESS
TEST(SimplictiLink, Disconnect_WhenNotConnected_IsNoOp)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    EXPECT_EQ(simpliciti_disconnect(&ctx), SIMPLICITI_SUCCESS);
    EXPECT_EQ(g_send_count, 0) << "no frame sent when not connected";
}

// --- Retry exhaustion for link messages ---

// After MAX_RETRIES retransmits + one exhaustion check, link state becomes DISCONNECTED
TEST(SimplictiLink, LinkRetryExhausted_BecomesDisconnected)
{
    simpliciti_context_t ctx = make_link_context(0xAABBCCDD);
    ASSERT_EQ(simpliciti_send_link(&ctx), SIMPLICITI_SUCCESS);
    g_link_cb_count = 0;

    // Drain all retries (each timeout retransmits until retries reaches 0)
    for (int i = 0; i < SIMPLICITI_MAX_RETRIES; i++)
    {
        g_mock_time += SIMPLICITI_ACK_TIMEOUT_MS + 1;
        ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);
    }
    // One more timeout triggers the exhaustion path
    g_mock_time += SIMPLICITI_ACK_TIMEOUT_MS + 1;
    ASSERT_EQ(simpliciti_check_outgoing_messages(&ctx), SIMPLICITI_SUCCESS);

    EXPECT_EQ(ctx.link_info.link_state, SIMPLICITI_LINK_STATE_DISCONNECTED);
    EXPECT_EQ(g_last_link_state,        SIMPLICITI_LINK_STATE_DISCONNECTED);
    EXPECT_GE(g_link_cb_count, 1);
}
