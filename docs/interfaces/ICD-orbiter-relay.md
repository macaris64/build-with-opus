# ICD — Orbiter ↔ Relay (and Relay ↔ Surface Proximity-1 Endpoint)

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Packet bodies: [packet-catalog.md](packet-catalog.md). Protocol stack: [../architecture/07-comms-stack.md](../architecture/07-comms-stack.md). Time: [../architecture/08-timing-and-clocks.md](../architecture/08-timing-and-clocks.md). Decisions: [../standards/decisions-log.md](../standards/decisions-log.md).

This ICD fixes the relay's two comm boundaries. It is written as a single document because the relay is a **bridge**: packets inbound on one leg are (after forwarding rules) emitted on the other, and the state machines couple.

- **§2 Orbiter ↔ Relay cross-link**: AOS cross-link profile (CCSDS 732.0-B-4, adapted), star topology per [Q-C7](../standards/decisions-log.md).
- **§3 Relay ↔ Surface (Proximity-1 endpoint, relay side)**: CCSDS 211.0-B-6 framing, **1 Hz hailing cadence** and **30 s LOS session-establishment timeout** per [Q-C5](../standards/decisions-log.md).
- **§4 Forwarding & routing**: which packets cross which boundary, how the relay rewrites routing tags.
- **§5 Time handoff**: inline time tag on every frame in both directions; DEGRADED transition at 4 h LOS.

Packet bodies are inherited verbatim from [packet-catalog.md](packet-catalog.md). All multi-byte fields are **big-endian** per [Q-C8](../standards/decisions-log.md). Surface-asset-side concerns (session aggregation, per-asset routing tables, ConOps-level acquisition scenarios) are deferred to [ICD-relay-surface.md](ICD-relay-surface.md) (Wave 2b) and cross-linked throughout this document.

## 1. Topology (Scale-5)

Per [Q-C7](../standards/decisions-log.md), SAKURA-II runs a **star topology**: one relay mediates between N orbiters and M surface assets. Direct inter-orbiter links do not exist in Phase B.

```
             Ground Station
                    │ (AOS, [ICD-orbiter-ground.md])
                    ▼
         ┌──────────────────┐
         │   Orbiter-01     │ ── cross-link (this ICD §2) ──┐
         │   ...            │ ── cross-link ──┐             │
         │   Orbiter-05     │ ── cross-link ──┼─── Relay-01 ┤
         └──────────────────┘                 │             │
                                              │   Proximity-1 (this ICD §3,
                                              │   [ICD-relay-surface.md])
                                              ▼
                                   ┌────────────────────────┐
                                   │ Rover-Land, UAV,       │
                                   │ Cryobot (surface side) │
                                   └────────────────────────┘
```

Scale-5 (5 orbiters) does **not** mean five relay ↔ orbiter links simultaneously active. The primary orbiter during LOS is the one with `time_authority: primary-during-los` in `_defs/mission.yaml` ([10 §7](../architecture/10-scaling-and-config.md)); the other four orbiters are cross-link-passive until the relay hands off. Only one orbiter ↔ relay session is active at a time.

## 2. Orbiter ↔ Relay Cross-Link Profile

### 2.1 Physical Layer (simulated)

| Property | Value |
|---|---|
| Nominal bit rate | 2 Mbps |
| Framing | AOS Transfer Frame 1024 B (same as ground downlink — lets Relay-01 reuse buffer pools) |
| Channel coding | simulated; no per-byte R-S in Phase B |
| Link state | Binary: `CL-AOS` (cross-link acquired) or `CL-LOS` |

The cross-link is always-on in steady-state; `CL-LOS` is exercised only in scenario SCN-OFF-02 (relay occlusion).

### 2.2 Cross-Link Virtual Channels

The cross-link uses its own VC plan distinct from the ground-downlink VC plan. There is no VC 63 idle fill on the cross-link; when no traffic exists, the relay emits a 1 Hz heartbeat frame on VC 0 instead.

