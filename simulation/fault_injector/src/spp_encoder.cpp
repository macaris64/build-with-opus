#include "spp_encoder.h"

#include <cstring>

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection).
 * Identical algorithm to MCU gateway apps (mcu_eps_gw.c MCU_EPS_GW_Crc16). */
uint16_t spp_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0U; i < len; ++i)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8U);
        for (int bit = 0; bit < 8; ++bit)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1U);
            }
        }
    }
    return crc;
}

/* Write the mandatory 10-byte secondary header at buf[0..9].
 * Wire layout (ccsds_wire secondary.rs §2.5):
 *   [0]    CUC P-Field  = 0x2F
 *   [1..4] CUC coarse time (4 B BE)
 *   [5..6] CUC fine time   (2 B BE)
 *   [7..8] FuncCode (u16 BE, nonzero — 0x0100 = HK telemetry)
 *   [9]    InstanceId  (u8, nonzero — 0x01) */
static void spp_encode_secondary_header(uint8_t *buf)
{
    buf[0] = 0x2FU;                 /* CUC P-Field */
    buf[1] = 0x00U; buf[2] = 0x00U; buf[3] = 0x00U; buf[4] = 0x00U; /* coarse */
    buf[5] = 0x00U; buf[6] = 0x00U;                                   /* fine   */
    buf[7] = 0x01U; buf[8] = 0x00U; /* FuncCode = 0x0100 (nonzero)   */
    buf[9] = 0x01U;                 /* InstanceId = 0x01 (nonzero)    */
}

void spp_encode_header(uint8_t *buf, uint16_t apid,
                       uint16_t seq_count, uint16_t data_len)
{
    /* Byte 0: version=000, type=0 (TM), sec_hdr_flag=1, apid[10:8]
     * sec_hdr_flag MUST be 1 (bit 3); ccsds_wire::PrimaryHeader::decode
     * checks the nibble (version|sec_hdr) == 0b0001 and rejects otherwise. */
    buf[0] = (uint8_t)(0x08U | ((apid >> 8U) & 0x07U));

    /* Byte 1: apid[7:0] */
    buf[1] = (uint8_t)(apid & 0xFFU);

    /* Byte 2: seq_flags=0b11 (standalone), seq_count[13:8] */
    buf[2] = (uint8_t)(0xC0U | ((seq_count >> 8U) & 0x3FU));

    /* Byte 3: seq_count[7:0] */
    buf[3] = (uint8_t)(seq_count & 0xFFU);

    /* Bytes 4–5: data_len BE */
    buf[4] = (uint8_t)((data_len >> 8U) & 0xFFU);
    buf[5] = (uint8_t)(data_len & 0xFFU);
}

/* Helper: write a big-endian u16 at p. */
static void put_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)((v >> 8U) & 0xFFU);
    p[1] = (uint8_t)(v & 0xFFU);
}

/* Helper: write a big-endian u32 at p. */
static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24U) & 0xFFU);
    p[1] = (uint8_t)((v >> 16U) & 0xFFU);
    p[2] = (uint8_t)((v >> 8U)  & 0xFFU);
    p[3] = (uint8_t)(v & 0xFFU);
}

size_t spp_encode_packet_drop(uint8_t *out,   size_t out_cap,
                               uint16_t seq_count,
                               uint8_t  link_id,
                               uint16_t drop_probability_x10000,
                               uint32_t duration_ms)
{
    if (out_cap < SPP_MAX_BYTES) { return 0U; }

    spp_encode_header(out, APID_PACKET_DROP, seq_count, SPP_DATA_LENGTH);
    spp_encode_secondary_header(out + 6U);

    uint8_t *ud = out + SPP_FAULT_PAYLOAD_OFFSET;
    ud[0] = link_id;
    ud[1] = 0x00U;  /* reserved */
    put_be16(&ud[2], drop_probability_x10000);
    put_be32(&ud[4], duration_ms);

    const uint16_t crc = spp_crc16(ud, 8U);
    put_be16(&ud[8], crc);

    return SPP_MAX_BYTES;
}

size_t spp_encode_clock_skew(uint8_t *out,   size_t out_cap,
                              uint16_t seq_count,
                              uint8_t  asset_class,
                              uint8_t  instance_id,
                              int32_t  offset_ms,
                              int32_t  rate_ppm_x1000,
                              uint32_t duration_s)
{
    if (out_cap < SPP_MAX_BYTES) { return 0U; }

    spp_encode_header(out, APID_CLOCK_SKEW, seq_count, SPP_DATA_LENGTH);
    spp_encode_secondary_header(out + 6U);

    uint8_t *ud = out + SPP_FAULT_PAYLOAD_OFFSET;
    ud[0] = asset_class;
    ud[1] = instance_id;
    put_be32(&ud[2],  (uint32_t)offset_ms);
    put_be32(&ud[6],  (uint32_t)rate_ppm_x1000);
    put_be32(&ud[10], duration_s);

    const uint16_t crc = spp_crc16(ud, 14U);
    put_be16(&ud[14], crc);

    return SPP_MAX_BYTES;
}

size_t spp_encode_safe_mode(uint8_t *out,    size_t out_cap,
                             uint16_t seq_count,
                             uint8_t  asset_class,
                             uint8_t  instance_id,
                             uint16_t trigger_reason_code)
{
    if (out_cap < SPP_MAX_BYTES) { return 0U; }

    spp_encode_header(out, APID_SAFE_MODE, seq_count, SPP_DATA_LENGTH);
    spp_encode_secondary_header(out + 6U);

    uint8_t *ud = out + SPP_FAULT_PAYLOAD_OFFSET;
    ud[0] = asset_class;
    ud[1] = instance_id;
    put_be16(&ud[2], trigger_reason_code);

    const uint16_t crc = spp_crc16(ud, 4U);
    put_be16(&ud[4], crc);

    return SPP_MAX_BYTES;
}

size_t spp_encode_sensor_noise(uint8_t *out,  size_t out_cap,
                                uint16_t seq_count,
                                uint8_t  asset_class,
                                uint8_t  instance_id,
                                uint16_t sensor_index,
                                uint8_t  noise_model,
                                int32_t  noise_param_1,
                                int32_t  noise_param_2,
                                uint32_t duration_ms)
{
    if (out_cap < SPP_MAX_BYTES) { return 0U; }

    spp_encode_header(out, APID_SENSOR_NOISE, seq_count, SPP_DATA_LENGTH);
    spp_encode_secondary_header(out + 6U);

    uint8_t *ud = out + SPP_FAULT_PAYLOAD_OFFSET;
    ud[0] = asset_class;
    ud[1] = instance_id;
    put_be16(&ud[2],  sensor_index);
    ud[4] = noise_model;
    ud[5] = 0x00U;  /* reserved */
    put_be32(&ud[6],  (uint32_t)noise_param_1);
    put_be32(&ud[10], (uint32_t)noise_param_2);
    put_be32(&ud[14], duration_ms);

    const uint16_t crc = spp_crc16(ud, 18U);
    put_be16(&ud[18], crc);

    return SPP_MAX_BYTES;
}
