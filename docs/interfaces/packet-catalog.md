# Packet Catalog

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Parent registry: [apid-registry.md](apid-registry.md). Protocol stack: [../architecture/07-comms-stack.md](../architecture/07-comms-stack.md). Time format: [../architecture/08-timing-and-clocks.md](../architecture/08-timing-and-clocks.md). Decisions: [../standards/decisions-log.md](../standards/decisions-log.md).

This is the **bit-level packet catalog** for SAKURA-II. Every Batch B2 ICD references entries here by `PKT-ID`; ICDs specify routing, rate, and VC mapping but never redefine packet bodies. When a new packet is added anywhere in the fleet, it gets a `PKT-ID` here first, then its APID/MID in [apid-registry.md](apid-registry.md), then its source macro.

Upstream dependencies this catalog respects verbatim:

- Primary header format — CCSDS 133.0-B-2, pinned in [07 §2](../architecture/07-comms-stack.md).
- Secondary header format — 10 bytes (7B CUC + 2B func-code + 1B instance) per [Q-C6](../standards/decisions-log.md), defined in [08 §2](../architecture/08-timing-and-clocks.md).
- Endianness — **big-endian** on the wire, every multi-byte field, every packet class. [Q-C8](../standards/decisions-log.md) is non-negotiable; the Rust encoder/decoder for all header fields is `ccsds_wire` and `cfs_bindings` — no ad-hoc conversion.
- APID allocations — [apid-registry.md](apid-registry.md).
- Fault-injection APID block `0x540`–`0x543` — reserved exclusively for the Gazebo → FSW fault-injection bridge, per [Q-F2](../standards/decisions-log.md). Any other use is a deviation.

## 1. Universal Header Layout

Every SAKURA-II Space Packet carries the same 16-byte framing prefix: 6-byte CCSDS primary header + 10-byte SAKURA-II secondary header. All fields in both headers are big-endian. Bit 0 of each byte is the MSB; byte 0 is transmitted first.

### 1.1 Primary Header (6 bytes, CCSDS 133.0-B-2)

```
   Byte 0         Byte 1         Byte 2         Byte 3         Byte 4         Byte 5
 7|6 5 4|3|2|1 0|7 6 5 4 3|2 1 0|7 6 5 4 3 2 1 0|7|6 5|4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0
 ┌─────┬─┬─┬───────────┬─────┬───────────────┬─┬───┬─────────┬───────────────┬───────────────┐
 │ VER │T│S│   APID (11 bits)  │ SEQF│   PSC (14 bits)    │   Packet Data Length (16 bits)   │
 └─────┴─┴─┴───────────────────┴─────┴──────────────────────┴───────────────────────────────────┘
   3b   1b 1b       11b          2b          14b                      16 bits, BE
```

| Offset | Width (bits) | Field | SAKURA-II value | Encoding |
|---|---|---|---|---|
| [0] bits 7–5 | 3 | Packet Version Number (VER) | `0b000` | literal |
| [0] bit 4 | 1 | Packet Type (T) | `0` = TM, `1` = TC | literal |
| [0] bit 3 | 1 | Secondary Header Flag (S) | `1` (always present) | literal |
| [0] bits 2–0 + [1] | 11 | APID | per [apid-registry.md](apid-registry.md) | **unsigned, BE** |
| [2] bits 7–6 | 2 | Sequence Flags (SEQF) | `0b11` (standalone) | literal |
| [2] bits 5–0 + [3] | 14 | Packet Sequence Count (PSC) | per-APID rolling counter | **unsigned, BE** |
| [4]–[5] | 16 | Packet Data Length | `total_length − 7` per CCSDS convention | **unsigned, BE** |

Rationale notes:
- **No segmentation**. SEQF is hard-coded `0b11`. If a payload exceeds the boundary max (see [07 §2](../architecture/07-comms-stack.md) packet-size limits table), the producer splits at the application layer — never via CCSDS SEQF.
- **PSC is per-APID, not per-boundary**. A forwarded packet carries the original producer's PSC; relays do not rewrite it. This preserves duplicate-detection integrity across the fleet.
- **"Packet Data Length − 7"** is the CCSDS convention: total packet length in bytes minus one minus the 6-byte primary header. A 20-byte total packet has data length field = `20 − 7 = 13` → `0x000D`.

### 1.2 Secondary Header (10 bytes, SAKURA-II fleet-wide)

```
   Byte 6                       Byte 12       Byte 13            Byte 14       Byte 15
 ┌──────────────────────────────────────────┬──────────────────────────────┬───────────────┐
 │     CUC time tag (7 bytes, CCSDS 301)    │ Function / HK code (2B, BE)  │  Instance ID  │
 └──────────────────────────────────────────┴──────────────────────────────┴───────────────┘
       P-Field=0x2F, 4B coarse, 2B fine                16 bits                   8 bits
```

| Offset | Width | Field | Encoding | Source of truth |
|---|---|---|---|---|
| [6] | 1 byte | CUC P-Field | fixed `0x2F` (TAI epoch, 4B coarse + 2B fine) | [08 §2](../architecture/08-timing-and-clocks.md) |
| [7]–[10] | 4 bytes | CUC coarse time (seconds since TAI epoch 1958-01-01 00:00:00 TAI) | **unsigned, BE** | [08 §2](../architecture/08-timing-and-clocks.md) |
| [11]–[12] | 2 bytes | CUC fine time (sub-second, unit = 2⁻¹⁶ s) | **unsigned, BE** | [08 §2](../architecture/08-timing-and-clocks.md) |
| [13]–[14] | 2 bytes | Function / Telemetry code | **unsigned, BE** | this doc, per packet |
| [15] | 1 byte | Instance ID (1-indexed within vehicle class, ≤ 255) | unsigned | [10 §6](../architecture/10-scaling-and-config.md) |

**`Function / Telemetry code`** semantics per packet type:

- **TC (uplink)**: command code. Commands are disambiguated by (APID, function_code); the command table for each commandable app lives in §5–§6 of this catalog.
- **TM (downlink)**: HK-packet-type code. Low bit `time_suspect` flag (bit 0, mask `0x0001`) is reserved per [08 §4](../architecture/08-timing-and-clocks.md); any HK type must leave bit 0 available for the time-suspect indicator. HK-type values are therefore always assigned as even numbers.

### 1.3 Total Overhead

