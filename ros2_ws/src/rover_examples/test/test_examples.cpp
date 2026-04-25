#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "rover_cryobot/hdlc_lite.hpp"
#include "rover_examples/hdlc_example.hpp"
#include "rover_examples/tether_pipeline_example.hpp"
#include "rover_examples/tm_bridge_example.hpp"

/* ── hdlc_example ────────────────────────────────────────────────────────── */

TEST(HdlcExample, ComputeCrcKat)
{
    const std::vector<uint8_t> input = {'1','2','3','4','5','6','7','8','9'};
    EXPECT_EQ(rover_examples::compute_crc(input), 0x29B1U);
}

TEST(HdlcExample, BuildNominalFrameSuccess)
{
    const std::vector<uint8_t> payload(10U, 0xABU);
    const auto frame = rover_examples::build_nominal_frame(payload);
    ASSERT_FALSE(frame.empty());
    EXPECT_EQ(frame.front(), rover_cryobot::HdlcLite::FLAG);
    EXPECT_EQ(frame.back(),  rover_cryobot::HdlcLite::FLAG);
}

TEST(HdlcExample, BuildNominalFrameOversized)
{
    /* 241 B exceeds MAX_PAYLOAD_NOMINAL — exception path returns empty. */
    const std::vector<uint8_t> payload(241U, 0x00U);
    const auto frame = rover_examples::build_nominal_frame(payload);
    EXPECT_TRUE(frame.empty());
}

TEST(HdlcExample, BuildCollapseFrameSuccess)
{
    const std::vector<uint8_t> payload(20U, 0xCDU);
    const auto frame = rover_examples::build_collapse_frame(payload);
    ASSERT_FALSE(frame.empty());
    EXPECT_EQ(frame.front(), rover_cryobot::HdlcLite::FLAG);
    EXPECT_EQ(frame.back(),  rover_cryobot::HdlcLite::FLAG);
}

TEST(HdlcExample, BuildCollapseFrameOversized)
{
    /* 81 B exceeds MAX_PAYLOAD_COLLAPSE — exception path returns empty. */
    const std::vector<uint8_t> payload(81U, 0x00U);
    const auto frame = rover_examples::build_collapse_frame(payload);
    EXPECT_TRUE(frame.empty());
}

TEST(HdlcExample, DecodeFrameSuccess)
{
    const std::vector<uint8_t> payload = {0x11, 0x7E, 0x7D, 0x5E, 0xFF};
    const auto wire = rover_examples::build_nominal_frame(payload);
    ASSERT_FALSE(wire.empty());
    const auto decoded = rover_examples::decode_frame(wire);
    EXPECT_EQ(decoded, payload);
}

TEST(HdlcExample, DecodeFrameCorrupt)
{
    /* A wire frame with only flag bytes has no valid content → error path. */
    const std::vector<uint8_t> bad_wire = {
        rover_cryobot::HdlcLite::FLAG, 0x00U, rover_cryobot::HdlcLite::FLAG};
    const auto decoded = rover_examples::decode_frame(bad_wire);
    EXPECT_TRUE(decoded.empty());
}

/* ── tm_bridge_example ───────────────────────────────────────────────────── */

TEST(TmBridgeExample, PackSingleHkSuccess)
{
    /* 16-byte CCSDS packet: 6B primary + 10B secondary, zero payload. */
    const auto pkt = rover_examples::pack_single_hk(0x300U, {}, 0U, 0U);
    ASSERT_EQ(pkt.size(), 16U);
    /* APID in bits [10:0] of the first two header bytes (big-endian). */
    const uint16_t apid = (static_cast<uint16_t>(pkt[0] & 0x07U) << 8U) | pkt[1];
    EXPECT_EQ(apid, 0x300U);
}

TEST(TmBridgeExample, PackSingleHkOversized)
{
    /* payload > 65 526 B hits the explicit guard and returns empty. */
    const std::vector<uint8_t> huge(65527U, 0xFFU);
    const auto pkt = rover_examples::pack_single_hk(0x300U, huge, 0U, 0U);
    EXPECT_TRUE(pkt.empty());
}

TEST(TmBridgeExample, PackHkSequenceEmpty)
{
    /* Zero payloads — loop never executes, result is empty. */
    const auto seq = rover_examples::pack_hk_sequence(0x300U, {}, 0U, 0U);
    EXPECT_TRUE(seq.empty());
}

TEST(TmBridgeExample, PackHkSequenceMultiple)
{
    /* Three valid payloads → three packets, sequence counter advances. */
    const std::vector<std::vector<uint8_t>> payloads = {
        {0x01U}, {0x02U}, {0x03U}};
    const auto seq = rover_examples::pack_hk_sequence(0x300U, payloads, 0U, 0U);
    ASSERT_EQ(seq.size(), 3U);
    for (const auto & pkt : seq) {
        EXPECT_GE(pkt.size(), 16U);
    }
}

TEST(TmBridgeExample, PackHkSequenceOversizedEntry)
{
    /* An oversized payload produces an empty packet that is skipped. */
    const std::vector<std::vector<uint8_t>> payloads = {
        std::vector<uint8_t>(65527U, 0xFFU)};
    const auto seq = rover_examples::pack_hk_sequence(0x300U, payloads, 0U, 0U);
    EXPECT_TRUE(seq.empty());
}

/* ── tether_pipeline_example ─────────────────────────────────────────────── */

TEST(TetherPipelineExample, HkToWireSuccess)
{
    /* Normal path: empty HK → CCSDS SPP (16 B) → NOMINAL HDLC frame. */
    const auto wire = rover_examples::hk_to_wire(0x400U, {}, 0U, 0U);
    ASSERT_FALSE(wire.empty());
    EXPECT_EQ(wire.front(), rover_cryobot::HdlcLite::FLAG);
    EXPECT_EQ(wire.back(),  rover_cryobot::HdlcLite::FLAG);
}

TEST(TetherPipelineExample, HkToWireFailure)
{
    /* SPP of 16 + 225 = 241 B exceeds NOMINAL limit (240 B) → empty path. */
    const std::vector<uint8_t> big_payload(225U, 0xAAU);
    const auto wire = rover_examples::hk_to_wire(0x400U, big_payload, 0U, 0U);
    EXPECT_TRUE(wire.empty());
}

TEST(TetherPipelineExample, HkToWireCollapseSuccess)
{
    /* Normal path: empty HK → CCSDS SPP (16 B) → BW-COLLAPSE HDLC frame. */
    const auto wire = rover_examples::hk_to_wire_collapse(0x400U, {}, 0U, 0U);
    ASSERT_FALSE(wire.empty());
    EXPECT_EQ(wire.front(), rover_cryobot::HdlcLite::FLAG);
    EXPECT_EQ(wire.back(),  rover_cryobot::HdlcLite::FLAG);
}

TEST(TetherPipelineExample, HkToWireCollapseFailure)
{
    /* SPP of 16 + 65 = 81 B exceeds COLLAPSE limit (80 B) → empty path. */
    const std::vector<uint8_t> big_payload(65U, 0xBBU);
    const auto wire = rover_examples::hk_to_wire_collapse(0x400U, big_payload, 0U, 0U);
    EXPECT_TRUE(wire.empty());
}
