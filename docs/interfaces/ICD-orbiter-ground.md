# ICD — Orbiter ↔ Ground

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Packet bodies: [packet-catalog.md](packet-catalog.md). Protocol stack: [../architecture/07-comms-stack.md](../architecture/07-comms-stack.md). Time: [../architecture/08-timing-and-clocks.md](../architecture/08-timing-and-clocks.md). APID registry: [apid-registry.md](apid-registry.md). Decisions: [../standards/decisions-log.md](../standards/decisions-log.md).

This Interface Control Document fixes the ground ↔ orbiter boundary for SAKURA-II. It covers:
- **Downlink**: AOS Transfer Frames (CCSDS 732.0-B-4), 1 Mbps nominal, four active Virtual Channels + one idle fill VC, strict-priority arbitration.
- **Uplink**: TC Space Data Link Protocol (CCSDS 232.0-B-4), 512 B max TC frame, COP-1 sequence-controlled mode for reliable command delivery, Type-BD expedited mode reserved for time-critical safing.
- **Time correlation**: ground-authored Time Correlation Packets (TCP) at 1 TCP per 60 s while in AOS.
- **File transfer**: CFDP Class 1 (CCSDS 727.0-B-5) on AOS VC 2.

Every packet transiting this boundary inherits its body from [packet-catalog.md](packet-catalog.md). This ICD routes those bodies, sets rates, and defines failure behavior — it does not redefine headers or fields. All multi-byte fields in this ICD are **big-endian** per [Q-C8](../standards/decisions-log.md).

## 1. Physical Layer (simulated)

Phase B runs this boundary in SITL through the `clock_link_model` container; the boundary is above the RF physics layer.

| Property | Value | Source |
|---|---|---|
| Nominal downlink bit rate | 1 Mbps | [07 §3](../architecture/07-comms-stack.md) |
| Nominal uplink bit rate | 64 kbps | project convention; configurable via `UPLINK_BPS` env var on `clock_link_model` |
| Channel coding | Reed-Solomon (255, 223) per CCSDS 131.0-B-4, **simulated** | [deviations.md](../standards/deviations.md) entry #1 |
| Configurable light-time | 4–24 minutes one-way | [08 §6](../architecture/08-timing-and-clocks.md); `LIGHT_TIME_OW_SECONDS` on compose overlay |
| Link state | Binary: `AOS` (acquired) or `LOS` (loss of signal) | `clock_link_model` publishes state on APID `0x601` (ground-internal) |

Link-state transitions are scenario-driven in Phase B. Handoff transitions (e.g. ground-station-to-ground-station) are out of scope — SAKURA-II simulates a single logical ground station per orbiter.

## 2. AOS Downlink Profile

### 2.1 Frame Format (inherited)

AOS Transfer Frame = 1024 bytes fixed (per [Q-C4](../standards/decisions-log.md)). Frame structure is defined in [07 §3](../architecture/07-comms-stack.md); this ICD does not redefine it. Fields repeated here for convenience only:

| Offset | Width | Field | Value |
|---|---|---|---|
| 0 | 2 B | Transfer Frame Version + SCID + VC ID | BE; SCID = 42 ([`_defs/mission_config.h`](../../_defs/mission_config.h)) |
| 2 | 3 B | VC Frame Count | BE, per-VC monotonic |
| 5 | 1 B | Signalling field | replay flag + VC frame count cycle |
| 6 | — | Data Field (M_PDU) | SPP multiplex; ~1012 B usable |
| − 6 | 4 B | Operational Control Field (OCF) | COP-1 CLCW when enabled (TM echo); zero on idle VC |
| − 2 | 2 B | Frame Error Control Field (FECF) | **CRC-16/CCITT-FALSE** over entire frame |

FECF covers the entire 1022 bytes preceding it (everything from Transfer Frame Version through OCF). CRC parameters are in [packet-catalog.md §2.4](packet-catalog.md).

