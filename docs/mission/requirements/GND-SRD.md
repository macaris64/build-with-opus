# Ground Segment Sub-System Requirements Document (GND-SRD)

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Parent SRD: [`SRD.md`](SRD.md). Traceability: [`traceability.md`](traceability.md). V&V: [`../verification/V&V-Plan.md`](../verification/V&V-Plan.md). Architecture: [`../../architecture/06-ground-segment-rust.md`](../../architecture/06-ground-segment-rust.md). Coding rules: [`../../../.claude/rules/general.md`](../../../.claude/rules/general.md), [`security.md`](../../../.claude/rules/security.md).

This sub-SRD covers the **ground segment** — the Rust-authored TM/TC pipeline, CFDP Class 1 receiver, operator UI, and APID routing under [`rust/`](../../../rust/). Per [`SRD.md §1`](SRD.md), every requirement here carries a `parent:` link to a `SYS-REQ-####` or declares "derived."

Scope: `rust/ground_station/`, `rust/cfs_bindings/`, future `rust/ccsds_wire/`, future `rust/vault/`. Out of scope: flight software ([`FSW-SRD.md`](FSW-SRD.md)), rover code ([`ROVER-SRD.md`](ROVER-SRD.md)), Gazebo simulation.

## 1. Record format

Same as [`FSW-SRD.md §1`](FSW-SRD.md).

## 2. Crate Boundaries

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| GND-REQ-0001 | The ground segment shall be implemented as a Cargo workspace under [`rust/`](../../../rust/) with at least the following crates: `ground_station`, `cfs_bindings`, `ccsds_wire` (planned), `vault` (planned). | SYS-REQ-0011 | [06](../../architecture/06-ground-segment-rust.md) | I | ALL | `Cargo.toml` inspection |
| GND-REQ-0002 | CCSDS primary + secondary header encode / decode shall be exclusively implemented in `ccsds_wire` (pure Rust) and `cfs_bindings` (FSW-adjacent); no ad-hoc BE/LE conversion shall exist outside these crates. | SYS-REQ-0022 | [Q-C8](../../standards/decisions-log.md), [06 §5](../../architecture/06-ground-segment-rust.md) | I, T | ALL | grep + proptest |
| GND-REQ-0003 | CFDP Class 1 reception shall be implemented as `Class1Receiver: CfdpProvider` wrapping the `cfdp-core` crate; Class 2 shall land later as an additional implementation of the same trait. | SYS-REQ-0025, SYS-REQ-0020 | [Q-C3](../../standards/decisions-log.md), [07 §5](../../architecture/07-comms-stack.md) | I, T | P4 | integration |

## 3. Code Quality

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| GND-REQ-0010 | Every crate shall declare `#![deny(clippy::all)]` at its root. | SYS-REQ-0071 | [.claude/rules/general.md](../../../.claude/rules/general.md) | I | ALL | grep |
| GND-REQ-0011 | `cargo clippy --all-targets -- -D warnings` shall pass with zero unjustified suppressions; every `#[allow(clippy::...)]` shall carry an inline justification comment. | SYS-REQ-0071 | [.claude/rules/general.md](../../../.claude/rules/general.md) | T | ALL | CI (planned) |
| GND-REQ-0012 | `unwrap()` on `Result` / `Option` shall not appear outside `#[cfg(test)]` code. | derived | [.claude/rules/general.md](../../../.claude/rules/general.md) | I | ALL | grep |
| GND-REQ-0013 | Every `unsafe` block shall carry a `// SAFETY:` comment explaining the invariant being upheld. | derived (security) | [.claude/rules/security.md](../../../.claude/rules/security.md) | I | ALL | grep + review |
| GND-REQ-0014 | `cargo audit` shall report zero HIGH or CRITICAL advisories; tracked-but-unpatched entries shall be recorded in [`../../standards/deviations.md`](../../standards/deviations.md). | SYS-REQ-0073 | [.claude/rules/security.md](../../../.claude/rules/security.md) | T | ALL | CI (planned) |

## 4. Radiation-Sensitive State

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| GND-REQ-0020 | Radiation-sensitive ground-side state (CFDP transaction-id counters, `ApidRouter` filter table, future `tai_ns` mirror) shall be held in a `Vault<T>` wrapper per [09 §5.2](../../architecture/09-failure-and-radiation.md). | SYS-REQ-0042 | [Q-F3](../../standards/decisions-log.md), [.claude/rules/general.md](../../../.claude/rules/general.md) | I | ALL | code review |

## 5. APID Routing and Egress

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| GND-REQ-0030 | The ground station `ApidRouter` shall reject any outbound TC whose APID resolves within the sim-fsw block (`0x500`–`0x57F`). | SYS-REQ-0041 | [06 §8.2](../../architecture/06-ground-segment-rust.md) | T | ALL | unit test |
| GND-REQ-0031 | The ground station shall decode AOS Transfer Frames (CCSDS 732.0-B-4) at 1024 B; the decoder shall reject frames with FECF (CRC-16) mismatches and emit a `LINK_ERROR` event on APID `0x602` (ground-internal). | SYS-REQ-0023 | [ICD-orbiter-ground.md §7.1](../../interfaces/ICD-orbiter-ground.md) | T | P1–P6 | unit + integration |
| GND-REQ-0032 | Decoded TM packets shall be dispatched to the per-APID decoder registry; unknown APIDs shall be logged but not discarded silently (surface to operator UI). | derived | [06 §5](../../architecture/06-ground-segment-rust.md) | T | ALL | unit test |

