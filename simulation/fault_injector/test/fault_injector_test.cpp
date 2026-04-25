/* fault_injector_test.cpp — Unit tests for spp_encoder and FaultInjector.
 *
 * Tests are self-contained: no external test framework dependency.
 * Returns 0 on all-pass, 1 on any failure. Registered with CTest.
 *
 * Coverage target: 100% branch on spp_encoder.cpp and fault_injector.cpp.
 */

#include "spp_encoder.h"
#include "fault_injector.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

/* ── Minimal test harness ──────────────────────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if ((cond)) {                                                         \
            ++g_pass;                                                         \
        } else {                                                              \
            ++g_fail;                                                         \
            std::fprintf(stderr, "FAIL %s:%d  " #cond "\n",                  \
                         __FILE__, __LINE__);                                 \
        }                                                                     \
    } while (0)

/* ── CRC-16 known-answer tests ────────────────────────────────────────────── */

static void test_crc16_empty()
{
    /* Given an empty buffer, When CRC is computed,
     * Then it equals the initial value 0xFFFF. */
    const uint16_t crc = spp_crc16(nullptr, 0U);
    CHECK(crc == 0xFFFFU);
}

static void test_crc16_known_answer_0x00()
{
    /* Given a single 0x00 byte, When CRC-16/CCITT-FALSE is computed,
     * Then it equals 0xE1F0 (KAT from standard test vectors). */
    const uint8_t  buf[] = {0x00U};
    const uint16_t crc   = spp_crc16(buf, 1U);
    CHECK(crc == 0xE1F0U);
}

static void test_crc16_known_answer_0xFF()
{
    /* Given a single 0xFF byte, Then CRC-16/CCITT-FALSE equals 0xFF00.
     * Derivation: init=0xFFFF; XOR with (0xFF<<8)=0x00FF; 8 shifts with
     * all-zero MSBs → 0x00FF<<8 = 0xFF00. */
    const uint8_t  buf[] = {0xFFU};
    const uint16_t crc   = spp_crc16(buf, 1U);
    CHECK(crc == 0xFF00U);
}

static void test_crc16_abc_sequence()
{
    /* Given bytes 0x01 0x02 0x03, When CRC is computed,
     * Then the result is deterministic and non-trivially verifiable. */
    const uint8_t  buf[] = {0x01U, 0x02U, 0x03U};
    const uint16_t crc1  = spp_crc16(buf, 3U);
    const uint16_t crc2  = spp_crc16(buf, 3U);
    CHECK(crc1 == crc2);       /* deterministic */
    CHECK(crc1 != 0xFFFFU);    /* non-trivial */
}

/* ── SPP encoder tests ────────────────────────────────────────────────────── */

static void test_spp_header_apid_and_length()
{
    /* Given APID 0x541 and data_len 15, When header is encoded,
     * Then APID bits land in bytes 0-1 and data_len in bytes 4-5. */
    uint8_t buf[6] = {};
    spp_encode_header(buf, 0x0541U, 0U, 15U);

    const uint16_t apid_decoded = (uint16_t)(((buf[0] & 0x07U) << 8U) | buf[1]);
    CHECK(apid_decoded == 0x0541U);

    const uint16_t dlen = (uint16_t)((buf[4] << 8U) | buf[5]);
    CHECK(dlen == 15U);
}

static void test_spp_header_seq_flags_standalone()
{
    /* Given any APID, When header is encoded,
     * Then bits [7:6] of byte 2 are 0b11 (standalone sequence flags). */
    uint8_t buf[6] = {};
    spp_encode_header(buf, 0x0540U, 7U, 9U);
    CHECK((buf[2] & 0xC0U) == 0xC0U);
}

static void test_spp_encode_packet_drop_length()
{
    /* Given a packet-drop fault, When encoded,
     * Then the total length is 16 bytes (6 hdr + 10 user-data). */
    uint8_t buf[SPP_MAX_BYTES];
    const size_t n = spp_encode_packet_drop(buf, sizeof(buf),
                                             0U,    /* seq */
                                             1U,    /* link_id ORBITER-RELAY */
                                             2500U, /* drop_prob */
                                             60000U);
    CHECK(n == 16U);
}

static void test_spp_encode_packet_drop_crc_appended()
{
    /* Given a packet-drop packet, When encoded,
     * Then the last two bytes are a valid CRC-16 over the 8 preceding user-data bytes. */
    uint8_t buf[SPP_MAX_BYTES];
    spp_encode_packet_drop(buf, sizeof(buf), 0U, 1U, 2500U, 60000U);

    const uint8_t *ud  = buf + 6U;
    const uint16_t expected = spp_crc16(ud, 8U);
    const uint16_t actual   = (uint16_t)((ud[8] << 8U) | ud[9]);
    CHECK(actual == expected);
}