| Item | Bytes |
|---|---|
| Primary header | 6 |
| Secondary header | 10 |
| **Total framing prefix** | **16** |

Every length budget in this catalog and every V&V latency test ([V&V-Plan.md](../mission/verification/V&V-Plan.md), Batch B4) counts this 16-byte overhead.

### 1.4 Cryobot Secondary-Header Variant (BW-collapse only)

When the cryobot tether is in BW-collapse mode (per [07 §6](../architecture/07-comms-stack.md) + [08 §7](../architecture/08-timing-and-clocks.md)), the secondary header is reduced to **4 bytes**:

| Offset | Width | Field | Encoding |
|---|---|---|---|
| [6]–[7] | 2 bytes | Coarse-time delta from last sync packet | **unsigned, BE** |
| [8] | 1 byte | Function / HK code (low-byte only) | unsigned |
| [9] | 1 byte | Instance ID | unsigned |

Packets emitted every N seconds carry the full 10-byte header (sync packets) to re-anchor the time-delta. The exact N is a tether-BW tradeoff fixed in [ICD-cryobot-tether.md](ICD-cryobot-tether.md) (planned).

This variant is used **only** on the cryobot tether boundary. All other boundaries carry the full 10-byte header without exception.

## 2. Conventions

### 2.1 Endianness

Per [Q-C8](../standards/decisions-log.md), **every multi-byte field in every packet is big-endian** (network byte order). This applies to all header fields in §1, all sensor words in §4, all command parameters in §5–§6, and all fault-injection payloads in §7. No packet in this catalog contains a little-endian or host-endian field; any attempt to add one is a deviation and must land in [deviations.md](../standards/deviations.md).

The single conversion loci are:
- **Rust**: `ccsds_wire` crate for headers + pure-Rust payload pack/unpack; `cfs_bindings` for FSW-adjacent type bridging.
- **C (FSW)**: CFE macros `CFE_MAKE_BIG16`, `CFE_MAKE_BIG32` (or the equivalent `htobe16`/`htobe32` from `<endian.h>`) at exactly the serialization boundary.
- **C++ (ROS 2)**: `htobe*` from `<endian.h>` inside the rover comm LifecycleNode's SPP framer — no other node touches wire bytes.

### 2.2 Fixed-Point Scaling

Sensor values are fixed-point integers. The conversion to physical units is:

```
physical = (raw_int × scale) + offset
```

Each TM packet in §4 specifies `(scale, offset, unit, raw_type)` for every sensor field. Ground-station decoders read these fields from the catalog at build time; a single table of (APID, field_name) → (scale, offset, unit) is generated in `rust/ground_station/src/catalog/`.

### 2.3 Sampling Rates

Rates listed in this catalog are **nominal**. The sampling rate is produced by the packet emitter (cFS app, FreeRTOS task, ROS 2 LifecycleNode); the downlink rate may be lower if a packet is rate-limited on its way out (see AOS VC budgets in [07 §3](../architecture/07-comms-stack.md)).

Three rate classes appear:

| Class | Rate | Used for |
|---|---|---|
| **Fast** | 10 Hz | ADCS state, critical telemetry, EDL phase |
| **Nominal HK** | 1 Hz | All HK packets in steady-state ops |
| **Slow** | 0.1 Hz (every 10 s) | Long-trend temperatures, cumulative counters |
| **Event-driven** | aperiodic | Events, faults, mode transitions |
| **On-demand** | command-triggered | Science payload captures, commanded diagnostics |

### 2.4 CRC-16/CCITT-FALSE

The authoritative CRC for SAKURA-II packet classes is **CRC-16/CCITT-FALSE** (a.k.a. CRC-16/IBM-3740, CRC-16/AUTOSAR). Parameters:

| Parameter | Value |
|---|---|
| Polynomial | `0x1021` (x¹⁶ + x¹² + x⁵ + 1) |
| Initial value | `0xFFFF` |
| Reflect input | **no** |
| Reflect output | **no** |
| Final XOR | `0x0000` |
| Test vector | CRC("123456789") = `0x29B1` |

CRC presence is **per packet class** — not per packet — and is summarized in the table below. For every class, the CRC (if any) is the **last 2 bytes** of the field it covers. The CRC itself is stored big-endian.

| Packet class | CRC presence | Coverage | Rationale |
|---|---|---|---|
| Orbiter TM (HK / Events) | **frame-level** (AOS FECF, [07 §3](../architecture/07-comms-stack.md)) | entire AOS Transfer Frame | AOS 732.0-B mandates FECF; duplicating at SPP level costs bandwidth for no integrity gain |
| Orbiter TC (uplink) | **frame-level** (TC SDLP FECF, [07 §3](../architecture/07-comms-stack.md)) + COP-1 | TC frame | Uplink already has sequence-controlled reliability |
| Relay TM | frame-level (AOS cross-link FECF) | AOS frame | Same as orbiter TM |
| Rover TM/TC (via Proximity-1) | **frame-level** (Proximity-1 FECW, CCSDS 211.0-B) | Proximity-1 frame | Proximity-1 mandates frame CRC |
| Cryobot tether TM/TC | **frame-level** (tether CRC-16, [07 §6](../architecture/07-comms-stack.md)) | tether frame | Project-local link defines its own CRC |
| cFS ↔ MCU (SpW / CAN / UART) | **bus-level** | physical-bus frame | SpW uses EEP, CAN has built-in CRC-15, UART uses HDLC-with-CRC-16 |
| **Sim injection (APID 0x540–0x57F)** | **payload-level** (trailing CRC-16/CCITT-FALSE in SPP user data — see §7) | SPP user data only (excludes primary + secondary headers) | In-process transport has no frame CRC; payload CRC guards against bugs in the injector |
| Ground-internal (APID 0x600–0x67F) | **none** | — | Local-bus-only, not on any RF link |

When a class lists `frame-level`, the SPP body does **not** carry a trailing CRC. When a class lists `payload-level`, the last 2 bytes of user data are the CRC over `(user_data[0] .. user_data[N-3])` — i.e. the CRC covers everything up to but not including itself.

### 2.5 Naming: `PKT-ID`

Every catalog entry has a stable `PKT-ID` of the form `PKT-<CLASS>-<APID>-<FUNC>`, e.g. `PKT-TM-0100-0002`. ICDs cite packets by `PKT-ID`. PSC and instance fields are not part of the `PKT-ID` because they are runtime-varying.