## 6. Operator UI

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| GND-REQ-0040 | The operator UI shall display telemetry timestamps in UTC, millisecond precision, ISO-8601 format. | SYS-REQ-0060 | [08 §5.5](../../architecture/08-timing-and-clocks.md) | D | ALL | SCN-NOM-01 |
| GND-REQ-0041 | The operator UI shall display the current light-time indicator (configured one-way delay); it shall update when `LIGHT_TIME_OW_SECONDS` changes. | SYS-REQ-0060 | [08 §6](../../architecture/08-timing-and-clocks.md), [ConOps §6](../conops/ConOps.md) | D | ALL | SCN-NOM-01 |
| GND-REQ-0042 | The operator UI shall reject commands whose authorization window has expired given the current light-time estimate, surfacing a user-visible error. | SYS-REQ-0061 | [08 §1](../../architecture/08-timing-and-clocks.md) | T, D | ALL | UI test |
| GND-REQ-0043 | The operator UI shall surface asset mode-state transitions (including safe-mode entry) within `light-time + 2 × retransmit-window` of the triggering event, consistent with [ConOps §5](../conops/ConOps.md) expectations. | SYS-REQ-0052 | [ConOps §5](../conops/ConOps.md) | T, D | ALL | SCN-OFF-01 |

## 7. Time Handling

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| GND-REQ-0050 | Internal ground-side timestamps shall be represented as UTC (`OffsetDateTime` or equivalent) for UI use and as TAI internally for log correlation. | SYS-REQ-0030 | [08 §5.5](../../architecture/08-timing-and-clocks.md) | I, T | ALL | unit test |
| GND-REQ-0051 | The ground station shall be the primary UTC authority for the fleet, emitting Time Correlation Packets to the orbiter at a cadence of 1 TCP per 60 s while AOS is established. | SYS-REQ-0031 | [08 §3](../../architecture/08-timing-and-clocks.md) | T | ALL | integration |
| GND-REQ-0052 | Leap-second offset application shall be a compile-time constant refreshed only on ground-station release; it shall not be updated at runtime. | derived | [08 §1](../../architecture/08-timing-and-clocks.md) | I | ALL | code review |

## 8. CFDP

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| GND-REQ-0060 | The CFDP Class 1 receiver shall accept segment size 1024 B, CRC-32 checksum (IEEE 802.3 polynomial), unidirectional (downlink only in MVC), on AOS VC 2. | SYS-REQ-0025 | [Q-C2](../../standards/decisions-log.md), [07 §5](../../architecture/07-comms-stack.md) | T | P4 | integration |
| GND-REQ-0061 | CFDP transaction timeout shall be 10 × nominal end-to-end latency (~100 min at 10 min light-time); timeout shall emit a logged "missing file" event but not attempt retransmission (Class 1 has no ARQ). | derived | [07 §5](../../architecture/07-comms-stack.md) | T | P4 | unit + integration |
| GND-REQ-0062 | Concurrent CFDP transactions shall be capped at 16. | derived | [07 §5](../../architecture/07-comms-stack.md) | T | P4 | unit |

## 9. Test Coverage

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| GND-REQ-0070 | Every Rust crate under `rust/` shall carry either in-module `#[cfg(test)]` unit tests or a `tests/` integration directory (both where applicable), with at least one failure-path test per crate. | SYS-REQ-0074 | [.claude/rules/testing.md](../../../.claude/rules/testing.md) | I, T | ALL | `cargo test` |
| GND-REQ-0071 | Parsers and encoders that accept unbounded input shall carry `proptest` property tests (round-trip for encoders; fuzz input for parsers). | derived | [.claude/rules/testing.md](../../../.claude/rules/testing.md) | T | ALL | `cargo test` |

## 10. Open / Deferred

- CFDP Class 2 ([Q-C3](../../standards/decisions-log.md)) — lands as a sibling implementation of `CfdpProvider`.
- `vault` crate implementation + SECDED feature flag ([09 §5.2, §9](../../architecture/09-failure-and-radiation.md)).
- `ccsds_wire` crate creation ([Q-C8](../../standards/decisions-log.md)) — referenced by GND-REQ-0002.
- Ferrocene-qualified build ([CLAUDE.md §Stack](../../../CLAUDE.md)) — tracked as a future target.

## 11. What this sub-SRD is NOT

- Not a coding rulebook. Rules live in [`.claude/rules/`](../../../.claude/rules/).
- Not a segment architecture doc. Crate-boundary rationale lives in [`../../architecture/06-ground-segment-rust.md`](../../architecture/06-ground-segment-rust.md).
- Not an ICD. Boundary contracts live in [`../../interfaces/`](../../interfaces/).
- Not a UI design doc. Layout, component library, and accessibility are out of scope.