| VC ID | Name | Direction | Carries | Priority |
|---|---|---|---|---|
| 0 | Heartbeat + relay HK | relay → orbiter | `PKT-TM-0200-0002` + idle heartbeat | 2 |
| 1 | Orbiter → surface TC | orbiter → relay | Downstream TCs (`0x3xx`, `0x4xx`) for surface assets | 1 (Highest) |
| 2 | Surface-forward TM | relay → orbiter | `0x300`–`0x45F` from surface, aggregated | 3 |
| 3 | CFDP forwarding | bidirectional | File-transfer PDUs originated on surface or orbiter | 4 (Low) |

### 2.3 Frame Header (cross-link adaptation)

The AOS Transfer Frame header is identical to [ICD-orbiter-ground.md §2.1](ICD-orbiter-ground.md); only the SCID/VC mapping differs. Cross-link frames carry the **relay's** SCID (reserved as 43 for Relay-01; allocation lives in [`_defs/mission_config.h`](../../_defs/mission_config.h) when the relay app is authored under Batch B3). Ground never sees cross-link SCIDs because the orbiter's forwarding app rewrites the SCID to 42 before emitting on AOS downlink.

### 2.4 FECF

**CRC-16/CCITT-FALSE** over the entire 1022-byte frame (identical to ground downlink). Parameters per [packet-catalog §2.4](packet-catalog.md). A cross-link FECF miss discards the frame at the receiver (relay or orbiter side) and emits an event — no cross-link ARQ.

### 2.5 Cross-Link Latency Budget

| Hop | Budget |
|---|---|
| Relay-side framer | 10 ms |
| Cross-link serialization (1024 B at 2 Mbps) | 4.1 ms |
| `clock_link_model` inter-asset delay | 10 ms fixed (LEO-to-Mars-relay proxy) |
| Orbiter-side deframer | 10 ms |
| **Total** | **~35 ms** |

## 3. Relay ↔ Surface Proximity-1 Endpoint

### 3.1 Frame Format (inherited)

CCSDS 211.0-B-6 Proximity-1. Frame size = **1024 B** (aligned with AOS per [07 §4](../architecture/07-comms-stack.md) for buffer-pool reuse). This ICD pins Phase B values; CCSDS 211.0-B-6 §3 defines the fields.

| Offset | Width | Field | Value |
|---|---|---|---|
| 0 | 1 B | Attached Sync Marker (ASM) start-of-frame | `0xFAF3` truncated to frame convention |
| 1 | 2 B | Transfer Frame Version + Quality Indicator + Spacecraft ID (Physical Channel ID) | BE |
| 3 | 1 B | Source-Destination Identifier | encodes (relay_id, surface_asset_id) |
| 4 | 2 B | VC ID + Frame Length | BE |
| 6 | — | Data Field (SPPs, M_PDU-style) | up to 1014 B |
| − 2 | 2 B | FECW (Frame Error Control Word) | **CRC-16/CCITT-FALSE** over preceding fields |

### 3.2 Proximity-1 Virtual Channels

| VC ID | Name | Direction | Carries |
|---|---|---|---|
| 0 | Surface → relay TM | surface → relay | `PKT-TM-03xx-*`, `PKT-TM-04xx-*` |
| 1 | Relay → surface TC | relay → surface | `PKT-TC-03xx-*`, `PKT-TC-04xx-*` forwarded from orbiter/ground |
| 2 | Hailing channel | bidirectional | Hailing beacons + session-establishment handshakes (see §3.4) |
| 3 | Expedited safing | bidirectional | Single-frame safing commands (Unreliable mode) |

### 3.3 Session Model

One Proximity-1 **session** per (relay, surface asset) pair. The relay maintains N sessions in parallel — one per active surface asset. Each session has independent VC sequence counts, FECW state, and acquisition history.

Session states — relay side:

```
   ┌──────┐    beacon received from       ┌──────────┐
   │ IDLE ├─────── surface asset ────────▶│  HAILING │
   └──────┘                                └─────┬────┘
      ▲                                          │ 3-way handshake complete
      │                                          ▼
      │                                   ┌──────────┐
      │   no frames from asset for 30 s   │ ACQUIRED │
      ├───────── (§3.5 LOS timeout) ──────┤          │
      │                                   └─────┬────┘
      │                                          │ data exchange (VC 0/1)
      │                                          ▼
      │                                   ┌──────────┐
      │                                   │  ACTIVE  │
      │                                   └─────┬────┘
      │         asset-initiated or              │
      └────── operator-commanded teardown  ─────┘
```

### 3.4 Hailing Cadence (Q-C5)

Per [Q-C5](../standards/decisions-log.md), **the relay emits a hailing beacon at 1 Hz while in state `IDLE` or `HAILING`** for any surface asset it is expecting. Hailing stops the moment the session transitions to `ACQUIRED`; it resumes when the session returns to `IDLE`.

#### 3.4.1 Hailing Beacon Packet

| Attribute | Value |
|---|---|
| PKT-ID (new, allocated by this ICD) | `PKT-PRX-HAIL-0001` |
| Direction | relay → surface (broadcast over VC 2) |
| Frequency | 1 Hz while any expected asset is not `ACQUIRED` |
| CRC | frame-level (Proximity-1 FECW) |

User-data layout:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 1 B | `relay_instance_id` | `u8` | Relay's instance ID (1 for Relay-01) |
| +1 | 1 B | `expected_asset_class_mask` | `bitfield8` | Bit 0=ROVER-LAND, 1=ROVER-UAV, 2=ROVER-CRYOBOT, 3=MCU, 4–7=RSVD |
| +2 | 7 B | `relay_tai_cuc` | CUC | Relay's current TAI at emit |
| +9 | 1 B | `reserved` | `u8` | `0x00` |
| **Total user data** | **10 B** | | | |

#### 3.4.2 Surface-Asset Response

On receipt of a hailing beacon, a surface asset replies with a `PKT-PRX-JOIN-0001` over VC 2 carrying `(asset_class, instance_id, last_session_id, asset_tai_cuc)`. Details and body layout land in [ICD-relay-surface.md](ICD-relay-surface.md) (Wave 2b).

### 3.5 LOS Timeout (Q-C5)

Per [Q-C5](../standards/decisions-log.md), **the session-establishment LOS timeout is 30 seconds**. The relay applies it as follows:

| State transition | Trigger |
|---|---|
| `IDLE` → `HAILING` | Operator command or scenario-driven asset-expected signal |
| `HAILING` → `ACQUIRED` | 3-way handshake (beacon → join → ack) completes within 30 s |
| **`HAILING` → `IDLE` (timeout)** | **No `JOIN` frame received from target asset within 30 s of entering `HAILING`** |
| `ACQUIRED` → `ACTIVE` | First data frame from asset successfully decoded (FECW clean) |
| **`ACTIVE` → `IDLE` (LOS)** | No frame received from asset for 30 consecutive seconds (≥ 30 intervening hailing-opportunity windows) |
| `ACQUIRED/ACTIVE` → `IDLE` | Operator teardown command |

The 30 s window is absolute wall-clock at the relay (`relay_tai_cuc` advances); it does not pause during brief intermittency. Transitions to `IDLE` emit `PKT-TM-0200-0002` at the next HK tick with `mode = DEGRADED` if no sessions remain `ACTIVE`.

### 3.6 Session Teardown Events

| Event | Emitted on | Event ID |
|---|---|---|
| Hailing timeout (`HAILING` → `IDLE`) | relay EVS | `0x02050001` |
| Data LOS (`ACTIVE` → `IDLE`) | relay EVS | `0x02050002` |
| FECW-mismatch spike | relay EVS | `0x02050003` |

EVS events cross the cross-link on VC 0 (alongside relay HK); orbiter forwards to ground on AOS VC 1.

### 3.7 Proximity-1 VC Rate Budget

