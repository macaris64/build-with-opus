# ICD — Subsystem MCU ↔ cFS

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Packet bodies: [packet-catalog.md](packet-catalog.md). Protocol stack: [../architecture/07-comms-stack.md](../architecture/07-comms-stack.md). APID registry: [apid-registry.md](apid-registry.md). Decisions: [../standards/decisions-log.md](../standards/decisions-log.md).

This ICD fixes the interface between FreeRTOS-based subsystem microcontrollers (`mcu_payload`, `mcu_rwa`, `mcu_eps`) and the cFS orbiter flight software. It covers:
- **Bus framing** for SpaceWire (ECSS-E-ST-50-12C), CAN (ISO 11898), and UART/HDLC.
- **SPP encapsulation** in each bus frame.
- **cFE Software Bus integration** via a shared `mcu_gateway` cFS app pattern.
- **MCU telemetry bodies** — concrete definitions for the `PKT-TM-0280-0002`, `PKT-TM-0290-0002`, `PKT-TM-02A0-0002` placeholders in [packet-catalog §4.6](packet-catalog.md).
- **MID scheme** — cFE v1 MIDs derive from APIDs per the registry formula `MID = 0x0800 | APID` (TM) / `0x1800 | APID` (TC).

All multi-byte fields are **big-endian** per [Q-C8](../standards/decisions-log.md).

## 1. Architecture Overview

Each MCU class runs a FreeRTOS firmware that:
1. Produces periodic HK packets (1 Hz nominal) and event/fault packets (aperiodic).
2. Consumes command packets via the same bus.
3. Slaves its time-of-day to the cFS gateway (see [08 §5.3](../architecture/08-timing-and-clocks.md)).

The cFS side owns a **per-bus gateway app**:

| MCU class | APID range | cFS gateway app | Physical bus (simulated) |
|---|---|---|---|
| `mcu_payload` | `0x280`–`0x28F` | `mcu_payload_gw` | SpaceWire (character-level) |
| `mcu_rwa` | `0x290`–`0x29F` | `mcu_rwa_gw` | CAN 2.0A (11-bit identifier) |
| `mcu_eps` | `0x2A0`–`0x2AF` | `mcu_eps_gw` | UART (115 200 baud, HDLC-framed) |

Each gateway app:
- Registers on the cFE Software Bus at startup (`CFE_ES_RegisterApp` → `CFE_SB_CreatePipe`).
- Subscribes to the TC MIDs for its MCU class (`MID = 0x18FF & (0x1800 | APID)` per §3).
- Polls the bus (SpW/CAN/UART) for inbound MCU TM frames and publishes them as cFE SB messages.
- Implements bus-specific reassembly for SPPs larger than a single bus frame (especially CAN, which is 8-byte payload).

## 2. Bus Framing Profiles

### 2.1 SpaceWire (`mcu_payload`)

- **Standard**: ECSS-E-ST-50-12C character-level encoding; packet delimited by EOP (End-of-Packet) or EEP (Error-End-of-Packet) markers.
- **Max SPP**: 256 B (per [07 §7](../architecture/07-comms-stack.md)).
- **Encapsulation**: one SPP per SpW packet. The SpW packet body is exactly the SPP bytes (primary header + secondary header + user data) with no additional wrapper.
- **Error handling**: receipt of EEP marker discards the in-flight packet and emits `EVENT: MCU-PAYLOAD-EEP` (`event_id = 0x03010001`); the MCU firmware retransmits at the next 1 Hz HK cycle (no per-frame ARQ).
- **Time budget**: 1 SPP ≤ 256 B → ≤ 2048 bits. At 1 Mbps simulated SpW rate, ≤ 2 ms per packet.

### 2.2 CAN 2.0A (`mcu_rwa`)

- **Standard**: ISO 11898, 11-bit identifier frames, up to 8 B payload per frame.
- **Max SPP**: 256 B → up to 32 CAN frames per SPP (with fragmentation).
- **Fragmentation protocol** (SAKURA-II-specific, inherits from ISO 15765-2 pattern):
  - Frame type byte (`frame_type`) is the first byte of every CAN frame payload:
    - `0x00` = **Single-frame** SPP (SPP length ≤ 7 B; unusual but allowed).
    - `0x10..0x1F` = **First-frame**; low nibble = high nibble of 12-bit total SPP length; next byte = low byte of length.
    - `0x20..0x2F` = **Consecutive-frame**; low nibble = sequence number 0..15 modulo.
  - Remaining 6–7 bytes carry SPP payload.
