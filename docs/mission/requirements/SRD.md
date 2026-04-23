# System Requirements Document (SRD)

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Parent ConOps: [`../conops/ConOps.md`](../conops/ConOps.md). Traceability index: [`traceability.md`](traceability.md). Verification plan: [`../verification/V&V-Plan.md`](../verification/V&V-Plan.md). Compliance matrix: [`../verification/compliance-matrix.md`](../verification/compliance-matrix.md). Bibliography: [`../../standards/references.md`](../../standards/references.md). Sub-SRDs: [`FSW-SRD.md`](FSW-SRD.md), [`GND-SRD.md`](GND-SRD.md), [`ROVER-SRD.md`](ROVER-SRD.md).

This is the **system-level requirements document** for SAKURA-II. It captures the contract between ConOps and the downstream per-segment SRDs. Every `SYS-REQ-####` here is **load-bearing** — sub-SRDs derive from these via `parent:` links, and V&V artefacts cite these IDs.

Per [`../../README.md` Contributing Conventions](../../README.md), requirement IDs are **immutable once assigned**. Derived requirements live in sub-SRDs and carry a `parent:` field. Never encode parentage into the ID.

## 1. Document conventions

### 1.1 Requirement record format

Every requirement row has these columns:

| Column | Meaning |
|---|---|
| **ID** | `SYS-REQ-####` — monotonically assigned, never reused |
| **Statement** | "The system shall ..." — testable, singular, unambiguous |
| **Rationale / Trace** | Upstream source: ConOps section, architecture doc, Q-ID decision, or standard |
| **Verification** | Method per NPR 7123.1C §3.1.7: **T** (Test), **A** (Analysis), **I** (Inspection), **D** (Demonstration) |
| **Phase** | Applicable phase(s) from [`mission-phases.md`](../conops/mission-phases.md); `ALL` if every phase |
| **V&V artefact** | Section / TC-ID in [`V&V-Plan.md`](../verification/V&V-Plan.md) that exercises it |

### 1.2 Verification method selection (per NPR 7123.1C)

- **Test (T)** — controlled stimulus with measured response (unit / integration / system tests).
- **Analysis (A)** — mathematical / simulation evidence (latency budgets, drift-rate calculations).
- **Inspection (I)** — review of code, schematics, configuration (MISRA compliance, `.critical_mem` placement).
- **Demonstration (D)** — end-to-end scenario run (SCN-NOM-01, SCN-OFF-01).

### 1.3 Phase tags

`P1` MOI, `P2` Relay Deployment, `P3` Surface EDL, `P4` Surface Ops, `P5` Cryobot Descent, `P6` Safe Mode. `ALL` applies across. See [`../conops/mission-phases.md`](../conops/mission-phases.md).

## 2. Mission-Level Requirements

### 2.1 Mission definition and scope

| ID | Statement | Rationale / Trace | Verification | Phase | V&V |
|---|---|---|---|---|---|
| SYS-REQ-0001 | The system shall provide a software-in-the-loop demonstrator of a heterogeneous Mars fleet consisting of at minimum one orbiter, one relay, one land rover, one UAV, and one subsurface cryobot (MVC fleet). | [ConOps §1–2](../conops/ConOps.md) | I, D | ALL | [V&V §3](../verification/V&V-Plan.md) |
| SYS-REQ-0002 | The system shall support scaling from the MVC fleet to the Scale-5 target (5 orbiters, 1 relay, 5+ land, 3+ UAV, 2+ cryobot) via configuration only — without new APIDs, new ICDs, or new per-instance architecture documents. | [10 §1](../../architecture/10-scaling-and-config.md), [Q-H2](../../standards/decisions-log.md) | I, A | ALL | [V&V §10 open item: E2E scaling tests](../verification/V&V-Plan.md) |
| SYS-REQ-0003 | The system shall simulate above the link-physics layer; RF-level bit errors are modelled via the `clock_link_model` container rather than RF-symbol simulation. | [00 §1](../../architecture/00-system-of-systems.md), [07 §3](../../architecture/07-comms-stack.md) | I | ALL | [V&V §2.3 system tier](../verification/V&V-Plan.md) |

### 2.2 Fleet segmentation

