#pragma once

#include <cstdint>
#include <cstddef>

/* Total SPP size for fault-injection packets: must exactly fill the 1016-byte
 * AOS data field (1024 B frame − 6 B header − 2 B FECF) so that
 * ccsds_wire::SpacePacket::parse passes its LengthMismatch check.
 * Layout: 6 B primary | 10 B secondary | ≤20 B fault payload | zero padding. */
static constexpr size_t SPP_MAX_BYTES = 1016U;

/* CCSDS Packet Data Field length - 1, declared in primary header bytes 4-5.
 * data_length = SPP_MAX_BYTES - 6 - 1 = 1009 per ccsds_wire::SpacePacket::parse
 * contract: declared_total = data_length + 7 must equal buf.len(). */
static constexpr uint16_t SPP_DATA_LENGTH = 1009U;

/* Byte offset where fault-specific user data begins inside the SPP buffer.
 * = 6 (primary header) + 10 (secondary header). */
static constexpr size_t SPP_FAULT_PAYLOAD_OFFSET = 16U;

/* CCSDS APID constants for the four fault types (ICD-sim-fsw.md §2). */
static constexpr uint16_t APID_PACKET_DROP  = 0x0540U;
static constexpr uint16_t APID_CLOCK_SKEW   = 0x0541U;
static constexpr uint16_t APID_SAFE_MODE    = 0x0542U;
static constexpr uint16_t APID_SENSOR_NOISE = 0x0543U;

/* User-data sizes for each fault type (packet-catalog §7), excluding secondary header. */
static constexpr size_t PAYLOAD_PACKET_DROP  = 10U;  /* 8 B data + 2 B CRC */
static constexpr size_t PAYLOAD_CLOCK_SKEW   = 16U;  /* 14 B data + 2 B CRC */
static constexpr size_t PAYLOAD_SAFE_MODE    = 6U;   /* 4 B data + 2 B CRC */
static constexpr size_t PAYLOAD_SENSOR_NOISE = 20U;  /* 18 B data + 2 B CRC */

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over the user-data bytes
 * that precede the trailing crc16 field (packet-catalog §7.x). */
uint16_t spp_crc16(const uint8_t *data, size_t len);

/**
 * spp_encode_header — Write a 6-byte CCSDS primary header into buf[0..5].
 *
 * Parameters:
 *   buf        — output buffer (must be at least 6 bytes)
 *   apid       — 11-bit APID (0x000–0x7FE; 0x7FF is idle/reserved)
 *   seq_count  — 14-bit rolling sequence count for this APID
 *   data_len   — CCSDS Packet Data Field length - 1 (use SPP_DATA_LENGTH for
 *                AOS-compatible fault packets)
 *
 * Sets sec_hdr_flag=1 (required by ccsds_wire::PrimaryHeader::decode).
 * All multi-byte fields are big-endian per Q-C8 / CCSDS 133.0-B-2.
 */
void spp_encode_header(uint8_t *buf, uint16_t apid,
                       uint16_t seq_count, uint16_t data_len);

/* Packet-drop SPP (APID 0x540, SPP_MAX_BYTES total, fault payload at offset 16).
 * Returns SPP_MAX_BYTES on success, 0 if out_cap < SPP_MAX_BYTES. */
size_t spp_encode_packet_drop(uint8_t *out,   size_t out_cap,
                               uint16_t seq_count,
                               uint8_t  link_id,
                               uint16_t drop_probability_x10000,
                               uint32_t duration_ms);

/* Clock-skew SPP (APID 0x541, SPP_MAX_BYTES total). */
size_t spp_encode_clock_skew(uint8_t *out,   size_t out_cap,
                              uint16_t seq_count,
                              uint8_t  asset_class,
                              uint8_t  instance_id,
                              int32_t  offset_ms,
                              int32_t  rate_ppm_x1000,
                              uint32_t duration_s);

/* Force-safe-mode SPP (APID 0x542, SPP_MAX_BYTES total). */
size_t spp_encode_safe_mode(uint8_t *out,    size_t out_cap,
                             uint16_t seq_count,
                             uint8_t  asset_class,
                             uint8_t  instance_id,
                             uint16_t trigger_reason_code);

/* Sensor-noise SPP (APID 0x543, SPP_MAX_BYTES total). */
size_t spp_encode_sensor_noise(uint8_t *out,  size_t out_cap,
                                uint16_t seq_count,
                                uint8_t  asset_class,
                                uint8_t  instance_id,
                                uint16_t sensor_index,
                                uint8_t  noise_model,
                                int32_t  noise_param_1,
                                int32_t  noise_param_2,
                                uint32_t duration_ms);