- **CAN ID allocation**: `id = 0x200 | (apid & 0x00F)`. All `mcu_rwa` packets use CAN IDs `0x200`–`0x20F`. TM vs TC is disambiguated by MCU-side addressing convention (even IDs = TM, odd IDs = TC) — **this is a SAKURA-II CAN-level convention, not CCSDS**.
- **CRC**: CAN's built-in 15-bit CRC suffices per [packet-catalog §2.4](packet-catalog.md); no additional layer CRC is computed.
- **Fault**: a frame with wrong `frame_type` sequence (e.g. consecutive frame arriving before a first frame) discards the in-progress SPP and emits `EVENT: MCU-RWA-CAN-FRAGMENT-LOST` (`event_id = 0x03020001`).

### 2.3 UART/HDLC (`mcu_eps`)

- **Physical**: 115 200 8N1 baud; max SPP 256 B.
- **Framing**: HDLC byte stuffing (RFC 1662 §4.2):
  - Frame flag byte = `0x7E`.
  - Escape byte = `0x7D`; escaped byte = original XOR `0x20`.
  - A frame is: `FLAG | <escaped SPP bytes> | CRC-16-lo | CRC-16-hi | FLAG` — note **CRC-16/CCITT-FALSE** at the frame level per [packet-catalog §2.4](packet-catalog.md), LSB-first inside the HDLC frame (per HDLC convention, which differs from CCSDS — this is an HDLC quirk noted here for clarity; the SPP fields themselves remain BE).
- **Timing**: 115.2 kbps → ~11.5 KB/s raw → ≤ ~88 ms per 256 B SPP with HDLC overhead.
- **Fault**: CRC-16 mismatch → discard frame + emit `EVENT: MCU-EPS-HDLC-CRC-FAIL` (`event_id = 0x03030001`).

## 3. cFE Software Bus Integration

### 3.1 MID Scheme

Per [apid-registry §cFE MID](apid-registry.md), MID derives from APID:

```
Telemetry:  MID_TM = 0x0800 | APID
Commands:   MID_TC = 0x1800 | APID
```

| APID | MID_TM | MID_TC | Purpose |
|---|---|---|---|
| `0x280` | `0x0A80` | `0x1A80` | `mcu_payload-01` HK + nominal cmd |
| `0x281`–`0x28F` | `0x0A81`–`0x0A8F` | `0x1A81`–`0x1A8F` | `mcu_payload-02..16` (instance IDs 2..16 via secondary-header `instance_id`; 15 reserved per-APID slots for future subtypes) |
| `0x290` | `0x0A90` | `0x1A90` | `mcu_rwa-01` HK + nominal cmd |
| `0x2A0` | `0x0AA0` | `0x1AA0` | `mcu_eps-01` HK + nominal cmd |

Instance multiplicity (e.g. five RWAs in Scale-5) is handled by the secondary-header `instance_id`, **not** by allocating additional APIDs, per [10 §6](../architecture/10-scaling-and-config.md).

### 3.2 Gateway App Lifecycle

Each gateway app follows the same cFE pattern:

```c
/* Pseudocode — full treatment in architecture/03-subsystem-mcus.md (B3) */
int32 MCU_RWA_GW_AppMain(void) {
    CFE_ES_RegisterApp();
    pipe = CFE_SB_CreatePipe(&pipe, 64, "MCU_RWA_GW_PIPE");
    /* TC subscriptions — one per APID in class */
    for (apid = 0x290; apid <= 0x29F; apid++) {
        CFE_SB_Subscribe(0x1800 | apid, pipe);
    }
    /* TM publish loop */
    for (;;) {
        /* Poll bus for inbound MCU TM; repack as SB msg */
        bus_poll_and_publish();
        /* Drain TC queue to bus */
        if (CFE_SB_ReceiveBuffer(&sb_msg, pipe, 10 /* ms */) == CFE_SUCCESS) {
            bus_emit_tc(sb_msg);
        }
    }
}
```

The gateway never re-encodes the SPP — it is a **transport** for already-catalogued packets. If a gateway needs to translate (e.g. CAN fragmentation ↔ single SB message), it does so at the **bus frame** layer, never at the SPP layer.

