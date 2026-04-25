// fault_injector_run — standalone SITL scenario runner for docker compose.
//
// Usage: fault_injector_run <host> <port> <scenario.yaml> [duration_s]
//
// Loads <scenario.yaml>, opens a UDP socket to <host>:<port>, then calls
// Tick() at TICK_INTERVAL_S intervals for <duration_s> seconds (default: 30).
// Exits 0 on success, non-zero on any error.
//
// Q-C8: all SPP encoding is delegated to spp_encoder.cpp (the sole BE locus).
// SYS-REQ-0040 (fault injection enabled in SITL); SYS-REQ-0041 guards that
// CFS_FLIGHT_BUILD is unset in all compose profiles — verified by ci.yml.
// Q-F1: injects sideband SPPs over UDP directly to the ground_station container.
// Q-F2: APID 0x541 (clock-skew) is the designated time_suspect trigger per Phase 40.

#include "fault_injector.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>

static constexpr double TICK_INTERVAL_S = 0.1;
static constexpr double DEFAULT_DURATION_S = 30.0;

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::fprintf(stderr,
            "Usage: %s <host> <port> <scenario.yaml> [duration_s]\n",
            argv[0]);
        return 1;
    }

    const char  *host   = argv[1];
    const int    port   = std::atoi(argv[2]);
    const char  *path   = argv[3];
    const double dur_s  = (argc >= 5) ? std::atof(argv[4]) : DEFAULT_DURATION_S;

    if (port <= 0 || port > 65535)
    {
        std::fprintf(stderr, "ERROR: invalid port %d\n", port);
        return 1;
    }

    FaultInjector fi;

    if (!fi.LoadScenario(path))
    {
        std::fprintf(stderr, "ERROR: LoadScenario(%s): %s\n",
                     path, fi.GetLastError().c_str());
        return 1;
    }

    if (!fi.Start(host, static_cast<uint16_t>(port)))
    {
        std::fprintf(stderr, "ERROR: Start(%s:%d): %s\n",
                     host, port, fi.GetLastError().c_str());
        return 1;
    }

    std::printf("fault_injector_run: scenario=%s target=%s:%d duration=%.1fs\n",
                fi.GetScenario().name.c_str(), host, port, dur_s);
    std::fflush(stdout);

    double t = 0.0;
    while (t < dur_s)
    {
        fi.Tick(t);
        usleep(static_cast<useconds_t>(TICK_INTERVAL_S * 1.0e6));
        t += TICK_INTERVAL_S;
    }

    fi.Stop();
    std::printf("fault_injector_run: scenario complete after %.1fs\n", t);
    return 0;
}