### 2.2 Virtual Channel Allocation (ground downlink)

The canonical VC plan mirrors [apid-registry.md §AOS Virtual Channel](apid-registry.md) and refines it with rate budgets:

| VC ID | Name | APID blocks | Rate class | Priority | Nominal bps |
|---|---|---|---|---|---|
| 0 | Real-time HK | `0x100`–`0x13F` (orbiter HK), `0x200`–`0x23F` (relay HK multiplexed after cross-link decap) | Fast / 1 Hz HK | 2 (Medium-High) | 50 000 |
| 1 | Event log | EVS (`0x101`, function `0x0004`), and any TC-Ack packets (function `0x0001`) | Low-rate, reliable | **1 (Highest)** | 20 000 |
| 2 | CFDP file transfer | CFDP PDUs, one per SPP | Bulk | 3 (Medium) | 800 000 |
| 3 | Rover-forward | `0x300`–`0x45F` (surface-asset TM relayed through orbiter) | Medium | 4 (Low) | 100 000 |
| 63 | Idle / fill | `0x7FF` | Fill | 5 (lowest) | balance to 1 Mbps |

**Arbitration**: strict priority at frame-start. If VC 1 has a pending frame when a new 1024-B slot opens, it wins unconditionally over VC 0/2/3; only when VC 1 is empty does VC 0 get a chance, and so on. VC 63 fills any slot that no higher VC claims.

**Rate budget check**: 50 + 20 + 800 + 100 = 970 kbps scheduled; +30 kbps for VC 63 fill → 1 Mbps total. Headroom on VCs 0 and 1 is deliberate — events and HK may burst without bumping CFDP.

### 2.3 VC Frame Count Cycle

Each VC carries its own 24-bit rolling frame count. Ground-station decoder tracks the last-seen count per VC and emits an `EVENT: AOS-VC-DISCONTINUITY` telemetry record on any gap (frame count not (prev + 1) mod 2²⁴). Gap does not discard following frames — higher-layer CFDP Class 1 is the retry authority (or rather: is explicitly not, since Class 1 has no retry; missed frames produce missed file data and a `missing-file` event; see §5).

### 2.4 Idle Frames and Link Keepalive

VC 63 frames are emitted with SPP APID `0x7FF` (idle packet per CCSDS 133.0-B-2 §4.1.2.7.1) in the M_PDU. The orbiter emits at least 1 idle frame per second even when no higher-VC data exists; ground-station decoder treats idle-frame absence for > 2 s (assumption: light-time + 2 s tolerance) as `LINK_SUSPECT`.

## 3. TC Uplink Profile

### 3.1 Frame Format (TC SDLP per CCSDS 232.0-B-4)

TC frames are variable-size; default maximum is **512 bytes** (per [07 §3](../architecture/07-comms-stack.md)). Smaller frames are legal and used for time-critical single-TC frames.

| Offset | Width | Field | Value |
|---|---|---|---|
| 0 | 2 B | Version + Bypass + Control Cmd + Reserved + SCID | BE; SCID = 42 |
| 2 | 2 B | VC ID + Frame Length | BE; 10-bit length field encodes `total_len − 1` |
| 4 | 1 B | Frame Sequence Number (FSN) | COP-1 sequence; monotonic per VC under Type-AD |
| 5 | — | Data Field (one or more SPPs, no multiplexing) | Up to 505 B payload |
| − 2 | 2 B | FECF | **CRC-16/CCITT-FALSE** |

Type-AD (Acknowledged, Data) is the default. Type-BD (Bypass, Data) is the expedited mode — no FSN check, no COP-1 — reserved for the safing exit TC `PKT-TC-0181-0100 (mode=NOMINAL)` when the vehicle is latched in SAFE with COP-1 queues potentially stalled.

### 3.2 TC Virtual Channel Allocation (uplink)