| ID | Statement | Rationale / Trace | Verification | Phase | V&V |
|---|---|---|---|---|---|
| SYS-REQ-0010 | The system shall be partitioned into four segments: Space Segment, Surface Segment, Ground Segment, Simulation Segment. | [00 §1](../../architecture/00-system-of-systems.md) | I | ALL | compliance-matrix §2 |
| SYS-REQ-0011 | Each segment shall be an independently deployable container set. | [00 §2](../../architecture/00-system-of-systems.md), [10 §2](../../architecture/10-scaling-and-config.md) | I | ALL | docker-runbook |

## 3. Communications Requirements

| ID | Statement | Rationale / Trace | Verification | Phase | V&V |
|---|---|---|---|---|---|
| SYS-REQ-0020 | All SAKURA-II flight boundaries shall carry CCSDS Space Packet Protocol (CCSDS 133.0-B-2) at the network layer (L3). | [07 §1](../../architecture/07-comms-stack.md) | I, T | ALL | unit decoder tests + SCN-NOM-01 |
| SYS-REQ-0021 | Every CCSDS Space Packet emitted or received by the system shall include a 10-byte secondary header carrying CUC time (7 B), function code (2 B), and instance ID (1 B). | [Q-C6](../../standards/decisions-log.md), [08 §2](../../architecture/08-timing-and-clocks.md) | T, I | ALL | `ccsds_wire` property tests |
| SYS-REQ-0022 | All multi-byte CCSDS fields shall be encoded big-endian on the wire, with conversion logic constrained to the `ccsds_wire` and `cfs_bindings` crates on the Rust side and no ad-hoc conversion elsewhere. | [Q-C8](../../standards/decisions-log.md), [06](../../architecture/06-ground-segment-rust.md) | I | ALL | `grep` lint; proptest roundtrip |
| SYS-REQ-0023 | Ground ↔ Orbiter downlink shall use AOS Transfer Frames of 1024 bytes (CCSDS 732.0-B-4); uplink shall use TC SDLP (CCSDS 232.0-B-4). | [Q-C4](../../standards/decisions-log.md), [07 §3](../../architecture/07-comms-stack.md) | T, I | P1–P6 | ICD-orbiter-ground tests |
| SYS-REQ-0024 | Relay ↔ Surface communications shall use Proximity-1 Data Link Protocol (CCSDS 211.0-B-6). | [07 §4](../../architecture/07-comms-stack.md), [ICD-relay-surface.md](../../interfaces/ICD-relay-surface.md) | T, I | P3–P5 | SCN-NOM-01 |
| SYS-REQ-0025 | File delivery shall use CFDP Class 1 (CCSDS 727.0-B-5) unacknowledged mode on AOS VC 2. | [07 §5](../../architecture/07-comms-stack.md), [Q-C3](../../standards/decisions-log.md) | T | P4 | `rust/ground_station/` integration |
| SYS-REQ-0026 | APID allocation shall follow the class-based scheme in [`apid-registry.md`](../../interfaces/apid-registry.md); instance multiplicity shall be handled via the secondary-header `instance_id` field, never by allocating additional APIDs. | [10 §6](../../architecture/10-scaling-and-config.md), [apid-registry.md](../../interfaces/apid-registry.md) | I | ALL | APID/MID consistency linter (planned) |

## 4. Timing and Clocks

| ID | Statement | Rationale / Trace | Verification | Phase | V&V |
|---|---|---|---|---|---|
| SYS-REQ-0030 | All on-board timestamps shall be TAI (leap-second-free); UTC conversion shall occur only at two boundaries: ground-station operator UI, and scenario authoring tools. | [08 §1](../../architecture/08-timing-and-clocks.md) | I, T | ALL | `ccsds_wire::cuc` tests |
| SYS-REQ-0031 | The time-authority ladder shall be: Ground (primary UTC) → Orbiter (primary during LOS) → Relay → Rover → Cryobot. | [Q-F4](../../standards/decisions-log.md), [08 §3](../../architecture/08-timing-and-clocks.md) | I | ALL | integration test |
| SYS-REQ-0032 | End-to-end fleet drift during LOS shall not exceed 50 ms over 4 hours (≤ 3.47 ppm rate), apportioned per [08 §4](../../architecture/08-timing-and-clocks.md). | [Q-F4](../../standards/decisions-log.md) | A, T | ALL | drift-budget analysis + integration |
| SYS-REQ-0033 | End-to-end fleet-sync precision shall be ≤ 1 ms. | [Q-F6](../../standards/decisions-log.md), [08 §2](../../architecture/08-timing-and-clocks.md) | A, T | ALL | integration |
| SYS-REQ-0034 | Any asset whose free-running clock estimate exceeds the per-asset drift threshold shall set the `time_suspect` flag (bit 0 of the TM secondary-header function code) on all outgoing telemetry until re-synced. | [08 §4](../../architecture/08-timing-and-clocks.md) | T | ALL | [V&V §4.1 `0x541` gate](../verification/V&V-Plan.md) |