## 3. Common Field Types

Shared across TM/TC packets to reduce catalog restatement.

| Type | Width | Encoding | Range | Notes |
|---|---|---|---|---|
| `u8` | 1 B | unsigned | 0–255 | |
| `i8` | 1 B | two's complement | −128 to +127 | |
| `u16` | 2 B | **BE** unsigned | 0–65535 | |
| `i16` | 2 B | **BE** two's complement | −32768 to +32767 | |
| `u32` | 4 B | **BE** unsigned | 0–4 294 967 295 | |
| `i32` | 4 B | **BE** two's complement | ±2.1×10⁹ | |
| `enum8` | 1 B | unsigned; values enumerated per packet | 0–255 | Undefined values are a decode error |
| `bitfield8` | 1 B | MSB-first bit assignments | — | Per-packet bit layout |
| `cuc7` | 7 B | per §1.2 | TAI time | Not used inside user data — CUC is header-only |

## 4. Telemetry (TM) Packet Catalog

### 4.1 Sensor Primitives (scaling conventions)

The "NASA-standard" sensor words reused by every HK packet in this catalog. Each primitive lists: raw wire type → physical value.

| Primitive | Wire type | LSB (scale) | Offset | Physical unit | Physical range | Overflow / sentinel |
|---|---|---|---|---|---|---|
| `voltage_mV` | `u16` BE | 1 | 0 | millivolts | 0–65 535 mV (0–65.535 V) | `0xFFFF` = invalid / off-nominal |
| `current_mA` | `i16` BE | 1 | 0 | milliamps (signed; + is source, − is sink) | ±32 767 mA | `0x8000` = invalid sentinel (reserved; never a real reading) |
| `current_10mA` | `i16` BE | 10 | 0 | 10 milliamps (for high-current loads, e.g. ADCS wheels) | ±327.67 A | `0x8000` = invalid |
| `temp_cC` | `i16` BE | 0.01 | 0 | centi-Celsius (value ÷ 100 = °C) | ±327.67 °C | `0x8000` = sensor faulted |
| `temp_dK` | `i16` BE | 0.1 | 0 | deci-Kelvin (value ÷ 10 = K) | 0–3276.7 K (used for cryogenic hardware) | `0x8000` = sensor faulted |
| `pressure_Pa` | `u32` BE | 1 | 0 | pascals | 0–4.29×10⁹ Pa | `0xFFFFFFFF` = over-range |
| `counter_32` | `u32` BE | 1 | 0 | monotonic count (boots, events, etc.) | — | Rolls over; consumers check `counter_mod 2³²` deltas |
| `bool_u8` | `u8` | — | — | — | `0x00` = false, `0x01` = true; all other values = invalid |
| `mode_enum8` | `enum8` | — | — | — | Per-app state-machine enum (values listed with each packet) |

All fields are big-endian on the wire per [Q-C8](../standards/decisions-log.md). Decoders check the sentinel value (`0xFFFF`, `0x8000`, `0xFFFFFFFF`) before applying scaling — a sentinel raises a flag, never a physical reading.

### 4.2 Orbiter TM Packets

#### `PKT-TM-0100-0002` — Sample App HK

| Attribute | Value |
|---|---|
| APID | `0x100` |
| MID (derived) | `0x0900` (= `0x0800 \| 0x100`) |
| Function code | `0x0002` |
| Owning app | `sample_app` (template reference) |
| Sampling rate | 1 Hz (Nominal HK) |
| AOS VC | 0 |
| CRC | frame-level (AOS FECF) |

User-data layout (after 16-byte header):

| Offset (from byte 16) | Width | Field | Type | Unit / semantics |
|---|---|---|---|---|
| +0 | 4 B | `command_accept_count` | `u32` BE | Lifetime count of accepted TCs |
| +4 | 4 B | `command_reject_count` | `u32` BE | Lifetime count of rejected TCs |
| +8 | 1 B | `app_mode` | `mode_enum8` | 0=INIT, 1=STANDBY, 2=ACTIVE, 3=SAFE, 255=FAULTED |
| +9 | 1 B | `reserved` | `u8` | Must be `0x00` |
| **Total user data** | **10 B** | | | |

**Total packet**: 16 B header + 10 B user = **26 B**. Every packet in this catalog reports `header + user = total`.

#### `PKT-TM-0101-0002` — Orbiter CDH HK

| Attribute | Value |
|---|---|
| APID | `0x101` |
| Function code | `0x0002` |
| Owning app | `orbiter_cdh` |
| Sampling rate | 1 Hz |
| AOS VC | 0 |
| CRC | frame-level (AOS FECF) |

User-data layout:

| Offset | Width | Field | Type | Unit / semantics |
|---|---|---|---|---|
| +0 | 4 B | `boot_count` | `counter_32` | Cold-boot counter |
| +4 | 4 B | `cpu_uptime_s` | `u32` BE | Seconds since last boot |
| +8 | 2 B | `cpu_load_pct_x10` | `u16` BE | CPU load (value ÷ 10 = %); 0–1000 |
| +10 | 2 B | `ram_used_kb` | `u16` BE | RAM in use (KiB) |
| +12 | 2 B | `sb_queue_max_fill` | `u16` BE | Peak Software Bus queue depth since last HK |
| +14 | 1 B | `mode` | `mode_enum8` | 0=BOOT, 1=INIT, 2=NOMINAL, 3=LOS-DEGRADED, 4=SAFE, 5=SURVIVAL, 255=FAULTED |
| +15 | 1 B | `reserved` | `u8` | `0x00` |
| **Total user data** | **16 B** | | | |

#### `PKT-TM-0130-0002` — Orbiter Power HK (Voltage / Current / Temperature ensemble)

| Attribute | Value |
|---|---|
| APID | `0x130` |
| Function code | `0x0002` |
| Owning app | `orbiter_power` |
| Sampling rate | 1 Hz (HK); underlying sensor sampling: 10 Hz internal, 1-of-10 downlinked |
| AOS VC | 0 |
| CRC | frame-level (AOS FECF) |

User-data layout — voltage/current/temperature canonical HK:

| Offset | Width | Field | Type | Unit | Nominal |
|---|---|---|---|---|---|
| +0 | 2 B | `bus_voltage_main` | `voltage_mV` | mV | 28 000 ± 500 mV |
| +2 | 2 B | `bus_voltage_aux` | `voltage_mV` | mV | 12 000 ± 300 mV |
| +4 | 2 B | `bus_current_main` | `current_10mA` | 10 mA | ±12 000 (±1.2 A steady) |
| +6 | 2 B | `bus_current_aux` | `current_mA` | mA | ±2 000 (±2 A) |
| +8 | 2 B | `battery_voltage` | `voltage_mV` | mV | 28 000 ± 3 000 mV |
| +10 | 2 B | `battery_current` | `current_10mA` | 10 mA | −50 000 to +30 000 (− = charging) |
| +12 | 2 B | `battery_soc_pct_x10` | `u16` BE | 0.1 % | 0–1000 |
| +14 | 2 B | `pdu_temp` | `temp_cC` | 0.01 °C | 2000–5000 (20–50 °C) |
| +16 | 2 B | `battery_temp` | `temp_cC` | 0.01 °C | 500–3000 (5–30 °C) |
| +18 | 2 B | `solar_array_temp_x` | `temp_cC` | 0.01 °C | −4000–+8000 (−40 to +80 °C) |
| +20 | 1 B | `load_switch_mask` | `bitfield8` | — | Bit 0=PAYLOAD, 1=ADCS-RWA, 2=COMM-TX, 3=COMM-RX, 4=HEATER-1, 5=HEATER-2, 6=RSVD, 7=SAFE-MODE-LATCH |
| +21 | 1 B | `fault_mask` | `bitfield8` | — | Bit 0=OVERVOLT, 1=UNDERVOLT, 2=OVERCURR, 3=OVERTEMP, 4=UNDERTEMP, 5=BATT-DISCONNECT, 6=SOLAR-ARRAY-FAULT, 7=RESERVED |
| +22 | 1 B | `mode` | `mode_enum8` | — | 0=NOMINAL, 1=LOW-POWER, 2=SAFE, 3=SURVIVAL, 255=FAULTED |
| +23 | 1 B | `reserved` | `u8` | — | `0x00` |
| **Total user data** | **24 B** | | | |

#### `PKT-TM-0110-0002` — Orbiter ADCS State

| Attribute | Value |
|---|---|
| APID | `0x110` |
| Function code | `0x0002` |
| Owning app | `orbiter_adcs` |
| Sampling rate | **10 Hz (Fast)** — control-loop rate |
| AOS VC | 0 |
| CRC | frame-level (AOS FECF) |

User-data layout:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0..+15 | 16 B | `attitude_quaternion[4]` | 4× `i32` BE (Q1.30 fixed-point; value ÷ 2³⁰) | unitless, |q|=1 |
| +16..+21 | 6 B | `angular_rate[3]` | 3× `i16` BE (LSB = 0.001 rad/s) | milli-rad/s |
| +22..+27 | 6 B | `wheel_speed[3]` | 3× `i16` BE (LSB = 0.1 RPM) | deci-RPM |
| +28..+33 | 6 B | `wheel_current[3]` | 3× `current_10mA` | 10 mA |
| +34..+39 | 6 B | `wheel_temp[3]` | 3× `temp_cC` | 0.01 °C |
| +40 | 1 B | `mode` | `mode_enum8` | 0=OFF, 1=RATE-DAMP, 2=SUN-POINT, 3=NADIR-POINT, 4=TARGET-TRACK, 5=SAFE, 255=FAULTED |
| +41 | 1 B | `fault_mask` | `bitfield8` | Bit 0=WHEEL-STALL, 1=STAR-TRACKER-LOSS, 2=IMU-FAULT, 3=SATURATION, 4–7=RSVD |
| +42 | 2 B | `reserved` | `u16` BE | `0x0000` |
| **Total user data** | **44 B** | | | |

#### `PKT-TM-0101-0004` — Orbiter Event Log (EVS)

| Attribute | Value |
|---|---|
| APID | `0x101` |
| Function code | `0x0004` (differs from HK `0x0002`) |
| Owning app | `orbiter_cdh` (routes events from all apps) |
| Sampling rate | Event-driven |
| AOS VC | **1** (low-rate reliable) |
| CRC | frame-level (AOS FECF) |

User-data layout (cFE EVS event format, truncated to SAKURA-II conventions):

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 4 B | `event_id` | `u32` BE | Global event ID (app-specific high byte + event code) |
| +4 | 1 B | `event_type` | `enum8` | 1=DEBUG, 2=INFO, 3=ERROR, 4=CRITICAL |
| +5 | 1 B | `source_app_id` | `u8` | cFE app table index of emitter |
| +6 | 2 B | `message_len` | `u16` BE | Length of `message_text` in bytes |
| +8..+8+N−1 | N B | `message_text` | UTF-8, no null terminator | `N = message_len`, max 120 |
| **Total user data** | **8 + N B (max 128 B)** | | | |

Event messages are variable-length. Ground-station decoder validates `message_len ≤ 120` before rendering.

### 4.3 Relay TM Packets

#### `PKT-TM-0200-0002` — Relay HK

| Attribute | Value |
|---|---|
| APID | `0x200` |
| Function code | `0x0002` |
| Owning task | FreeRTOS `relay_hk_task` |
| Sampling rate | 1 Hz |
| AOS VC (cross-link) | 0 |
| CRC | frame-level (AOS cross-link FECF) |

User-data layout:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0 | 8 B | `tai_ns_local` | `u64` BE (not a type in §3; BE on the wire) | nanoseconds since TAI epoch, per [08 §5.2](../architecture/08-timing-and-clocks.md) |
| +8 | 4 B | `uptime_s` | `u32` BE | seconds |
| +12 | 2 B | `bus_voltage` | `voltage_mV` | mV |
| +14 | 2 B | `bus_current` | `current_mA` | mA |
| +16 | 2 B | `processor_temp` | `temp_cC` | 0.01 °C |
| +18 | 2 B | `rx_tx_rf_temp` | `temp_cC` | 0.01 °C |
| +20 | 1 B | `session_count_prox1` | `u8` | Active Proximity-1 sessions (0–N surface assets) |
| +21 | 1 B | `mode` | `mode_enum8` | 0=BOOT, 1=IDLE, 2=NOMINAL, 3=DEGRADED (time-authority lost), 4=SAFE |
| +22 | 1 B | `time_authority_state` | `enum8` | 0=SLAVE-TO-ORBITER, 1=FREE-RUN, 2=SUSPECT |
| +23 | 1 B | `reserved` | `u8` | `0x00` |
| **Total user data** | **24 B** | | | |