| VC ID | Name | MAP | Carried packets | Mode |
|---|---|---|---|---|
| 0 | Routine TC | 0 | `PKT-TC-0181-*`, `PKT-TC-0182-*`, `PKT-TC-0183-*`, `PKT-TC-0185-*` | Type-AD (COP-1) |
| 1 | Safety-interlocked TC | 1 | `PKT-TC-0184-8000`, `PKT-TC-0184-8100`, cryobot safety commands forwarded via orbiter routing | Type-AD (COP-1) |
| 7 | Expedited / safing | — | `PKT-TC-0181-0100` with `new_mode = NOMINAL` (safe-mode exit) and `PKT-TC-0184-8200` (emergency-off) | Type-BD |

Cryobot and rover TCs that must traverse the orbiter are routed to VC 0 with the onboard `orbiter_comm` app forwarding to the appropriate downstream link; see [ICD-orbiter-relay.md](ICD-orbiter-relay.md) for the cross-link profile.

### 3.3 COP-1 Parameters

| Parameter | Value |
|---|---|
| Sliding window size (FOP-1) | 15 frames |
| Retransmit timeout (FOP-1 T1) | `2 × (one-way light-time + 5 s)` — scenario-dependent; default 1225 s at 10 min LT |
| Max retransmissions | 3 |
| CLCW report rate (FARM-1) | 1 CLCW per downlink AOS frame via the OCF field on VC 0 |
| Lockout recovery | Ground-initiated `Set V(R)` directive; logged as operational event |

At nominal 10-min light-time, a round-trip is ~20 min; the 1225 s timeout tolerates one retransmit cycle with margin. Under Scale-5 where multiple orbiters share ground comm, each orbiter runs its own independent COP-1 state — no interleaving across SCIDs.

### 3.4 TC Frame Rate Budget

64 kbps uplink ÷ 512 B max = 16 TC frames/s theoretical. Operationally the ground station emits ≤ 2 TC frames/s steady-state (COP-1 is the limiter: no forward progress past the window without CLCW acks). In bulk-command scenarios (e.g. scenario upload) the window is filled in < 1 s; the rest of the transfer time is ack-bound.

## 4. Time Correlation Packet (TCP)

### 4.1 Cadence and Purpose

Per [Q-F4](../standards/decisions-log.md) + [08 §3](../architecture/08-timing-and-clocks.md), the ground station emits **1 TCP per 60 s** while in AOS. Each TCP carries ground's UTC (converted to TAI) and a round-trip delay probe. The orbiter's cFE TIME service uses the probe to correct local TAI against ground TAI, and publishes the corrected SCLK downstream to the relay.

### 4.2 Packet Identity

| Attribute | Value |
|---|---|
| PKT-ID | `PKT-TC-0183-0400` (added to `orbiter_comm` TC block `0x183` — this ICD allocates, pending packet-catalog §5 expansion) |
| APID | `0x183` |
| Function code | `0x0400` |
| VC | TC VC 0 (routine) |
| Rate | 1 per 60 s while `AOS` |

User-data layout:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 7 B | `ground_tai_cuc` | CUC (per [packet-catalog §1.2](packet-catalog.md)) | Ground's current TAI at packet emission |
| +7 | 4 B | `rtt_probe_id` | `u32` BE | Echo token; orbiter replies on next HK cycle with this value |
| +11 | 1 B | `ntp_stratum` | `u8` | Ground host's NTP stratum at emission (0 = unsynced, 1 = primary, ≤ 15 = bounded) |
| +12 | 1 B | `reserved` | `u8` | `0x00` |
| **Total user data** | **13 B** | | | |

### 4.3 RTT Probe Echo

The orbiter echoes `rtt_probe_id` inside the next `PKT-TM-0101-0002` via a new optional trailer **(to be catalogued before Phase C)** or as an event (`PKT-TM-0101-0004` with `event_id = 0x01080001`). Ground computes round-trip by matching `rtt_probe_id` against the emit timestamp locally cached.

## 5. CFDP Class 1 on VC 2

### 5.1 Routing

