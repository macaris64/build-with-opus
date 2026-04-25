# orbiter_comm — Communications Manager

Radio link manager and CFDP Class 1 session controller for the SAKURA-II orbiter. COMM maintains AOS/LOS state, manages four AOS virtual-channel downlink rates, and tracks up to 16 concurrent CFDP file-transfer sessions.

## Purpose

- Track link state (LOS / AOS) with a 10-cycle timeout; emit `LINK_STATE_CHANGE_INF_EID` on transitions
- Manage four AOS virtual-channel (VC) downlink rates (VC0–VC3)
- Provide a CFDP Class 1 session table (16 entries) for uplink file transfers
- Relay TCP command frames received from the ground (`PROCESS_TCP_CC`)

## Message IDs

| Direction | MID (hex) | APID | Purpose |
|---|---|---|---|
| TC in | `0x1983` | `0x183` | Ground commands |
| TM out | `0x0920` | `0x120` | HK telemetry |

## Command Codes (TC MID `0x1983`)

| CC | Name | Payload | Effect |
|---|---|---|---|
| `0x00` | `ORBITER_COMM_NOOP_CC` | none | Increment `CmdCounter`; log version |
| `0x01` | `ORBITER_COMM_RESET_CC` | none | Zero `CmdCounter` / `ErrCounter` |
| `0x02` | `ORBITER_COMM_SET_DOWNLINK_RATE_CC` | `uint8 VcId`, `uint32 RateKbps` | Set downlink rate for VC 0–3 |
| `0x03` | `ORBITER_COMM_PROCESS_TCP_CC` | TCP frame payload | Relay ground TCP frame to SB |

## AOS Virtual-Channel Default Rates

| VC | Default Rate (kbps) | Typical content |
|---|---|---|
| VC0 | 50 | Critical HK / command echo |
| VC1 | 20 | Engineering housekeeping |
| VC2 | 800 | Science data downlink |
| VC3 | 100 | CFDP file transfer |

Rate changes reject if `VcId > 3` or `RateKbps == 0`; `ErrCounter` incremented and `ORBITER_COMM_CMD_ERR_EID` emitted.

## CFDP Session Table

16 entries; each entry tracks: transaction ID (`RttProbeId`), source/destination entity IDs, file size, and transfer state. Table is zeroed on `RESET_CC`.

> `[Q-C8]` `RttProbeId` is stored in host-byte-order internally; conversion to big-endian wire format occurs in `cfs_bindings`/`ccsds_wire` only — no ad-hoc byte-swap in this app.

## Link-State Machine

```
LOS ──(frame received)──► AOS
AOS ──(10 cycles no frame)──► LOS
```

Each state transition emits `ORBITER_COMM_LINK_STATE_CHANGE_INF_EID` with the new state.

## Telemetry — `ORBITER_COMM_HkTlm_t` (MID `0x0920`)

| Field | Type | Description |
|---|---|---|
| `CmdCounter` | `uint32` | Accepted commands since last RESET |
| `ErrCounter` | `uint32` | Rejected / errored commands |
| `LinkState` | `uint8` | 0=LOS, 1=AOS |
| `VcRates[4]` | `uint32[4]` | Current downlink rate per VC (kbps) |
| `CfdpSessions` | `uint8` | Active CFDP sessions |

## Events

| EID | Name | Type | Trigger |
|---|---|---|---|
| 1 | `ORBITER_COMM_STARTUP_INF_EID` | INFO | Init complete |
| 2 | `ORBITER_COMM_CMD_ERR_EID` | ERROR | Unknown MID, bad CC, invalid params |
| 3 | `ORBITER_COMM_CMD_NOOP_INF_EID` | INFO | NOOP accepted |
| 4 | `ORBITER_COMM_DOWNLINK_RATE_SET_INF_EID` | INFO | VC rate updated |
| 5 | `ORBITER_COMM_LINK_STATE_CHANGE_INF_EID` | INFO | AOS/LOS transition |
| 6 | `ORBITER_COMM_TCP_FRAME_INF_EID` | INFO | TCP frame relayed |

## Source Layout

```
orbiter_comm/
├── CMakeLists.txt
└── fsw/
    ├── src/
    │   ├── cfe.h
    │   ├── orbiter_comm.h
    │   ├── orbiter_comm.c
    │   ├── orbiter_comm_events.h
    │   └── orbiter_comm_version.h    v1.0.0
    └── unit-test/
        └── orbiter_comm_test.c
```

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure -R orbiter_comm_unit_tests
```

## Compliance

- `[Q-C8]` No ad-hoc byte-swap; all BE↔LE conversion in `cfs_bindings`/`ccsds_wire`
- `[Q-F3]` No radiation-sensitive state in v0.1; anchors added in Phase 43
- No dynamic allocation; MISRA C:2012 required rules

## Source of Truth

- [`docs/architecture/01-orbiter-cfs.md`](../../docs/architecture/01-orbiter-cfs.md) §3.3 — COMM role
- [`docs/interfaces/apid-registry.md`](../../docs/interfaces/apid-registry.md) — APID 0x120 / 0x183
- [`docs/interfaces/ICD-mcu-cfs.md`](../../docs/interfaces/ICD-mcu-cfs.md) — CFDP session table
