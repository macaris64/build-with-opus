# orbiter_cdh — Command & Data Handling

Central command-and-control app for the SAKURA-II orbiter. CDH is the single
authority for orbiter mode, aggregates peer HK traffic, and applies EVS filter
commands relayed from the ground.

## Purpose

- Accept mode-transition commands and enforce the `SAFE → NOMINAL → EMERGENCY`
  state machine
- Accept EVS filter commands and apply them to event reporting
- Count inbound peer HK messages (`PeerHkRcvCount`) to verify app-health on the
  ground without a full telemetry decom
- Publish a combined HK telemetry packet on `ORBITER_CDH_HK_MID`

## Message IDs

| Direction | MID (hex) | APID | Purpose |
|---|---|---|---|
| TC in | `0x1981` | `0x181` | Ground commands |
| TM out | `0x0901` | `0x101` | Combined HK telemetry |

Peer HK MIDs (`ORBITER_ADCS_HK_MID`, `ORBITER_COMM_HK_MID`, etc.) are
subscribed so CDH can count them; the packets are not re-published.

## Command Codes (TC MID `0x1981`)

| CC | Name | Payload | Effect |
|---|---|---|---|
| `0x00` | `ORBITER_CDH_NOOP_CC` | none | Increment `CmdCounter`; log version |
| `0x01` | `ORBITER_CDH_RESET_CC` | none | Zero `CmdCounter` / `ErrCounter` |
| `0x02` | `ORBITER_CDH_MODE_TRANSITION_CC` | `uint8 Mode` | Set orbiter mode (0=SAFE, 1=NOMINAL, 2=EMERGENCY); reject if > `ORBITER_CDH_MODE_MAX` |
| `0x03` | `ORBITER_CDH_EVS_FILTER_CC` | (reserved) | Stub for EVS filter application |

### Mode Transition Payload

```c
typedef struct {
    CFE_MSG_Message_t Header;
    uint8             Mode;       /* 0=SAFE, 1=NOMINAL, 2=EMERGENCY */
    uint8             Padding[3];
} ORBITER_CDH_ModeTransCmd_t;
```

Invalid mode values (> 2) increment `ErrCounter` and emit
`ORBITER_CDH_MODE_INVALID_ERR_EID`.

## Telemetry — `ORBITER_CDH_HkTlm_t` (MID `0x0901`)

| Field | Type | Description |
|---|---|---|
| `CurrentMode` | `uint8` | Active orbiter mode (SAFE / NOMINAL / EMERGENCY) |
| `CmdCounter` | `uint32` | Accepted commands since last RESET |
| `ErrCounter` | `uint32` | Rejected / errored commands |
| `PeerHkRcvCount` | `uint32` | Cumulative peer HK packets received |

## Orbiter Mode Values

| Value | Name | Notes |
|---|---|---|
| `0` | SAFE | Default power-up mode; restricted operations |
| `1` | NOMINAL | Full mission operations |
| `2` | EMERGENCY | Fault response; may curtail non-critical functions |

## Events

| EID | Name | Type | Trigger |
|---|---|---|---|
| 1 | `ORBITER_CDH_STARTUP_INF_EID` | INFO | Init complete |
| 2 | `ORBITER_CDH_CMD_ERR_EID` | ERROR | Unknown MID, bad CC, SB error |
| 3 | `ORBITER_CDH_CMD_NOOP_INF_EID` | INFO | NOOP accepted |
| 4 | `ORBITER_CDH_MODE_TRANSITION_INF_EID` | INFO | Mode changed |
| 5 | `ORBITER_CDH_MODE_INVALID_ERR_EID` | ERROR | Mode value out of range |
| 6 | `ORBITER_CDH_EVS_FILTER_INF_EID` | INFO | EVS filter applied |

## Source Layout

```
orbiter_cdh/
├── CMakeLists.txt
└── fsw/
    ├── src/
    │   ├── cfe.h
    │   ├── orbiter_cdh.h
    │   ├── orbiter_cdh.c
    │   ├── orbiter_cdh_events.h
    │   └── orbiter_cdh_version.h    v1.0.0
    └── unit-test/
        └── orbiter_cdh_test.c
```

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure -R orbiter_cdh_unit_tests
```

## Compliance

- `[Q-F3]` No radiation-sensitive state in v0.1; anchors added in Phase 43
- No dynamic allocation; MISRA C:2012 required rules

## Source of Truth

- [`docs/architecture/01-orbiter-cfs.md`](../../docs/architecture/01-orbiter-cfs.md) §3.1 — CDH role
- [`docs/interfaces/apid-registry.md`](../../docs/interfaces/apid-registry.md) — APID 0x101 / 0x181