### 4.4 Rover (Land) TM Packets

#### `PKT-TM-0300-0002` — Rover-Land HK

| Attribute | Value |
|---|---|
| APID | `0x300` |
| Function code | `0x0002` |
| Owning node | `rover_land/comm_node` LifecycleNode |
| Sampling rate | 1 Hz (HK) |
| Proximity-1 VC | 0 |
| CRC | frame-level (Proximity-1 FECW) |

User-data layout:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0 | 12 B | `pose_xyz_m_x1000[3]` | 3× `i32` BE (mm) | millimeters in local frame |
| +12 | 12 B | `vel_xyz_mmps[3]` | 3× `i32` BE (mm/s) | mm/s |
| +24 | 16 B | `orientation_quaternion[4]` | 4× `i32` BE (Q1.30) | unitless |
| +40 | 2 B | `bus_voltage` | `voltage_mV` | mV |
| +42 | 2 B | `bus_current` | `current_mA` | mA |
| +44 | 2 B | `battery_soc_pct_x10` | `u16` BE | 0.1 % (0–1000) |
| +46 | 2 B | `cpu_temp` | `temp_cC` | 0.01 °C |
| +48 | 2 B | `motor_temp_fwd_left` | `temp_cC` | 0.01 °C |
| +50 | 2 B | `motor_temp_fwd_right` | `temp_cC` | 0.01 °C |
| +52 | 2 B | `motor_temp_aft_left` | `temp_cC` | 0.01 °C |
| +54 | 2 B | `motor_temp_aft_right` | `temp_cC` | 0.01 °C |
| +56 | 1 B | `mode` | `mode_enum8` | 0=IDLE, 1=NAV, 2=TELEOP, 3=SAFE, 4=HIBERNATE |
| +57 | 1 B | `fault_mask` | `bitfield8` | Per-rover flags |
| +58 | 2 B | `reserved` | `u16` BE | `0x0000` |
| **Total user data** | **60 B** | | | |

### 4.5 Rover (Cryobot) TM Packets

The cryobot has the most bandwidth pressure. Two HK variants:

#### `PKT-TM-0400-0002` — Cryobot HK, Nominal

| Attribute | Value |
|---|---|
| APID | `0x400` |
| Function code | `0x0002` |
| Tether mode | nominal (10 Mbps) |
| Secondary header | full 10 bytes |
| Sampling rate | 1 Hz |
| CRC | frame-level (tether CRC-16/CCITT-FALSE, [07 §6](../architecture/07-comms-stack.md)) |

User-data layout:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0 | 4 B | `depth_m_x100` | `i32` BE (0.01 m) | centi-meters below surface |
| +4 | 4 B | `descent_rate_mmps` | `i32` BE | mm/s (signed: − = ascending) |
| +8 | 2 B | `drill_rpm` | `i16` BE | RPM |
| +10 | 2 B | `drill_current` | `current_10mA` | 10 mA |
| +12 | 2 B | `drill_torque_Nm_x100` | `i16` BE | centi-Newton-meters |
| +14 | 2 B | `borehole_pressure` | `u16` BE (LSB = 100 Pa) | 100-Pa units; range 0–6.55 MPa |
| +16 | 2 B | `borehole_temp_dK` | `temp_dK` | 0.1 K (absolute) |
| +18 | 2 B | `chassis_temp_dK` | `temp_dK` | 0.1 K |
| +20 | 2 B | `tether_tension_N_x10` | `u16` BE (LSB = 0.1 N) | deci-Newtons |
| +22 | 2 B | `bus_voltage` | `voltage_mV` | mV |
| +24 | 2 B | `bus_current` | `current_10mA` | 10 mA |
| +26 | 1 B | `mode` | `mode_enum8` | 0=STOW, 1=DESCEND, 2=SCIENCE, 3=ASCEND, 4=SAFE, 5=COMMS-ONLY |
| +27 | 1 B | `fault_mask` | `bitfield8` | Bit 0=DRILL-STALL, 1=OVER-TEMP, 2=TETHER-OVERTENSION, 3=LEAK, 4=POWER-FAULT, 5=COMMS-DEGRADED, 6–7=RSVD |
| **Total user data** | **28 B** | | | |

#### `PKT-TM-0400-0004` — Cryobot HK, BW-Collapse

| Attribute | Value |
|---|---|
| APID | `0x400` |
| Function code | `0x0004` |
| Tether mode | BW-collapse (100 kbps) |
| Secondary header | **reduced 4 bytes** per §1.4 |
| Sampling rate | 1 Hz |
| CRC | frame-level (tether CRC-16/CCITT-FALSE) |

User-data layout (tight — omits non-critical sensors):

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0 | 2 B | `depth_m` | `u16` BE | meters (integer; nominal HK gives full 0.01 m resolution) |
| +2 | 2 B | `drill_rpm` | `i16` BE | RPM |
| +4 | 1 B | `borehole_temp_cK_minus_200` | `u8` | centikelvin − 20000 (so 0–255 maps to 200.00 K – 202.55 K; extreme narrow range for cryogenic science) |
| +5 | 1 B | `mode` | `mode_enum8` | Per §4.5 nominal |
| +6 | 1 B | `fault_mask` | `bitfield8` | Per §4.5 nominal |
| +7 | 1 B | `reserved` | `u8` | `0x00` |
| **Total user data** | **8 B** | | | |

Full packet with 4-byte reduced secondary header + 6-byte primary + 8-byte user = **18 B total** (vs. 44 B for nominal HK). Rationale: under BW-collapse, mission operators only need depth, drill RPM, borehole temperature for trending, plus mode/fault. Other fields are dark until tether returns to nominal.

### 4.6 Subsystem MCU TM Packets

The `mcu_*` packets share a common envelope defined in [ICD-mcu-cfs.md](ICD-mcu-cfs.md) (planned). This catalog reserves the APID block and lists placeholders; the per-MCU body lands when the ICD is authored.

| PKT-ID (placeholder) | APID | MCU | Purpose |
|---|---|---|---|
| `PKT-TM-0280-0002` | `0x280` | `mcu_payload` | Instrument state + thermal |
| `PKT-TM-0290-0002` | `0x290` | `mcu_rwa` | Reaction-wheel assembly detailed telemetry (feeds into `orbiter_adcs` HK) |
| `PKT-TM-02A0-0002` | `0x2A0` | `mcu_eps` | Detailed EPS telemetry (feeds into `orbiter_power` HK) |