Per-session bit budget (nominal, single surface asset):

| VC | Share | bps |
|---|---|---|
| 0 (surface TM) | 60 % | ~1.2 Mbps (for 2 Mbps nominal Proximity-1) |
| 1 (relay TC) | 5 % | ~100 kbps |
| 2 (hailing) | 5 % | 1 packet/s × ~30 B framed = ~240 bps — dominant cost is being "in the air" not the payload |
| 3 (expedited) | 30 % | reserved / idle |

Under Scale-5 with 2–3 surface assets, VC 2 emits up to 3 beacon packets per second — one per expected asset — still negligible in the overall budget.

## 4. Forwarding & Routing Rules

### 4.1 Surface TM → Orbiter → Ground

Data path: Proximity-1 VC 0 → relay SB → cross-link VC 2 → orbiter SB → AOS VC 3 (rover-forward).

Relay actions:
1. Decode Proximity-1 frame, verify FECW. On fail → discard + event.
2. Extract SPP from M_PDU. Validate APID is in surface range (`0x300`–`0x45F`) or MCU/cryobot range.
3. **Do not rewrite SPP primary/secondary headers** — the PSC, instance ID, and time tag belong to the originating asset and are needed for ground-side operator attribution.
4. Re-encapsulate SPP into cross-link AOS frame on VC 2.
5. Emit cross-link frame with new VC Frame Count (cross-link VC 2 count, independent of any downstream AOS count).

### 4.2 Orbiter → Surface TC

Data path: AOS uplink VC 0/1 → orbiter SB → cross-link VC 1 → relay SB → Proximity-1 VC 1 → surface asset.

Relay actions:
1. Receive cross-link frame, verify cross-link FECF.
2. Extract SPP, read APID. Use routing table (loaded from `_defs/mission.yaml` at relay boot; schema in [10 §3](../architecture/10-scaling-and-config.md)) to determine target surface asset.
3. Confirm session for target asset is `ACTIVE`. If not, buffer (max 32 SPPs per asset) until `ACTIVE` or drop with `REJECTED-SESSION-DOWN` event.
4. Emit on Proximity-1 VC 1 with fresh Proximity-1 VC frame count for that session.

### 4.3 No Inter-Orbiter Forwarding

Per [Q-C7](../standards/decisions-log.md), the relay refuses to forward traffic between orbiters. A cross-link frame whose source SCID and destination SCID are both in the orbiter range (and not the relay's 43) is discarded with `EVENT: INTER-ORBITER-BLOCKED` (`event_id = 0x02060001`). This is the primary defense against routing bugs introduced when multi-orbiter mesh lands in Phase B+.

## 5. Time Handoff

### 5.1 Inline Time Tag (both boundaries)

Per [08 §3](../architecture/08-timing-and-clocks.md), every cross-link frame and every Proximity-1 frame carries an inline time tag. The time tag is the relay's current TAI at frame emission, encoded as a CUC (7 B) in a fixed position in the M_PDU preamble (not the SPP secondary header — the SPP secondary header's time tag belongs to the SPP producer, not the relay). The frame-level time tag is what the surface asset uses to discipline its clock to the relay.

### 5.2 Degradation

Per [08 §5.2](../architecture/08-timing-and-clocks.md), the relay's `time_task` maintains `tai_ns` via 1 ms tick. If no valid orbiter time tag has been seen for 4 hours (LOS drift threshold), the relay transitions its `time_authority_state` field in `PKT-TM-0200-0002` from `SLAVE-TO-ORBITER` to `FREE-RUN`, and every outgoing Proximity-1 frame sets the low bit (`time_suspect = 1`) of its secondary header's command/telemetry code.

### 5.3 Cross-Link Re-Sync

When cross-link re-acquires after LOS:
1. Orbiter emits its current TAI in the first cross-link frame (VC 0 heartbeat).
2. Relay's `time_task` computes `delta = orbiter_tai - relay_tai` and slews (bounded rate; no step-jump) to absorb the offset.
3. `time_authority_state` reverts to `SLAVE-TO-ORBITER` only when `|delta| < 1 ms` is held for 10 consecutive cross-link frames.

