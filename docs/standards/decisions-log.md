# Authoritative Decisions Log

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Bibliography: [references.md](references.md). Deviations (distinct): [deviations.md](deviations.md).

This file is the **single registry for every Q-* design decision** in SAKURA-II. Every architecture, interface, and DevEx doc that needs to cite a decision does so by **Q-ID** and links back here; the full treatment lives in the named **definition site** doc.

## Conventions

- **Q-C\***: Comms / protocol stack decisions.
- **Q-F\***: Failure / fault / EDAC decisions.
- **Q-H\***: HPSC / scaling / host-posture decisions.
- **Status**: `resolved` (locked with definition site), `pending` (answer given, definition-site doc not yet written), `open` (answer not yet given).

When a decision is pending, the answer here is authoritative; the definition-site doc will quote this row verbatim when written.

## Registry

| ID | Topic | Answer (authoritative) | Definition site | Status |
|---|---|---|---|---|
| Q-C1 | Cryobot tether bitrate | 10 Mbps nominal; 100 kbps under BW-collapse. | [07-comms-stack.md §6](../architecture/07-comms-stack.md) | resolved |
| Q-C2 | CFDP checksum | CRC-32 (IEEE 802.3 polynomial) for Class 1. | [07-comms-stack.md §5](../architecture/07-comms-stack.md) | resolved |
| Q-C3 | CFDP Class 2 boundary | Use the `cfdp-core` crate with a transport module. Boundary is the `CfdpProvider` trait in `rust/ground_station/src/cfdp/`. Class 1 implements `CfdpProvider` via `Class1Receiver`; Class 2 lands later as an additional implementation against the same trait. | [06-ground-segment-rust.md](../architecture/06-ground-segment-rust.md) (B3, planned) | pending |
| Q-C4 | AOS Transfer Frame size | 1024 bytes. | [07-comms-stack.md §3](../architecture/07-comms-stack.md) | resolved |
| Q-C5 | Proximity-1 hailing cadence / LOS timeout | 1 Hz hailing during acquisition; 30 s session-establishment LOS timeout. | [ICD-orbiter-relay.md §3](../interfaces/ICD-orbiter-relay.md) | resolved |
| Q-C6 | CCSDS secondary header format | Time (CUC 7B) + command/telemetry code (2B BE) + instance ID (1B) — total **10 bytes**. | [08-timing-and-clocks.md §2](../architecture/08-timing-and-clocks.md) | resolved |
| Q-C7 | Scale-5 inter-orbiter topology | **Star topology (relay-mediated).** Direct inter-orbiter mesh is out of scope for Phase B. Simplifies routing tables; Phase B+ seam for mesh preserved in relay forwarding code. | [02-smallsat-relay.md §1](../architecture/02-smallsat-relay.md) | resolved |
| Q-C8 | Endianness + conversion locus | Big-endian on the wire (CCSDS-aligned). Conversion locus: `cfs_bindings` on the FSW-adjacent Rust side + new crate `ccsds_wire` for pure Rust pack/unpack. No ad-hoc BE↔LE conversion anywhere else. | [06-ground-segment-rust.md](../architecture/06-ground-segment-rust.md) (B3, planned) | pending |
| Q-C9 | Cryobot tether link-layer framing | **HDLC-lite**: `0x7E` flag-delimited frames with `0x7D`-XOR-`0x20` byte stuffing; 1-byte `mode` + 2-byte BE length + payload + 2-byte BE CRC-16/CCITT-FALSE trailer. Supersedes ASM-preamble framing from early 07 §6 draft. Rationale: byte-oriented framing suits bidirectional serial-over-fiber with no bit-level correlator; natively supported by POSIX `tty` / Rust `serialport` / Python `pyserial`. | [ICD-cryobot-tether.md §3](../interfaces/ICD-cryobot-tether.md) | resolved |
| Q-C10 | `CcsdsError::SequenceCountOutOfRange` variant | Resolved with option (a): `CcsdsError::SequenceCountOutOfRange(u16)` added as the 9th variant in arch §2.8 during Phase 05. 14-bit ceiling enforced at `SequenceCount::new` boundary; raw rejected value preserved in the payload for logging. | [06-ground-segment-rust.md §2.8](../architecture/06-ground-segment-rust.md) | resolved |
| Q-F1 | Fault-injection transport | Functional-fault injection via cFE Software Bus messages on-target. Sim container publishes SB messages via a dedicated injector task. | [07-comms-stack.md §8](../architecture/07-comms-stack.md) | resolved |
| Q-F2 | Minimum fault set | Packet drop (`0x540`), clock skew (`0x541`), force safe-mode (`0x542`), sensor-noise corruption (`0x543`). | [07-comms-stack.md §8](../architecture/07-comms-stack.md) | resolved |
| Q-F3 | EDAC / memory protection hooks | C-side anchor = `__attribute__((section(".critical_mem")))`. Rust-side anchor = dedicated `Vault<T>` wrapper. EDAC is primarily an FSW C concern; `Vault<T>` provides a parallel abstraction on the ground/relay Rust side for future cross-validation. | [09-failure-and-radiation.md §5](../architecture/09-failure-and-radiation.md) | resolved |
| Q-F4 | Time authority posture + drift budget | Hybrid ladder (ground → orbiter → relay → rover → cryobot). End-to-end LOS drift budget: 50 ms / 4 h (3.47 ppm) with per-asset allocation. | [08-timing-and-clocks.md §3–4](../architecture/08-timing-and-clocks.md) | resolved |
| Q-F6 | Fleet-sync precision | 1 ms end-to-end. CUC fine field carries ~15 µs precision, which exceeds this bound by design. | [08-timing-and-clocks.md §2](../architecture/08-timing-and-clocks.md) | resolved |
| Q-H1 | HPSC execution model | SMP Linux with cFS running as tasks (Linux pthreads under SCHED_FIFO). No per-core AMP partitions. | [10-scaling-and-config.md §4](../architecture/10-scaling-and-config.md) | resolved |
| Q-H2 | Scaling configuration surfaces | Exactly four: Docker compose profiles + `_defs/mission.yaml` + cFS compile-time C headers + ROS 2 launch files. | [10-scaling-and-config.md §1](../architecture/10-scaling-and-config.md) | resolved |
| Q-H4 | MCU bus chip families | Bus class per MCU role is pinned: SpW for `mcu_payload`, CAN 2.0A for `mcu_rwa`, UART/HDLC for `mcu_eps`. Specific silicon parts remain TBR and will be tracked in [deviations.md](deviations.md) when locked. | [03-subsystem-mcus.md §1](../architecture/03-subsystem-mcus.md) | resolved |
| Q-H8 | HPSC cross-compilation | Phase B Docker runs on x86_64 Linux host; HPSC cross-build target deferred. | [build-runbook.md](../dev/build-runbook.md) (B5, planned) | open |