## 5. Telecommand (TC) Packet Catalog — Orbiter

Every command is `(APID, function_code)`. Commands listed below are the Phase B minimum set; additional app-internal commands land with their app in Batch B3.

### 5.1 Universal TC Conventions

- **Authentication**: Phase B uses COP-1 sequence-controlled uplink for reliability. Cryptographic authentication is a Phase C concern.
- **Acknowledgment**: Every TC with `function_code & 0x8000 == 0` (i.e. non-safing, non-overrideable) produces a `PKT-TM-<APID>-0001` "TC Ack" packet within 100 ms of CDH receipt. Format below.
- **Safety-interlocked commands** (load switches, safe-mode exit) use `function_code` with bit 15 set — these commands require a pre-arming TC within 10 s (arm-then-fire pattern). Details in [ICD-orbiter-ground.md](ICD-orbiter-ground.md) (planned).

### 5.2 TC Ack Packet (shared)

| Attribute | Value |
|---|---|
| APID | (TC-source APID, e.g. `0x181` for CDH) |
| Function code | `0x0001` |
| Sampling rate | Event-driven (one per executed TC) |
| AOS VC | 1 (event log) |

User-data layout:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 2 B | `ack_for_function_code` | `u16` BE | Function code of the TC being ack'd |
| +2 | 2 B | `ack_for_psc` | `u16` BE | PSC of the TC being ack'd |
| +4 | 1 B | `result` | `enum8` | 0=ACCEPTED, 1=REJECTED-CHECKSUM, 2=REJECTED-RANGE, 3=REJECTED-MODE, 4=REJECTED-AUTH, 5=EXECUTED-OK, 6=EXECUTED-WARNING, 7=EXECUTED-FAILED |
| +5 | 1 B | `reserved` | `u8` | `0x00` |
| +6 | 4 B | `result_detail` | `u32` BE | App-specific detail code (e.g. out-of-range parameter index) |
| **Total user data** | **10 B** | | | |

### 5.3 `PKT-TC-0181-0100` — Orbiter CDH: Set Mode

| Attribute | Value |
|---|---|
| APID | `0x181` |
| Function code | `0x0100` |
| Target app | `orbiter_cdh` |
| Safety-interlocked | no |

User-data:

| Offset | Width | Field | Type | Values |
|---|---|---|---|---|
| +0 | 1 B | `new_mode` | `mode_enum8` | per `PKT-TM-0101-0002` mode enum (0–5, 255) |
| +1 | 1 B | `reason_code` | `u8` | Free-form op reason tag |
| **Total user data** | **2 B** | | | |

### 5.4 `PKT-TC-0181-0200` — Orbiter CDH: Event Filter Set

| Attribute | Value |
|---|---|
| APID | `0x181` |
| Function code | `0x0200` |

User-data:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 1 B | `filter_app_id` | `u8` | 0 = all apps |
| +1 | 1 B | `min_event_type` | `enum8` | 1=DEBUG, 2=INFO, 3=ERROR, 4=CRITICAL |
| +2 | 4 B | `event_mask` | `u32` BE | Bitmask: 1=include event_id, 0=suppress |
| **Total user data** | **6 B** | | | |

### 5.5 `PKT-TC-0182-0100` — Orbiter ADCS: Target Quaternion

| Attribute | Value |
|---|---|
| APID | `0x182` |
| Function code | `0x0100` |
| Target app | `orbiter_adcs` |

User-data:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0..+15 | 16 B | `target_quaternion[4]` | 4× `i32` BE (Q1.30) | unitless |
| +16 | 1 B | `slew_mode` | `enum8` | 0=FASTEST, 1=MINIMUM-POWER, 2=MINIMUM-DISTURBANCE |
| +17 | 1 B | `reserved` | `u8` | `0x00` |
| **Total user data** | **18 B** | | | |

### 5.6 `PKT-TC-0184-8100` — Orbiter Power: Load Switch (safety-interlocked)

| Attribute | Value |
|---|---|
| APID | `0x184` |
| Function code | `0x8100` (high bit set → arm-then-fire required) |
| Target app | `orbiter_power` |
| Safety-interlocked | **yes** — must be preceded by `PKT-TC-0184-8000` "arm" within 10 s |

User-data:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 1 B | `switch_index` | `u8` | Index into `PKT-TM-0130-0002` `load_switch_mask` bit positions (0–7) |
| +1 | 1 B | `new_state` | `bool_u8` | 0=OFF, 1=ON |
| +2 | 2 B | `confirm_magic` | `u16` BE | Must equal `0xC0DE` — guards against bit-flip-driven commanding |
| **Total user data** | **4 B** | | | |

### 5.7 `PKT-TC-0184-8000` — Orbiter Power: Arm Load Switch

Companion to §5.6. Same body layout minus `new_state` (just `switch_index` + `confirm_magic`). Sending arm + fire within 10 s executes the switch; a fire without a valid recent arm is rejected with `REJECTED-AUTH`.

## 6. Telecommand (TC) Packet Catalog — Cryobot

The cryobot is tether-gated and commands are especially safety-critical (drill stall, thermal overrun). Every cryobot TC is ack'd and every safety-interlocked TC uses arm-then-fire.

### 6.1 `PKT-TC-0440-0100` — Cryobot: Set Mode

| Attribute | Value |
|---|---|
| APID | `0x440` |
| Function code | `0x0100` |
| Target | cryobot primary controller |
| Safety-interlocked | partial — transition to `DESCEND` requires arm-then-fire |

User-data:

| Offset | Width | Field | Type | Values |
|---|---|---|---|---|
| +0 | 1 B | `new_mode` | `mode_enum8` | Per §4.5 (0=STOW, 1=DESCEND, 2=SCIENCE, 3=ASCEND, 4=SAFE, 5=COMMS-ONLY) |
| +1 | 1 B | `reserved` | `u8` | `0x00` |
| +2 | 2 B | `confirm_magic` | `u16` BE | `0xCB01` (cryobot) |
| **Total user data** | **4 B** | | | |

### 6.2 `PKT-TC-0440-8200` — Cryobot: Set Drill Target RPM (safety-interlocked)