## 5. Failure and Radiation

| ID | Statement | Rationale / Trace | Verification | Phase | V&V |
|---|---|---|---|---|---|
| SYS-REQ-0040 | The system shall support functional fault injection via cFE Software Bus messages on CCSDS APID range `0x540`–`0x543` (packet drop, clock skew, force safe-mode, sensor-noise corruption). | [Q-F1](../../standards/decisions-log.md), [Q-F2](../../standards/decisions-log.md), [09 §2](../../architecture/09-failure-and-radiation.md) | T, D | ALL | [V&V §4](../verification/V&V-Plan.md) |
| SYS-REQ-0041 | The sim-fsw APID block `0x500`–`0x57F` shall be compile-time unreachable in any `CFS_FLIGHT_BUILD` target; no runtime path shall emit packets in this block over a ground RF link. | [07 §8](../../architecture/07-comms-stack.md), [ICD-sim-fsw §5](../../interfaces/ICD-sim-fsw.md), [01 §11](../../architecture/01-orbiter-cfs.md) | I, T | ALL | `scripts/check_sim_apids.py` (planned) + unit test |
| SYS-REQ-0042 | Radiation-sensitive state in C code shall be marked with `__attribute__((section(".critical_mem")))` with a MISRA Rule 8.11 deviation comment; the Rust-side analogue shall be the `Vault<T>` wrapper. | [Q-F3](../../standards/decisions-log.md), [09 §5](../../architecture/09-failure-and-radiation.md) | I | ALL | grep check + code review |
| SYS-REQ-0043 | Clock-skew fault injection shall not modify the `.critical_mem`-resident time-store value; it shall compose at the read-hook boundary only. | [09 §5.3](../../architecture/09-failure-and-radiation.md), [ICD-sim-fsw §3.2](../../interfaces/ICD-sim-fsw.md) | T | ALL | [V&V §4.2 Q-F3 regression](../verification/V&V-Plan.md) |

## 6. Safe-Mode

| ID | Statement | Rationale / Trace | Verification | Phase | V&V |
|---|---|---|---|---|---|
| SYS-REQ-0050 | Every asset class shall implement a safe-mode entry path triggered by local invariant violation, watchdog timeout, or operator `0x542` force-safe command. | [ConOps §7](../conops/ConOps.md), [09 §4](../../architecture/09-failure-and-radiation.md) | T, D | ALL | [V&V §4.1 `0x542` gate](../verification/V&V-Plan.md) |
| SYS-REQ-0051 | No asset shall exit safe-mode autonomously during Phase A/B; RESUME requires operator TC. | [ConOps §7](../conops/ConOps.md) | I, T | P6 | integration test |
| SYS-REQ-0052 | Safe-mode entry events shall be logged via cFE EVS (cFS assets) or `rclcpp::log` (ROS 2 assets) and propagated to ground within `light-time + 2 × retransmit-window`. | [ConOps §5](../conops/ConOps.md) | T | ALL | [V&V §3.2 TC-SCN-OFF-01-C](../verification/V&V-Plan.md) |

## 7. Operator Interface

| ID | Statement | Rationale / Trace | Verification | Phase | V&V |
|---|---|---|---|---|---|
| SYS-REQ-0060 | The ground station shall surface telemetry to the operator with UTC timestamps (millisecond precision, ISO-8601) and shall display a light-time indicator. | [ConOps §2](../conops/ConOps.md), [08 §5.5](../../architecture/08-timing-and-clocks.md) | D, I | ALL | SCN-NOM-01 UI check |
| SYS-REQ-0061 | The ground station shall reject commands whose validity window has passed (authorization window expired given the current light-time estimate). | [08 §1](../../architecture/08-timing-and-clocks.md) | T | ALL | ground-station unit test |

