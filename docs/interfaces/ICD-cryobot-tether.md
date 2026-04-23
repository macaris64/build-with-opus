# ICD — Cryobot Tether (HDLC-lite Physical/Link Layer)

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Packet bodies: [packet-catalog.md](packet-catalog.md). Protocol stack: [../architecture/07-comms-stack.md](../architecture/07-comms-stack.md). Timing: [../architecture/08-timing-and-clocks.md](../architecture/08-timing-and-clocks.md). Decisions: [../standards/decisions-log.md](../standards/decisions-log.md). Upstream sibling: [ICD-relay-surface.md](ICD-relay-surface.md).

This ICD fixes the **physical and link layer** of the tether between a surface rover (typically rover-land) and the subsurface cryobot. The tether is project-local (not CCSDS) and is the only link in SAKURA-II that carries CCSDS Space Packets inside a non-CCSDS frame.

Scope:
- §2 **Physical layer** — optical serial electrical characteristics (simulated).
- §3 **Link-layer framing** — **HDLC-lite** byte-stuffed framing per [Q-C9](../standards/decisions-log.md).
- §4 **Operating modes** — **Nominal** (10 Mbps) and **BW-Collapse** (100 kbps).
- §5 **BW-Collapse transition** — triggers, timing, recovery.
- §6 **Time sync & sync-packet re-anchoring** — required for reduced-secondary-header recovery.
- §7 **Per-packet routing** — which PKT-IDs use which mode and framing.
- §8 **Fault behavior** — tether-specific failures and recovery.

All multi-byte CCSDS fields inside the payload are **big-endian** per [Q-C8](../standards/decisions-log.md). The HDLC-lite framing byte layer is byte-oriented and endianness-neutral — but the CRC-16/CCITT-FALSE trailer bytes are emitted MSB-first (big-endian) on the wire, deliberately matching CCSDS convention and differing from traditional HDLC's LSB-first CRC emission.

**Normative**: the HDLC-lite framing here supersedes the ASM-preamble framing that appeared in early drafts of [07 §6](../architecture/07-comms-stack.md). See [Q-C9](../standards/decisions-log.md).

## 1. Context

The cryobot is a subsurface drilling platform that deploys down a tethered borehole from a surface rover-land. The tether carries:
- **Power**: separate conductor; not in scope.
- **Data**: bidirectional serial link, project-local framing (this ICD).

Once tether handoff completes (per [ICD-relay-surface §8.3](ICD-relay-surface.md)), all cryobot ↔ ground comms traverse this tether. The cryobot is otherwise Proximity-1-silent.

```
  ┌──────────────────┐        tether (this ICD)      ┌─────────────────┐
  │  Rover-Land      │◀────── HDLC-lite frames ──────▶│    Cryobot     │
  │  (surface)       │      nominal 10 Mbps             │  (subsurface)  │
  │                  │      collapse 100 kbps           │                │
  │  bridges SPP ←→  │                                  │  CCSDS SPP     │
  │  Prx-1 to relay  │                                  │  producer      │
  └──────────────────┘                                  └─────────────────┘
```

## 2. Physical Layer (simulated)

| Property | Value | Notes |
|---|---|---|
| Media | Optical serial fiber (simulated as TCP socket in Phase B SITL) | Real hardware would be a fiber-in-tether link |
| Nominal bit rate | **10 Mbps** | per [Q-C1](../standards/decisions-log.md) |
| BW-collapse bit rate | **100 kbps** | per [Q-C1](../standards/decisions-log.md) |
| Encoding | 8N1 equivalent (byte-stream) | HDLC-lite expects whole bytes |
| Duplex | Full-duplex | Independent Tx/Rx channels |
| Latency | Propagation < 100 µs per 100 m tether; simulated as 1 ms fixed round-trip | Simulation-only; real tether depends on depth |

Phase B does not simulate per-bit BER. The `clock_link_model` container injects whole-frame drops parameterized by mode (per [packet-catalog §7.1](packet-catalog.md) `link_id = ROVER-CRYOBOT`).

## 3. Link-Layer Framing — HDLC-lite (Q-C9)

### 3.1 Frame Structure

Each frame is emitted on the wire as:

```
   ┌──────┬─────────────────────────────────────────┬────────┬──────┐
   │ 0x7E │       escaped frame body                │ CRC-16 │ 0x7E │
   └──────┴─────────────────────────────────────────┴────────┴──────┘
    FLAG      1 B mode + 2 B length BE + payload       2 B     FLAG
```

