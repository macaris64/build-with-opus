# orbiter_adcs — Attitude Determination and Control System

Attitude control app for the SAKURA-II orbiter. ADCS accepts target quaternion commands, validates unit-norm, drives the reaction-wheel assembly (via stubs relayed to `mcu_rwa_gw`), and publishes both housekeeping and wheel telemetry.

## Purpose

- Accept and validate target attitude quaternions (unit-norm tolerance ±0.001)
- Command reaction-wheel torques (Phase 35+ stubs; real driver in `mcu_rwa_gw`)
- Publish HK and wheel-speed telemetry on separate MIDs
- Report attitude errors via EVS when norm validation fails

## Message IDs

| Direction | MID (hex) | APID | Purpose |
|---|---|---|---|
| TC in | `0x1982` | `0x182` | Ground commands |
| TM out | `0x0910` | `0x110` | HK telemetry |
| TM out | `0x0911` | `0x111` | Wheel telemetry (stub) |

## Command Codes (TC MID `0x1982`)

| CC | Name | Payload | Effect |
|---|---|---|---|
| `0x00` | `ORBITER_ADCS_NOOP_CC` | none | Increment `CmdCounter`; log version |
| `0x01` | `ORBITER_ADCS_RESET_CC` | none | Zero `CmdCounter` / `ErrCounter` |
| `0x02` | `ORBITER_ADCS_SET_TARGET_QUAT_CC` | `ORBITER_ADCS_Quat_t` | Set target attitude quaternion; reject if |norm − 1| > 0.001 |

### Target Quaternion Payload

```c
typedef struct {
    CFE_MSG_Message_t Header;
    float             Qx;
    float             Qy;
    float             Qz;
    float             Qw;
} ORBITER_ADCS_QuatCmd_t;
```

Quaternion unit-norm check: `|Qx² + Qy² + Qz² + Qw² − 1.0f| <= 0.001f`.  
Failure increments `ErrCounter` and emits `ORBITER_ADCS_QUAT_INVALID_ERR_EID`.

```c
typedef struct {
    float Qx;
    float Qy;
    float Qz;
    float Qw;
} ORBITER_ADCS_Quat_t;
```

## Telemetry — `ORBITER_ADCS_HkTlm_t` (MID `0x0910`)

| Field | Type | Description |
|---|---|---|
| `CmdCounter` | `uint32` | Accepted commands since last RESET |
| `ErrCounter` | `uint32` | Rejected / errored commands |
| `TargetQuat` | `ORBITER_ADCS_Quat_t` | Most-recently accepted target quaternion |

## Telemetry — Wheel TM (MID `0x0911`)

Four-wheel reaction-wheel speed stub (populated by `mcu_rwa_gw` in Phase 35+). Fields reserved; real wheel speeds arrive once `MCU_RWA_HK_MID` subscriber path is live.

## Events

| EID | Name | Type | Trigger |
|---|---|---|---|
| 1 | `ORBITER_ADCS_STARTUP_INF_EID` | INFO | Init complete |
| 2 | `ORBITER_ADCS_CMD_ERR_EID` | ERROR | Unknown MID, bad CC, SB error |
| 3 | `ORBITER_ADCS_CMD_NOOP_INF_EID` | INFO | NOOP accepted |
| 4 | `ORBITER_ADCS_QUAT_SET_INF_EID` | INFO | Target quaternion accepted |
| 5 | `ORBITER_ADCS_QUAT_INVALID_ERR_EID` | ERROR | Quaternion norm out of tolerance |

## Source Layout

```
orbiter_adcs/
├── CMakeLists.txt
└── fsw/
    ├── src/
    │   ├── cfe.h
    │   ├── orbiter_adcs.h
    │   ├── orbiter_adcs.c
    │   ├── orbiter_adcs_events.h
    │   └── orbiter_adcs_version.h    v1.0.0
    └── unit-test/
        └── orbiter_adcs_test.c
```

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure -R orbiter_adcs_unit_tests
```

## Compliance

- `[Q-F3]` No radiation-sensitive state in v0.1; anchors added in Phase 43
- `[Q-H4]` RWA bus family pinned to CAN 2.0A via `mcu_rwa_gw`
- No dynamic allocation; MISRA C:2012 required rules

## Source of Truth

- [`docs/architecture/01-orbiter-cfs.md`](../../docs/architecture/01-orbiter-cfs.md) §3.2 — ADCS role
- [`docs/interfaces/apid-registry.md`](../../docs/interfaces/apid-registry.md) — APID 0x110 / 0x111 / 0x182
- [`docs/architecture/03-subsystem-mcus.md`](../../docs/architecture/03-subsystem-mcus.md) — RWA MCU interface
