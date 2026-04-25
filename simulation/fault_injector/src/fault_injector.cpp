#include "fault_injector.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

/* ── Minimal YAML tokenizer ──────────────────────────────────────────────────
 * Handles the specific scenario format:
 *   scenario: <name>
 *   faults:
 *     - type: <fault_type>
 *       key: value
 *       ...
 * Supports string, integer, and floating-point leaf values. */

static std::string trim(const std::string &s)
{
    const size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) { return ""; }
    const size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1U);
}

static bool parse_kv(const std::string &line,
                     std::string &key, std::string &val)
{
    const size_t colon = line.find(':');
    if (colon == std::string::npos) { return false; }
    key = trim(line.substr(0U, colon));
    val = trim(line.substr(colon + 1U));
    return !key.empty();
}

/* Strip the leading "- " list marker.
 * Caller guarantees s starts with "- " (rfind-0U guard in LoadScenario). */
static std::string strip_list_marker(const std::string &s)
{
    return trim(s.substr(2U));
}

static FaultType parse_fault_type(const std::string &s,
                                  std::string &err)
{
    if (s == "packet_drop")  { return FaultType::PACKET_DROP; }
    if (s == "clock_skew")   { return FaultType::CLOCK_SKEW; }
    if (s == "safe_mode")    { return FaultType::SAFE_MODE; }
    if (s == "sensor_noise") { return FaultType::SENSOR_NOISE; }
    err = "unknown fault type: " + s;
    return FaultType::PACKET_DROP;  /* fallback; caller checks err */
}

/* ── FaultInjector implementation ─────────────────────────────────────────── */

FaultInjector::FaultInjector() = default;

FaultInjector::~FaultInjector()
{
    Stop();
}

bool FaultInjector::LoadScenario(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        last_error_ = "cannot open scenario file: " + path;
        return false;
    }

    scenario_ = Scenario{};
    last_error_.clear();

    FaultEvent   current{};
    bool         in_fault  = false;
    bool         type_seen = false;
    std::string  line;
    std::string  key;
    std::string  val;
    int          lineno = 0;

    while (std::getline(f, line))
    {
        ++lineno;
        const std::string tline = trim(line);
        if (tline.empty() || tline[0] == '#') { continue; }

        /* Detect start of a new fault list item. */
        const bool is_list_item = (tline.rfind("- ", 0U) == 0U);
        const std::string content = is_list_item ? strip_list_marker(tline) : tline;

        if (!parse_kv(content, key, val)) { continue; }

        if (key == "scenario")
        {
            scenario_.name = val;
            continue;
        }
        if (key == "faults") { continue; }  /* list header */

        /* New fault list item always starts with "type". */
        if (is_list_item && key == "type")
        {
            if (in_fault && type_seen)
            {
                scenario_.faults.push_back(current);
            }
            current   = FaultEvent{};
            in_fault  = true;
            type_seen = true;
            current.type = parse_fault_type(val, last_error_);
            if (!last_error_.empty()) { return false; }
            continue;
        }

        if (!in_fault) { continue; }

        /* Parse per-fault fields. */
        if (key == "at_tai_offset_s")
        {
            current.at_tai_offset_s = std::stod(val);
        }
        else if (key == "link_id")
        {
            current.link_id = (uint8_t)std::stoul(val);
        }
        else if (key == "drop_probability_x10000")
        {
            current.drop_probability_x10000 = (uint16_t)std::stoul(val);
        }
        else if (key == "drop_duration_ms")
        {
            current.drop_duration_ms = (uint32_t)std::stoul(val);
        }
        else if (key == "asset_class")
        {
            current.asset_class = (uint8_t)std::stoul(val);
        }
        else if (key == "instance_id")
        {
            current.instance_id = (uint8_t)std::stoul(val);
        }
        else if (key == "offset_ms")
        {
            current.offset_ms = (int32_t)std::stoi(val);
        }
        else if (key == "rate_ppm_x1000")
        {
            current.rate_ppm_x1000 = (int32_t)std::stoi(val);
        }
        else if (key == "clock_duration_s")
        {
            current.clock_duration_s = (uint32_t)std::stoul(val);
        }
        else if (key == "trigger_reason_code")
        {
            current.trigger_reason_code = (uint16_t)std::stoul(val);
        }
        else if (key == "sensor_index")
        {
            current.sensor_index = (uint16_t)std::stoul(val);
        }
        else if (key == "noise_model")
        {
            current.noise_model = (uint8_t)std::stoul(val);
        }
        else if (key == "noise_param_1")
        {
            current.noise_param_1 = (int32_t)std::stoi(val);
        }
        else if (key == "noise_param_2")
        {
            current.noise_param_2 = (int32_t)std::stoi(val);
        }
        else if (key == "noise_duration_ms")
        {
            current.noise_duration_ms = (uint32_t)std::stoul(val);
        }
        /* Unknown keys are silently ignored for forward-compatibility. */
    }

    if (in_fault && type_seen)
    {
        scenario_.faults.push_back(current);
    }

    if (scenario_.name.empty())
    {
        last_error_ = "scenario: field missing in " + path;
        return false;
    }

    return true;
}

bool FaultInjector::Start(const std::string &host, uint16_t port)
{
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0)
    {
        last_error_ = std::string("socket() failed: ") + std::strerror(errno);
        return false;
    }

    struct sockaddr_in dst{};
    dst.sin_family      = AF_INET;
    dst.sin_port        = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &dst.sin_addr) != 1)
    {
        last_error_ = "invalid host address: " + host;
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    /* Connect the UDP socket so send() can be used without a destination.
     * connect() on a UDP socket to a valid loopback address never fails in
     * practice; the failure branch requires network-injection. */
    /* GCOV_EXCL_START */
    if (connect(sock_fd_, reinterpret_cast<struct sockaddr *>(&dst),
                sizeof(dst)) < 0)
    {
        last_error_ = std::string("connect() failed: ") + std::strerror(errno);
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }
    /* GCOV_EXCL_STOP */

    return true;
}

