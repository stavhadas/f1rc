#include <gtest/gtest.h>

extern "C"
{
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hldc.h"
#include "hldc_internal.h"
#include "crc.h"
#include "tracing.h"
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Byte-stuff a single byte into `out`; returns number of bytes written (1 or 2). */
static size_t stuff_byte(uint8_t byte, uint8_t out[2])
{
    if (byte == 0x7E)
    {
        out[0] = 0x7D;
        out[1] = 0x5E;
        return 2;
    }
    if (byte == 0x7D)
    {
        out[0] = 0x7D;
        out[1] = 0x5D;
        return 2;
    }
    out[0] = byte;
    return 1;
}

/**
 * Builds a spec-compliant raw HDLC wire frame into `out` and returns its length.
 *
 * Wire layout: [0x7E][addr][ctrl][stuffed_payload…][stuffed_crc_lo][stuffed_crc_hi][0x7E]
 *
 * CRC is computed over [addr, ctrl, payload] (unstuffed) and stored in
 * little-endian byte order so the decoder's *(uint16_t*) read recovers the
 * original value on a little-endian host.
 *
 * Byte-stuffing is applied to every payload byte and both CRC bytes.
 * The address and control bytes are NOT stuffed (they follow the start flag
 * directly, matching the decoder's dedicated ADDRESS / CONTROL states).
 */
static size_t build_raw_frame(uint8_t addr, uint8_t ctrl,
                              const uint8_t *payload, size_t plen,
                              uint8_t *out, size_t out_size)
{
    // CRC input: addr || ctrl || payload (all unstuffed)
    uint8_t crc_input[2 + MAX_PAYLOAD_SIZE];
    crc_input[0] = addr;
    crc_input[1] = ctrl;
    if (plen > 0 && payload != nullptr)
        memcpy(crc_input + 2, payload, plen);
    uint16_t crc = crc16_calculate(crc_input, 2 + plen, 0xFFFF);

    size_t idx = 0;
    out[idx++] = 0x7E; // start flag
    out[idx++] = addr; // address  (no stuffing — dedicated decoder state)
    out[idx++] = ctrl; // control  (no stuffing — dedicated decoder state)

    uint8_t tmp[2];
    for (size_t i = 0; i < plen; i++)
    {
        size_t n = stuff_byte(payload[i], tmp);
        memcpy(out + idx, tmp, n);
        idx += n;
    }

    // CRC stored little-endian so decoder's *(uint16_t*) recovers `crc` directly
    size_t n;
    n = stuff_byte(crc & 0xFF, tmp);
    memcpy(out + idx, tmp, n);
    idx += n;
    n = stuff_byte((crc >> 8) & 0xFF, tmp);
    memcpy(out + idx, tmp, n);
    idx += n;

    out[idx++] = 0x7E; // end flag
    return idx;
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class HldcTest : public ::testing::Test
{
protected:
    hldc_context_t ctx;
    hldc_frame_t last_received;
    bool frame_received;
    bool error_received;
    hldc_status_t last_error_status;

    static HldcTest *instance;

    static void on_frame(hldc_frame_t * frame)
    {
        instance->last_received = *frame;
        instance->frame_received = true;
    }

    static void on_error(hldc_status_t status)
    {
        instance->last_error_status = status;
        instance->error_received = true;
    }

    void SetUp() override
    {
        hldc_reset_context(&ctx);
        ctx.on_frame = HldcTest::on_frame;
        ctx.on_error = HldcTest::on_error;
        frame_received = false;
        error_received = false;
        last_error_status = HLDC_STATUS_OK;
        memset(&last_received, 0, sizeof(last_received));
        instance = this;
    }

    /**
     * Encode `frame`, reset the decoder context, then push the encoded bytes
     * through the decoder. Returns the first error from either step.
     */
    hldc_status_t encode_and_decode(hldc_frame_t * frame)
    {
        uint8_t buf[HLDC_MAX_FRAME_SIZE * 2];
        size_t len = sizeof(buf);
        hldc_status_t s = hldc_encode(frame, buf, &len);
        if (s != HLDC_STATUS_OK)
            return s;

        hldc_reset_context(&ctx);
        ctx.on_frame = HldcTest::on_frame;
        ctx.on_error = HldcTest::on_error;
        return hldc_push_bytes(&ctx, buf, len);
    }
};

HldcTest *HldcTest::instance = nullptr;

// ===========================================================================
// Encoder — parameter validation  (Tests 16-21)
// ===========================================================================

TEST(HldcEncode, NullFrame)
{
    uint8_t buf[HLDC_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    EXPECT_EQ(hldc_encode(nullptr, buf, &len), HLDC_STATUS_INPUT_PARAMS_ERROR);
}

TEST(HldcEncode, NullBuffer)
{
    hldc_frame_t frame = {};
    size_t len = HLDC_MAX_FRAME_SIZE;
    EXPECT_EQ(hldc_encode(&frame, nullptr, &len), HLDC_STATUS_INPUT_PARAMS_ERROR);
}

TEST(HldcEncode, NullLength)
{
    hldc_frame_t frame = {};
    uint8_t buf[HLDC_MAX_FRAME_SIZE];
    EXPECT_EQ(hldc_encode(&frame, buf, nullptr), HLDC_STATUS_INPUT_PARAMS_ERROR);
}

TEST(HldcEncode, BufferTooSmall)
{
    hldc_frame_t frame = {};
    frame.payload_length = 10;
    // Buffer holds only 5 bytes — smaller than HLDC_FRAME_OVERHEAD (6) + 10
    uint8_t buf[5];
    size_t len = sizeof(buf);
    EXPECT_EQ(hldc_encode(&frame, buf, &len), HLDC_STATUS_INPUT_PARAMS_ERROR);
}

TEST(HldcEncode, OutputLength_EmptyPayload)
{
    hldc_frame_t frame = {};
    frame.header.address = 0xFF;
    frame.header.control = 0x03;
    frame.payload_length = 0;

    uint8_t buf[HLDC_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(hldc_encode(&frame, buf, &len), HLDC_STATUS_OK);
    EXPECT_EQ(len, (size_t)HLDC_FRAME_OVERHEAD);
}

TEST(HldcEncode, OutputLength_WithPayload)
{
    hldc_frame_t frame = {};
    frame.header.address = 0xFF;
    frame.header.control = 0x03;
    const char msg[] = "Hello";
    frame.payload_length = sizeof(msg) - 1;
    memcpy(frame.payload, msg, frame.payload_length);

    uint8_t buf[HLDC_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(hldc_encode(&frame, buf, &len), HLDC_STATUS_OK);
    EXPECT_EQ(len, HLDC_FRAME_OVERHEAD + frame.payload_length);
}

// ===========================================================================
// Encoder — wire format  (Tests 1, 4, 6)
// ===========================================================================

// Test 1 — Basic frame
TEST(HldcEncode, BasicFrame_WireFormat)
{
    hldc_frame_t frame = {};
    frame.header.address = 0xFF;
    frame.header.control = 0x03;
    const uint8_t payload[] = {'H', 'e', 'l', 'l', 'o'};
    frame.payload_length = sizeof(payload);
    memcpy(frame.payload, payload, frame.payload_length);

    uint8_t buf[HLDC_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(hldc_encode(&frame, buf, &len), HLDC_STATUS_OK);

    EXPECT_EQ(buf[0], (uint8_t)0x7E) << "start flag";
    EXPECT_EQ(buf[1], (uint8_t)0xFF) << "address";
    EXPECT_EQ(buf[2], (uint8_t)0x03) << "control";
    EXPECT_EQ(buf[3], (uint8_t)'H') << "first payload byte";
    EXPECT_EQ(buf[len - 1], (uint8_t)0x7E) << "end flag";
}

// Test 4 — Byte-stuffing: 0x7E in payload
TEST(HldcEncode, ByteStuff_FlagInPayload)
{
    hldc_frame_t frame = {};
    frame.header.address = 0xFF;
    frame.header.control = 0x03;
    frame.payload[0] = 0x7E;
    frame.payload_length = 1;

    uint8_t buf[HLDC_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(hldc_encode(&frame, buf, &len), HLDC_STATUS_OK);

    // The 0x7E payload byte must appear as the escape pair 0x7D 0x5E
    bool found_escape_pair = false;
    for (size_t i = 1; i < len - 1; i++)
    { // skip frame delimiters
        if (buf[i] == 0x7D && buf[i + 1] == 0x5E)
        {
            found_escape_pair = true;
            break;
        }
    }
    EXPECT_TRUE(found_escape_pair) << "0x7E payload byte must be encoded as 0x7D 0x5E";

    // No bare 0x7E between the two frame flags
    for (size_t i = 1; i < len - 1; i++)
    {
        EXPECT_NE(buf[i], (uint8_t)0x7E) << "bare 0x7E found at wire index " << i;
    }
}

// Test 6 — Byte-stuffing: 0x7D in payload
TEST(HldcEncode, ByteStuff_EscapeInPayload)
{
    hldc_frame_t frame = {};
    frame.header.address = 0xFF;
    frame.header.control = 0x03;
    frame.payload[0] = 0x7D;
    frame.payload_length = 1;

    uint8_t buf[HLDC_MAX_FRAME_SIZE];
    size_t len = sizeof(buf);
    ASSERT_EQ(hldc_encode(&frame, buf, &len), HLDC_STATUS_OK);

    bool found_escape_pair = false;
    for (size_t i = 1; i < len - 1; i++)
    {
        if (buf[i] == 0x7D && buf[i + 1] == 0x5D)
        {
            found_escape_pair = true;
            break;
        }
    }
    EXPECT_TRUE(found_escape_pair) << "0x7D payload byte must be encoded as 0x7D 0x5D";
}

// ===========================================================================
// Decoder — manually-built frames  (Tests 1, 2, 3, 5, 6)
// ===========================================================================

// Test 1 (decoder half) — Basic frame
TEST_F(HldcTest, Decode_BasicFrame)
{
    const uint8_t payload[] = {'H', 'e', 'l', 'l', 'o'};
    uint8_t raw[HLDC_MAX_FRAME_SIZE * 2];
    size_t raw_len = build_raw_frame(0xFF, 0x03, payload, sizeof(payload),
                                     raw, sizeof(raw));

    ASSERT_EQ(hldc_push_bytes(&ctx, raw, raw_len), HLDC_STATUS_OK);
    EXPECT_TRUE(frame_received);
    ASSERT_EQ(last_received.payload_length, sizeof(payload));
    EXPECT_EQ(memcmp(last_received.payload, payload, sizeof(payload)), 0);
}

// Test 5 — Empty payload
TEST_F(HldcTest, Decode_EmptyPayload)
{
    uint8_t raw[HLDC_MAX_FRAME_SIZE];
    size_t raw_len = build_raw_frame(0xFF, 0x03, nullptr, 0,
                                     raw, sizeof(raw));

    ASSERT_EQ(hldc_push_bytes(&ctx, raw, raw_len), HLDC_STATUS_OK);
    EXPECT_TRUE(frame_received);
    EXPECT_EQ(last_received.payload_length, 0u);
}

// Test 2 — Byte-stuffing: 0x7E in payload
TEST_F(HldcTest, Decode_ByteStuff_Flag)
{
    const uint8_t payload[] = {0x7E};
    uint8_t raw[HLDC_MAX_FRAME_SIZE];
    size_t raw_len = build_raw_frame(0xFF, 0x03, payload, sizeof(payload),
                                     raw, sizeof(raw));

    ASSERT_EQ(hldc_push_bytes(&ctx, raw, raw_len), HLDC_STATUS_OK);
    EXPECT_TRUE(frame_received);
    ASSERT_EQ(last_received.payload_length, 1u);
    EXPECT_EQ(last_received.payload[0], (uint8_t)0x7E);
}

// Test 3 — Byte-stuffing: 0x7D in payload
TEST_F(HldcTest, Decode_ByteStuff_Escape)
{
    const uint8_t payload[] = {0x7D};
    uint8_t raw[HLDC_MAX_FRAME_SIZE];
    size_t raw_len = build_raw_frame(0xFF, 0x03, payload, sizeof(payload),
                                     raw, sizeof(raw));

    ASSERT_EQ(hldc_push_bytes(&ctx, raw, raw_len), HLDC_STATUS_OK);
    EXPECT_TRUE(frame_received);
    ASSERT_EQ(last_received.payload_length, 1u);
    EXPECT_EQ(last_received.payload[0], (uint8_t)0x7D);
}

// Test 4 — Byte-stuffing: sequential 0x7E 0x7D 0x7E 0x7D
TEST_F(HldcTest, Decode_ByteStuff_Sequential)
{
    const uint8_t payload[] = {0x7E, 0x7D, 0x7E, 0x7D};
    uint8_t raw[HLDC_MAX_FRAME_SIZE];
    size_t raw_len = build_raw_frame(0xFF, 0x03, payload, sizeof(payload),
                                     raw, sizeof(raw));

    ASSERT_EQ(hldc_push_bytes(&ctx, raw, raw_len), HLDC_STATUS_OK);
    EXPECT_TRUE(frame_received);
    ASSERT_EQ(last_received.payload_length, sizeof(payload));
    EXPECT_EQ(memcmp(last_received.payload, payload, sizeof(payload)), 0);
}

// Test 6 — Back-to-back frames sharing a single 0x7E delimiter
TEST_F(HldcTest, Decode_BackToBack)
{
    const uint8_t p1[] = {'A', 'B', 'C'};
    const uint8_t p2[] = {'X', 'Y', 'Z'};

    uint8_t frame1[HLDC_MAX_FRAME_SIZE], frame2[HLDC_MAX_FRAME_SIZE];
    size_t len1 = build_raw_frame(0xFF, 0x03, p1, sizeof(p1), frame1, sizeof(frame1));
    size_t len2 = build_raw_frame(0xFF, 0x03, p2, sizeof(p2), frame2, sizeof(frame2));

    // Concatenate: the end flag of frame1 serves as the start flag of frame2
    uint8_t combined[HLDC_MAX_FRAME_SIZE * 2];
    memcpy(combined, frame1, len1);
    memcpy(combined + len1, frame2 + 1, len2 - 1); // skip frame2's redundant start flag
    size_t combined_len = len1 + len2 - 1;

    // Use a dedicated counter callback for this test
    struct BackToBackCtx
    {
        int count;
        hldc_frame_t frames[2];
    };
    static BackToBackCtx btb;
    btb.count = 0;
    ctx.on_frame = [](hldc_frame_t *f)
    {
        if (btb.count < 2)
            btb.frames[btb.count] = *f;
        btb.count++;
    };

    hldc_push_bytes(&ctx, combined, combined_len);

    EXPECT_EQ(btb.count, 2);

    ASSERT_EQ(btb.frames[0].payload_length, sizeof(p1));
    EXPECT_EQ(memcmp(btb.frames[0].payload, p1, sizeof(p1)), 0);

    ASSERT_EQ(btb.frames[1].payload_length, sizeof(p2));
    EXPECT_EQ(memcmp(btb.frames[1].payload, p2, sizeof(p2)), 0);
}

// ===========================================================================
// Round-trip: encode → decode  (Tests 1, 2, 3, 4, 5)
// ===========================================================================

// Test 1 (round-trip) — Basic frame
TEST_F(HldcTest, RoundTrip_BasicFrame)
{
    hldc_frame_t frame = {};
    frame.header.address = 0xFF;
    frame.header.control = 0x03;
    const uint8_t payload[] = {'H', 'e', 'l', 'l', 'o'};
    frame.payload_length = sizeof(payload);
    memcpy(frame.payload, payload, frame.payload_length);

    ASSERT_EQ(encode_and_decode(&frame), HLDC_STATUS_OK);
    EXPECT_TRUE(frame_received);
    ASSERT_EQ(last_received.payload_length, sizeof(payload));
    EXPECT_EQ(memcmp(last_received.payload, payload, sizeof(payload)), 0);
}

// Test 2 (round-trip) — Byte-stuffing: flag byte
TEST_F(HldcTest, RoundTrip_ByteStuff_Flag)
{
    hldc_frame_t frame = {};
    frame.header.address = 0xFF;
    frame.header.control = 0x03;
    frame.payload[0] = 0x7E;
    frame.payload_length = 1;

    ASSERT_EQ(encode_and_decode(&frame), HLDC_STATUS_OK);
    EXPECT_TRUE(frame_received);
    ASSERT_EQ(last_received.payload_length, 1u);
    EXPECT_EQ(last_received.payload[0], (uint8_t)0x7E);
}

// Test 3 (round-trip) — Byte-stuffing: escape byte
TEST_F(HldcTest, RoundTrip_ByteStuff_Escape)
{
    hldc_frame_t frame = {};
    frame.header.address = 0xFF;
    frame.header.control = 0x03;
    frame.payload[0] = 0x7D;
    frame.payload_length = 1;

    ASSERT_EQ(encode_and_decode(&frame), HLDC_STATUS_OK);
    EXPECT_TRUE(frame_received);
    ASSERT_EQ(last_received.payload_length, 1u);
    EXPECT_EQ(last_received.payload[0], (uint8_t)0x7D);
}

// Test 4 (round-trip) — Byte-stuffing: sequential special bytes
TEST_F(HldcTest, RoundTrip_ByteStuff_Sequential)
{
    hldc_frame_t frame = {};
    frame.header.address = 0xFF;
    frame.header.control = 0x03;
    const uint8_t payload[] = {0x7E, 0x7D, 0x7E, 0x7D};
    frame.payload_length = sizeof(payload);
    memcpy(frame.payload, payload, frame.payload_length);

    ASSERT_EQ(encode_and_decode(&frame), HLDC_STATUS_OK);
    EXPECT_TRUE(frame_received);
    ASSERT_EQ(last_received.payload_length, sizeof(payload));
    EXPECT_EQ(memcmp(last_received.payload, payload, sizeof(payload)), 0);
}

// Test 5 (round-trip) — Empty payload
TEST_F(HldcTest, RoundTrip_EmptyPayload)
{
    hldc_frame_t frame = {};
    frame.header.address = 0xFF;
    frame.header.control = 0x03;
    frame.payload_length = 0;

    ASSERT_EQ(encode_and_decode(&frame), HLDC_STATUS_OK);
    EXPECT_TRUE(frame_received);
    EXPECT_EQ(last_received.payload_length, 0u);
}

// ===========================================================================
// Error handling  (Tests 7–11)
// ===========================================================================

// Test 7 — Corrupted CRC: flip one bit in the payload region
TEST_F(HldcTest, Error_CorruptedCRC)
{
    const uint8_t payload[] = {'H', 'e', 'l', 'l', 'o'};
    uint8_t raw[HLDC_MAX_FRAME_SIZE];
    size_t raw_len = build_raw_frame(0xFF, 0x03, payload, sizeof(payload),
                                     raw, sizeof(raw));

    // Flip bit 0 of the first payload byte (wire index 3: after start+addr+ctrl)
    raw[3] ^= 0x01;

    hldc_push_bytes(&ctx, raw, raw_len);
    EXPECT_TRUE(error_received);
    EXPECT_EQ(last_error_status, HLDC_STATUS_CRC_ERROR);
    EXPECT_FALSE(frame_received);
}

// Test 8 — Premature EOF: end flag arrives before a full 2-byte CRC is buffered
TEST_F(HldcTest, Error_PrematureEOF)
{
    // [start][addr][ctrl][1 payload byte][end] — only 1 accumulated byte,
    // but the decoder needs at least 2 (HLDC_TRAILER_SIZE) to extract the CRC
    const uint8_t stream[] = {0x7E, 0xFF, 0x03, 0xAA, 0x7E};
    hldc_push_bytes(&ctx, stream, sizeof(stream));
    EXPECT_TRUE(error_received);
    EXPECT_EQ(last_error_status, HLDC_STATUS_DECODE_ERROR);
    EXPECT_FALSE(frame_received);
}

// Test 9 — Runt frame: end flag arrives in PAYLOAD state with zero accumulated bytes
TEST_F(HldcTest, Error_RuntFrame)
{
    // [start][addr][ctrl][end] — no payload/CRC bytes at all
    const uint8_t stream[] = {0x7E, 0xFF, 0x03, 0x7E};
    hldc_push_bytes(&ctx, stream, sizeof(stream));
    EXPECT_TRUE(error_received);
    EXPECT_EQ(last_error_status, HLDC_STATUS_DECODE_ERROR);
    EXPECT_FALSE(frame_received);
}

// Test 10 — Giant frame: incoming payload exceeds the internal payload buffer
TEST_F(HldcTest, Error_GiantFrame_Overflow)
{
    // Build one contiguous stream: [start][addr][ctrl][MAX_PAYLOAD_SIZE + HLDC_TRAILER_SIZE + 1 data bytes]
    // Total = 3 + 1027 = 1030 bytes — exactly fills the decode buffer.
    // The 1027th payload byte pushes payload_length to MAX_PAYLOAD_SIZE + HLDC_TRAILER_SIZE, triggering overflow.
    uint8_t stream[3 + MAX_PAYLOAD_SIZE + HLDC_TRAILER_SIZE + 1];
    stream[0] = 0x7E;
    stream[1] = 0xFF;
    stream[2] = 0x03;
    memset(stream + 3, 0x55, sizeof(stream) - 3);

    hldc_push_bytes(&ctx, stream, sizeof(stream));
    EXPECT_TRUE(error_received);
    EXPECT_EQ(last_error_status, HLDC_STATUS_DECODE_ERROR);
    EXPECT_FALSE(frame_received);
}

// Test 11 — Dangling escape: 0x7D immediately before the end flag
TEST_F(HldcTest, Error_DanglingEscape)
{
    // Build a valid frame, then replace the last data byte (before end flag) with 0x7D
    const uint8_t payload[] = {0x01, 0x02, 0x03};
    uint8_t raw[HLDC_MAX_FRAME_SIZE];
    size_t raw_len = build_raw_frame(0xFF, 0x03, payload, sizeof(payload),
                                     raw, sizeof(raw));

    // raw[raw_len - 1] is the end flag (0x7E)
    // raw[raw_len - 2] is the last byte before the end flag
    // Overwrite it with 0x7D to create a dangling escape sequence
    raw[raw_len - 2] = 0x7D;

    hldc_push_bytes(&ctx, raw, raw_len);
    // Dangling 0x7D is consumed as an escape prefix; no error fires until the next frame boundary
    EXPECT_FALSE(error_received);
    EXPECT_FALSE(frame_received);
}

// Test 12: Incomplete packet than a valid packet, make sure that the valid packet is decoded correctly and the incomplete one is discarded
TEST_F(HldcTest, Error_IncompleteThenValid)
{
    // Build an incomplete frame (missing end flag)
    const uint8_t payload1[] = {0x01, 0x02, 0x03};
    uint8_t raw1[HLDC_MAX_FRAME_SIZE];
    size_t raw1_len = build_raw_frame(0xFF, 0x03, payload1, sizeof(payload1),
                                      raw1, sizeof(raw1));
    // Remove the end flag to make it incomplete
    raw1_len -= 1;

    // Build a valid frame
    const uint8_t payload2[] = {0x0A, 0x0B, 0x0C};
    uint8_t raw2[HLDC_MAX_FRAME_SIZE];
    size_t raw2_len = build_raw_frame(0xFF, 0x03, payload2, sizeof(payload2),
                                      raw2, sizeof(raw2));
    // Concatenate the incomplete frame followed by the valid frame
    uint8_t combined[HLDC_MAX_FRAME_SIZE * 2];
    memcpy(combined, raw1, raw1_len);
    memcpy(combined + raw1_len, raw2, raw2_len);
    size_t combined_len = raw1_len + raw2_len;
    hldc_status_t s = hldc_push_bytes(&ctx, combined, combined_len);
    EXPECT_EQ(s, HLDC_STATUS_OK); // Decoder should process the valid frame after discarding the incomplete one
    EXPECT_TRUE(frame_received);
    ASSERT_EQ(last_received.payload_length, sizeof(payload2));
    EXPECT_EQ(memcmp(last_received.payload, payload2, sizeof(payload2)), 0);
}
