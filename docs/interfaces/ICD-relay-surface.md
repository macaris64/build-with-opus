# ICD — Relay ↔ Surface (Proximity-1, Surface Side)

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Packet bodies: [packet-catalog.md](packet-catalog.md). Protocol stack: [../architecture/07-comms-stack.md](../architecture/07-comms-stack.md). Relay side of this boundary: [ICD-orbiter-relay.md](ICD-orbiter-relay.md). Decisions: [../standards/decisions-log.md](../standards/decisions-log.md).

This ICD fixes the **surface-asset side** of the Proximity-1 boundary — the mirror of [ICD-orbiter-relay.md §3](ICD-orbiter-relay.md). Together the two docs define the complete relay ↔ surface link; this one is the authority for anything that happens **on the surface asset**.

Scope:
- §2 **Surface-asset state machine** (complement to relay state machine).
- §3 **Join / handshake packet bodies** (`PKT-PRX-JOIN-0001` and friends — forward-referenced from ICD-orbiter-relay).
- §4 **Reliable Sequence mode** (CCSDS 211.0-B-6 Sequence-Controlled) — used for critical HK and all TCs.
- §5 **Expedited mode** — time-critical safing commands only.
- §6 **M-File delivery protocol** — surface-originated bulk data (imagery, science drops) over Proximity-1.
- §7 **30 s LOS behavior** under Martian surface conditions (dust, occlusion, terrain shadow).
- §8 **Per-asset specifics** — rover-land, rover-UAV, rover-cryobot-surface-endpoint.

All multi-byte fields **big-endian** per [Q-C8](../standards/decisions-log.md). Frame-level CRC is CCSDS 211.0-B-6 FECW = CRC-16/CCITT-FALSE per [packet-catalog §2.4](packet-catalog.md).

**Out of scope here**: the rover ↔ cryobot tether link is covered in [ICD-cryobot-tether.md](ICD-cryobot-tether.md). A cryobot Proximity-1 endpoint is only exercised when the cryobot is still on the surface before tether deployment — see §8.3.

## 1. Topology & Role Summary

```
               ┌────────────┐  Prx-1 (this ICD)  ┌────────────────────┐
               │  Relay-01  │◀──────────────────▶│  Surface Asset-N   │
               │ (authority)│   FECW CRC-16       │ (rover/UAV/...)    │
               └────────────┘                     └────────────────────┘
```

- **Relay** is the Proximity-1 **authority** (CCSDS 211.0-B-6 terminology): it initiates hailing, grants session, and emits the inline time tag.
- **Surface asset** is the Proximity-1 **user**: it responds to hailing, maintains a single session to the relay, and surrenders to the relay's time authority.

This asymmetry drives the state machines in §2 and the hailing/join handshake in §3.

## 2. Surface-Asset State Machine

Each surface asset maintains exactly **one** Proximity-1 session at any moment. The asset sees fewer states than the relay because only one peer (the relay) exists on its side.

```
   ┌──────────┐   power-on / reset        ┌───────────────┐
   │ UNSYNCED ├──────────────────────────▶│   LISTENING   │
   └──────────┘                           └───────┬───────┘
        ▲                                         │ hail received (§3.1)
        │                                         ▼
        │                                 ┌───────────────┐
        │                                 │   JOINING     │
        │                                 │  (emit JOIN,  │
        │                                 │   await ACK)  │
        │                                 └───────┬───────┘
        │                                         │ JOIN-ACK received
        │                                         │ within 30 s
        │                                         ▼
        │                                 ┌───────────────┐
        │  no inbound frame for           │    ACTIVE     │
        │  30 s (§7) OR teardown ─────────┤               │
        │  from relay                     └───────────────┘
        └──────────────────────────────────────────┘
```