## How To Cite

Inline reference pattern in any other doc:

> *...per [Q-C5](../standards/decisions-log.md), hailing frames at 1 Hz during acquisition...*

Never restate the answer alongside the citation — the citation is the whole quote. If the reading context needs more than one line of recap, that doc is the **definition site** and should carry the full treatment; other docs stay thin.

## Propagation Manifest (non-normative)

A change to any row here requires updating the docs listed under its definition site and all forward-ref sites. The minimum propagation map for the currently pending five answers:

| Row | Forward-ref sites requiring update when row changes |
|---|---|
| Q-C3 | 07 §5.2, deviations.md #3, REPO_MAP.md, packet-catalog.md, ICD-orbiter-ground.md |
| Q-C5 | 07 §4, 08 §3, 02-smallsat-relay.md, ICD-relay-surface.md |
| Q-C7 | 07 §10, 10 §10, ICD-orbiter-relay.md, 02-smallsat-relay.md |
| Q-C8 | 07 §2, 08 §2, REPO_MAP.md, packet-catalog.md, 06-ground-segment-rust.md |
| Q-F3 | 08 §8 (already forward-ref), 09-failure-and-radiation.md, `.claude/rules/{cfs-apps,general}.md` |
| Q-C9 | 07 §6 (updated), ICD-cryobot-tether.md (definition site), packet-catalog.md §1.4 + §4.5 (inherit via forward-ref — no direct framing mention needed) |

## What this doc is NOT

- Not a deviations tracker — [deviations.md](deviations.md) is. A deviation is "we do X where the standard says Y"; a Q-* entry is "we picked choice A from the options the standard allows."
- Not a requirements doc — that tier lives under `mission/requirements/` in Phase C.
- Not a changelog — answer history is in git; this file always shows the current authoritative answer.
