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
