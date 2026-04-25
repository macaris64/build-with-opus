# Space ROS 2 Workspace — Package Index

This workspace implements the rover software stack for the Space ROS 2 layer of the mission.
All packages follow the lifecycle-node pattern and adhere to the conventions in `CLAUDE.md`.

## Package Overview

| Package | Version | APID Block | Description |
|---|---|---|---|
| `rover_common` | 1.0.0 | shared | Shared QoS profiles, message types (`Hk`, `Tc`, `LinkState`, `SimFault`), `TmBridge` CCSDS encoder, `LifecycleBase` |
| `rover_teleop` | 0.1.0 | 0x200–0x2FF | Teleoperation node (`TeleopNode`) |
| `rover_land` | 0.1.0 | 0x300–0x37F | Wheeled locomotion navigation node (`NavNode`) |
| `rover_uav` | 0.1.0 | 0x3C0–0x3FF | Flight control node with IMU integration (`FlightCtrlNode`) |
| `rover_cryobot` | 0.1.0 | 0x400–0x43F | Drill control node with HDLC-lite tether framing (`DrillCtrlNode`, `TetherClient`, `HdlcLite`) |
| `rover_bringup` | — | — | Launch files for bringing up the full rover stack |
| `rover_examples` | 0.1.0 | — | Demonstrative examples with 100% branch coverage (tutorial code) |

## Quick Start

```bash
# Build all packages
cd ros2_ws
colcon build --symlink-install

# Run all tests
colcon test
colcon test-result --verbose
```

## Architecture

```
rover_common  (foundation layer)
    ├── LifecycleBase      — CRTP base class for all rover lifecycle nodes
    ├── TmBridge           — CCSDS TM Space Packet encoder (Q-C6, Q-C8, big-endian)
    ├── QoS profiles       — SENSOR_QOS, CMD_QOS, HK_QOS, TELEOP_QOS
    └── Message types      — Hk, Tc, LinkState, SimFault (rosidl-generated)

rover_teleop  → TeleopNode        (joy subscriber, cmd_vel publisher, APID 0x200)
rover_land    → NavNode           (cmd_vel publisher, odom subscriber, 100 ms HK timer, APID 0x300)
rover_uav     → FlightCtrlNode    (imu subscriber, state_est publisher, 10 ms loop, APID 0x3C0)
rover_cryobot → DrillCtrlNode     (drill_cmd subscriber, 1 Hz HK timer, APID 0x400)
              → TetherClient      (TmBridge + HdlcLite pipeline)
              → HdlcLite          (HDLC-lite framing per ICD-cryobot-tether.md §3, Q-C9)
rover_examples → tutorial library (HdlcLite, TmBridge, TetherClient usage patterns)
```

## Coding Standards

- All nodes inherit `rover_common::LifecycleBase` (never plain `rclcpp::Node`)
- QoS from `rover_common::` named constants only — never inline `rclcpp::QoS(...)` at call sites
- Logging via `RCLCPP_INFO(get_logger(), ...)` — `std::cout` and `printf` are banned
- All callbacks non-blocking; offload heavy work to a callback group
- CCSDS multi-byte fields big-endian (Q-C8); no ad-hoc byte swaps
- HDLC-lite tether framing via `HdlcLite::encode/decode` only (Q-C9)
- Compiler flags: `-Wall -Wextra -Werror -std=c++17` on all targets

## APID Registry (Rover Layer)

| Block | Owner | Purpose |
|---|---|---|
| 0x200–0x2FF | `rover_teleop` | Teleop TM |
| 0x300–0x37F | `rover_land` | Land rover TM |
| 0x3C0–0x3FF | `rover_uav` | UAV TM |
| 0x400–0x43F | `rover_cryobot` | Cryobot drill TM |

Full registry: `docs/interfaces/apid-registry.md`
