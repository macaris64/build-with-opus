# 07 — Communications Stack

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Bibliography: [../standards/references.md](../standards/references.md). System context: [00-system-of-systems.md](00-system-of-systems.md) §2–3. Timestamp format: [08-timing-and-clocks.md](08-timing-and-clocks.md) §2. APID allocation: [../interfaces/apid-registry.md](../interfaces/apid-registry.md).

This doc fixes the protocol stack at every boundary in SAKURA-II. It is the **upstream input** for every ICD in [`../interfaces/`](../interfaces/) — ICDs pick values from menus defined here; they do not redefine layer choices.

Scope: layers L1 (framing) through L5 (application). L0 (the physics of the radio / tether / bus) is simulated by the `clock_link_model` container and is outside the CCSDS stack proper.

## 1. Layered View (Per Boundary)

Every boundary in [`00-system-of-systems.md`](00-system-of-systems.md) §2 carries the same abstract stack but with different layer choices. The table is the master lookup — ICDs refer here.

| Boundary | L1 Framing | L2 Link | L3 Network | L4 Application | User data |
|---|---|---|---|---|---|
| Ground ↔ Orbiter | CCSDS 131.0-B channel coding (simulated R-S) | **AOS** (CCSDS 732.0-B, VCs 0/1/2) — downlink; **TC SDLP** (CCSDS 232.0-B) — uplink | **SPP** (CCSDS 133.0-B-2) | CFDP Class 1 (CCSDS 727.0-B) *or* direct TM/TC | HK, Events, Files |
| Orbiter ↔ Relay (cross-link) | project-local symbol framing | AOS (cross-link profile) | SPP | Relay-forward PDUs | Forwarded surface traffic |
| Relay ↔ Surface rover | CCSDS 211.0-B Proximity-1 framing | Proximity-1 Data Link | SPP | Direct TM/TC | Rover HK, commands |
| Rover ↔ Cryobot (tether) | project-local optical serial framing | project-local reliable-delivery (sub-CCSDS) | **SPP-in-local-frame** (SPP with possibly-reduced secondary header per §6) | Direct TM/TC | Cryobot HK, drill cmds |
| Orbiter cFS ↔ MCU | SpW character / CAN frame / UART byte | ECSS-E-ST-50-12C (SpW) / ISO 11898 (CAN) / UART | SPP (encapsulated in bus frame) | Direct TM/TC via cFS gateway app | Subsystem TM/TC |
| Gazebo ↔ FSW | in-process / shared-mem | project-local | SPP (APID `0x500`–`0x57F` block) | Sensor-inject + fault-inject | Sensor values, fault flags |

Three load-bearing choices on this page:

1. **SPP is the common network layer at every flight boundary.** The Rust ground station, cFS orbiter, FreeRTOS relay, FreeRTOS MCUs, and ROS 2 rovers all speak SPP at the common waist. Anything that is not SPP at this layer is a deviation and must be recorded.
2. **Cryobot tether is the only boundary with a project-local link layer.** Proximity-1 does not fit the tether's optical-serial physical profile, so we wrap SPP in a local frame. The SPP layer is preserved, so the catalog and registry still apply.
3. **Gazebo-to-FSW uses a sideband APID block.** These packets exist so FSW can accept injected sensor values and fault flags without opening a "sim vs. flight" code path. The APID block (`0x500`–`0x57F`) is flight-mode-gated off in any non-SITL build — see §9.

## 2. Space Packet Protocol (L3) — Fleet-Wide Settings

CCSDS 133.0-B-2. All fields are as specified in the standard; this section pins the SAKURA-II-specific choices.

### Primary header (always 6 bytes)

| Field | Bits | SAKURA-II value |
|---|---|---|
| Packet Version Number | 3 | `0b000` (v1) |
| Packet Type | 1 | `0` = TM, `1` = TC |
| Secondary Header Flag | 1 | `1` (always present — SAKURA-II never omits the time tag) |
| APID | 11 | see [`../interfaces/apid-registry.md`](../interfaces/apid-registry.md) |
| Sequence Flags | 2 | `0b11` (standalone); segmentation not used |
| Packet Sequence Count | 14 | per-APID rolling counter; monotonic within a boot |
| Packet Data Length | 16 | `total_length - 7` per CCSDS 133.0-B-2 convention |

### Secondary header (always 10 bytes)

Per [08-timing-and-clocks.md](08-timing-and-clocks.md) §2:

| Field | Width |
|---|---|
| CUC time tag (CCSDS 301.0-B-4, P-Field `0x2F`) | 7 bytes |
| Command / telemetry code | 2 bytes big-endian |
| Instance ID | 1 byte |