| Attribute | Value |
|---|---|
| APID | `0x440` |
| Function code | `0x8200` |
| Target | cryobot drill controller |
| Safety-interlocked | yes — requires prior `PKT-TC-0440-8000` arm |

User-data:

| Offset | Width | Field | Type | Unit |
|---|---|---|---|---|
| +0 | 2 B | `target_rpm` | `i16` BE | RPM (0 = stop; negative = reverse, for jam-recovery) |
| +2 | 2 B | `max_current_10mA` | `u16` BE (LSB = 10 mA) | Current trip limit; > 1000 (10 A) forbidden |
| +4 | 2 B | `confirm_magic` | `u16` BE | `0xCB02` |
| +6 | 2 B | `reserved` | `u16` BE | `0x0000` |
| **Total user data** | **8 B** | | | |

### 6.3 `PKT-TC-0440-0300` — Cryobot: Request BW-Collapse HK

On-demand command: operator forces the cryobot into BW-collapse HK mode (e.g. for tether-link testing) without a real BW event.

| Attribute | Value |
|---|---|
| APID | `0x440` |
| Function code | `0x0300` |

User-data:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 1 B | `enable` | `bool_u8` | 0 = return to nominal HK, 1 = force BW-collapse HK |
| +1 | 1 B | `reserved` | `u8` | `0x00` |
| +2 | 2 B | `duration_s` | `u16` BE | 0 = until cleared; else seconds |
| **Total user data** | **4 B** | | | |

### 6.4 `PKT-TC-0440-8000` — Cryobot: Arm (shared)

Companion to all safety-interlocked cryobot TCs. User data is 2 bytes: `confirm_magic = 0xCBA0`. An arm is valid for 10 s; a fire without a valid recent arm is rejected with `REJECTED-AUTH`.

## 7. Sim Injection Packet Catalog (APID 0x500–0x57F)

> **Reservation**: the APID block `0x540`–`0x543` is reserved **exclusively** for the Gazebo → FSW fault-injection bridge per [Q-F2](../standards/decisions-log.md). Any other use in any doc or code path is a bug. Enforcement: the APID block is hardcoded invalid under `CFS_FLIGHT_BUILD` ([07 §8](../architecture/07-comms-stack.md)).

These packets cross the in-process sim→FSW boundary and are not protected by any frame CRC. Per §2.4, they carry a **trailing CRC-16/CCITT-FALSE** in the last 2 bytes of user data, computed over `user_data[0 .. N−3]`.

### 7.1 `PKT-SIM-0540-0001` — Inject: Packet Drop

| Attribute | Value |
|---|---|
| APID | `0x540` |
| Function code | `0x0001` |
| Source | Gazebo container `fault_injector` task |
| Sink | cFS `sim_adapter` app → `CFE_SB_Publish` to subscribing apps |
| Sampling / trigger | Scenario-driven |
| CRC | **payload-level** (trailing CRC-16/CCITT-FALSE) |

User-data:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 1 B | `link_id` | `enum8` | 0=GND-ORBITER, 1=ORBITER-RELAY, 2=RELAY-ROVER, 3=ROVER-CRYOBOT, 4=CFS-MCU |
| +1 | 1 B | `reserved` | `u8` | `0x00` |
| +2 | 2 B | `drop_probability_x10000` | `u16` BE | 0–10000 (0.01 % steps) |
| +4 | 4 B | `duration_ms` | `u32` BE | 0 = until cleared |
| +8 | 2 B | `crc16` | `u16` BE | CRC-16/CCITT-FALSE over bytes [0..7] |
| **Total user data** | **10 B** | | | |

### 7.2 `PKT-SIM-0541-0001` — Inject: Clock Skew

| Attribute | Value |
|---|---|
| APID | `0x541` |
| Function code | `0x0001` |
| CRC | payload-level |

User-data:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 1 B | `asset_class` | `enum8` | 0=ORBITER, 1=RELAY, 2=ROVER-LAND, 3=ROVER-UAV, 4=ROVER-CRYOBOT, 5=MCU |
| +1 | 1 B | `instance_id` | `u8` | 1–255 |
| +2 | 4 B | `offset_ms` | `i32` BE | Additive offset in milliseconds (can be negative) |
| +6 | 4 B | `rate_ppm_x1000` | `i32` BE | Drift rate in parts-per-million × 1000 (so 1 ppm = 1000) |
| +10 | 4 B | `duration_s` | `u32` BE | 0 = until cleared |
| +14 | 2 B | `crc16` | `u16` BE | CRC over [0..13] |
| **Total user data** | **16 B** | | | |

### 7.3 `PKT-SIM-0542-0001` — Inject: Force Safe-Mode

| Attribute | Value |
|---|---|
| APID | `0x542` |
| Function code | `0x0001` |
| CRC | payload-level |

User-data:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 1 B | `asset_class` | `enum8` | Per §7.2 |
| +1 | 1 B | `instance_id` | `u8` | 1–255 |
| +2 | 2 B | `trigger_reason_code` | `u16` BE | Free-form op tag surfaced in the resulting EVS event |
| +4 | 2 B | `crc16` | `u16` BE | CRC over [0..3] |
| **Total user data** | **6 B** | | | |

### 7.4 `PKT-SIM-0543-0001` — Inject: Sensor-Noise Corruption

| Attribute | Value |
|---|---|
| APID | `0x543` |
| Function code | `0x0001` |
| CRC | payload-level |

User-data:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 1 B | `asset_class` | `enum8` | Per §7.2 |
| +1 | 1 B | `instance_id` | `u8` | 1–255 |
| +2 | 2 B | `sensor_index` | `u16` BE | Per-sensor-table index (table in [ICD-sim-fsw.md](ICD-sim-fsw.md), planned) |
| +4 | 1 B | `noise_model` | `enum8` | 0=OFF, 1=GAUSSIAN, 2=UNIFORM, 3=STUCK-AT, 4=BIT-FLIP |
| +5 | 1 B | `reserved` | `u8` | `0x00` |
| +6 | 4 B | `noise_param_1` | `i32` BE | Model-dependent (σ for Gaussian, bit-position for BIT-FLIP, etc.) |
| +10 | 4 B | `noise_param_2` | `i32` BE | Model-dependent (μ for Gaussian, etc.) |
| +14 | 4 B | `duration_ms` | `u32` BE | 0 = until cleared |
| +18 | 2 B | `crc16` | `u16` BE | CRC over [0..17] |
| **Total user data** | **20 B** | | | |

