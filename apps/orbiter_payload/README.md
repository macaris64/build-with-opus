# orbiter_payload — Science Payload Manager

Science payload manager for the SAKURA-II orbiter. PAYLOAD controls instrument power, arbitrates science-mode transitions, and enforces a power-guard that prevents mode changes while the payload is unpowered.

## Purpose

- Control payload instrument power (on/off)
- Arbitrate science-mode transitions; reject changes when payload is unpowered
- Aggregate `MCU_PAYLOAD_HK_MID` data relayed from `mcu_payload_gw`
- Publish combined payload housekeeping

## Message IDs

| Direction | MID (hex) | APID | Purpose |
|---|---|---|---|
| TC in | `0x1985` | `0x185` | Ground commands |
| TM out | `0x0940` | `0x140` | HK telemetry |
| HK in | `MCU_PAYLOAD_HK_MID` (`0x0A80`) | — | MCU payload data from `mcu_payload_gw` |

## Command Codes (TC MID `0x1985`)

| CC | Name | Payload | Effect |
|---|---|---|---|
| `0x00` | `ORBITER_PAYLOAD_NOOP_CC` | none | Increment `CmdCounter`; log version |
| `0x01` | `ORBITER_PAYLOAD_RESET_CC` | none | Zero `CmdCounter` / `ErrCounter` |
| `0x02` | `ORBITER_PAYLOAD_SET_POWER_CC` | `uint8 State` | Power payload on (1) or off (0) |
| `0x03` | `ORBITER_PAYLOAD_SET_SCIENCE_MODE_CC` | `uint8 Mode` | Set science mode (0–3); rejected if payload unpowered |

## Science Modes

| Value | Name | Notes |
|---|---|---|
| `0` | IDLE | Default; instruments passive |
| `1` | IMAGING | Optical imager active |
| `2` | SPECTROSCOPY | Spectrometer active |
| `3` | CALIBRATION | Internal calibration sequence |

A science-mode change attempted while `PowerState == 0` (unpowered) increments `ErrCounter` and emits `ORBITER_PAYLOAD_POWER_GUARD_ERR_EID`.

## Power-Guard Logic

```
SET_SCIENCE_MODE_CC received
    └── if (PowerState == 0)  → reject → POWER_GUARD_ERR_EID
    └── if (Mode > 3)         → reject → CMD_ERR_EID
    └── else                  → apply  → SCIENCE_MODE_SET_INF_EID
```

## Radiation-Sensitive State [Q-F3]

```c
static uint8 ORBITER_PAYLOAD_PowerState
    __attribute__((section(".critical_mem"))) = 0U;   /* 0=off */

static uint8 ORBITER_PAYLOAD_ScienceMode
    __attribute__((section(".critical_mem"))) = 0U;   /* 0=IDLE */
```

## Telemetry — `ORBITER_PAYLOAD_HkTlm_t` (MID `0x0940`)

| Field | Type | Description |
|---|---|---|
| `CmdCounter` | `uint32` | Accepted commands since last RESET |
| `ErrCounter` | `uint32` | Rejected / errored commands |
| `PowerState` | `uint8` | 0=off, 1=on |
| `ScienceMode` | `uint8` | Active science mode (0–3) |

## Events

| EID | Name | Type | Trigger |
|---|---|---|---|
| 1 | `ORBITER_PAYLOAD_STARTUP_INF_EID` | INFO | Init complete |
| 2 | `ORBITER_PAYLOAD_CMD_ERR_EID` | ERROR | Unknown MID, bad CC, invalid params |
| 3 | `ORBITER_PAYLOAD_CMD_NOOP_INF_EID` | INFO | NOOP accepted |
| 4 | `ORBITER_PAYLOAD_POWER_SET_INF_EID` | INFO | Payload power toggled |
| 5 | `ORBITER_PAYLOAD_SCIENCE_MODE_SET_INF_EID` | INFO | Science mode changed |
| 6 | `ORBITER_PAYLOAD_POWER_GUARD_ERR_EID` | ERROR | Science mode rejected — payload unpowered |

## Source Layout

```
orbiter_payload/
├── CMakeLists.txt
└── fsw/
    ├── src/
    │   ├── cfe.h
    │   ├── orbiter_payload.h
    │   ├── orbiter_payload.c
    │   ├── orbiter_payload_events.h
    │   └── orbiter_payload_version.h    v1.0.0
    └── unit-test/
        └── orbiter_payload_test.c
```

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure -R orbiter_payload_unit_tests
```

## Compliance

- `[Q-F3]` `PowerState` and `ScienceMode` in `.critical_mem` — radiation-tolerant scratchpad
- `[Q-H4]` Payload MCU bus family pinned to SpaceWire via `mcu_payload_gw`
- No dynamic allocation; MISRA C:2012 required rules

## Source of Truth

- [`docs/architecture/01-orbiter-cfs.md`](../../docs/architecture/01-orbiter-cfs.md) §3.5 — PAYLOAD role
- [`docs/interfaces/apid-registry.md`](../../docs/interfaces/apid-registry.md) — APID 0x140 / 0x185
- [`docs/architecture/03-subsystem-mcus.md`](../../docs/architecture/03-subsystem-mcus.md) — Payload MCU interface