Combined framing overhead: **16 bytes** per Space Packet before user data. Every length / latency budget in SAKURA-II accounts for this.

Per [Q-C8](../standards/decisions-log.md), all multi-byte header fields are **big-endian**, and the single Rust encoder/decoder for both primary and secondary headers is the crate [`rust/ccsds_wire/`](../REPO_MAP.md) (designed in [`06-ground-segment-rust.md`](06-ground-segment-rust.md), Batch B3). No ad-hoc `u32::from_be_bytes` or equivalent lives outside that crate on the Rust side; on the C side the `cfs_bindings` crate provides the FSW-adjacent conversion helpers.

### Packet size limits

| Boundary | Max user data | Rationale |
|---|---|---|
| Ground ↔ Orbiter (AOS VC 0/1) | 1008 B = 1024 B AOS frame − 16 B SPP header | Fits one SPP in one AOS frame — avoids segmentation (§3) |
| Ground ↔ Orbiter (AOS VC 2, CFDP) | 1008 B per SPP (one CFDP PDU per SPP) | Matches 1024 B CFDP segment |
| Orbiter ↔ Relay | 1008 B (same AOS frame size) | |
| Relay ↔ Surface (Proximity-1) | 1008 B in nominal Proximity-1 frames | Matches upstream budget |
| Rover ↔ Cryobot (nominal) | 240 B | Small optical frames; rationale in §6 |
| Rover ↔ Cryobot (BW-collapse) | 80 B | Ditto, tightened |
| cFS ↔ MCU (SpW / CAN / UART) | 256 B | Fits in typical bus-frame budgets |
| Gazebo ↔ FSW | 1008 B | In-process; size is cosmetic |

Segmentation is not used (sequence flags always `0b11`). If a payload exceeds the boundary's max, the producer must split at the application layer (e.g. multiple HK packets) or use CFDP.

## 3. AOS Space Data Link Protocol (L2) — Ground Downlink & Cross-Link

CCSDS 732.0-B-4. Per Q-C4, **AOS Transfer Frame size is 1024 bytes**.

### Frame structure

| Field | Width | Value |
|---|---|---|
| Transfer Frame Version | 2 bits | `0b01` (AOS) |
| Spacecraft ID | 8 bits | `42` (from [`../../_defs/mission_config.h`](../../_defs/mission_config.h)) |
| Virtual Channel ID | 6 bits | see VC table below |
| Virtual Channel Frame Count | 24 bits | per-VC monotonic |
| Signalling field | 8 bits | replay flag, VC frame count cycle |
| Insert zone | — | not used |
| Data Field | variable | M_PDU (multiplex of SPPs) |
| Operational Control Field | 4 bytes | COP-1 CLCW when enabled (TC side); zero-filled on TM |
| Frame Error Control | 2 bytes | CRC-16-CCITT-FALSE |

**Total 1024 B** = 6 B primary header + data field + 4 B OCF + 2 B FECF, giving ~1012 B of data field per frame. The 1008 B SPP maximum leaves 4 B slack for the M_PDU first-header pointer (CCSDS 732.0-B-4 §4.1.4.2).

### Virtual Channel assignment (ground ↔ orbiter)

Mirrors and refines [`../interfaces/apid-registry.md`](../interfaces/apid-registry.md) §AOS:

| VC ID | Name | Direction | APID blocks carried | Rate class |
|---|---|---|---|---|
| 0 | Real-time HK | downlink | `0x100`–`0x13F` | High, best-effort |
| 1 | Event log | downlink | EVS subset (APID `0x100`–`0x10F`, cmd code low bits ≥ `0x8000`) | Low, reliable |
| 2 | CFDP | downlink | CFDP PDUs (APID allocated per CFDP entity) | Bulk |
| 3 | Rover-forward | downlink | `0x300`–`0x45F` (surface-asset TM, relayed through orbiter) | Medium |
| 63 | Idle | downlink | `0x7FF` fill | Fill only |

VC arbitration policy: **strict priority** (VC 1 > VC 0 > VC 2 > VC 3 > VC 63). VCs below priority 1 are never preempted for frame-fill — each frame goes to exactly one VC per the priority order at frame-start.

### Uplink frame format (TC SDLP)