## 8. Build Quality

| ID | Statement | Rationale / Trace | Verification | Phase | V&V |
|---|---|---|---|---|---|
| SYS-REQ-0070 | All C code under `apps/**` shall compile cleanly with `-Wall -Wextra -Werror -pedantic` under C17. | [CLAUDE.md §Coding Standards](../../../CLAUDE.md) | T | ALL | CI (planned) |
| SYS-REQ-0071 | All Rust code shall pass `cargo clippy -- -D warnings` with zero unjustified suppressions. | [CLAUDE.md](../../../CLAUDE.md), [.claude/rules/general.md](../../../.claude/rules/general.md) | T, I | ALL | CI (planned) |
| SYS-REQ-0072 | Dynamic memory allocation (`malloc`/`calloc`/`realloc`/`free`) shall not occur in `apps/**` flight-path code. | [.claude/rules/general.md](../../../.claude/rules/general.md) | I | ALL | cppcheck + code review |
| SYS-REQ-0073 | The `cargo audit` check shall report zero HIGH or CRITICAL vulnerabilities in the Rust dependency tree. | [.claude/rules/security.md](../../../.claude/rules/security.md) | T | ALL | CI (planned) |
| SYS-REQ-0074 | Unit-test branch coverage shall be 100 % on C and Rust code, with deviations recorded in [`../../standards/deviations.md`](../../standards/deviations.md). | [CLAUDE.md](../../../CLAUDE.md), [.claude/rules/testing.md](../../../.claude/rules/testing.md) | A, T | ALL | `cargo tarpaulin` + `gcov` |

## 9. Traceability and Documentation

| ID | Statement | Rationale / Trace | Verification | Phase | V&V |
|---|---|---|---|---|---|
| SYS-REQ-0080 | Every requirement ID `XXX-REQ-####` shall appear exactly once in [`traceability.md`](traceability.md). | [`../../README.md` CI section](../../README.md) | T | ALL | [`scripts/traceability-lint.py`](../../../scripts/traceability-lint.py) |
| SYS-REQ-0081 | Every cited standard (CCSDS / NPR / NASA-STD / ECSS / IEEE) shall have a bibliography entry in [`../../standards/references.md`](../../standards/references.md). | [references.md changelog](../../standards/references.md) | T | ALL | citation-integrity linter (planned) |
| SYS-REQ-0082 | Every Q-\* decision shall carry exactly one definition-site doc and resolve to `resolved`, `pending`, or `open` in [`../../standards/decisions-log.md`](../../standards/decisions-log.md). | [decisions-log.md §Conventions](../../standards/decisions-log.md) | I | ALL | grep + code review |

## 10. Open / Deferred

Deliberately not requirements today; tracked so they aren't forgotten.

- **SEU / bit-flip stimulus**: reserved per [09 §6](../../architecture/09-failure-and-radiation.md); becomes `SYS-REQ-00XX` when Phase-B-plus lands the stimulus packet.
- **HPSC cross-build**: deferred per [Q-H8](../../standards/decisions-log.md); becomes `SYS-REQ-00XX` when the cross-toolchain target lands.
- **CFDP Class 2**: deferred per [Q-C3](../../standards/decisions-log.md); becomes `SYS-REQ-00XX` when Class 2 lands at the `CfdpProvider` trait.
- **Dynamic time-authority election (Scale-5 multi-orbiter)**: deferred per [10 §7](../../architecture/10-scaling-and-config.md).

## 11. What this SRD is NOT

- Not a segment-level SRD — those live in [`FSW-SRD.md`](FSW-SRD.md), [`GND-SRD.md`](GND-SRD.md), [`ROVER-SRD.md`](ROVER-SRD.md).
- Not a compliance matrix — standards-to-evidence mapping lives in [`../verification/compliance-matrix.md`](../verification/compliance-matrix.md).
- Not a test case catalogue — test artefacts live next to source. This SRD cites them by section.
- Not a deviations list — [`../../standards/deviations.md`](../../standards/deviations.md) owns that.
- Not a configuration-management plan — [`../configuration/CM-Plan.md`](../configuration/CM-Plan.md) owns release policy.
