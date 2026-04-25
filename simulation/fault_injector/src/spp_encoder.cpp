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

void spp_encode_header(uint8_t *buf, uint16_t apid,
                       uint16_t seq_count, uint16_t data_len)
{
    /* Byte 0: version=000, type=0 (TM), sec_hdr_flag=0, apid[10:8] */
    buf[0] = (uint8_t)((apid >> 8U) & 0x07U);

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
    const size_t total = 6U + PAYLOAD_PACKET_DROP;  /* 16 bytes */
    if (out_cap < total) { return 0U; }

    /* data_len = PAYLOAD_PACKET_DROP - 1 = 9 */
    spp_encode_header(out, APID_PACKET_DROP, seq_count,
                      (uint16_t)(PAYLOAD_PACKET_DROP - 1U));

    uint8_t *ud = out + 6U;
    ud[0] = link_id;
    ud[1] = 0x00U;  /* reserved */
    put_be16(&ud[2], drop_probability_x10000);
    put_be32(&ud[4], duration_ms);

    /* CRC over user-data bytes [0..7] (before the CRC field) */
    const uint16_t crc = spp_crc16(ud, 8U);
    put_be16(&ud[8], crc);

    return total;
}

size_t spp_encode_clock_skew(uint8_t *out,   size_t out_cap,
                              uint16_t seq_count,
                              uint8_t  asset_class,
                              uint8_t  instance_id,
                              int32_t  offset_ms,
                              int32_t  rate_ppm_x1000,
                              uint32_t duration_s)
{
    const size_t total = 6U + PAYLOAD_CLOCK_SKEW;  /* 22 bytes */
    if (out_cap < total) { return 0U; }

    /* data_len = PAYLOAD_CLOCK_SKEW - 1 = 15 */
    spp_encode_header(out, APID_CLOCK_SKEW, seq_count,
                      (uint16_t)(PAYLOAD_CLOCK_SKEW - 1U));

    uint8_t *ud = out + 6U;
    ud[0] = asset_class;
    ud[1] = instance_id;
    put_be32(&ud[2],  (uint32_t)offset_ms);
    put_be32(&ud[6],  (uint32_t)rate_ppm_x1000);
    put_be32(&ud[10], duration_s);

    const uint16_t crc = spp_crc16(ud, 14U);
    put_be16(&ud[14], crc);

    return total;
}

size_t spp_encode_safe_mode(uint8_t *out,    size_t out_cap,
                             uint16_t seq_count,
                             uint8_t  asset_class,
                             uint8_t  instance_id,
                             uint16_t trigger_reason_code)
{
    const size_t total = 6U + PAYLOAD_SAFE_MODE;  /* 12 bytes */
    if (out_cap < total) { return 0U; }

    /* data_len = PAYLOAD_SAFE_MODE - 1 = 5 */
    spp_encode_header(out, APID_SAFE_MODE, seq_count,
                      (uint16_t)(PAYLOAD_SAFE_MODE - 1U));

    uint8_t *ud = out + 6U;
    ud[0] = asset_class;
    ud[1] = instance_id;
    put_be16(&ud[2], trigger_reason_code);

    const uint16_t crc = spp_crc16(ud, 4U);
    put_be16(&ud[4], crc);

    return total;
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
    const size_t total = 6U + PAYLOAD_SENSOR_NOISE;  /* 26 bytes */
    if (out_cap < total) { return 0U; }

    /* data_len = PAYLOAD_SENSOR_NOISE - 1 = 19 */
    spp_encode_header(out, APID_SENSOR_NOISE, seq_count,
                      (uint16_t)(PAYLOAD_SENSOR_NOISE - 1U));

    uint8_t *ud = out + 6U;
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

    return total;
}