static void test_spp_encode_clock_skew_length()
{
    /* Given a clock-skew fault, When encoded,
     * Then total length is 22 bytes (6 hdr + 16 user-data). */
    uint8_t buf[SPP_MAX_BYTES];
    const size_t n = spp_encode_clock_skew(buf, sizeof(buf),
                                            0U, 0U, 1U,
                                            500,   /* offset_ms */
                                            0,     /* rate_ppm_x1000 */
                                            2U);
    CHECK(n == 22U);
}

static void test_spp_encode_safe_mode_length()
{
    /* Given a safe-mode fault, When encoded,
     * Then total length is 12 bytes (6 hdr + 6 user-data). */
    uint8_t buf[SPP_MAX_BYTES];
    const size_t n = spp_encode_safe_mode(buf, sizeof(buf),
                                           0U, 0U, 1U, 0x0001U);
    CHECK(n == 12U);
}

static void test_spp_encode_sensor_noise_length()
{
    /* Given a sensor-noise fault, When encoded,
     * Then total length is 26 bytes (6 hdr + 20 user-data). */
    uint8_t buf[SPP_MAX_BYTES];
    const size_t n = spp_encode_sensor_noise(buf, sizeof(buf),
                                              0U,
                                              0U, 1U,    /* asset, instance */
                                              0x0001U,   /* sensor_index */
                                              1U,        /* noise_model GAUSSIAN */
                                              100, 0,    /* params */
                                              5000U);
    CHECK(n == 26U);
}

static void test_spp_encode_returns_zero_if_buf_too_small()
{
    /* Given a buffer smaller than the minimum packet, When encoding,
     * Then the encoder returns 0 (bounds-guard). */
    uint8_t tiny[4];
    const size_t n = spp_encode_packet_drop(tiny, sizeof(tiny),
                                             0U, 0U, 0U, 0U);
    CHECK(n == 0U);
}

static void test_spp_apid_in_encoded_clock_skew()
{
    /* Given a clock-skew packet, When encoded,
     * Then APID bytes decode to 0x0541. */
    uint8_t buf[SPP_MAX_BYTES];
    spp_encode_clock_skew(buf, sizeof(buf), 0U, 0U, 1U, 0, 0, 0U);
    const uint16_t apid = (uint16_t)(((buf[0] & 0x07U) << 8U) | buf[1]);
    CHECK(apid == 0x0541U);
}

/* ── FaultInjector scenario-parser tests ──────────────────────────────────── */

static std::string write_tmp_scenario(const char *content)
{
    const std::string path = "/tmp/fi_test_scenario.yaml";
    std::ofstream f(path);
    f << content;
    return path;
}

static void test_load_scenario_clock_skew_parses()
{
    /* Given a valid clock-skew scenario YAML, When loaded,
     * Then the fault list contains one event with correct fields. */
    const std::string path = write_tmp_scenario(
        "scenario: SCN-TEST-01\n"
        "faults:\n"
        "  - type: clock_skew\n"
        "    at_tai_offset_s: 0.1\n"
        "    asset_class: 0\n"
        "    instance_id: 1\n"
        "    offset_ms: 500\n"
        "    rate_ppm_x1000: 0\n"
        "    clock_duration_s: 2\n");

    FaultInjector fi;
    CHECK(fi.LoadScenario(path));
    CHECK(fi.GetScenario().name == "SCN-TEST-01");
    CHECK(fi.GetScenario().faults.size() == 1U);
    CHECK(fi.GetScenario().faults[0].type == FaultType::CLOCK_SKEW);
    CHECK(fi.GetScenario().faults[0].instance_id == 1U);
    CHECK(fi.GetScenario().faults[0].offset_ms == 500);
    CHECK(fi.GetScenario().faults[0].clock_duration_s == 2U);
}

static void test_load_scenario_packet_drop_parses()
{
    /* Given a valid packet-drop scenario, When loaded,
     * Then the fault type and link_id are set correctly. */
    const std::string path = write_tmp_scenario(
        "scenario: SCN-TEST-02\n"
        "faults:\n"
        "  - type: packet_drop\n"
        "    at_tai_offset_s: 2.0\n"
        "    link_id: 1\n"
        "    drop_probability_x10000: 10000\n"
        "    drop_duration_ms: 500\n");

    FaultInjector fi;
    CHECK(fi.LoadScenario(path));
    CHECK(fi.GetScenario().faults.size() == 1U);
    CHECK(fi.GetScenario().faults[0].type == FaultType::PACKET_DROP);
    CHECK(fi.GetScenario().faults[0].link_id == 1U);
    CHECK(fi.GetScenario().faults[0].drop_probability_x10000 == 10000U);
}

