#include "rover_cryobot/hdlc_lite.hpp"

#include <stdexcept>

namespace rover_cryobot
{

/* ── CRC-16/CCITT-FALSE ────────────────────────────────────────────────────
 * poly=0x1021, init=0xFFFF, no input/output reflection, no final XOR.
 * Test vector: crc16_ccitt_false({'1'..'9'}) == 0x29B1.
 */
uint16_t HdlcLite::crc16_ccitt_false(const std::vector<uint8_t> & data)
{
    uint16_t crc = 0xFFFFU;
    for (uint8_t byte : data) {
        crc ^= (static_cast<uint16_t>(byte) << 8U);
        for (int i = 0; i < 8; ++i) {
            if ((crc & 0x8000U) != 0U) {
                crc = static_cast<uint16_t>((crc << 1U) ^ 0x1021U);
            } else {
                crc = static_cast<uint16_t>(crc << 1U);
            }
        }
    }
    return crc;
}

/* ── Byte-stuffing helper ──────────────────────────────────────────────── */

static void stuff_byte(std::vector<uint8_t> & out, uint8_t b)
{
    if (b == HdlcLite::FLAG) {
        out.push_back(HdlcLite::ESC);
        out.push_back(static_cast<uint8_t>(HdlcLite::FLAG ^ 0x20U));  /* 0x5E */
    } else if (b == HdlcLite::ESC) {
        out.push_back(HdlcLite::ESC);
        out.push_back(static_cast<uint8_t>(HdlcLite::ESC ^ 0x20U));   /* 0x5D */
    } else {
        out.push_back(b);
    }
}

/* ── encode ─────────────────────────────────────────────────────────────── */

std::vector<uint8_t> HdlcLite::encode(
    uint8_t mode,
    const std::vector<uint8_t> & payload)
{
    /* Enforce per-mode payload size limit (§3.1, ICD-cryobot-tether.md). */
    if (mode == MODE_NOMINAL && payload.size() > MAX_PAYLOAD_NOMINAL) {
        throw std::invalid_argument(
            "HdlcLite::encode: payload exceeds NOMINAL mode limit (240 B)");
    }
    if (mode == MODE_COLLAPSE && payload.size() > MAX_PAYLOAD_COLLAPSE) {
        throw std::invalid_argument(
            "HdlcLite::encode: payload exceeds BW-COLLAPSE mode limit (80 B)");
    }

    auto length = static_cast<uint16_t>(payload.size());

    /* Build unstuffed body: mode + length(2B BE) + payload, then CRC.
     * CRC is computed over all of these before CRC bytes are appended. */
    std::vector<uint8_t> body;
    body.reserve(3U + payload.size() + 2U);
    body.push_back(mode);
    body.push_back(static_cast<uint8_t>(length >> 8U));
    body.push_back(static_cast<uint8_t>(length & 0xFFU));
    body.insert(body.end(), payload.begin(), payload.end());

    uint16_t crc = crc16_ccitt_false(body);  /* CRC over mode+length+payload */
    body.push_back(static_cast<uint8_t>(crc >> 8U));   /* MSB first (BE, Q-C8) */
    body.push_back(static_cast<uint8_t>(crc & 0xFFU));

    /* Apply byte stuffing to the entire body (mode+length+payload+CRC) and
     * wrap with FLAG delimiters. */
    std::vector<uint8_t> wire;
    wire.reserve(2U + body.size() * 2U);  /* worst case: every byte stuffed */
    wire.push_back(FLAG);
    for (uint8_t b : body) {
        stuff_byte(wire, b);
    }
    wire.push_back(FLAG);

    return wire;
}

/* ── decode ─────────────────────────────────────────────────────────────── */

std::optional<std::vector<uint8_t>> HdlcLite::decode(
    const std::vector<uint8_t> & wire)
{
    /* Minimum valid frame: FLAG + mode(1) + length(2) + CRC(2) + FLAG = 7 B */
    if (wire.size() < 7U || wire.front() != FLAG || wire.back() != FLAG) {
        return std::nullopt;
    }

    /* Unstuff the inner bytes (between the two FLAG delimiters). */
    std::vector<uint8_t> body;
    body.reserve(wire.size());
    bool escape_next = false;
    for (size_t i = 1U; i < wire.size() - 1U; ++i) {
        uint8_t b = wire[i];
        if (escape_next) {
            if (b == 0x5EU) {
                body.push_back(FLAG);
            } else if (b == 0x5DU) {
                body.push_back(ESC);
            } else {
                return std::nullopt;  /* Invalid escape sequence */
            }
            escape_next = false;
        } else if (b == ESC) {
            escape_next = true;
        } else if (b == FLAG) {
            return std::nullopt;  /* Unescaped FLAG inside frame body */
        } else {
            body.push_back(b);
        }
    }
    if (escape_next) {
        return std::nullopt;  /* Trailing ESC with no following byte */
    }

    /* body = mode(1) + length(2) + payload(N) + CRC(2); minimum 5 bytes. */
    if (body.size() < 5U) {
        return std::nullopt;
    }

    auto length = static_cast<uint16_t>(
        (static_cast<uint16_t>(body[1]) << 8U) | body[2]);

    /* Check that body length matches the declared payload length. */
    if (body.size() != 1U + 2U + static_cast<size_t>(length) + 2U) {
        return std::nullopt;
    }

    /* Extract payload. */
    std::vector<uint8_t> payload(
        body.begin() + 3,
        body.begin() + 3 + static_cast<ptrdiff_t>(length));

    /* Extract and verify CRC. */
    size_t crc_off = 3U + static_cast<size_t>(length);
    auto extracted_crc = static_cast<uint16_t>(
        (static_cast<uint16_t>(body[crc_off]) << 8U) | body[crc_off + 1U]);

    /* Recompute CRC over mode + length + payload (first 3+length bytes). */
    std::vector<uint8_t> crc_input(body.begin(), body.begin() + 3 + static_cast<ptrdiff_t>(length));
    uint16_t computed_crc = crc16_ccitt_false(crc_input);

    if (computed_crc != extracted_crc) {
        return std::nullopt;
    }

    return payload;
}

}  // namespace rover_cryobot