CCSDS 232.0-B-4. TC frames carry SPPs in the ground→orbiter direction; frame size is not fixed at 1024 B since the link-layer segmentation is finer. Default maximum TC frame: **512 B** to tolerate uplink bit-rate limits. COP-1 sequence-controlled mode is enabled for safety-critical TCs; expedited mode (Type-BD) for time-critical mode commands (e.g. safe-mode exit). Mapping table lives in [`../interfaces/ICD-orbiter-ground.md`](../interfaces/ICD-orbiter-ground.md) (planned, Batch B2).

### Channel coding (L1 below AOS)

Simulated Reed-Solomon (255, 223) per CCSDS 131.0-B-4. The `clock_link_model` container enforces R-S bit-error inflation / correction as a configurable effect; it does **not** compute real R-S symbols (SITL performance optimization). The boundary between "simulated bit errors" and "corrected bits seen by AOS" is documented in [`05-simulation-gazebo.md`](05-simulation-gazebo.md) (planned, Batch B3) and in [`../interfaces/ICD-sim-fsw.md`](../interfaces/ICD-sim-fsw.md) (planned, Batch B2).

## 4. Proximity-1 (L2) — Relay ↔ Surface

CCSDS 211.0-B-6. Used at the relay ↔ surface-asset boundary.

### Pinned choices

- **Frame size**: 1024 B (aligned with AOS for easier cross-layer buffer reuse in Relay-01 code).
- **Mode**: Reliable (Sequence-Controlled) for TM/TC; Expedited (Unreliable) reserved for time-critical safing only.
- **Hailing**: follows CCSDS 211.0-B-6 §3.3. Per [Q-C5](../standards/decisions-log.md), **hailing cadence = 1 Hz during acquisition** and **session-establishment LOS timeout = 30 s**. Full acquisition state machine in [`../interfaces/ICD-relay-surface.md`](../interfaces/ICD-relay-surface.md) (Batch B2, planned).
- **Session aggregation**: one Proximity-1 session per (relay, surface asset) pair. Relay-01 maintains N sessions in parallel (one per active surface asset).

## 5. CFDP (L4) — Files

CCSDS 727.0-B-5. Per the authoritative decision **Class 1 now, Class 2 later**.

### Class 1 configuration (active in Phase B)

| Parameter | Value |
|---|---|
| Segment size | 1024 B user data per File Data PDU |
| Checksum | CRC-32 (IEEE 802.3 polynomial) — per Q-C2 |
| Direction | unidirectional (downlink only in MVC) |
| Directives emitted | Metadata PDU + File Data PDUs + EOF PDU |
| Directives consumed (ground) | Metadata + File Data + EOF |
| Transaction timeout (ground) | 10 × (nominal end-to-end latency) = 10 × ~10 min = ~100 min; timeout triggers a logged missing-file event, not an exit — Class 1 has no retransmission |
| Max concurrent transactions (ground) | 16 |

File Data PDUs travel over AOS VC 2 (dedicated bulk VC), one File Data PDU per SPP, one SPP per AOS frame. At a nominal 1 Mbps downlink, throughput is ~100 KB/s raw — orders of magnitude above MVC file rates.

### Class 2 seam (reserved, not implemented)

The Rust ground-station crate exposes:

```rust
pub trait CfdpReceiver {
    fn on_pdu(&mut self, pdu: &[u8]) -> Result<(), CfdpError>;
    fn finalize_transaction(&mut self, id: TransactionId) -> Result<TransactionOutcome, CfdpError>;
}
```

Phase B implements `Class1Receiver: CfdpReceiver`. Class 2 will land as `Class2Receiver: CfdpReceiver` — same trait, additional state machine for NAKs/checkpoints/EOF-Ack. Full design in [`06-ground-segment-rust.md`](06-ground-segment-rust.md) (planned, Batch B3).

Per [Q-C3](../standards/decisions-log.md), the outer **provider** boundary is a separate trait `CfdpProvider` that wraps the `CfdpReceiver` receive path together with the provider-side send path; the boundary lives in `rust/ground_station/src/cfdp/` and wraps the `cfdp-core` crate. Class 1 and (future) Class 2 both implement `CfdpProvider`. The trait body is defined in [`06-ground-segment-rust.md`](06-ground-segment-rust.md).

## 6. Cryobot Tether — Project-Local Link

Per [Q-C1](../standards/decisions-log.md): **10 Mbps nominal, 100 kbps under BW-collapse**, **SPP-in-local-frame**. Per [Q-C9](../standards/decisions-log.md), the link-layer framing is **HDLC-lite** (byte-stuffed with `0x7E` flag + `0x7D` escape) rather than ASM-preamble synchronous framing — the byte-oriented approach suits the bidirectional serial-over-fiber tether and is natively supported by standard serial libraries.