| Event | From state | To state | Notes |
|---|---|---|---|
| Reset / boot | any | `UNSYNCED` | Cold start |
| Radio enabled (per asset-local mode) | `UNSYNCED` | `LISTENING` | Rx open; no Tx yet |
| Valid hail frame decoded (§3.1) | `LISTENING` | `JOINING` | Asset emits `PKT-PRX-JOIN-0001` |
| `JOIN-ACK` from relay within 30 s | `JOINING` | `ACTIVE` | Session established |
| **JOINING → LISTENING (timeout)** | `JOINING` | `LISTENING` | **No `JOIN-ACK` within 30 s** per [Q-C5](../standards/decisions-log.md) |
| **ACTIVE → LISTENING (data LOS)** | `ACTIVE` | `LISTENING` | **No inbound frame for 30 consecutive seconds** per [Q-C5](../standards/decisions-log.md) |
| Relay-initiated teardown | `ACTIVE` | `LISTENING` | `RELEASE` frame from relay (see §3.3) |
| Asset-initiated teardown | `ACTIVE` | `LISTENING` | Asset mode transition (e.g. hibernation) |
| Radio disabled | any | `UNSYNCED` | Asset local decision |

Asset HK (`PKT-TM-0300-0002` for rover-land, etc.) carries the Proximity-1 state in the `mode` / `fault_mask` fields per each asset's row in [packet-catalog §4.4, §4.5](packet-catalog.md).

## 3. Join / Handshake Packets

The relay emits `PKT-PRX-HAIL-0001` (defined in [ICD-orbiter-relay §3.4.1](ICD-orbiter-relay.md)); the surface asset's responses are defined here.

### 3.1 Hail-Receive Semantics (surface side)