| Field | Width | Encoding | Notes |
|---|---|---|---|
| Flag (start) | 1 B | Literal `0x7E` | Frame boundary |
| Mode | 1 B | `0x00` = NOMINAL, `0x01` = BW-COLLAPSE, `0x02` = SYNC (see §6), `0x03` = LINK-KEEPALIVE | Byte-stuffed like all body bytes |
| Length | 2 B, big-endian | Payload byte length (≤ 240 nominal, ≤ 80 collapse) | Payload length **before** stuffing |
| Payload | ≤ 240 or ≤ 80 B | One SPP | Byte-stuffed if the SPP contains `0x7E` or `0x7D` |
| CRC-16 | 2 B, big-endian | **CRC-16/CCITT-FALSE** over `mode + length + unstuffed payload` | See §3.3 |
| Flag (end) | 1 B | Literal `0x7E` | Frame boundary (serves as next frame's start flag if back-to-back) |

### 3.2 Byte Stuffing

Inside the frame body (mode + length + payload + CRC-16), any byte value of `0x7E` or `0x7D` is escaped:

```
0x7E  →  0x7D  0x5E   (= 0x7E XOR 0x20)
0x7D  →  0x7D  0x5D   (= 0x7D XOR 0x20)
```

The receiver unescapes the reverse transformation. Byte stuffing inflates the worst-case frame size by at most 2× (all-`0x7E` payload) — in practice inflation is < 1 % for typical SPP content.

The two flag bytes (`0x7E`) surrounding the frame are **not** stuffed — they are the only unescaped `0x7E` bytes on the wire.

### 3.3 CRC-16/CCITT-FALSE

Parameters per [packet-catalog §2.4](packet-catalog.md): poly `0x1021`, init `0xFFFF`, no reflect, no final XOR, test vector `CRC("123456789") = 0x29B1`.

**Coverage**: CRC is computed over the **un-stuffed** bytes `mode + length (2 B) + payload (N B)`. The CRC itself is emitted on the wire in big-endian order (MSB first), then byte-stuffed like any other body byte.

### 3.4 Rationale (Why HDLC-lite, not ASM)

Per [Q-C9](../standards/decisions-log.md):
- **Byte-oriented** serial libraries (POSIX `tty`, `serialport` crate, Python `pyserial`) natively support `0x7E`-flag framing; no bit-level correlator needed.
- **Asymmetric errors**: a corrupted byte in the length field is caught by the CRC — the receiver re-synchronizes on the next `0x7E`, losing at most one frame.
- **Full-duplex friendly**: flag bytes can interleave in either direction without the bit-phase-alignment issues an ASM correlator introduces.

The trade-off — slightly higher worst-case frame overhead from byte stuffing — is acceptable given the ≥ 60× budget headroom at 10 Mbps nominal.

## 4. Operating Modes

The tether has exactly two operating modes for payload content: **Nominal** and **BW-Collapse**. A third `SYNC` frame mode (§6) and a fourth `LINK-KEEPALIVE` mode (§3.1) are also on the wire but carry no payload — they are link-management only.

### 4.1 Nominal Mode (10 Mbps)

| Property | Value |
|---|---|
| Frame `mode` field | `0x00` |
| Max payload | 240 B per frame |
| SPP secondary header | **Full 10 bytes** per [packet-catalog §1.2](packet-catalog.md) |
| Representative HK packet | `PKT-TM-0400-0002` (44 B total with header) |
| Representative TC packet | `PKT-TC-0440-*` (per [packet-catalog §6](packet-catalog.md)) |
| Time-tag source | full CUC in every SPP secondary header |

### 4.2 BW-Collapse Mode (100 kbps)

| Property | Value |
|---|---|
| Frame `mode` field | `0x01` |
| Max payload | 80 B per frame |
| SPP secondary header | **Reduced 4 bytes** per [packet-catalog §1.4](packet-catalog.md) |
| Representative HK packet | `PKT-TM-0400-0004` (8 B user data + 6 B primary + 4 B reduced secondary = 18 B total) |
| Representative TC packet | Only `PKT-TC-0440-0100` (set mode) and `PKT-TC-0440-0300` (BW-collapse HK req) allowed in collapse mode; all safety-interlocked TCs are blocked — reason: confirm-magic bytes + arm-then-fire flow exceeds the 80-B budget |
| Time-tag source | coarse-time delta from last sync packet (§6) + periodic full-sync refresh |

### 4.3 Mode Transition Rules

Mode is **per-direction**: each endpoint can be in a different mode for its outbound direction than for its inbound. In practice the modes stay synchronized because BW-collapse on the downlink (cryobot → rover) is usually triggered by physics (tether-heat attenuation) that affects both directions equally.

| Current outbound mode | Next outbound mode | Trigger | Latency |
|---|---|---|---|
| NOMINAL | BW-COLLAPSE | §5 collapse trigger | ≤ 1 frame cycle (50 ms) |
| BW-COLLAPSE | NOMINAL | §5 restore trigger | ≤ 1 frame cycle; gated on 3 consecutive clean-CRC frames |
| NOMINAL | NOMINAL | (no change) | — |
| BW-COLLAPSE | BW-COLLAPSE | (no change) | — |

The **first frame after a mode transition** must be a `SYNC` frame (§6) to re-anchor the receiver's time-delta state machine.

## 5. BW-Collapse Trigger & Recovery

### 5.1 Trigger Conditions (enter BW-Collapse)

Any one of the following triggers a local NOMINAL → BW-COLLAPSE transition:

| Trigger | Source | Threshold |
|---|---|---|
| Rolling CRC-mismatch rate > 5 % over last 100 frames | Local receiver | Immediate |
| Sustained retransmit-backoff rate > 20 % over last 10 s | Local Tx scheduler | Immediate |
| Operator command `PKT-TC-0440-0300` with `enable = 1` | Explicit | Immediate |
| Tether-electrical fault (simulated as a scenario event via `PKT-SIM-0540-0001` with `link_id = ROVER-CRYOBOT`) | Fault injection | Immediate |

### 5.2 Restore Conditions (exit BW-Collapse)

All of the following must hold simultaneously:
- Rolling CRC-mismatch rate < 0.5 % over last 300 frames.
- No operator-commanded lock (`PKT-TC-0440-0300` with `enable = 1` and nonzero `duration_s` still ticking).
- No active `PKT-SIM-0540-0001` with `link_id = ROVER-CRYOBOT`.

Then the endpoint emits a `SYNC` frame (§6) followed immediately by its first NOMINAL frame.

### 5.3 Hysteresis

To prevent mode thrashing:
- After exiting BW-COLLAPSE, the endpoint is **locked in NOMINAL for at least 10 s** even if the 5 % CRC trigger fires again (unless the trigger is operator-commanded or fault-injected).
- After entering BW-COLLAPSE, the endpoint is **locked in BW-COLLAPSE for at least 5 s** before considering a restore.

### 5.4 Collapse-mode TM/TC Allowlist

| Packet class | Allowed in COLLAPSE? | Notes |
|---|---|---|
| `PKT-TM-0400-0004` (BW-collapse HK) | **Yes, preferred** | Primary HK source |
| `PKT-TM-0400-0002` (nominal HK) | No | Cannot fit in 80-B frame |
| `PKT-TC-0440-0100` (Set Mode) | Yes | Body is 4 B; fits |
| `PKT-TC-0440-0300` (BW-Collapse HK request) | Yes | Body is 4 B; used for operator recovery |
| `PKT-TC-0440-0500` (Tether Handoff) | Yes | Body is 4 B |
| `PKT-TC-0440-8000` (Arm) | No | Part of safety flow; blocked to keep safety flows simple |
| `PKT-TC-0440-8200` (Set Drill RPM, safety) | No | Blocked — safety flows require full arm/fire pattern |
| M-File packets | No | Bulk data cannot afford collapse-mode framing |
| EVS events | **Yes, Expedited** | Critical events bypass the TC allowlist; emitted as best-effort |

A rover-land TC for a safety-interlocked cryobot command sent while the tether is in COLLAPSE is rejected by the rover-land's tether bridge with a `REJECTED-LINK-COLLAPSED` TC-Ack per [packet-catalog §5.2](packet-catalog.md).

## 6. Sync Packet & Time Re-Anchoring

### 6.1 Purpose

In BW-COLLAPSE the SPP secondary header carries a **coarse-time delta** (2 B BE) rather than a full CUC (7 B). The delta is computed against the last received sync anchor. Without periodic sync, the cryobot's derived time eventually slips past the 50 ms / 4 h LOS drift bound from [08 §4](../architecture/08-timing-and-clocks.md).

### 6.2 Sync Frame Format

Sync frames use the HDLC-lite frame with `mode = 0x02`. Payload:

| Offset | Width | Field | Type | Semantics |
|---|---|---|---|---|
| +0 | 7 B | `anchor_tai_cuc` | CUC (per [packet-catalog §1.2](packet-catalog.md)) | Full TAI anchor emitted by the tether master (rover-land side) |
| +7 | 4 B | `anchor_sequence` | `u32` BE | Monotonic sync counter |
| +11 | 1 B | `mode_next` | `enum8` | 0x00 = next data frames are NOMINAL; 0x01 = BW-COLLAPSE |
| +12 | 1 B | `reserved` | `u8` | `0x00` |
| **Total payload** | **13 B** | | | |

Sync frames are **always 13-byte payload** regardless of the data mode that follows. Sync frames count against the collapse-mode 80-B budget but not against the data-frame rate budget (they are rare).

### 6.3 Sync Cadence

| Data mode | Sync interval | Rationale |
|---|---|---|
| NOMINAL | every 60 s | Cheap insurance; preserves mode-flip readiness |
| BW-COLLAPSE | **every 10 s** | Tightest bound that keeps coarse-time delta within 2-byte range (~65 s) with margin |
| Transition (either direction) | 1 sync frame immediately preceding first data frame in new mode | Re-anchors receiver |

The 10-second BW-collapse cadence costs 13 B / 10 s = 1.3 B/s = 10.4 bps — negligible against the 100 kbps link.

### 6.4 Sync Loss

If the cryobot (receiver) misses 3 consecutive scheduled sync frames (i.e. 30 s without an anchor in COLLAPSE), it:
1. Flags `time_suspect = 1` on every outbound packet's secondary-header low bit (per [08 §4](../architecture/08-timing-and-clocks.md)).
2. Emits an event `PKT-TM-0400-0004` with `fault_mask` bit `COMMS-DEGRADED` set.
3. Continues operating on its own TCXO per the cryobot's 0.21 ppm drift allocation (per [08 §4](../architecture/08-timing-and-clocks.md)).

On sync-frame re-acquisition, the cryobot slews (bounded-rate) to absorb the accumulated offset; no step-jump.

## 7. Packet Routing Matrix

### 7.1 Outbound from Cryobot (Cryobot → Rover-Land)

| PKT-ID | Mode | Frame payload budget | Source |
|---|---|---|---|
| `PKT-TM-0400-0002` (nominal HK) | NOMINAL | 44 B in 240-B frame | [packet-catalog §4.5](packet-catalog.md) |
| `PKT-TM-0400-0004` (BW-collapse HK) | BW-COLLAPSE | 18 B in 80-B frame | [packet-catalog §4.5](packet-catalog.md) |
| `PKT-TM-0400-0003` (EVS events) | either | variable; ≤ 240 NOMINAL, ≤ 80 COLLAPSE | [packet-catalog §4.2 EVS format](packet-catalog.md) |
| `PKT-TM-0400-0001` (TC-Ack) | either | 26 B | [packet-catalog §5.2](packet-catalog.md) |

### 7.2 Inbound to Cryobot (Rover-Land → Cryobot)

| PKT-ID | Mode allowlist | Source |
|---|---|---|
| `PKT-TC-0440-0100` (Set Mode) | NOMINAL + COLLAPSE | [packet-catalog §6.1](packet-catalog.md) |
| `PKT-TC-0440-0300` (BW-Collapse HK req) | NOMINAL + COLLAPSE | [packet-catalog §6.3](packet-catalog.md) |
| `PKT-TC-0440-0500` (Tether Handoff) | NOMINAL + COLLAPSE | [ICD-relay-surface §8.4](ICD-relay-surface.md) |
| `PKT-TC-0440-8000` (Arm) | NOMINAL only | [packet-catalog §6.4](packet-catalog.md) |
| `PKT-TC-0440-8200` (Set Drill RPM) | NOMINAL only | [packet-catalog §6.2](packet-catalog.md) |

### 7.3 Sync Frame (bidirectional)

Emitted by rover-land (tether master) on the cadence in §6.3. Sync is never initiated by the cryobot; the cryobot may **request** a sync refresh via an event packet but does not emit sync frames itself.

## 8. Fault Behavior

| Fault | Detected at | Action | Event ID |
|---|---|---|---|
| HDLC-lite frame CRC-16 mismatch | receiver | Discard frame; receiver does not NAK (tether has no ARQ); emit event | `0x0B010001` |
| Malformed byte-stuffing escape sequence | receiver | Discard partial frame; re-sync on next `0x7E` | `0x0B010002` |
| Sustained CRC error rate > 5 % / 100 frames | Tx scheduler | Trigger BW-COLLAPSE (§5.1) | `0x0B020001 COLLAPSE-ENTERED` |
| CRC error rate clean → stable for 300 frames | Tx scheduler | Trigger restore to NOMINAL (§5.2) | `0x0B020002 COLLAPSE-EXITED` |
| Sync frame missed 3× in a row (COLLAPSE) | receiver | Set `time_suspect`; keep operating | `0x0B030001 SYNC-LOST` |
| Safety TC attempted in COLLAPSE | rover-land Tx | Reject TC with `REJECTED-LINK-COLLAPSED`; no frame emitted | `0x0B040001` |
| Tether electrical fault (scenario-driven) | `clock_link_model` | Drop all frames; local endpoints transition to COLLAPSE after §5.1 trigger | external |
| Frame length > 240 (NOMINAL) or > 80 (COLLAPSE) | receiver | Discard frame | `0x0B010003` |

## 9. Latency & Bandwidth Budget

### 9.1 Nominal (10 Mbps)

| Packet | Size on wire (w/ stuffing + flags) | TX time |
|---|---|---|
| `PKT-TM-0400-0002` (44 B SPP + 5 B framer) | ~51 B worst case | ~41 µs |
| `PKT-TC-0440-8200` (24 B SPP + 5 B framer) | ~31 B | ~25 µs |
| Sync frame (13 B payload + 5 B framer) | ~19 B | ~15 µs |
| Round-trip for 1 Hz HK | dominated by 1 ms simulated propagation | ≥ 1 ms |

1 Mbps sustained use fills ~10 % of 10 Mbps nominal; BW headroom for bursty M-File equivalents (see §9.3).

### 9.2 BW-Collapse (100 kbps)

| Packet | Size on wire | TX time |
|---|---|---|
| `PKT-TM-0400-0004` (18 B SPP + 5 B framer) | ~25 B | ~2 ms |
| Sync frame | ~19 B | ~1.5 ms |
| Operator-commanded TCs (Set Mode etc.) | ~15 B | ~1.2 ms |

1 Hz HK at ~2 ms/frame + 0.1 Hz sync at ~1.5 ms/frame + occasional TC → ≤ 3 ms airtime per second out of 10 ms available per second. Headroom adequate.

### 9.3 M-File Policy

M-File (per [ICD-relay-surface §6](ICD-relay-surface.md)) **does not operate over the tether in BW-COLLAPSE**. In NOMINAL, cryobot-originated imagery or science blobs are chunked into SPPs of ≤ 240 B and dribbled through at whatever rate the tether has headroom for — typically ~1 Mbps sustained, yielding ~125 KB/s after framing overhead. Data reaching rover-land is then re-emitted as M-File over Proximity-1.

## 10. Compliance Matrix

### 10.1 Framing (Normative)

| Requirement | Reference |
|---|---|
| `0x7E` flag-delimited frames | §3.1 |
| Byte stuffing of `0x7E`/`0x7D` via `0x7D`-XOR-`0x20` | §3.2 |
| CRC-16/CCITT-FALSE trailer, 2 B BE | §3.3 + [packet-catalog §2.4](packet-catalog.md) |
| 1 B `mode` + 2 B BE length | §3.1 |

### 10.2 Packet Inheritance

| PKT-ID | Source of body definition |
|---|---|
| PKT-TM-0400-0002 | [packet-catalog §4.5](packet-catalog.md) |
| PKT-TM-0400-0004 | [packet-catalog §4.5](packet-catalog.md) |
| PKT-TC-0440-0100 | [packet-catalog §6.1](packet-catalog.md) |
| PKT-TC-0440-0300 | [packet-catalog §6.3](packet-catalog.md) |
| PKT-TC-0440-0500 | [ICD-relay-surface §8.4](ICD-relay-surface.md) |
| PKT-TC-0440-8000 | [packet-catalog §6.4](packet-catalog.md) |
| PKT-TC-0440-8200 | [packet-catalog §6.2](packet-catalog.md) |
| Sync frame payload | this ICD §6.2 |

### 10.3 Decisions Honored

| Decision | Honored by |
|---|---|
| [Q-C1](../standards/decisions-log.md) Tether 10 Mbps / 100 kbps | §2, §4 |
| [Q-C8](../standards/decisions-log.md) Big-endian | All length/CRC/sync multi-byte fields BE |
| [Q-C9](../standards/decisions-log.md) HDLC-lite framing | §3 |
| [Q-F4](../standards/decisions-log.md) 50 ms / 4 h drift | §6 sync cadence + §6.4 sync-loss behavior |

## 11. What this ICD is NOT

- Not an RF/optical link budget — SAKURA-II simulates above the channel physics.
- Not a cryobot application design — the cryobot firmware module specification lives in [`../architecture/04-rovers-spaceros.md`](../architecture/04-rovers-spaceros.md) (Batch B3) and supplementary cryobot-specific notes in a forthcoming section there.
- Not a tether mechanical spec — bend radius, tension limits, and tether unreel mechanics are hardware concerns outside the Phase B doc set.
- Not the relay ↔ surface link — that is [ICD-relay-surface.md](ICD-relay-surface.md).
- Not the tether-power scheme — separate conductor; not carried on this data link.