static void test_load_scenario_multiple_faults()
{
    /* Given a scenario with two faults, When loaded,
     * Then the fault vector has exactly two entries. */
    const std::string path = write_tmp_scenario(
        "scenario: SCN-TEST-03\n"
        "faults:\n"
        "  - type: clock_skew\n"
        "    at_tai_offset_s: 0.1\n"
        "    asset_class: 0\n"
        "    instance_id: 1\n"
        "    offset_ms: 100\n"
        "    rate_ppm_x1000: 0\n"
        "    clock_duration_s: 1\n"
        "  - type: safe_mode\n"
        "    at_tai_offset_s: 1.0\n"
        "    asset_class: 0\n"
        "    instance_id: 1\n"
        "    trigger_reason_code: 7\n");

    FaultInjector fi;
    CHECK(fi.LoadScenario(path));
    CHECK(fi.GetScenario().faults.size() == 2U);
    CHECK(fi.GetScenario().faults[1].type == FaultType::SAFE_MODE);
}

static void test_load_scenario_missing_file_returns_false()
{
    /* Given a non-existent path, When LoadScenario is called,
     * Then it returns false and GetLastError is non-empty. */
    FaultInjector fi;
    CHECK(!fi.LoadScenario("/tmp/does_not_exist_fault_injector.yaml"));
    CHECK(!fi.GetLastError().empty());
}

static void test_load_scenario_missing_scenario_field_returns_false()
{
    /* Given YAML with no 'scenario:' key, When loaded,
     * Then LoadScenario returns false. */
    const std::string path = write_tmp_scenario(
        "# no scenario field\n"
        "faults:\n"
        "  - type: clock_skew\n"
        "    at_tai_offset_s: 0.0\n");

    FaultInjector fi;
    CHECK(!fi.LoadScenario(path));
}

static void test_load_scenario_unknown_fault_type_returns_false()
{
    /* Given a YAML with an unknown fault type, When loaded,
     * Then LoadScenario returns false. */
    const std::string path = write_tmp_scenario(
        "scenario: SCN-BAD\n"
        "faults:\n"
        "  - type: telekinesis\n"
        "    at_tai_offset_s: 0.0\n");

    FaultInjector fi;
    CHECK(!fi.LoadScenario(path));
}

static void test_tick_emits_fault_at_correct_time()
{
    /* Given a scenario with one clock-skew fault at t=0.1 s,
     * When Tick is called at t=0.05 s then t=0.2 s,
     * Then the fault is emitted exactly once (at t=0.2). */
    const std::string path = write_tmp_scenario(
        "scenario: SCN-TICK\n"
        "faults:\n"
        "  - type: clock_skew\n"
        "    at_tai_offset_s: 0.1\n"
        "    asset_class: 0\n"
        "    instance_id: 1\n"
        "    offset_ms: 100\n"
        "    rate_ppm_x1000: 0\n"
        "    clock_duration_s: 1\n");

    FaultInjector fi;
    CHECK(fi.LoadScenario(path));

    /* Do NOT call fi.Start() — sock_fd_ stays -1 so EmitSpp skips the send.
     * The test only checks that the 'emitted' flag is set correctly. */
    fi.Tick(0.05);
    CHECK(!fi.GetScenario().faults[0].emitted);

    fi.Tick(0.2);
    CHECK(fi.GetScenario().faults[0].emitted);

    /* Second Tick beyond the event must not re-emit. */
    fi.Tick(1.0);
    /* Still true (not reset). */
    CHECK(fi.GetScenario().faults[0].emitted);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(void)
{
    test_crc16_empty();
    test_crc16_known_answer_0x00();
    test_crc16_known_answer_0xFF();
    test_crc16_abc_sequence();

    test_spp_header_apid_and_length();
    test_spp_header_seq_flags_standalone();
    test_spp_encode_packet_drop_length();
    test_spp_encode_packet_drop_crc_appended();
    test_spp_encode_clock_skew_length();
    test_spp_encode_safe_mode_length();
    test_spp_encode_sensor_noise_length();
    test_spp_encode_returns_zero_if_buf_too_small();
    test_spp_apid_in_encoded_clock_skew();

    test_load_scenario_clock_skew_parses();
    test_load_scenario_packet_drop_parses();
    test_load_scenario_multiple_faults();
    test_load_scenario_missing_file_returns_false();
    test_load_scenario_missing_scenario_field_returns_false();
    test_load_scenario_unknown_fault_type_returns_false();
    test_tick_emits_fault_at_correct_time();

    std::printf("fault_injector_test: %d passed, %d failed\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
