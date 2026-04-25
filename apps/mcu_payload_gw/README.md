# mcu_payload_gw — Payload MCU SpaceWire Gateway

cFS-side gateway that bridges the SpaceWire bus to the Software Bus for the science Payload MCU. Receives SpW packets from the MCU, checks for Error End-of-Packet (EEP) markers, and publishes clean frames to `MCU_PAYLOAD_HK_MID`. EEP-terminated frames indicate a link-layer error and are dropped — never forwarded.

## Purpose

- Poll the SpaceWire bus at 100 Hz for inbound payload telemetry packets
- Detect SpW Error End-of-Packet (EEP, marker byte `0x01`) and discard affected frames
- Publish frames terminated with normal EOP (`0x00`) to `MCU_PAYLOAD_HK_MID` for `orbiter_payload`
- Detect bus silence (≥ 3 consecutive cycles) and emit `BUS_SILENT_ERR_EID`
- Relay TC commands from the SB to the MCU bus (stub in Phase 35; real driver in Phase 42)

## Message IDs

| Direction | MID (hex) | APID | Purpose |
|---|---|---|---|
| TC in | `0x1A80` | — | TC commands forwarded to Payload MCU |
| TM out | `0x0A80` | `0x280` | Validated payload telemetry frames |

## SpaceWire Frame Format (ECSS-E-ST-50-12C)

```
[SPP payload bytes] [end marker]
```

| End Marker | Value | Meaning |
|---|---|---|
| EOP — End-of-Packet (normal) | `0x00` | Frame complete; publish to SB |
| EEP — Error End-of-Packet | `0x01` | Link-layer error; frame dropped |

### Validation

1. Check `len >= 1`.
2. If `frame[len-1] == 0x01` (EEP) → return `MCU_PAYLOAD_GW_ERR_EEP`; frame dropped, `ErrCounter++`, emit `MCU_PAYLOAD_GW_EEP_ERR_EID`.
3. Otherwise (EOP or no marker) → `CFE_SUCCESS`; publish via `CFE_SB_TransmitMsg`.

## Key Constants

| Constant | Value | Description |
|---|---|---|
| `MCU_PAYLOAD_GW_PIPE_DEPTH` | 64 | SB pipe depth |
| `MCU_PAYLOAD_GW_MAX_FRAME_LEN` | 260 | Max SpW frame (256B SPP + end marker) |
| `MCU_PAYLOAD_GW_SILENCE_CYCLES` | 3 | No-data cycles before BUS_SILENT event |
| `MCU_PAYLOAD_GW_SPW_EOP` | `0x00` | Normal End-of-Packet marker |
| `MCU_PAYLOAD_GW_SPW_EEP` | `0x01` | Error End-of-Packet marker |

## Radiation-Sensitive State [Q-F3]

```c
static uint32 MCU_PAYLOAD_GW_SilenceCount
    __attribute__((section(".critical_mem"))) = 0U;
```

## Events

| EID | Name | Type | Trigger |
|---|---|---|---|
| 1 | `MCU_PAYLOAD_GW_STARTUP_INF_EID` | INFO | Init complete |
| 2 | `MCU_PAYLOAD_GW_CMD_ERR_EID` | ERROR | Unknown MID or SB error |
| 3 | `MCU_PAYLOAD_GW_EEP_ERR_EID` | ERROR | EEP marker detected — frame dropped |
| 4 | `MCU_PAYLOAD_GW_BUS_SILENT_ERR_EID` | ERROR | Bus silent for ≥ 3 cycles |

## HK Telemetry Fields

| Field | Type | Description |
|---|---|---|
| `FramesValid` | `uint32` | Cumulative frames with normal EOP |
| `FramesCorrupt` | `uint32` | Cumulative frames with EEP marker |
| `ErrCounter` | `uint32` | Rejected commands / SB errors |
| `BusSilent` | `uint8` | 1 = bus silent alarm active |

## Source Layout

```
mcu_payload_gw/
├── CMakeLists.txt
└── fsw/
    ├── src/
    │   ├── cfe.h
    │   ├── mcu_payload_gw.h
    │   ├── mcu_payload_gw.c
    │   ├── mcu_payload_gw_events.h
    │   └── mcu_payload_gw_version.h    v1.0.0
    └── unit-test/
        └── mcu_payload_gw_test.c
```

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure -R mcu_payload_gw_unit_tests
```

## Compliance

- `[Q-F3]` `MCU_PAYLOAD_GW_SilenceCount` in `.critical_mem`
- `[Q-H4]` Bus family pinned to SpaceWire (ECSS-E-ST-50-12C)
- No dynamic allocation; MISRA C:2012 required rules

## Source of Truth

- [`docs/interfaces/ICD-mcu-cfs.md`](../../docs/interfaces/ICD-mcu-cfs.md) §4 — SpaceWire framing
- [`docs/architecture/03-subsystem-mcus.md`](../../docs/architecture/03-subsystem-mcus.md) — Payload MCU bus assignment
- [`docs/interfaces/apid-registry.md`](../../docs/interfaces/apid-registry.md) — APID 0x280