All CFDP PDUs from orbiter → ground travel on AOS VC 2. One CFDP File Data PDU per SPP; one SPP per AOS frame. At 800 kbps nominal VC 2 rate and 1008 B SPP user data, raw throughput is ~100 KB/s before CFDP overhead.

### 5.2 CFDP Configuration

Inherited verbatim from [07 §5](../architecture/07-comms-stack.md):

| Parameter | Value |
|---|---|
| Segment size | 1024 B user data per File Data PDU |
| Checksum | CRC-32 (IEEE 802.3) per [Q-C2](../standards/decisions-log.md) |
| Direction | Orbiter → ground only (uplink CFDP deferred) |
| Transaction timeout (ground) | 10 × (nominal end-to-end latency) |
| Max concurrent transactions | 16 |
| Class 2 boundary | `CfdpProvider` trait in `rust/ground_station/src/cfdp/` per [Q-C3](../standards/decisions-log.md); Class 2 not active in Phase B |

### 5.3 Missing-File Event

A CFDP Class 1 transaction whose total received byte count at `transaction_timeout` expiry is less than the Metadata PDU's `file_size` generates a `PKT-TM-0101-0004` (event log) with `event_type = ERROR`, `event_id = 0x01050001`, `message_text = "CFDP-CLASS1-MISSING: <transaction_id> received N of M bytes"`. No retransmission is attempted — Class 1 has none.

## 6. End-to-End Latency Budget

Reference scenario SCN-NOM-01 (rover HK arriving at operator UI):

| Hop | Budget | Source |
|---|---|---|
| Rover DDS publish → Rover comm-node SPP | 10 ms | nominal LifecycleNode callback |
| Proximity-1 rover → relay | 50 ms | [ICD-relay-surface.md](ICD-relay-surface.md) (Wave 2b) |
| Cross-link relay → orbiter | 50 ms | [ICD-orbiter-relay.md](ICD-orbiter-relay.md) |
| Orbiter SB routing + AOS framing | 50 ms | |
| AOS 1 Mbps serialization (1024 B frame) | 8.2 ms | 1024 × 8 / 1e6 |
| `clock_link_model` light-time delay | 4–24 min (configurable) | [08 §6](../architecture/08-timing-and-clocks.md) |
| Ground-station AOS decode + SPP decode + UI emit | 50 ms | |
| **One-way total (excluding light-time)** | **~218 ms** | |

The V&V budget test in [V&V-Plan.md](../mission/verification/V&V-Plan.md) asserts `actual_one_way_latency − configured_light_time < 500 ms` for every HK PKT-ID.

## 7. Fault Behavior

Minimum required behaviors; full fault tree lives in [../architecture/09-failure-and-radiation.md](../architecture/09-failure-and-radiation.md).

### 7.1 FECF Mismatch (downlink)

- Action: **discard the frame** at ground-station AOS decoder; emit local `LINK_ERROR` event on APID `0x602` (ground-internal).
- No retransmission attempt — AOS has no ARQ for TM.
- Impact on CFDP Class 1: missing bytes trigger §5.3 missing-file event.

### 7.2 FECF Mismatch (uplink)

- Action: orbiter FARM-1 rejects the frame; CLCW reports `LOCKOUT = 0, WAIT = 0, RETRANSMIT = 1`.
- Ground FOP-1 sees the retransmit flag and replays from last-ack'd FSN.

### 7.3 Link Transition AOS → LOS

- Orbiter: `CFE_TIME` continues in TAI/SCLK free-run; drift budget per [08 §4](../architecture/08-timing-and-clocks.md). No outgoing frames halted; idle VC 63 continues — the `clock_link_model` simply drops TM bytes during LOS.
- Ground: last-received frame timestamp ages; at `AGE > 120 s` the link state transitions to `LOS` and the operator UI surfaces the gap.

### 7.4 Invalid APID on Downlink

