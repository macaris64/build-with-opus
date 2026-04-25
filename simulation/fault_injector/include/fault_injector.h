#pragma once

#include "spp_encoder.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

/* Maximum APID value reserved for fault injection (exclusive upper bound). */
static constexpr uint16_t FAULT_APID_MIN = APID_PACKET_DROP;
static constexpr uint16_t FAULT_APID_MAX = APID_SENSOR_NOISE;

/* Fault event types mirroring the four ICD-defined packets. */
enum class FaultType : uint8_t
{
    PACKET_DROP   = 0U,
    CLOCK_SKEW    = 1U,
    SAFE_MODE     = 2U,
    SENSOR_NOISE  = 3U,
};

/* Per-fault configuration loaded from a scenario YAML. */
struct FaultEvent
{
    FaultType type;
    double    at_tai_offset_s;  /* sim-time offset from scenario start [s] */

    /* PACKET_DROP fields */
    uint8_t  link_id;
    uint16_t drop_probability_x10000;
    uint32_t drop_duration_ms;

    /* CLOCK_SKEW / SAFE_MODE / SENSOR_NOISE shared fields */
    uint8_t  asset_class;
    uint8_t  instance_id;

    /* CLOCK_SKEW fields */
    int32_t  offset_ms;
    int32_t  rate_ppm_x1000;
    uint32_t clock_duration_s;

    /* SAFE_MODE fields */
    uint16_t trigger_reason_code;

    /* SENSOR_NOISE fields */
    uint16_t sensor_index;
    uint8_t  noise_model;
    int32_t  noise_param_1;
    int32_t  noise_param_2;
    uint32_t noise_duration_ms;

    bool     emitted;           /* set true once the SPP has been sent */
};

/* Loaded scenario. */
struct Scenario
{
    std::string             name;
    std::vector<FaultEvent> faults;
};

/**
 * FaultInjector — Scenario-driven CCSDS SPP emitter.
 *
 * Usage:
 *   FaultInjector fi;
 *   if (!fi.LoadScenario("scenarios/clock_skew.yaml")) { ... }
 *   fi.Start("127.0.0.1", 12345);  // bind/connect UDP socket
 *   // In simulation loop:
 *   fi.Tick(sim_time_seconds);
 *   fi.Stop();
 */
class FaultInjector
{
public:
    FaultInjector();
    ~FaultInjector();

    /* Disable copy; FaultInjector owns a UDP socket fd. */
    FaultInjector(const FaultInjector &)            = delete;
    FaultInjector &operator=(const FaultInjector &) = delete;

    /**
     * LoadScenario — Parse a YAML scenario file.
     * Returns true on success; false on any parse error (check GetLastError()).
     */
    bool        LoadScenario(const std::string &path);

    /**
     * Start — Open a UDP socket and store the destination endpoint.
     * Must be called once before Tick(). Returns true on success.
     */
    bool        Start(const std::string &host, uint16_t port);

    /**
     * Tick — Emit any pending fault SPPs whose at_tai_offset_s <= sim_time_s.
     * Call once per simulation step from the WorldPlugin or standalone loop.
     */
    void        Tick(double sim_time_s);

    /**
     * Stop — Close the UDP socket; resets the object for reuse.
     */
    void        Stop();

    const std::string &GetLastError() const { return last_error_; }
    const Scenario    &GetScenario()  const { return scenario_; }

private:
    size_t EmitSpp(uint16_t apid, const FaultEvent &ev);
    /* Wrap spp[0..spp_len) in a 1024-byte AOS frame (CCSDS 732.0-B-4,
     * SCID=42, VCID=0, CRC-16/IBM-3740 FECF) and send it via UDP.
     * Required so AosFramer in ground_station can decode the packet (Q-F2). */
    void SendAsAosFrame(const uint8_t *spp, size_t spp_len);

    Scenario    scenario_;
    std::string last_error_;
    int         sock_fd_{-1};

    /* Per-APID rolling sequence counts (14-bit, mod 16384). */
    uint16_t seq_counts_[4]{};
    /* VC Frame Count for AOS primary header (24-bit, mod 2^24). */
    uint32_t vcfc_{0};
};
