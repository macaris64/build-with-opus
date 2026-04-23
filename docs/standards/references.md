# External Standards & References

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Deviations from anything listed here are recorded in [deviations.md](deviations.md).

This is the single bibliography for SAKURA-II. **Every standard / specification cited anywhere in `docs/` must appear here**, and no doc may cite a standard that is absent from this file. The citation-integrity linter (planned) enforces this.

Cite-by-ID only. Long-form titles live here; docs use the short ID (`CCSDS 133.0-B-2`, `NPR 7150.2D`, `ECSS-E-ST-40C`). URLs are given where a freely available version is canonical; where only a controlled download exists, the issuing body is noted.

## 1. CCSDS — Consultative Committee for Space Data Systems

Authoritative index: https://public.ccsds.org/Publications/default.aspx

### Space Link & Physical

| ID | Title | Used by |
|---|---|---|
| CCSDS 131.0-B-4 | TM Synchronization and Channel Coding | `architecture/07-comms-stack.md` |
| CCSDS 132.0-B-3 | TM Space Data Link Protocol | `07-comms-stack.md`, `interfaces/ICD-orbiter-ground.md` |
| CCSDS 232.0-B-4 | TC Space Data Link Protocol | `07-comms-stack.md`, `ICD-orbiter-ground.md` |
| CCSDS 732.0-B-4 | AOS Space Data Link Protocol | `07-comms-stack.md`, `ICD-orbiter-ground.md`, `ICD-orbiter-relay.md` |
| CCSDS 211.0-B-6 | Proximity-1 Space Link Protocol — Data Link Layer | `07-comms-stack.md`, `ICD-relay-surface.md` |

### Network / Transfer

| ID | Title | Used by |
|---|---|---|
| CCSDS 133.0-B-2 | Space Packet Protocol (SPP) | `07-comms-stack.md`, `interfaces/apid-registry.md`, every ICD |
| CCSDS 133.1-B-3 | Encapsulation Service | Deferred — cited only if we wrap non-SPP PDUs |

### File / Application

| ID | Title | Used by |
|---|---|---|
| CCSDS 727.0-B-5 | CFDP — CCSDS File Delivery Protocol | `07-comms-stack.md`, `06-ground-segment-rust.md`, `ICD-orbiter-ground.md` |

### Time

| ID | Title | Used by |
|---|---|---|
| CCSDS 301.0-B-4 | Time Code Formats | `architecture/08-timing-and-clocks.md` |
| CCSDS 302.0-B-1 | Time Management | `08-timing-and-clocks.md` |

### Informational / Overview

| ID | Title | Used by |
|---|---|---|
| CCSDS 130.0-G-4 | Overview of Space Communications Protocols | `07-comms-stack.md` introduction |

## 2. NASA Directives & Standards

### NPR — NASA Procedural Requirements

| ID | Title | Used by |
|---|---|---|
| NPR 7123.1C | NASA Systems Engineering Processes and Requirements | `mission/requirements/SRD.md` (Phase C) |
| NPR 7150.2D | NASA Software Engineering Requirements (classification, assurance) | `mission/requirements/FSW-SRD.md` (Phase C), `mission/verification/compliance-matrix.md` (Phase C) |

### NASA-STD — NASA Technical Standards

| ID | Title | Used by |
|---|---|---|
| NASA-STD-8739.8B | Software Assurance and Software Safety | `mission/verification/V&V-Plan.md`, Phase-C compliance matrix |
| NASA-STD-7009A | Standard for Models and Simulations | `V&V-Plan.md`, `architecture/05-simulation-gazebo.md` |

### NASA Flight Software

Not a formal standard, but referenced for implementation detail:

| ID | Title | Used by |
|---|---|---|
| cFE Application Developers Guide | Core Flight Executive, current release (nasa.github.io/cFS) | `architecture/01-orbiter-cfs.md` |
| cFE OSAL API Reference | OSAL, current release | `01-orbiter-cfs.md`, `architecture/03-subsystem-mcus.md` |
| cFE PSP Developers Guide | PSP, current release | `01-orbiter-cfs.md`, `architecture/09-failure-and-radiation.md` |

## 3. ECSS — European Cooperation for Space Standardization

Authoritative index: https://ecss.nl/standards/active-standards/

Cited for pedigree and cross-reference; SAKURA-II does not formally comply with ECSS, but aligns where feasible.