## 6. Fault Behavior

| Fault | Action | Event |
|---|---|---|
| Cross-link FECF mismatch | Discard frame | `0x02070001 CROSS-LINK-FECF-FAIL` |
| Cross-link VC frame-count gap | Accept frame, emit event | `0x02070002 CROSS-LINK-VC-GAP` |
| Proximity-1 FECW mismatch | Discard frame | `0x02070003 PRX-FECW-FAIL` |
| Hailing timeout | Transition `HAILING` → `IDLE` | `0x02050001 PRX-HAIL-TIMEOUT` |
| Data LOS 30 s | Transition `ACTIVE` → `IDLE` | `0x02050002 PRX-DATA-LOS` |
| Inter-orbiter forwarding attempt | Discard frame | `0x02060001 INTER-ORBITER-BLOCKED` |
| Session-down while TC buffering queue full | Drop oldest TC, emit event | `0x02070004 TC-BUFFER-OVERFLOW` |

## 7. Compliance Matrix

### 7.1 Cross-Link (Orbiter ↔ Relay)

| PKT-ID | APID | Cross-link VC | Direction | Inherits from |
|---|---|---|---|---|
| PKT-TM-0200-0002 (Relay HK) | `0x200` | 0 | relay → orbiter | [packet-catalog §4.3](packet-catalog.md) |
| Heartbeat idle (1 Hz) | — | 0 | relay → orbiter | this ICD §2.2 |
| All `0x3xx`–`0x45F` TM | per asset | 2 | relay → orbiter | [packet-catalog §4.4, §4.5](packet-catalog.md) |
| All `0x3xx`–`0x45F` TC | per asset | 1 | orbiter → relay | [packet-catalog §6](packet-catalog.md) |
| CFDP PDUs (surface-origin) | per entity | 3 | bidirectional | [07 §5](../architecture/07-comms-stack.md) |

### 7.2 Proximity-1 (Relay ↔ Surface, relay side)

| PKT-ID | APID / code | Prx-1 VC | Direction | Inherits from |
|---|---|---|---|---|
| PKT-PRX-HAIL-0001 (new) | session layer | 2 | relay → surface | this ICD §3.4 |
| PKT-PRX-JOIN-0001 (forward-ref) | session layer | 2 | surface → relay | [ICD-relay-surface.md](ICD-relay-surface.md) (Wave 2b) |
| PKT-TM-0300-0002 (Rover-Land HK) | `0x300` | 0 | surface → relay | [packet-catalog §4.4](packet-catalog.md) |
| PKT-TM-0400-0002 / 0004 (Cryobot HK) | `0x400` | 0 | surface → relay | [packet-catalog §4.5](packet-catalog.md) |
| PKT-TC-0440-* (Cryobot TCs) | `0x440` | 1 | relay → surface | [packet-catalog §6](packet-catalog.md) |
| PKT-TC-03xx-* (Rover TCs) | `0x380`–`0x3BF`, `0x3C0`–`0x3FF` | 1 | relay → surface | [packet-catalog §5](packet-catalog.md) forward-ref |

## 8. What this ICD is NOT

- Not the full Proximity-1 protocol treatment — CCSDS 211.0-B-6 is the normative source; §3 pins SAKURA-II-specific choices only.
- Not the surface-side state machine — [ICD-relay-surface.md](ICD-relay-surface.md) (Wave 2b) covers per-asset acquisition details, session-aggregation heuristics, and cryobot-specific session behavior.
- Not the orbiter's downstream AOS-to-ground routing — that is [ICD-orbiter-ground.md](ICD-orbiter-ground.md).
- Not an app-design doc — the `orbiter_comm` app that drives the cross-link and the `freertos_relay` firmware that drives Proximity-1 are specified in `architecture/02-smallsat-relay.md` (Batch B3).