### Framing (HDLC-lite)

| Field | Width | Notes |
|---|---|---|
| Flag (start) | 1 byte | Literal `0x7E` — frame boundary |
| Mode | 1 byte | `0x00` = NOMINAL, `0x01` = BW-COLLAPSE, `0x02` = SYNC, `0x03` = LINK-KEEPALIVE |
| Length | 2 bytes BE | Payload byte length (before stuffing) |
| Payload | ≤ 240 (nominal) or ≤ 80 (collapse) bytes | One SPP, byte-stuffed |
| CRC-16 | 2 bytes BE | **CRC-16/CCITT-FALSE** over `mode + length + unstuffed payload` |
| Flag (end) | 1 byte | Literal `0x7E` |

Byte stuffing: inside the frame body (mode + length + payload + CRC-16), any `0x7E` becomes `0x7D 0x5E` and any `0x7D` becomes `0x7D 0x5D`. The two flag bytes are never stuffed. Full treatment — including mode transitions, sync packet re-anchoring under BW-collapse, and fault-behavior details — lives in [`../interfaces/ICD-cryobot-tether.md`](../interfaces/ICD-cryobot-tether.md).

Total framing overhead: 7 bytes worst-case per SPP (2 flags + mode + 2 length + 2 CRC), plus ≤ 1 % inflation from byte stuffing on typical payloads.

### SPP secondary header under BW-collapse

Per [08-timing-and-clocks.md](08-timing-and-clocks.md) §7, the tether reduces secondary header to 4 bytes during BW-collapse:

| Field | Width |
|---|---|
| Coarse-time delta (from last sync packet) | 2 bytes BE |
| Cmd/TLM code | 1 byte |
| Instance ID | 1 byte |

Full 10-byte secondary header is restored in a **sync packet** emitted at least every N seconds (N bounded by the LOS drift budget from `08` §7; concrete value chosen in `ICD-cryobot-tether.md`). The receiver is stateful: it re-anchors on each sync packet.

### Throughput at BW-collapse

100 kbps → 12.5 KB/s. A BW-collapse HK packet is ~50 B payload + 9 B frame + 4 B reduced SPP header = 63 B → ~200 packets/s headroom in theory, ~20 packets/s realistic after retransmissions and sync packets. Matches ConOps SCN-OFF-01 "short HK summary frames only" expectation.

## 7. cFS ↔ MCU Buses (SpW / CAN / UART)

Per Q-H4 (open for Batch B3), specific chip families are TBR. Bus choices per MCU class are fixed in [`../interfaces/apid-registry.md`](../interfaces/apid-registry.md):

| MCU class | Bus | L1/L2 frame format | L3 |
|---|---|---|---|
| `mcu_payload` | SpaceWire (ECSS-E-ST-50-12C) | Character-based; packet via EOP marker | SPP encapsulated in SpW packet |
| `mcu_rwa` | CAN (ISO 11898) | 11-bit identifier frames, ≤ 8 B each | SPP fragmented across CAN frames; reassembled by cFS gateway |
| `mcu_eps` | UART | HDLC-style byte stuffing | SPP framed with HDLC flags |

Framing details land in [`../interfaces/ICD-mcu-cfs.md`](../interfaces/ICD-mcu-cfs.md) (Batch B2). The cFS-side gateway app pattern is common across all three — see [`03-subsystem-mcus.md`](03-subsystem-mcus.md) (planned, Batch B3).

## 8. Gazebo ↔ FSW Sideband

APID block `0x500`–`0x57F`. Two sub-ranges:

| APID | Purpose |
|---|---|
| `0x500`–`0x53F` | Sensor value injection (`sim → FSW`) |
| `0x540`–`0x56F` | Fault flag injection (`sim → FSW`) — see [`09-failure-and-radiation.md`](09-failure-and-radiation.md) |
| `0x570`–`0x57F` | FSW → sim diagnostics (state echo, confirmation) |

These packets use the same SPP format as flight packets. The key safety property is that the **APID block is hardcoded invalid in a `CFS_FLIGHT_BUILD` preprocessor path** — any flight build (which SAKURA-II Phase B is NOT, but the HPSC target will become) refuses to subscribe to or publish on these APIDs. Enforcement detail lives in [`05-simulation-gazebo.md`](05-simulation-gazebo.md) (Batch B3) and [`../interfaces/ICD-sim-fsw.md`](../interfaces/ICD-sim-fsw.md) (Batch B2).