| ID | Title | Used by |
|---|---|---|
| ECSS-E-ST-10C Rev.1 | System Engineering — General Requirements | `mission/requirements/SRD.md` (Phase C) |
| ECSS-E-ST-10-02C | Verification | `mission/verification/V&V-Plan.md`, Phase-C compliance matrix |
| ECSS-E-ST-40C | Software Engineering | `mission/requirements/FSW-SRD.md` (Phase C), Phase-C compliance matrix |
| ECSS-Q-ST-80C Rev.1 | Software Product Assurance | Phase-C compliance matrix |
| ECSS-E-ST-50-12C Rev.1 | SpaceWire — Links, Nodes, Routers and Networks | `architecture/03-subsystem-mcus.md`, `interfaces/ICD-mcu-cfs.md` |

## 4. Coding & Process Standards

| ID | Title | Used by |
|---|---|---|
| MISRA C:2012 | Guidelines for Use of C in Critical Systems | Cited via [`../../.claude/rules/general.md`](../../.claude/rules/general.md); not re-stated |
| DO-178C / ED-12C | Software Considerations in Airborne Systems and Equipment Certification | Referenced for pedigree only (not applied in full); cited by `_defs/mission_config.h` comment |
| IEEE 802.3 | Ethernet — CRC-32 polynomial (used as CFDP Class 1 checksum per Q-C2) | `architecture/06-ground-segment-rust.md`, `architecture/07-comms-stack.md`, ICDs, `standards/decisions-log.md` |
| IEEE 1012 | IEEE Standard for System, Software, and Hardware Verification and Validation | `mission/verification/V&V-Plan.md`, `mission/verification/compliance-matrix.md`, `standards/deviations.md` |

## 5. Implementation References (non-standards)

Frameworks, kernels, and toolchains whose semantics the architecture docs rely on. Not standards, but the architecture docs must cite specific versions so behavior is reproducible.

| Reference | Version policy | Used by |
|---|---|---|
| NASA cFS / cFE (github: nasa/cFS) | Pinned via submodule; policy in `mission/configuration/CM-Plan.md` (Phase C) | `01-orbiter-cfs.md` |
| Space ROS (space-ros.org) | Distribution decision in `architecture/04-rovers-spaceros.md` | `04-rovers-spaceros.md`, `interfaces/ICD-relay-surface.md` |
| ROS 2 `rclcpp_lifecycle` API | ROS 2 distro bundled by Space ROS | `04-rovers-spaceros.md` |
| FreeRTOS Kernel Reference Manual | v10.6.x baseline | `02-smallsat-relay.md`, `03-subsystem-mcus.md` |
| Gazebo Harmonic — ModelPlugin API | Harmonic LTS | `05-simulation-gazebo.md` |
| Rust stable + `cargo` | `rust-toolchain.toml` pins minor version (file pending) | `06-ground-segment-rust.md` |

## 6. Internal Cross-References (for completeness)

Not standards, but docs cite these as normative:

- [`../../CLAUDE.md`](../../CLAUDE.md) — Project instructions for Claude Code; authoritative for stack, build commands, and PR process.
- [`../../.claude/rules/*.md`](../../.claude/rules/) — Path-scoped coding conventions. Normative for all code under `apps/`, `ros2_ws/`, `simulation/`, `rust/`. Docs cite these rules by path; never restate them.
- [`../../_defs/mission_config.h`](../../_defs/mission_config.h) — Mission-level compile-time constants. Authoritative source for `SPACECRAFT_ID` used in [`../interfaces/apid-registry.md`](../interfaces/apid-registry.md).
- [`../../_defs/targets.cmake`](../../_defs/targets.cmake) — Build-time mission definition. Authoritative source for `MISSION_APPS` list mirrored in [`../REPO_MAP.md`](../REPO_MAP.md) and [`../architecture/01-orbiter-cfs.md`](../architecture/01-orbiter-cfs.md) (planned).

## Changelog Policy

- A reference here is **locked** once any doc cites it. Version bumps (e.g. CCSDS 133.0-B-2 → B-3) require a PR that updates both this file and every citing doc atomically.
- Adding a new reference is cheap — add the row, then cite from the appropriate doc.
- Removing a reference requires deleting all citations first; the linter enforces this by failing if a citation resolves to a non-existent entry here.
