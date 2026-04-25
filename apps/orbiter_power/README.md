# orbiter_power — Electrical Power System Arbiter

EPS arbiter app for the SAKURA-II orbiter. POWER enforces a load-switch interlock table that gates individual power loads based on the current orbiter mode (SAFE / NOMINAL / ECLIPSE), preventing unsafe power states regardless of the commanding source.

## Purpose

- Accept load-switch and power-mode commands from the ground
- Enforce the safety interlock table: reject switch commands that violate mode constraints
- Track current power mode and per-load switch state
- Aggregate `MCU_EPS_HK_MID` telemetry relayed from `mcu_eps_gw`
- Publish combined EPS housekeeping

## Message IDs

| Direction | MID (hex) | APID | Purpose |
|---|---|---|---|
| TC in | `0x1984` | `0x184` | Ground commands |
| TM out | `0x0930` | `0x130` | HK telemetry |
| HK in | `MCU_EPS_HK_MID` (`0x0AA0`) | — | MCU EPS data from `mcu_eps_gw` |

## Command Codes (TC MID `0x1984`)

| CC | Name | Payload | Effect |
|---|---|---|---|
| `0x00` | `ORBITER_POWER_NOOP_CC` | none | Increment `CmdCounter`; log version |
| `0x01` | `ORBITER_POWER_RESET_CC` | none | Zero `CmdCounter` / `ErrCounter` |
| `0x02` | `ORBITER_POWER_LOAD_SWITCH_CC` | `uint8 LoadId`, `uint8 State` | Switch load on/off; checked against interlock table |
| `0x03` | `ORBITER_POWER_SET_POWER_MODE_CC` | `uint8 Mode` | Set power mode (0=SAFE, 1=NOMINAL, 2=ECLIPSE) |

## Safety Interlock Table

| Load | Prohibited in SAFE | Prohibited in ECLIPSE | Notes |
|---|---|---|---|
| Load 0 | No | No | Always switchable |
| Load 1 | Yes | No | Critical loads only in SAFE |
| Load 2 | Yes | Yes | Non-essential; off in SAFE and ECLIPSE |
| Load 3 | Yes | No | — |

A switch command that violates the table for the current mode increments `ErrCounter` and emits `ORBITER_POWER_INTERLOCK_FAIL_ERR_EID`. The load state is not changed.

## Radiation-Sensitive State [Q-F3]

```c
static uint8 ORBITER_POWER_CurrentMode
    __attribute__((section(".critical_mem"))) = 0U;   /* 0=SAFE */

static uint8 ORBITER_POWER_LoadState[4]
    __attribute__((section(".critical_mem")));          /* per-load on/off */
```

## Telemetry — `ORBITER_POWER_HkTlm_t` (MID `0x0930`)

| Field | Type | Description |
|---|---|---|
| `CmdCounter` | `uint32` | Accepted commands since last RESET |
| `ErrCounter` | `uint32` | Rejected / errored commands |
| `CurrentMode` | `uint8` | Active power mode (SAFE / NOMINAL / ECLIPSE) |
| `LoadState[4]` | `uint8[4]` | Per-load switch state (0=off, 1=on) |

## Events

| EID | Name | Type | Trigger |
|---|---|---|---|
| 1 | `ORBITER_POWER_STARTUP_INF_EID` | INFO | Init complete |
| 2 | `ORBITER_POWER_CMD_ERR_EID` | ERROR | Unknown MID, bad CC, SB error |
| 3 | `ORBITER_POWER_CMD_NOOP_INF_EID` | INFO | NOOP accepted |
| 4 | `ORBITER_POWER_LOAD_SWITCH_INF_EID` | INFO | Load switched successfully |
| 5 | `ORBITER_POWER_INTERLOCK_FAIL_ERR_EID` | ERROR | Switch rejected by interlock |
| 6 | `ORBITER_POWER_MODE_SET_INF_EID` | INFO | Power mode changed |

## Source Layout

```
orbiter_power/
├── CMakeLists.txt
└── fsw/
    ├── src/
    │   ├── cfe.h
    │   ├── orbiter_power.h
    │   ├── orbiter_power.c
    │   ├── orbiter_power_events.h
    │   └── orbiter_power_version.h    v1.0.0
    └── unit-test/
        └── orbiter_power_test.c
```

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure -R orbiter_power_unit_tests
```

## Compliance

- `[Q-F3]` `CurrentMode` and `LoadState` in `.critical_mem` — radiation-tolerant scratchpad
- `[Q-H4]` EPS MCU bus family pinned to UART/HDLC via `mcu_eps_gw`
- No dynamic allocation; MISRA C:2012 required rules

## Source of Truth

- [`docs/architecture/01-orbiter-cfs.md`](../../docs/architecture/01-orbiter-cfs.md) §3.4 — POWER role
- [`docs/interfaces/apid-registry.md`](../../docs/interfaces/apid-registry.md) — APID 0x130 / 0x184
- [`docs/architecture/03-subsystem-mcus.md`](../../docs/architecture/03-subsystem-mcus.md) — EPS MCU interface