On decoding a valid `PKT-PRX-HAIL-0001` (FECW clean, APID correct, asset's class bit set in `expected_asset_class_mask`):

1. Asset snapshots its local TAI and computes `delta = relay_tai_cuc − asset_tai_cuc`.
2. Asset enters `JOINING` state.
3. Asset emits `PKT-PRX-JOIN-0001` (§3.2) on Proximity-1 VC 2 within 100 ms.
4. Asset starts a 30 s timer; if no `PKT-PRX-JOIN-ACK-0001` (§3.3) arrives before expiry, asset drops to `LISTENING`.

If the asset's class bit is NOT set in `expected_asset_class_mask`, the asset stays in `LISTENING` — the hail is for somebody else.

### 3.2 `PKT-PRX-JOIN-0001` (surface → relay)

| Attribute | Value |
|---|---|
| PKT-ID | `PKT-PRX-JOIN-0001` |
| APID | session layer — not from the CCSDS APID registry; carried on Proximity-1 VC 2 with `apid = 0x000` and the session layer identifying via function code |
| Function code | `0x0001` |
| Frequency | once per hail-receive transition |
| CRC | frame-level (Proximity-1 FECW) |

User-data layout (after 16-byte SPP header):

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 1 B | `asset_class` | `enum8` | 0=ORBITER (never sent), 1=RELAY (never sent), 2=ROVER-LAND, 3=ROVER-UAV, 4=ROVER-CRYOBOT-SURFACE, 5=MCU |
| +1 | 1 B | `instance_id` | `u8` | Asset's own instance ID (1–255) |
| +2 | 4 B | `last_session_id` | `u32` BE | ID of the most recent session; 0 if this is first-ever join |
| +6 | 7 B | `asset_tai_cuc` | CUC (per [packet-catalog §1.2](packet-catalog.md)) | Asset's current TAI at emit |
| +13 | 4 B | `asset_capabilities` | `u32` BE | Bitmask: bit 0=HK, 1=FILES, 2=M-FILE, 3=EXPEDITED-SAFING, 4–31=reserved |
| +17 | 1 B | `asset_mode` | `mode_enum8` | Per asset's own mode enum ([packet-catalog §4.4, §4.5](packet-catalog.md)) |
| +18 | 2 B | `reserved` | `u16` BE | `0x0000` |
| **Total user data** | **20 B** | | | |

### 3.3 `PKT-PRX-JOIN-ACK-0001` (relay → surface)

| Attribute | Value |
|---|---|
| PKT-ID | `PKT-PRX-JOIN-ACK-0001` |
| Function code | `0x0002` |
| Emitter | relay, after validating `PKT-PRX-JOIN-0001` |
| CRC | frame-level (Proximity-1 FECW) |

User-data layout:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 4 B | `session_id` | `u32` BE | Newly allocated session ID (relay-monotonic) |
| +4 | 7 B | `relay_tai_cuc` | CUC | Relay's current TAI at ACK emit |
| +11 | 2 B | `prx1_vc_plan` | `u16` BE | Bitmask: bit 0=VC 0 granted, 1=VC 1 granted, 2=VC 2 always on, 3=VC 3 granted, 4=VC 4 (M-File) granted, 5=Reliable Sequence enabled, 6–15=reserved |
| +13 | 1 B | `rekey_interval_s` | `u8` | Seconds between sync-refresh within the session (0 = none) |
| +14 | 2 B | `reserved` | `u16` BE | `0x0000` |
| **Total user data** | **16 B** | | | |

### 3.4 `PKT-PRX-RELEASE-0001` (relay → surface)

Relay emits when operator tears down the session or when the relay is rebooting.

| Attribute | Value |
|---|---|
| PKT-ID | `PKT-PRX-RELEASE-0001` |
| Function code | `0x0003` |

User-data layout:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 4 B | `session_id` | `u32` BE | |
| +4 | 1 B | `reason_code` | `enum8` | 0=OPERATOR-TEARDOWN, 1=RELAY-SHUTDOWN, 2=PROTOCOL-ERROR, 3=ASSET-SILENT-30S, 4=HANDOFF-TO-OTHER-RELAY |
| +5 | 3 B | `reserved` | — | `0x000000` |
| **Total user data** | **8 B** | | | |

## 4. Reliable Sequence Mode (Sequence-Controlled)

CCSDS 211.0-B-6 Sequence-Controlled service. This is the default mode for:
- Critical HK (any HK packet with `mode = SAFE` or `fault_mask != 0`).
- All TCs (every `PKT-TC-*` transiting this boundary).
- M-File (§6) data frames — file delivery must be sequence-complete.

### 4.1 Per-VC Sequence Count

Every Proximity-1 frame on a Sequence-Controlled VC carries a 10-bit Virtual Channel Frame Count. The receiver tracks the last-acknowledged count and acknowledges every frame (or batch) via the inbound direction's OCF-equivalent field (CCSDS 211.0-B-6 §3.4.2 "Frame Directive Field").

### 4.2 Acknowledgment Cadence

| Direction | Ack every N frames | Rationale |
|---|---|---|
| Surface → Relay | 4 | Surface is the bulk-data producer; ack-per-4 saves uplink bandwidth from the relay |
| Relay → Surface | 1 | Commands are rare; ack-per-frame minimizes TC latency |

### 4.3 Retransmission

On detecting a frame-count gap, the receiver sends `NAK(missing_count_range)`. The sender retransmits the missing range.

- **Max retransmissions per frame**: 3.
- **Timeout between NAK and retransmit**: 2 × (nominal one-way Proximity-1 latency) ≈ 200 ms at 2 Mbps nominal.
- On exceeding retransmit count: emit `PKT-TM-<asset>-0004` EVS event with `event_id = 0x0A010001` (`PRX-RELIABLE-RETRY-EXHAUSTED`), and **drop the frame** — Sequence-Controlled in SAKURA-II does not escalate to session teardown.

### 4.4 When Reliable Sequence is NOT Used

| Condition | Mode | Why |
|---|---|---|
| Hailing / join handshake | Expedited (§5) | Pre-session, no state machine to protect |
| Safing commands (`PKT-TC-*-8200`) | Expedited | Time-critical; retransmission latency unacceptable |
| `PKT-TM-0400-0004` (cryobot BW-collapse HK) | Reliable | Critical HK takes priority over BW — link layer is Sequence-Controlled but application layer accepts drops gracefully |
| Idle / heartbeat frames | Expedited | No content to protect |

## 5. Expedited Mode (Unreliable)

Proximity-1 Expedited service bypasses the sequence state machine. Used sparingly — every packet class's mode is fixed in §4 or §9.

Frames in Expedited mode:
- Do not consume a VC frame count sequence.
- Are not acknowledged.
- Are not retransmitted on FECW miss.

Expedited is the mode of last resort: if Sequence-Controlled is wedged (e.g. Reliable retry exhausted and session transitions to `LISTENING`), the asset emits a final Expedited HK summary before dropping to `LISTENING`.

## 6. M-File Delivery Protocol

M-File is SAKURA-II's surface-to-relay **file delivery** mechanism. It is distinct from (and lighter than) CFDP Class 1: M-File operates at the Proximity-1 layer, inside the relay ↔ surface session, and does not cross the cross-link. Once a file lands at the relay, it is pushed onto CFDP Class 1 ([07 §5](../architecture/07-comms-stack.md)) for the relay → orbiter → ground leg.

### 6.1 Use Cases

| Use case | Asset | Typical size | Cadence |
|---|---|---|---|
| Panorama imagery | rover-land | 1–8 MB per pan | command-triggered |
| UAV down-looking mosaic | rover-UAV | 2–16 MB per flight | per flight |
| Science sample spectra | rover-cryobot (surface-staged) | 50 KB – 2 MB | per sample |
| Event-triggered diagnostic bundle | any | 10–500 KB | per event |

### 6.2 Packet Types

M-File uses four packet types, all on new Proximity-1 VC 4 (granted via the `prx1_vc_plan` bitmask in `PKT-PRX-JOIN-ACK-0001`).

| PKT-ID | Direction | Function code | Purpose |
|---|---|---|---|
| `PKT-MFILE-META-0001` | surface → relay | `0x0010` | Start-of-file: name, size, checksum |
| `PKT-MFILE-DATA-0001` | surface → relay | `0x0011` | File data chunk (≤ 992 B user data per frame) |
| `PKT-MFILE-EOF-0001` | surface → relay | `0x0012` | End-of-file: last sequence number, full-file checksum |
| `PKT-MFILE-ACK-0001` | relay → surface | `0x0013` | Transaction ack (success / failure) |

All M-File frames use Reliable Sequence (§4).

### 6.3 `PKT-MFILE-META-0001` Body

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 4 B | `transaction_id` | `u32` BE | Surface-asset-assigned, monotonic per asset |
| +4 | 4 B | `total_size_bytes` | `u32` BE | Full file byte count |
| +8 | 4 B | `total_chunks` | `u32` BE | Total number of `PKT-MFILE-DATA-0001` frames |
| +12 | 4 B | `crc32_full_file` | `u32` BE | CRC-32 (IEEE 802.3) over the entire file payload |
| +16 | 1 B | `file_type` | `enum8` | 0=RAW, 1=PNG, 2=JPEG, 3=FITS, 4=CBOR, 5=TEXT-UTF8, 6–255=reserved |
| +17 | 1 B | `compression` | `enum8` | 0=NONE, 1=GZIP, 2=ZSTD |
| +18 | 1 B | `priority` | `u8` | 0=BULK (default), 1=EVENT, 2=CRITICAL |
| +19 | 1 B | `filename_len` | `u8` | ≤ 64 bytes |
| +20..+83 | up to 64 B | `filename` | UTF-8, no null terminator | |
| **Total user data** | **20 + filename_len B** | | | |

### 6.4 `PKT-MFILE-DATA-0001` Body

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 4 B | `transaction_id` | `u32` BE | Matches META |
| +4 | 4 B | `chunk_index` | `u32` BE | 0-indexed |
| +8 | 2 B | `chunk_len` | `u16` BE | Bytes of payload that follow (≤ 992 to fit in Proximity-1 frame after SPP header + VC framing) |
| +10..+10+L−1 | L B | `payload` | raw | `L = chunk_len` |
| **Total user data** | **10 + chunk_len B** | | | |

### 6.5 `PKT-MFILE-EOF-0001` Body

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 4 B | `transaction_id` | `u32` BE | |
| +4 | 4 B | `last_chunk_index` | `u32` BE | Must equal META's `total_chunks - 1` |
| +8 | 4 B | `crc32_sent` | `u32` BE | Sender's reassembled-file CRC — must match META's `crc32_full_file` |
| +12 | 4 B | `reserved` | `u32` BE | `0x00000000` |
| **Total user data** | **16 B** | | | |

### 6.6 `PKT-MFILE-ACK-0001` Body

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 4 B | `transaction_id` | `u32` BE | |
| +4 | 1 B | `result` | `enum8` | 0=ACCEPTED, 1=REJECTED-CHECKSUM, 2=REJECTED-CHUNK-GAP, 3=REJECTED-SESSION-LOST, 4=REJECTED-STORAGE-FULL |
| +5 | 3 B | `reserved` | — | `0x000000` |
| +8 | 4 B | `bytes_accepted` | `u32` BE | Total bytes the relay successfully received |
| **Total user data** | **12 B** | | | |

### 6.7 Concurrent Transactions

Per surface asset: up to **4 concurrent M-File transactions**. The asset's transaction IDs are monotonic; the relay dedups via `(asset_class, instance_id, transaction_id)`. Exceeding 4 concurrent forces the asset to block new `PKT-MFILE-META-0001` emissions until an earlier transaction is ACKed.

### 6.8 Mid-Transaction Session Loss

On session LOS (§7) mid-file-delivery:
- Relay emits `PKT-MFILE-ACK-0001` with `result = REJECTED-SESSION-LOST` (when session resumes).
- Asset's application layer decides to retry (re-emit META + all DATA frames) or abandon. M-File itself does not auto-resume.

## 7. 30 s LOS Behavior — Surface Environment

Per [Q-C5](../standards/decisions-log.md), a surface asset transitions `ACTIVE → LISTENING` after 30 consecutive seconds with no inbound Proximity-1 frame. Under Martian surface conditions this threshold is exercised by:

| Surface fault scenario | Mechanism | Typical recovery |
|---|---|---|
| Dust-storm attenuation | RF SNR drops below FECW-correctable threshold | Dust settles (seconds to hours) |
| Terrain shadow (rover behind ridge) | LOS geometry blocks the relay | Rover rolls out of shadow |
| UAV high-roll maneuver | UAV attitude steers antenna off-axis | Maneuver completes |
| Relay LEO pass out of zenith window | Relay below horizon | Next pass window |

### 7.1 Asset Behavior on `ACTIVE → LISTENING`

1. Emit final Expedited HK summary (`PKT-TM-<asset>-0002` with `fault_mask` bit `COMMS-DEGRADED` set).
2. Abort in-flight M-File transactions (§6.8).
3. Queue outbound HK packets to a bounded ring buffer (size varies per asset; bounded to prevent memory exhaustion).
4. Re-enter `LISTENING` and await a hail.
5. On re-acquire, the asset's HK buffer is drained in arrival order — ground sees a burst of back-dated HK with `time_suspect` clear (timestamps are from when HK was sampled, not when transmitted).

### 7.2 Relay Behavior on Surface-Side LOS

Already specified in [ICD-orbiter-relay §3.5](ICD-orbiter-relay.md). The relay emits `PKT-TM-0200-0002` with `session_count_prox1` decremented and an EVS event `0x02050002 PRX-DATA-LOS`.

### 7.3 "Short" LOS Tolerance

Single missed hail (≤ 1 s) does not trigger any state change. The 30 s window is measured from the last successfully decoded inbound frame; brief dropouts consume no session state.

## 8. Per-Asset Specifics

### 8.1 Rover-Land (APID block `0x300`–`0x3BF`)

- **HK rate**: 1 Hz (`PKT-TM-0300-0002`).
- **Proximity-1 VCs used**: 0 (HK), 1 (TC), 2 (session), 3 (expedited safing), 4 (M-File).
- **Characteristic surface-LOS events**: terrain shadow, dust-storm attenuation.
- **M-File typical sizes**: pan imagery 1–8 MB.
- **Mobility-driven behavior**: during high-attitude maneuvers (crossing obstacles, egress from lander), rover-land reduces HK VC bandwidth and prioritizes session-keepalive.

### 8.2 Rover-UAV (APID block `0x3C0`–`0x3FF`)

- **HK rate**: 5 Hz during flight, 1 Hz on ground.
- **Proximity-1 VCs used**: 0, 1, 2, 3, 4.
- **Characteristic LOS events**: high-roll maneuvers, altitude nulls, blade-shroud shadowing during takeoff.
- **M-File typical sizes**: down-looking mosaic 2–16 MB per flight; on-board compression (GZIP) mandatory for BW budget reasons.
- **Session priority**: during flight, UAV prioritizes session-keepalive over HK (accepts HK under-sampling to preserve comms).

### 8.3 Rover-Cryobot Surface-Staged (APID block `0x400`–`0x45F`)

Cryobot is a special case. Before tether deployment the cryobot sits on/adjacent to rover-land and uses Proximity-1 directly. After tether deployment the cryobot's Proximity-1 endpoint **goes silent** — all comms route through the rover-land tether (per [ICD-cryobot-tether.md](ICD-cryobot-tether.md)).

Phase transitions:

| Phase | Cryobot Proximity-1 state | Cryobot tether state |
|---|---|---|
| Pre-deploy | `ACTIVE` (one Proximity-1 session to relay) | `DISCONNECTED` |
| Tether handoff | `LISTENING` → `ACTIVE` (tether takes over) | `INITIALIZING` → `NOMINAL` |
| Descent | (Proximity-1 not used) | `NOMINAL` |
| Post-mission ascent | (Proximity-1 not used) | `NOMINAL` |

The transition is **commanded** via `PKT-TC-0440-0500` (new, allocated by this ICD; catalog update below) — no auto-handoff. Failure of the commanded handoff leaves the cryobot on Proximity-1 with a `fault_mask` bit set.

### 8.4 `PKT-TC-0440-0500` — Cryobot: Tether Handoff (new)

| Attribute | Value |
|---|---|
| APID | `0x440` |
| Function code | `0x0500` |
| Safety-interlocked | yes — requires `PKT-TC-0440-8000` arm |

User-data:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 1 B | `to_medium` | `enum8` | 0=PRX-1, 1=TETHER |
| +1 | 1 B | `reserved` | `u8` | `0x00` |
| +2 | 2 B | `confirm_magic` | `u16` BE | `0xCBA3` |
| **Total user data** | **4 B** | | | |

This packet is added to [packet-catalog §6](packet-catalog.md) by a follow-up edit; the definition here is authoritative until the catalog is updated.

## 9. Fault Behavior

| Fault | Detected at | Action | Event ID |
|---|---|---|---|
| Proximity-1 FECW mismatch | surface asset Rx | Discard + event | `0x0A020001` |
| Session-layer `JOIN-ACK` timeout (30 s) | surface asset | `JOINING → LISTENING` | `0x0A020002` |
| Data LOS 30 s | surface asset | `ACTIVE → LISTENING` | `0x0A020003` |
| Reliable retry exhausted | surface asset Tx | Drop frame + event; keep session | `0x0A010001` |
| M-File mid-transaction session loss | surface asset | Abort transaction, application decides retry | `0x0A020004` |
| `RELEASE` with `reason=HANDOFF-TO-OTHER-RELAY` | surface asset | Transition to `LISTENING`, remember `last_session_id`; re-hail welcome from new relay | `0x0A020005` |
| Malformed `JOIN-ACK` (CRC clean but fields invalid) | surface asset | Discard, stay in `JOINING`; second bad ACK → `LISTENING` | `0x0A020006` |

## 10. Compliance Matrix

### 10.1 Session-Layer Packets (this ICD + ICD-orbiter-relay)

| PKT-ID | Originator | Direction | Prx-1 VC | Body defined in |
|---|---|---|---|---|
| PKT-PRX-HAIL-0001 | relay | relay → surface | 2 | [ICD-orbiter-relay §3.4.1](ICD-orbiter-relay.md) |
| PKT-PRX-JOIN-0001 | surface | surface → relay | 2 | this ICD §3.2 |
| PKT-PRX-JOIN-ACK-0001 | relay | relay → surface | 2 | this ICD §3.3 |
| PKT-PRX-RELEASE-0001 | relay | relay → surface | 2 | this ICD §3.4 |

### 10.2 Data Packets (inherited from packet-catalog)

| PKT-ID | APID | Prx-1 VC | Mode | Inherits from |
|---|---|---|---|---|
| PKT-TM-0300-0002 (Rover-Land HK) | `0x300` | 0 | Reliable | [packet-catalog §4.4](packet-catalog.md) |
| PKT-TM-0400-0002 (Cryobot HK nominal, pre-tether) | `0x400` | 0 | Reliable | [packet-catalog §4.5](packet-catalog.md) |
| PKT-TM-0400-0004 (Cryobot HK BW-collapse, pre-tether) | `0x400` | 0 | Reliable | [packet-catalog §4.5](packet-catalog.md) |
| PKT-TC-0440-0100 (Cryobot Set Mode) | `0x440` | 1 | Reliable | [packet-catalog §6.1](packet-catalog.md) |
| PKT-TC-0440-0300 (Cryobot BW-Collapse HK req) | `0x440` | 1 | Reliable | [packet-catalog §6.3](packet-catalog.md) |
| PKT-TC-0440-0500 (Tether Handoff, new) | `0x440` | 1 | Reliable | this ICD §8.4 |
| PKT-TC-0440-8200 (Cryobot Drill RPM, safety) | `0x440` | 1 | Reliable | [packet-catalog §6.2](packet-catalog.md) |
| Safing escape | per asset | 3 | Expedited | [packet-catalog §5](packet-catalog.md) |

### 10.3 M-File Packets (this ICD)

| PKT-ID | Direction | Prx-1 VC | Mode | Body defined in |
|---|---|---|---|---|
| PKT-MFILE-META-0001 | surface → relay | 4 | Reliable | §6.3 |
| PKT-MFILE-DATA-0001 | surface → relay | 4 | Reliable | §6.4 |
| PKT-MFILE-EOF-0001 | surface → relay | 4 | Reliable | §6.5 |
| PKT-MFILE-ACK-0001 | relay → surface | 4 | Reliable | §6.6 |

### 10.4 Decisions Honored

| Decision | How this ICD honors it |
|---|---|
| [Q-C5](../standards/decisions-log.md) Proximity-1 hailing + 30 s LOS | §2 state machine transitions + §7 surface-specific LOS |
| [Q-C7](../standards/decisions-log.md) Star topology | Each asset has **exactly one** active session; §3.4 `HANDOFF-TO-OTHER-RELAY` reason exists but is not exercised in Phase B |
| [Q-C8](../standards/decisions-log.md) Big-endian | All multi-byte fields in §3, §6, §8 are BE |

## 11. What this ICD is NOT

- Not the relay-side Proximity-1 treatment — that is [ICD-orbiter-relay §3](ICD-orbiter-relay.md).
- Not the rover ↔ cryobot tether protocol — that is [ICD-cryobot-tether.md](ICD-cryobot-tether.md).
- Not CFDP-on-Proximity-1 — Phase B uses M-File for surface-originated files; CFDP Class 1 runs only on AOS per [07 §5](../architecture/07-comms-stack.md).
- Not a surface-asset lifecycle / ConOps doc — those live under `mission/conops/` (Phase A foundation + Batch B5 expansion).