Per Q-F1, **functional-fault injection is carried on cFE Software Bus messages** on-target — the sim container publishes SB messages via a dedicated injector task, and cFS apps subscribe through normal SB subscription. No special API, no new code path. The Gazebo→FSW SPP packets above are the wire format; the on-target receive side is a cFE SB `CFE_SB_Publish` call by the simulator-side adapter app.

### Minimum fault set (Phase B, from Q-F2)

| Fault | APID (within `0x540`–`0x56F`) | Target | Semantic |
|---|---|---|---|
| Packet drop | `0x540` | Link name | Drop probability + duration |
| Clock skew | `0x541` | Asset ID | Offset + rate (ppm) |
| Force safe-mode | `0x542` | Asset ID | Trigger asset-local safing |
| Sensor-noise corruption | `0x543` | Sensor instance | Noise-model override |

Each is parameterized via SB message payload; full field layout is in [`../interfaces/ICD-sim-fsw.md`](../interfaces/ICD-sim-fsw.md). Expansion beyond this set is addressed in [`09-failure-and-radiation.md §6`](09-failure-and-radiation.md) (scale-up seams).

## 9. End-to-End Paths

### Path A — Rover HK to Operator (SCN-NOM-01, from ConOps)

```
rover DDS topic (1 Hz HK)
  → rover comm LifecycleNode: pack SPP (APID 0x300, code = 0x0001)
  → Proximity-1 frame (relay ← rover)
  → Relay-01 unframe + forward
  → AOS cross-link frame (orbiter ← relay)
  → Orbiter-01 SB: "forward-from-relay" routing
  → AOS VC 3 downlink (ground ← orbiter)
  → clock_link_model: +10 min light-time delay
  → ground_station AOS decoder → SPP decoder → TM pipeline
  → Operator UI update (UTC timestamp on display = TAI decoded − TAI-UTC offset)
```

Total framing overhead: SPP 16 B + AOS 12 B + Proximity-1 overhead + cross-link overhead ≈ 50 B for a 20 B HK payload, i.e. ~3.5× inflation. Budgeted in the V&V link-budget test (Batch B4).

### Path B — Fault Injection to Safing (example)

```
scenario YAML → sim injector adapter → SPP on APID 0x542 → cFE SB
  → asset's safe-mode app subscribes → asset enters safe-mode state
  → EVS event → AOS VC 1 (Event log)
  → ground → Operator alert
```

## 10. Decisions Resolved / Tracked for Batch B2 / B3

Resolved decisions (authoritative row in [`../standards/decisions-log.md`](../standards/decisions-log.md); full treatment lands in the definition-site doc named):

- **[Q-C5](../standards/decisions-log.md) Proximity-1 hailing cadence / LOS timeout** — **resolved**: 1 Hz hailing during acquisition, 30 s LOS session-establishment timeout. Full treatment in [`../interfaces/ICD-relay-surface.md`](../interfaces/ICD-relay-surface.md) (Batch B2, planned).
- **[Q-C7](../standards/decisions-log.md) Inter-orbiter cross-link topology (Scale-5)** — **resolved**: **star topology (relay-mediated)**; no inter-orbiter mesh in Phase B. Full treatment in [`02-smallsat-relay.md`](02-smallsat-relay.md) (Batch B3, planned); scaling implications in [`10-scaling-and-config.md`](10-scaling-and-config.md) §10.
- **[Q-C8](../standards/decisions-log.md) Endianness + conversion locus** — **resolved**: big-endian on the wire (CCSDS-aligned); conversion locus = `cfs_bindings` (FSW-adjacent Rust) + new crate `ccsds_wire` (pure Rust pack/unpack). Full treatment in [`06-ground-segment-rust.md`](06-ground-segment-rust.md) (Batch B3, planned).

Still open:

- **Cryobot sync-packet cadence** — tightens drift budget vs BW cost; lands in [`../interfaces/ICD-cryobot-tether.md`](../interfaces/ICD-cryobot-tether.md) (Batch B2, planned).
- **[Q-H4](../standards/decisions-log.md) MCU bus chip families** — TBR; lands in [`03-subsystem-mcus.md`](03-subsystem-mcus.md) (Batch B3, planned).

## 11. What this doc is NOT

- Not an ICD — it does not specify per-boundary packet inventories (those live in `interfaces/`).
- Not a coding guide — L1/L2/L3/L4 choices here are architecture; implementation rules live in `.claude/rules/*.md`.
- Not an authority on timestamp internals — that is [08-timing-and-clocks.md](08-timing-and-clocks.md).
- Not a physical link budget — RF / optical physics is out of scope; SAKURA-II simulates above the link-physics layer.
