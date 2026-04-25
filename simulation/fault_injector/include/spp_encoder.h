#pragma once

#include <cstdint>
#include <cstddef>

/* Maximum encoded SPP size for any fault-injection packet (primary header +
 * largest user-data payload: 0x543 = 6 + 20 = 26 bytes). */
static constexpr size_t SPP_MAX_BYTES = 26U;

/* CCSDS APID constants for the four fault types (ICD-sim-fsw.md §2). */
static constexpr uint16_t APID_PACKET_DROP  = 0x0540U;
static constexpr uint16_t APID_CLOCK_SKEW   = 0x0541U;
static constexpr uint16_t APID_SAFE_MODE    = 0x0542U;
static constexpr uint16_t APID_SENSOR_NOISE = 0x0543U;

/* User-data sizes for each fault type (packet-catalog §7). */
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
 *   data_len   — CCSDS Packet Data Field length - 1 (= user_data_bytes - 1)
 *
 * All multi-byte fields are big-endian per Q-C8 / CCSDS 133.0-B-2.
 * sec_hdr_flag is 0 for sideband fault-injection packets (no secondary header
 * defined in packet-catalog §7).
 */
void spp_encode_header(uint8_t *buf, uint16_t apid,
                       uint16_t seq_count, uint16_t data_len);

/* Packet-drop SPP (APID 0x540, 16 bytes total).
 * Returns the number of bytes written into out (always PAYLOAD_PACKET_DROP + 6 = 16). */
size_t spp_encode_packet_drop(uint8_t *out,   size_t out_cap,
                               uint16_t seq_count,
                               uint8_t  link_id,
                               uint16_t drop_probability_x10000,
                               uint32_t duration_ms);

/* Clock-skew SPP (APID 0x541, 22 bytes total). */
size_t spp_encode_clock_skew(uint8_t *out,   size_t out_cap,
                              uint16_t seq_count,
                              uint8_t  asset_class,
                              uint8_t  instance_id,
                              int32_t  offset_ms,
                              int32_t  rate_ppm_x1000,
                              uint32_t duration_s);

/* Force-safe-mode SPP (APID 0x542, 12 bytes total). */
size_t spp_encode_safe_mode(uint8_t *out,    size_t out_cap,
                             uint16_t seq_count,
                             uint8_t  asset_class,
                             uint8_t  instance_id,
                             uint16_t trigger_reason_code);

/* Sensor-noise SPP (APID 0x543, 26 bytes total). */
size_t spp_encode_sensor_noise(uint8_t *out,  size_t out_cap,
                                uint16_t seq_count,
                                uint8_t  asset_class,
                                uint8_t  instance_id,
                                uint16_t sensor_index,
                                uint8_t  noise_model,
                                int32_t  noise_param_1,
                                int32_t  noise_param_2,
                                uint32_t duration_ms);