### 7.5 Reservation Summary

| APID | Function code | Purpose | Reserved exclusively |
|---|---|---|---|
| `0x540` | `0x0001` | Packet drop | **Fault-injection bridge** (per Q-F2) |
| `0x541` | `0x0001` | Clock skew | **Fault-injection bridge** |
| `0x542` | `0x0001` | Force safe-mode | **Fault-injection bridge** |
| `0x543` | `0x0001` | Sensor-noise corruption | **Fault-injection bridge** |

Additional APIDs in the sim block (`0x544`–`0x56F` for future fault types; `0x500`–`0x53F` for sensor injection; `0x570`–`0x57F` for FSW→sim diagnostics) are allocated but not catalogued here — their definitions land with [ICD-sim-fsw.md](ICD-sim-fsw.md) (Batch B2).

## 8. Full Sampling-Rate Roster

All catalogued packets, ordered by APID:

| PKT-ID | Class | Rate | Trigger |
|---|---|---|---|
| PKT-TM-0100-0002 | Orbiter HK (sample_app) | 1 Hz | periodic |
| PKT-TM-0101-0002 | Orbiter CDH HK | 1 Hz | periodic |
| PKT-TM-0101-0004 | Orbiter Event Log | aperiodic | event-driven |
| PKT-TM-0110-0002 | Orbiter ADCS State | **10 Hz** | periodic |
| PKT-TM-0130-0002 | Orbiter Power HK | 1 Hz | periodic |
| PKT-TM-0200-0002 | Relay HK | 1 Hz | periodic |
| PKT-TM-0280-0002 | MCU Payload TM | TBR (ICD-mcu-cfs.md) | periodic |
| PKT-TM-0290-0002 | MCU RWA TM | TBR | periodic |
| PKT-TM-02A0-0002 | MCU EPS TM | TBR | periodic |
| PKT-TM-0300-0002 | Rover-Land HK | 1 Hz | periodic |
| PKT-TM-0400-0002 | Cryobot HK (nominal) | 1 Hz | periodic (full 10-B sec-hdr) |
| PKT-TM-0400-0004 | Cryobot HK (BW-collapse) | 1 Hz | periodic (reduced 4-B sec-hdr) |
| PKT-TC-0181-0100 | Orbiter CDH Set Mode | aperiodic | command |
| PKT-TC-0181-0200 | Orbiter CDH Event Filter | aperiodic | command |
| PKT-TC-0182-0100 | Orbiter ADCS Target-Q | aperiodic | command |
| PKT-TC-0184-8000 | Orbiter Power Arm | aperiodic | command (safety) |
| PKT-TC-0184-8100 | Orbiter Power Load-Switch | aperiodic | command (safety, armed) |
| PKT-TC-0440-0100 | Cryobot Set Mode | aperiodic | command |
| PKT-TC-0440-0300 | Cryobot Request BW-Collapse HK | aperiodic | command |
| PKT-TC-0440-8000 | Cryobot Arm | aperiodic | command (safety) |
| PKT-TC-0440-8200 | Cryobot Set Drill RPM | aperiodic | command (safety, armed) |
| PKT-SIM-0540-0001 | Inject: Packet Drop | aperiodic | scenario |
| PKT-SIM-0541-0001 | Inject: Clock Skew | aperiodic | scenario |
| PKT-SIM-0542-0001 | Inject: Force Safe-Mode | aperiodic | scenario |
| PKT-SIM-0543-0001 | Inject: Sensor Noise | aperiodic | scenario |

## 9. Bandwidth Accounting (indicative)

Steady-state Nominal HK only, one orbiter + one relay + one rover-land + one cryobot (nominal tether):

| Source | Packets/s | Bytes/packet (w/ 16-B hdr) | Bit-rate (bps) |
|---|---|---|---|
| PKT-TM-0100-0002 (sample_app) | 1 | 26 | 208 |
| PKT-TM-0101-0002 (CDH HK) | 1 | 32 | 256 |
| PKT-TM-0110-0002 (ADCS) | 10 | 60 | 4800 |
| PKT-TM-0130-0002 (Power) | 1 | 40 | 320 |
| PKT-TM-0200-0002 (Relay HK) | 1 | 40 | 320 |
| PKT-TM-0300-0002 (Rover-Land HK) | 1 | 76 | 608 |
| PKT-TM-0400-0002 (Cryobot HK nominal) | 1 | 44 | 352 |
| **Total steady-state HK** | — | — | **~6.9 kbps before framing overhead** |

At AOS 1024 B frames: ~8 framed kbps. Well below the nominal 1 Mbps downlink budget; the 1 Mbps link has ~125× headroom for events, CFDP, and rate-class upgrades (e.g. EDL ADCS at 100 Hz).

Under cryobot BW-collapse, PKT-TM-0400-0004 replaces PKT-TM-0400-0002 with 18 B/packet = 144 bps (vs. 352 bps nominal) — a 2.4× reduction at the cryobot layer.

## 10. Catalog Governance

- **Adding a packet**: new row here first; new APID entry in [apid-registry.md](apid-registry.md) next; new MID macro in `apps/<app>/fsw/src/<app>_msgids.h` last. The registry linter (planned) enforces this ordering in CI.
- **Changing a field's type, scale, or offset** is an ICD-breaking change and requires a PR that updates every ICD that references the PKT-ID.
- **Removing a packet** requires confirmation that no ICD cites its `PKT-ID` (grep). PKT-IDs are never reused — a removed packet's ID stays allocated-to-historical.
- **Field ordering** is fixed once defined. Later fields may only be *added* at the end of the user-data region and only with a new function_code (sharing APID with the old packet). This lets decoders ignore unknown function codes gracefully.

## 11. What this doc is NOT

- Not an ICD — it says nothing about *which boundary* a packet crosses, *at what rate*, or *via which VC*. That is each ICD's job.
- Not a protocol spec — framing, AOS/Proximity-1, VC arbitration, and CFDP belong in [07-comms-stack.md](../architecture/07-comms-stack.md).
- Not a requirements doc — behavioral requirements (e.g. "the orbiter shall downlink HK at 1 Hz") belong under `mission/requirements/` (Phase C).
- Not a source-code deliverable — the decoder tables generated from this catalog live in `rust/ground_station/src/catalog/` and `apps/*/fsw/src/*_msgids.h`; this catalog is their input, not their container.