void FaultInjector::Tick(double sim_time_s)
{
    for (auto &ev : scenario_.faults)
    {
        if (!ev.emitted && sim_time_s >= ev.at_tai_offset_s)
        {
            EmitSpp(static_cast<uint16_t>(
                        static_cast<uint16_t>(APID_PACKET_DROP) +
                        static_cast<uint16_t>(ev.type)),
                    ev);
            ev.emitted = true;
        }
    }
}

void FaultInjector::Stop()
{
    if (sock_fd_ >= 0)
    {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

size_t FaultInjector::EmitSpp(uint16_t apid, const FaultEvent &ev)
{
    /* Caller (Tick) always computes APID_PACKET_DROP + FaultType (0-3), so
     * apid is always in [FAULT_APID_MIN, FAULT_APID_MAX]. Verified by Tick(). */
    assert(apid >= FAULT_APID_MIN && apid <= FAULT_APID_MAX);

    uint8_t buf[SPP_MAX_BYTES]{};
    const size_t idx = apid - FAULT_APID_MIN;
    const uint16_t seq = seq_counts_[idx];
    seq_counts_[idx] = (uint16_t)((seq + 1U) & 0x3FFFU);  /* 14-bit rollover */

    size_t len = 0U;
    switch (ev.type)
    {
    case FaultType::PACKET_DROP:
        len = spp_encode_packet_drop(buf, sizeof(buf), seq,
                                     ev.link_id,
                                     ev.drop_probability_x10000,
                                     ev.drop_duration_ms);
        break;
    case FaultType::CLOCK_SKEW:
        len = spp_encode_clock_skew(buf, sizeof(buf), seq,
                                    ev.asset_class, ev.instance_id,
                                    ev.offset_ms, ev.rate_ppm_x1000,
                                    ev.clock_duration_s);
        break;
    case FaultType::SAFE_MODE:
        len = spp_encode_safe_mode(buf, sizeof(buf), seq,
                                   ev.asset_class, ev.instance_id,
                                   ev.trigger_reason_code);
        break;
    case FaultType::SENSOR_NOISE:
        len = spp_encode_sensor_noise(buf, sizeof(buf), seq,
                                      ev.asset_class, ev.instance_id,
                                      ev.sensor_index, ev.noise_model,
                                      ev.noise_param_1, ev.noise_param_2,
                                      ev.noise_duration_ms);
        break;
    }

    if (len == 0U) { return 0U; }

    SendAsAosFrame(buf, len);
    return len;
}

/* AOS Transfer Frame constants (CCSDS 732.0-B-4, Q-C4). */
static constexpr size_t AOS_FRAME_LEN    = 1024U;
static constexpr size_t AOS_HEADER_LEN   = 6U;
static constexpr size_t AOS_FECF_LEN     = 2U;
static constexpr size_t AOS_DATA_LEN     = AOS_FRAME_LEN - AOS_HEADER_LEN - AOS_FECF_LEN; /* 1016 */
static constexpr size_t AOS_FECF_OFFSET  = AOS_FRAME_LEN - AOS_FECF_LEN; /* 1022 */
/* SAKURA-II spacecraft ID (matches AosFramer test constant). */
static constexpr uint8_t AOS_SCID = 42U;
/* VC 0 carries fault-inject SPPs — rejected by ApidRouter (Q-F2). */
static constexpr uint8_t AOS_VCID = 0U;

void FaultInjector::SendAsAosFrame(const uint8_t *spp, size_t spp_len)
{
    if (sock_fd_ < 0 || spp_len == 0U) { return; }

    uint8_t frame[AOS_FRAME_LEN] = {};

    /* Primary header (CCSDS 732.0-B-4 §4.1). */
    frame[0] = (uint8_t)(0x40U | (AOS_SCID >> 2U));                          /* TF v=01, SCID[7:2] */
    frame[1] = (uint8_t)(((AOS_SCID & 0x03U) << 6U) | (AOS_VCID & 0x3FU)); /* SCID[1:0], VCID   */
    frame[2] = (uint8_t)((vcfc_ >> 16U) & 0xFFU);                            /* VCFC[23:16]        */
    frame[3] = (uint8_t)((vcfc_ >>  8U) & 0xFFU);                            /* VCFC[15:8]         */
    frame[4] = (uint8_t)( vcfc_         & 0xFFU);                            /* VCFC[7:0]          */
    frame[5] = 0x00U;                                                          /* no OCF, no replay  */
    vcfc_ = (vcfc_ + 1U) & 0x00FFFFFFU;                                       /* 24-bit rollover    */

    /* Data field: SPP at the start, remainder zero-padded. */
    const size_t copy_len = (spp_len < AOS_DATA_LEN) ? spp_len : AOS_DATA_LEN;
    std::memcpy(&frame[AOS_HEADER_LEN], spp, copy_len);

    /* FECF: CRC-16/IBM-3740 = CRC-16/CCITT-FALSE over bytes [0..1022).
     * spp_crc16 uses the same algorithm (poly 0x1021, init 0xFFFF). */
    const uint16_t fecf = spp_crc16(frame, AOS_FECF_OFFSET);
    frame[AOS_FECF_OFFSET]     = (uint8_t)((fecf >> 8U) & 0xFFU); /* big-endian (Q-C8) */
    frame[AOS_FECF_OFFSET + 1] = (uint8_t)( fecf        & 0xFFU);

    (void)send(sock_fd_, frame, AOS_FRAME_LEN, 0);
}
