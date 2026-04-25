# mcu_eps_gw — EPS MCU UART/HDLC Gateway

cFS-side gateway that bridges the UART/HDLC serial bus to the Software Bus for the EPS (Electrical Power System) MCU. Receives HDLC-framed SPP packets from the MCU, validates framing and CRC-16/CCITT-FALSE, and publishes valid frames to `MCU_EPS_HK_MID`. Corrupt frames are dropped and logged — never forwarded.

## Purpose

- Poll the UART/HDLC bus at 10 Hz for inbound EPS telemetry frames
- Validate HDLC framing (0x7E flags, byte-unstuffing, CRC-16/CCITT-FALSE)
- Publish validated SPP frames to `MCU_EPS_HK_MID` for `orbiter_power`
- Detect bus silence (≥ 3 consecutive cycles with no valid frame) and emit `BUS_SILENT_ERR_EID`
- Relay TC commands from the SB to the MCU bus (stub in Phase 35; real driver in Phase 42)

## Message IDs

| Direction | MID (hex) | APID | Purpose |
|---|---|---|---|
| TC in | `0x1AA0` | — | TC commands forwarded to EPS MCU |
| TM out | `0x0AA0` | `0x2A0` | Validated EPS telemetry frames |

## HDLC Frame Format

```
[0x7E] [payload bytes, 0x7D-escaped] [CRC-lo] [CRC-hi] [0x7E]
```

- Flag byte: `0x7E`
- Escape byte: `0x7D`; escaped value = next byte XOR `0x20`
- CRC: CRC-16/CCITT-FALSE (poly=0x1021, init=0xFFFF), computed over unescaped payload; stored LSB-first per ICD §2.3
- Minimum valid frame length: 4 bytes (two flags + two CRC bytes)
- Maximum unescaped payload: `MCU_EPS_GW_MAX_UNESCAPED` = 258 bytes

### Validation Steps

1. `len >= 4` and `frame[0] == 0x7E` and `frame[len-1] == 0x7E`
2. Unescape bytes between flags into local `unescaped[]` array
3. Compute CRC-16/CCITT-FALSE over `unescaped[0..ulen-3]`
4. Compare against `unescaped[ulen-2]` (LSB) and `unescaped[ulen-1]` (MSB)
5. Mismatch → `MCU_EPS_GW_ERR_HDLC_CRC`; frame dropped, `ErrCounter++`

## Key Constants

| Constant | Value | Description |
|---|---|---|
| `MCU_EPS_GW_PIPE_DEPTH` | 16 | SB pipe depth |
| `MCU_EPS_GW_MAX_FRAME_LEN` | 514 | Maximum raw HDLC frame (bytes) |
| `MCU_EPS_GW_MAX_UNESCAPED` | 258 | Maximum unescaped payload (bytes) |
| `MCU_EPS_GW_SILENCE_CYCLES` | 3 | Consecutive no-data cycles before BUS_SILENT event |
| `MCU_EPS_GW_HDLC_FLAG` | `0x7E` | HDLC frame delimiter |
| `MCU_EPS_GW_HDLC_ESC` | `0x7D` | HDLC escape byte |
| `MCU_EPS_GW_HDLC_ESC_XOR` | `0x20` | XOR mask applied to escaped value |

## Radiation-Sensitive State [Q-F3]

```c
static uint32 MCU_EPS_GW_SilenceCount
    __attribute__((section(".critical_mem"))) = 0U;
```

## Events

| EID | Name | Type | Trigger |
|---|---|---|---|
| 1 | `MCU_EPS_GW_STARTUP_INF_EID` | INFO | Init complete |
| 2 | `MCU_EPS_GW_CMD_ERR_EID` | ERROR | Unknown MID or SB error |
| 3 | `MCU_EPS_GW_HDLC_CRC_FAIL_ERR_EID` | ERROR | CRC mismatch — frame dropped |
| 4 | `MCU_EPS_GW_BUS_SILENT_ERR_EID` | ERROR | Bus silent for ≥ 3 cycles |

## HK Telemetry Fields

| Field | Type | Description |
|---|---|---|
| `FramesValid` | `uint32` | Cumulative frames that passed validation |
| `FramesCorrupt` | `uint32` | Cumulative frames with CRC errors |
| `ErrCounter` | `uint32` | Rejected commands / SB errors |
| `BusSilent` | `uint8` | 1 = bus silent alarm active |

## Source Layout

```
mcu_eps_gw/
├── CMakeLists.txt
└── fsw/
    ├── src/
    │   ├── cfe.h
    │   ├── mcu_eps_gw.h
    │   ├── mcu_eps_gw.c
    │   ├── mcu_eps_gw_events.h
    │   └── mcu_eps_gw_version.h    v1.0.0
    └── unit-test/
        └── mcu_eps_gw_test.c
```

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure -R mcu_eps_gw_unit_tests
```

## Compliance

- `[Q-C8]` No ad-hoc byte-swap; CRC bytes read as raw octets per ICD §2.3
- `[Q-F3]` `MCU_EPS_GW_SilenceCount` in `.critical_mem`
- `[Q-H4]` Bus family pinned to UART/HDLC (ECSS-E-ST-50-03)
- No dynamic allocation; MISRA C:2012 required rules

## Source of Truth

- [`docs/interfaces/ICD-mcu-cfs.md`](../../docs/interfaces/ICD-mcu-cfs.md) §2.3 — UART/HDLC framing
- [`docs/architecture/03-subsystem-mcus.md`](../../docs/architecture/03-subsystem-mcus.md) — EPS MCU bus assignment
- [`docs/interfaces/apid-registry.md`](../../docs/interfaces/apid-registry.md) — APID 0x2A0