### 3.3 Task Scheduling

| Gateway | Priority (cFE pri #) | Stack | Period |
|---|---|---|---|
| `mcu_payload_gw` | 90 | 8 KiB | 100 Hz poll |
| `mcu_rwa_gw` | 95 (ADCS-adjacent) | 8 KiB | 100 Hz poll |
| `mcu_eps_gw` | 80 | 4 KiB | 10 Hz poll |

These are compile-time constants in `_defs/mission_config.h`; concrete numbers land with the gateway-app source under `apps/mcu_*_gw/`.

## 4. MCU TM Packet Bodies

These definitions expand the placeholders in [packet-catalog §4.6](packet-catalog.md). Once this ICD is merged, the catalog §4.6 table is updated to point its body-column at this §4.

### 4.1 `PKT-TM-0280-0002` — `mcu_payload` HK

| Attribute | Value |
|---|---|
| APID | `0x280` |
| Function code | `0x0002` |
| Sampling rate | 1 Hz |
| Bus | SpaceWire |
| CRC | bus-level (SpW character framing — no SPP-level CRC) |

User-data layout:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0 | 4 B | `boot_count` | `counter_32` | count |
| +4 | 4 B | `uptime_s` | `u32` BE | seconds |
| +8 | 2 B | `instrument_state_word` | `u16` BE | bit-encoded per §4.1.1 |
| +10 | 2 B | `bus_voltage` | `voltage_mV` | mV |
| +12 | 2 B | `instrument_current` | `current_mA` | mA |
| +14 | 2 B | `instrument_temp` | `temp_cC` | 0.01 °C |
| +16 | 2 B | `detector_temp` | `temp_dK` | 0.1 K (absolute — cryogenic) |
| +18 | 2 B | `heater_duty_x10` | `u16` BE | 0.1 % (0–1000) |
| +20 | 1 B | `mode` | `mode_enum8` | 0=OFF, 1=WARMUP, 2=IDLE, 3=SCIENCE, 4=CAL, 5=SAFE, 255=FAULTED |
| +21 | 1 B | `fault_mask` | `bitfield8` | Bit 0=DETECTOR-OVERTEMP, 1=LASER-FAULT, 2=DATA-QUEUE-OVERFLOW, 3=SPW-LOST, 4–7=RSVD |
| +22 | 2 B | `reserved` | `u16` BE | `0x0000` |
| **Total user data** | **24 B** | | | |

#### 4.1.1 `instrument_state_word` bit map

| Bits | Field | Meaning |
|---|---|---|
| 15..12 | `active_channel` | 0–15 channel index |
| 11..8 | `integration_time_exp` | 2^n milliseconds; 0–15 → 1–32768 ms |
| 7..4 | `filter_wheel_pos` | 0–15 filter position |
| 3..0 | `shutter_state` | 0=CLOSED, 1=OPEN, 2=MOVING, 3–15=reserved |

### 4.2 `PKT-TM-0290-0002` — `mcu_rwa` HK

| Attribute | Value |
|---|---|
| APID | `0x290` |
| Function code | `0x0002` |
| Sampling rate | **10 Hz** (Fast — feeds `orbiter_adcs`) |
| Bus | CAN |
| CRC | bus-level (CAN CRC-15) |

User-data layout:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0 | 2 B | `wheel_index` | `u16` BE | 0–255 valid; high byte reserved for fleet |
| +2 | 4 B | `wheel_speed_dRPM` | `i32` BE | LSB = 0.1 RPM, full ±214 million RPM — way beyond physical; useful bits are lower 24 |
| +6 | 2 B | `wheel_current` | `current_10mA` | 10 mA |
| +8 | 2 B | `wheel_temp` | `temp_cC` | 0.01 °C |
| +10 | 2 B | `bearing_temp` | `temp_cC` | 0.01 °C |
| +12 | 2 B | `commanded_torque_Nm_x1000` | `i16` BE | milli-Newton-meters |
| +14 | 2 B | `measured_torque_Nm_x1000` | `i16` BE | milli-Newton-meters |
| +16 | 2 B | `motor_current` | `current_10mA` | 10 mA |
| +18 | 1 B | `mode` | `mode_enum8` | 0=OFF, 1=SPIN-UP, 2=SPEED-CONTROL, 3=TORQUE-CONTROL, 4=BRAKE, 5=SAFE, 255=FAULTED |
| +19 | 1 B | `fault_mask` | `bitfield8` | Bit 0=STALL, 1=OVERCURRENT, 2=OVERTEMP, 3=BEARING-WARNING, 4=HALL-SENSOR-FAULT, 5–7=RSVD |
| **Total user data** | **20 B** | | | |

Each RWA instance publishes its own PKT-TM-0290-0002; ADCS consumes the three-unit set and produces the aggregated `PKT-TM-0110-0002`.

### 4.3 `PKT-TM-02A0-0002` — `mcu_eps` HK

| Attribute | Value |
|---|---|
| APID | `0x2A0` |
| Function code | `0x0002` |
| Sampling rate | 1 Hz |
| Bus | UART/HDLC |
| CRC | frame-level (HDLC CRC-16/CCITT-FALSE) |

User-data layout:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0 | 4 B | `uptime_s` | `u32` BE | seconds |
| +4 | 2 B | `solar_voltage` | `voltage_mV` | mV |
| +6 | 2 B | `solar_current` | `current_10mA` | 10 mA |
| +8 | 2 B | `battery_voltage` | `voltage_mV` | mV |
| +10 | 2 B | `battery_current_in` | `current_10mA` | 10 mA (charging rate) |
| +12 | 2 B | `battery_current_out` | `current_10mA` | 10 mA (discharge rate) |
| +14 | 2 B | `battery_temp` | `temp_cC` | 0.01 °C |
| +16 | 2 B | `battery_soc_pct_x10` | `u16` BE | 0.1 % |
| +18 | 2 B | `battery_soh_pct_x10` | `u16` BE | 0.1 % (state of health) |
| +20 | 2 B | `pdu_temp` | `temp_cC` | 0.01 °C |
| +22 | 2 B | `load_switch_state` | `u16` BE | bit i = switch i (i=0..15); 1=ON |
| +24 | 2 B | `load_switch_current[0]` | `current_10mA` | switch-0 load current |
| +26 | 2 B | `load_switch_current[1]` | `current_10mA` | switch-1 load current |
| +28 | 2 B | `load_switch_current[2]` | `current_10mA` | switch-2 load current |
| +30 | 2 B | `load_switch_current[3]` | `current_10mA` | switch-3 load current |
| +32 | 1 B | `mode` | `mode_enum8` | 0=NOMINAL, 1=LOW-POWER, 2=CHARGE-ONLY, 3=SURVIVAL, 4=FAULTED |
| +33 | 1 B | `fault_mask` | `bitfield8` | Bit 0=OVERVOLT, 1=UNDERVOLT, 2=OVERCURR, 3=OVERTEMP, 4=UNDERTEMP, 5=BATT-DISCONNECT, 6=SOLAR-FAULT, 7=UART-CRC-FAIL-RECENT |
| +34 | 2 B | `reserved` | `u16` BE | `0x0000` |
| **Total user data** | **36 B** | | | |

MCU EPS telemetry feeds the orbiter's `PKT-TM-0130-0002` — the orbiter's Power HK aggregates load-switch state and battery SoC after gateway passthrough.

## 5. MCU TC Packet Bodies

### 5.1 `PKT-TC-0280-0100` — `mcu_payload` Set Mode

| Attribute | Value |
|---|---|
| APID | `0x280` |
| Function code | `0x0100` |

User-data:

| Offset | Width | Field | Type | Values |
|---|---|---|---|---|
| +0 | 1 B | `new_mode` | `mode_enum8` | Per §4.1 mode enum |
| +1 | 1 B | `reserved` | `u8` | `0x00` |
| **Total user data** | **2 B** | | | |

### 5.2 `PKT-TC-0290-0100` — `mcu_rwa` Set Wheel Speed (safety-interlocked)

| Attribute | Value |
|---|---|
| APID | `0x290` |
| Function code | `0x8100` (arm-then-fire) |

User-data:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0 | 2 B | `wheel_index` | `u16` BE | 0–2 for the 3-wheel set |
| +2 | 4 B | `target_speed_dRPM` | `i32` BE | LSB = 0.1 RPM |
| +6 | 2 B | `slew_rate_dRPMps` | `u16` BE | LSB = 0.1 RPM/s |
| +8 | 2 B | `confirm_magic` | `u16` BE | `0xABCD` |
| +10 | 2 B | `reserved` | `u16` BE | `0x0000` |
| **Total user data** | **12 B** | | | |

### 5.3 `PKT-TC-02A0-0100` — `mcu_eps` Load-Switch Passthrough

MCU-side load-switch commands are forwarded through the orbiter's `PKT-TC-0184-*` chain (orbiter-power app arms and fires via the cFS EPS gateway). A direct MCU-level load-switch TC is defined here for gateway-diagnostic purposes only and **requires** the same arm-then-fire + confirm-magic pattern as `PKT-TC-0184-8100`. Body layout matches `PKT-TC-0184-8100` verbatim; operational use is restricted to scenarios explicitly flagged as diagnostics in [V&V-Plan.md](../mission/verification/V&V-Plan.md) (Batch B4).

## 6. Fault Behavior

| Fault | Detected at | Action | Event ID |
|---|---|---|---|
| SpW EEP mid-packet | Gateway | Discard in-flight SPP; re-enable Rx | `0x03010001` |
| CAN fragment sequence error | Gateway | Discard in-flight SPP; re-enable Rx | `0x03020001` |
| UART HDLC CRC mismatch | Gateway | Discard frame | `0x03030001` |
| MCU bus silent > 3 × HK period | Gateway | Publish synthetic `PKT-TM-02xx-0003` "MCU-SILENT" TM with `mode = FAULTED`, instance_id echo | `0x03040001` |
| TC rejected by MCU (bad body) | MCU → gateway → SB | `PKT-TM-02xx-0001` TC-Ack with `REJECTED-RANGE` | N/A |
| Confirm-magic mismatch on safety TC | Gateway | Drop TC before bus emit; TC-Ack with `REJECTED-AUTH` | N/A |

`PKT-TM-02xx-0003` is not explicitly enumerated in packet-catalog §4.6; it is reserved here by this ICD and added to catalog §4.6 as a synthetic-TM variant emitted only by the gateway when bus silence is detected. Body = 2 B (`last_known_mode` + `silence_duration_s_x10`).

## 7. Compliance Matrix

### 7.1 TM (MCU → cFS → downstream)

| PKT-ID | APID | Bus | Rate | Body defined in |
|---|---|---|---|---|
| PKT-TM-0280-0002 | `0x280` | SpW | 1 Hz | this ICD §4.1 |
| PKT-TM-0290-0002 | `0x290` | CAN | 10 Hz | this ICD §4.2 |
| PKT-TM-02A0-0002 | `0x2A0` | UART | 1 Hz | this ICD §4.3 |
| PKT-TM-02xx-0003 (bus-silence synthetic) | `0x280`/`0x290`/`0x2A0` | gateway-emitted | aperiodic | this ICD §6 |
| PKT-TM-02xx-0004 (EVS event log, MCU-class) | per APID | gateway-emitted | event-driven | [packet-catalog §4.2](packet-catalog.md) EVS format |

### 7.2 TC (cFS → MCU)

| PKT-ID | APID | Bus | Mode | Body defined in |
|---|---|---|---|---|
| PKT-TC-0280-0100 | `0x280` | SpW | routine | this ICD §5.1 |
| PKT-TC-0290-8100 | `0x290` | CAN | safety-interlocked | this ICD §5.2 |
| PKT-TC-02A0-8100 (diag) | `0x2A0` | UART | safety-interlocked | this ICD §5.3 (= `PKT-TC-0184-8100` body) |
| PKT-TC-02xx-8000 (Arm, shared) | per APID | bus | safety-arm | follows `PKT-TC-0184-8000` pattern ([packet-catalog §5.7](packet-catalog.md)) |

## 8. What this ICD is NOT

- Not an MCU firmware design — actual firmware modules for `mcu_payload`/`mcu_rwa`/`mcu_eps` are specified in [`../architecture/03-subsystem-mcus.md`](../architecture/03-subsystem-mcus.md) (Batch B3).
- Not a hardware PCB-level spec — pin assignments, transceiver ICs, and bus termination live in (future) hardware documentation outside the Phase B set.
- Not a cFS app-development guide — cFS app patterns are in [`../architecture/01-orbiter-cfs.md`](../architecture/01-orbiter-cfs.md) (Batch B3).
- Not a performance-tuning guide — poll rates listed in §3.3 are starting points; performance tuning lands in [V&V-Plan.md](../mission/verification/V&V-Plan.md) (Batch B4).