Ground-station receives an SPP with an APID outside the registry (e.g. `0x500`–`0x57F` sim-injection APIDs — those are never supposed to cross this boundary). Action: **discard + log**; emit `LINK_ERROR` with `reason_code = 0x0001 (INVALID_APID)`. This is the primary defense if the `CFS_FLIGHT_BUILD` guard ever regresses on a scenario with `fault_injection.enabled = true`.

## 8. Compliance Matrix

Every PKT-ID routed across this boundary, by VC, with rate and inheritance pointer.

### 8.1 Downlink (orbiter → ground)

| PKT-ID | APID | VC | Nominal rate | Inherits body from |
|---|---|---|---|---|
| PKT-TM-0100-0002 | `0x100` | 0 | 1 Hz | [packet-catalog §4.2](packet-catalog.md) |
| PKT-TM-0101-0002 | `0x101` | 0 | 1 Hz | [packet-catalog §4.2](packet-catalog.md) |
| PKT-TM-0101-0004 | `0x101` | 1 | aperiodic | [packet-catalog §4.2](packet-catalog.md) |
| PKT-TM-0110-0002 | `0x110` | 0 | 10 Hz | [packet-catalog §4.2](packet-catalog.md) |
| PKT-TM-0130-0002 | `0x130` | 0 | 1 Hz | [packet-catalog §4.2](packet-catalog.md) |
| PKT-TM-0200-0002 | `0x200` | 0 (relayed) | 1 Hz | [packet-catalog §4.3](packet-catalog.md) |
| PKT-TM-0300-0002 | `0x300` | 3 (rover-forward) | 1 Hz | [packet-catalog §4.4](packet-catalog.md) |
| PKT-TM-0400-0002 | `0x400` | 3 | 1 Hz | [packet-catalog §4.5](packet-catalog.md) |
| PKT-TM-0400-0004 | `0x400` | 3 | 1 Hz | [packet-catalog §4.5](packet-catalog.md) |
| TC-Ack `PKT-TM-*-0001` (various APIDs) | per originator | 1 | event-driven | [packet-catalog §5.2](packet-catalog.md) |
| CFDP PDUs | per CFDP entity | 2 | bulk | [07 §5](../architecture/07-comms-stack.md) |

### 8.2 Uplink (ground → orbiter)

| PKT-ID | APID | VC | Mode | Inherits body from |
|---|---|---|---|---|
| PKT-TC-0181-0100 | `0x181` | 0 (Type-AD) / 7 (Type-BD if safing exit) | COP-1 | [packet-catalog §5.3](packet-catalog.md) |
| PKT-TC-0181-0200 | `0x181` | 0 | COP-1 | [packet-catalog §5.4](packet-catalog.md) |
| PKT-TC-0182-0100 | `0x182` | 0 | COP-1 | [packet-catalog §5.5](packet-catalog.md) |
| PKT-TC-0183-0400 (TCP) | `0x183` | 0 | COP-1 | this ICD §4 |
| PKT-TC-0184-8000 | `0x184` | 1 (safety) | COP-1 | [packet-catalog §5.7](packet-catalog.md) |
| PKT-TC-0184-8100 | `0x184` | 1 (safety, armed) | COP-1 | [packet-catalog §5.6](packet-catalog.md) |
| Rover/cryobot TC passthrough | `0x3*`, `0x4*` | 0 | COP-1 | per respective ICDs (Wave 2b) |

## 9. What this ICD is NOT

- Not a source-code interface — decoder/encoder APIs live in `rust/ground_station/src/` and are generated from [packet-catalog.md](packet-catalog.md).
- Not a protocol spec — AOS/TC SDLP/CFDP are CCSDS standards already summarised in [07-comms-stack.md](../architecture/07-comms-stack.md).
- Not a CM document — the SCID and mission name live in [`_defs/mission_config.h`](../../_defs/mission_config.h); changes there require registry + catalog updates.
- Not an RF link budget — SAKURA-II simulates above the RF layer; real-link budget is a future hardware exercise.
