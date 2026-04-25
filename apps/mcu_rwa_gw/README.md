# mcu_rwa_gw — RWA MCU CAN 2.0A Gateway

cFS-side gateway that bridges the CAN 2.0A bus to the Software Bus for the Reaction-Wheel Assembly (RWA) MCU. Receives ISO 15765-2-style fragmented CAN frames from the MCU, validates the fragment sequence, and publishes complete valid frames to `MCU_RWA_HK_MID`. Out-of-sequence fragments are dropped and logged — never forwarded.

## Purpose

- Poll the CAN 2.0A bus at 100 Hz for inbound RWA telemetry frames
- Validate CAN ISO 15765-2 fragmentation (Single-Frame / First-Frame / Consecutive-Frame sequence)
- Detect Consecutive-Frame received before First-Frame (`ERR_FRAGMENT_LOST`) and drop the frame
- Publish validated frames to `MCU_RWA_HK_MID` for `orbiter_adcs`
- Detect bus silence (≥ 3 consecutive cycles) and emit `BUS_SILENT_ERR_EID`
- Relay TC commands from the SB to the MCU bus (stub in Phase 35; real driver in Phase 42)

## Message IDs

| Direction | MID (hex) | APID | Purpose |
|---|---|---|---|
| TC in | `0x1A90` | — | TC commands forwarded to RWA MCU |
| TM out | `0x0A90` | `0x290` | Validated RWA telemetry frames |

## CAN Frame Format (ISO 15765-2)

```
frame[0]  — frame type byte
frame[1..8] — payload (up to 8 bytes)
```

| Frame Type Byte | Meaning | State transition |
|---|---|---|
| `0x00` | Single-Frame (SF) | Complete; clear `in_progress` |
| `0x10`–`0x1F` | First-Frame (FF) | Start of multi-frame; set `in_progress = 1` |
| `0x20`–`0x2F` | Consecutive-Frame (CF) | Valid only if `in_progress == 1` |
| Other | Invalid format | `ERR_FRAME_FORMAT`; frame dropped |

### Fragment Sequence Validation

A Consecutive-Frame received when `CanInProgress == 0` (no preceding First-Frame) indicates a lost First-Frame. The CF is dropped with `MCU_RWA_GW_CAN_FRAGMENT_LOST_ERR_EID` and `ErrCounter++`. No partial data is forwarded to the SB.

## Key Constants

| Constant | Value | Description |
|---|---|---|
| `MCU_RWA_GW_PIPE_DEPTH` | 64 | SB pipe depth |
| `MCU_RWA_GW_MAX_FRAME_LEN` | 9 | Max CAN frame (1B type + 8B payload) |
| `MCU_RWA_GW_SILENCE_CYCLES` | 3 | No-data cycles before BUS_SILENT event |
| `MCU_RWA_GW_CAN_SF` | `0x00` | Single-Frame type byte |
| `MCU_RWA_GW_CAN_FF_MIN` | `0x10` | First-Frame range minimum |
| `MCU_RWA_GW_CAN_FF_MAX` | `0x1F` | First-Frame range maximum |
| `MCU_RWA_GW_CAN_CF_MIN` | `0x20` | Consecutive-Frame range minimum |
| `MCU_RWA_GW_CAN_CF_MAX` | `0x2F` | Consecutive-Frame range maximum |

## Radiation-Sensitive State [Q-F3]

```c
static uint32 MCU_RWA_GW_SilenceCount
    __attribute__((section(".critical_mem"))) = 0U;

static uint8  MCU_RWA_GW_CanInProgress
    __attribute__((section(".critical_mem"))) = 0U;
```

`CanInProgress` must survive power-cycling within a session to correctly resume an interrupted multi-frame transfer after a watchdog reset.

## Events

| EID | Name | Type | Trigger |
|---|---|---|---|
| 1 | `MCU_RWA_GW_STARTUP_INF_EID` | INFO | Init complete |
| 2 | `MCU_RWA_GW_CMD_ERR_EID` | ERROR | Unknown MID or SB error |
| 3 | `MCU_RWA_GW_CAN_FRAGMENT_LOST_ERR_EID` | ERROR | CF before FF — frame dropped |
| 4 | `MCU_RWA_GW_BUS_SILENT_ERR_EID` | ERROR | Bus silent for ≥ 3 cycles |

## HK Telemetry Fields

| Field | Type | Description |
|---|---|---|
| `FramesValid` | `uint32` | Cumulative frames that passed validation |
| `FramesCorrupt` | `uint32` | Cumulative frames with sequence errors |
| `ErrCounter` | `uint32` | Rejected commands / SB errors |
| `BusSilent` | `uint8` | 1 = bus silent alarm active |

## Source Layout

```
mcu_rwa_gw/
├── CMakeLists.txt
└── fsw/
    ├── src/
    │   ├── cfe.h
    │   ├── mcu_rwa_gw.h
    │   ├── mcu_rwa_gw.c
    │   ├── mcu_rwa_gw_events.h
    │   └── mcu_rwa_gw_version.h    v1.0.0
    └── unit-test/
        └── mcu_rwa_gw_test.c
```

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure -R mcu_rwa_gw_unit_tests
```

## Compliance

- `[Q-F3]` `MCU_RWA_GW_SilenceCount` and `MCU_RWA_GW_CanInProgress` in `.critical_mem`
- `[Q-H4]` Bus family pinned to CAN 2.0A (ISO 11898 / ISO 15765-2)
- No dynamic allocation; MISRA C:2012 required rules

## Source of Truth

- [`docs/interfaces/ICD-mcu-cfs.md`](../../docs/interfaces/ICD-mcu-cfs.md) §3 — CAN 2.0A framing
- [`docs/architecture/03-subsystem-mcus.md`](../../docs/architecture/03-subsystem-mcus.md) — RWA MCU bus assignment
- [`docs/interfaces/apid-registry.md`](../../docs/interfaces/apid-registry.md) — APID 0x290
