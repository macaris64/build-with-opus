# rover_common

Shared foundation for all SAKURA-II surface rover packages (`rover_land`, `rover_uav`, `rover_cryobot`). Provides QoS profiles, message types, a CCSDS telemetry bridge helper, and a lifecycle node base class.

## Contents

```
rover_common/
├── include/rover_common/
│   ├── qos.hpp             — four named QoS profile constants
│   ├── tm_bridge.hpp       — TmBridge: packs HK data into CCSDS TM packets
│   └── lifecycle_base.hpp  — LifecycleBase: default-SUCCESS lifecycle callbacks
├── msg/
│   ├── Hk.msg              — generic housekeeping telemetry frame
│   ├── Tc.msg              — ground telecommand descriptor
│   ├── LinkState.msg       — Proximity-1 session state
│   └── SimFault.msg        — fault-injection descriptor (Q-F1 / Q-F2)
├── src/
│   └── tm_bridge.cpp
└── test/
    └── test_tm_bridge.cpp  — 10 gtest cases + lifecycle round-trip (11 total)
```

## QoS Profiles (`qos.hpp`)

All profiles are named constants in `namespace rover_common`. Never use inline `rclcpp::QoS(...)` at publisher/subscriber call sites — reference these instead.

| Constant | Reliability | History | Depth | Typical use |
|---|---|---|---|---|
| `SENSOR_QOS` | Best effort | Keep last | 5 | `/scan`, `/imu`, `/odom` |
| `CMD_QOS` | Reliable | Keep last | 10 | `/cmd_vel`, `/drill_cmd` |
| `HK_QOS` | Reliable | Keep last | 1 | housekeeping at 1 Hz |
| `TELEOP_QOS` | Reliable | Keep last | 10 | `/teleop/twist`, `/teleop/waypoint` |

## TmBridge (`tm_bridge.hpp`)

Packs ROS 2 housekeeping data into CCSDS Space Packets for Proximity-1 forwarding.

```cpp
#include "rover_common/tm_bridge.hpp"

rover_common::TmBridge bridge(0x300U);        // rover_land APID base
auto pkt = bridge.pack_hk(payload, tai_coarse, tai_fine);
// pkt: 6-byte primary header + 10-byte secondary header + payload (big-endian)
```

**Wire format (per Q-C6, Q-C8):**

| Bytes | Field | Notes |
|---|---|---|
| 0–5 | CCSDS primary header | version=0, type=0 (TM), sec_hdr=1, APID 11-bit, seq_flags=0b11, 14-bit counter |
| 6–9 | TAI coarse (4 B BE) | CUC coarse time |
| 10–12 | TAI fine (3 B BE) | CUC fine time |
| 13–14 | func_code (2 B BE) | 0x0000 for HK |
| 15 | instance_id (1 B) | 0x00 for HK |
| 16+ | user data | verbatim payload bytes |

- Sequence counter is 14-bit and wraps from `0x3FFF` → `0x0000` per CCSDS 133.0.
- `pack_hk` returns an empty vector if `payload.size() > 65526` (caller must chunk).
- `reset_seq(uint16_t to = 0)` resets the counter — intended for testing.

## LifecycleBase (`lifecycle_base.hpp`)

Header-only base class. All five lifecycle callbacks return `SUCCESS` by default. Phase 37 nodes inherit and override only what they need:

```cpp
#include "rover_common/lifecycle_base.hpp"

class NavNode : public rover_common::LifecycleBase {
public:
    NavNode() : rover_common::LifecycleBase("nav_node") {}

    CallbackReturn on_configure(const rclcpp_lifecycle::State &) override {
        // create publishers, subscribers, timers
        return CallbackReturn::SUCCESS;
    }
    // on_activate, on_deactivate, on_cleanup, on_shutdown — inherited defaults
};
```

## Message Types

| Message | Fields | Use |
|---|---|---|
| `Hk` | `uint32 seq`, `float64 timestamp`, `uint8[32] payload` | Periodic HK telemetry from any rover node |
| `Tc` | `uint16 apid`, `uint8 func_code`, `uint8[64] payload` | Ground telecommand dispatched by tm_bridge |
| `LinkState` | `bool session_active`, `uint8 signal_strength`, `float64 last_contact` | Prx-1 link health published on session events |
| `SimFault` | `uint16 fault_id`, `uint8[4] params` | Fault injection per Q-F1 / Q-F2 |

## APID Allocation

| Rover class | APID block |
|---|---|
| `rover_land` | `0x300`–`0x33F` |
| `rover_uav` | `0x340`–`0x37F` |
| `rover_cryobot` | `0x380`–`0x3BF` |

## Build & Test

```bash
# From ros2_ws/
colcon build --packages-select rover_common --symlink-install
colcon test  --packages-select rover_common
colcon test-result --verbose
# Expected: 11 tests, 0 failures
```

## Compliance

- Q-C8: all CCSDS encoding in `TmBridge::pack_hk` is big-endian; no `to_le_bytes` or ad-hoc byte swaps anywhere in this package.
- Q-F3: no radiation-sensitive state in the ROS 2 layer; radiation anchors belong in the C FSW under `apps/`.
- `.claude/rules/ros2-nodes.md`: QoS profiles are named constants; all nodes use `rclcpp_lifecycle::LifecycleNode`.
