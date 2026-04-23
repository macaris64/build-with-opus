# Compliance Matrix

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Bibliography: [../../standards/references.md](../../standards/references.md). V&V plan: [`V&V-Plan.md`](V&V-Plan.md). SRD: [`../requirements/SRD.md`](../requirements/SRD.md). Deviations: [`../../standards/deviations.md`](../../standards/deviations.md). Traceability: [`../requirements/traceability.md`](../requirements/traceability.md).

This matrix expands [`V&V-Plan.md §9`](V&V-Plan.md) into a **line-level per-standard map**. For each cited standard, it names the clauses (or the objective, for NPRs) that apply to SAKURA-II, binds them to a SAKURA-II requirement ID + V&V artefact, and records alignment status: **Compliant** / **Partial** / **Deferred** / **Not Applicable**.

Per [`V&V-Plan §1.2`](V&V-Plan.md), SAKURA-II *aligns with* NASA / ECSS standards on a PDR-surrogate track; it does not formally *claim conformance*. The "compliance" label here is an engineering alignment assessment, not a certification.

## 1. Conventions

| Column | Meaning |
|---|---|
| **Clause / Objective** | Specific clause number (CCSDS / ECSS) or objective (NPR) |
| **Topic** | One-line summary |
| **SAKURA-II requirement** | `XXX-REQ-####` from the SRDs |
| **V&V artefact** | Section in [`V&V-Plan.md`](V&V-Plan.md) or test artefact |
| **Status** | Compliant / Partial (deviation row) / Deferred / N/A |

**Status definitions**:

- **Compliant** — SAKURA-II fully implements the clause; V&V artefact provides evidence.
- **Partial** — SAKURA-II implements the clause with a recorded deviation in [`deviations.md`](../../standards/deviations.md).
- **Deferred** — SAKURA-II will meet the clause at a future phase (Phase-B-plus, HPSC target, Phase C closure). Tracked here so it is not forgotten.
- **N/A** — Clause does not apply to a SITL demonstrator (e.g. RF hardware qualification).

## 2. CCSDS — Space Link Protocols

### 2.1 CCSDS 133.0-B-2 — Space Packet Protocol

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §4.1.1 | Primary header 6 B | SYS-REQ-0021 | [`ccsds_wire` proptest](V&V-Plan.md) | Compliant |
| §4.1.2 | APID 11-bit | SYS-REQ-0026 | APID/MID linter | Compliant |
| §4.1.3 | Sequence flags + count | SYS-REQ-0020 | unit | Compliant |
| §4.1.4 | Packet data length | SYS-REQ-0020 | unit | Compliant |
| §4.2 | Secondary header | SYS-REQ-0021 | proptest | Compliant (10 B SAKURA-II profile) |

### 2.2 CCSDS 732.0-B-4 — AOS Space Data Link Protocol

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §4 | Transfer frame structure | SYS-REQ-0023 | [ICD-orbiter-ground §3](../../interfaces/ICD-orbiter-ground.md) | Compliant (1024 B) |
| §4.1 | Virtual channels | SYS-REQ-0023 | [07 §3](../../architecture/07-comms-stack.md) VC table | Compliant |
| §4.1.4.2 | M_PDU first-header pointer | SYS-REQ-0023 | [07 §3](../../architecture/07-comms-stack.md) | Compliant |
| §5 | FECF (CRC-16) | GND-REQ-0031 | ground unit test | Compliant |

### 2.3 CCSDS 232.0-B-4 — TC SDLP

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §4 | TC frame format | SYS-REQ-0023 | ICD-orbiter-ground | Compliant |
| §6 (COP-1) | Sequence-controlled TC | SYS-REQ-0023 | integration | Compliant (for safety-critical TCs) |

### 2.4 CCSDS 211.0-B-6 — Proximity-1

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §3 | Data Link layer | SYS-REQ-0024 | [ICD-relay-surface](../../interfaces/ICD-relay-surface.md) | Compliant |
| §3.3 | Hailing | SYS-REQ-0024 | [Q-C5](../../standards/decisions-log.md), [07 §4](../../architecture/07-comms-stack.md) | Compliant (1 Hz) |
| §4 | Reliable mode | ROV-REQ-0020 | integration | Compliant |

### 2.5 CCSDS 131.0-B-4 — Channel Coding

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §4 (R-S 255,223) | Reed-Solomon symbol path | SYS-REQ-0003 | `clock_link_model` BER injection | **Partial** — see [deviations §1](../../standards/deviations.md) (simulated, not computed) |

### 2.6 CCSDS 727.0-B-5 — CFDP

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §4 (Class 1) | Class 1 service | SYS-REQ-0025, GND-REQ-0060 | ground integration | Compliant |
| §4 (Class 2) | Class 2 service | — | — | **Deferred** ([Q-C3](../../standards/decisions-log.md)) |
| §5.2 | PDU formats | SYS-REQ-0025 | integration | Compliant |

### 2.7 CCSDS 301.0-B-4 / 302.0-B-1 — Time

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| 301 §3.2.4 (CUC) | CUC format P-Field `0x2F` | SYS-REQ-0021 | `ccsds_wire::cuc` | Compliant (7 B total) |
| 302 §3 (Time management) | Time authority / correlation | SYS-REQ-0031, GND-REQ-0051 | integration | Compliant (hybrid ladder) |
| 302 (leap seconds) | Leap-second policy | SYS-REQ-0030 | code review | **Partial** — see [deviations §2](../../standards/deviations.md) (TAI internal, UI boundary conversion) |

## 3. NASA Directives and Standards

### 3.1 NPR 7123.1C — NASA Systems Engineering Processes and Requirements

| Objective | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §3.1.3 (req capture) | Requirements documented with ID + statement + rationale | SYS-REQ-0080 | [`traceability.md`](../requirements/traceability.md) | Compliant |
| §3.1.7 (verification) | Verification method per requirement (T/A/I/D) | — | [`V&V-Plan §2`](V&V-Plan.md) | Compliant |
| §3.1.8 (traceability) | Traceability index + linter | SYS-REQ-0080 | [`traceability-lint.py`](../../../scripts/traceability-lint.py) | Compliant |
| §3.4 (reviews) | Phase-gate reviews | — | Phase-C review gates | **Partial** — review cadence formalised; independent IV&V deferred ([V&V §10](V&V-Plan.md)) |

### 3.2 NPR 7150.2D — NASA Software Engineering Requirements

| Objective | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §3.2 (classification) | Software class designation | — | [FSW-SRD §1](../requirements/FSW-SRD.md) preamble | **Deferred** — class pinning belongs to program-tier scope |
| §3.3 (assurance) | Assurance plan | — | [`V&V-Plan`](V&V-Plan.md) | Compliant |
| §4.x (processes) | Engineering processes | SYS-REQ-0070..0074 | various | Compliant (per-rule bindings below) |

### 3.3 NASA-STD-8739.8B — Software Assurance and Software Safety

| Objective | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §4.2 (plans) | Software assurance plan | — | [`V&V-Plan`](V&V-Plan.md) | Compliant |
| §4.3 (activities) | Static analysis | SYS-REQ-0070, SYS-REQ-0071, SYS-REQ-0073 | [`V&V §5.2`](V&V-Plan.md) | Compliant (cppcheck + clippy + audit) |
| §4.4 (safe coding) | MISRA adherence | FSW-REQ-0020..0024 | cppcheck + code review | Compliant (with deviations per [deviations.md](../../standards/deviations.md)) |
| §5 (reviews) | Defect tracking | — | GitHub Issues + decisions-log | **Partial** — informal Phase B; Phase-C CM plan formalises |

### 3.4 NASA-STD-7009A — Models and Simulations

| Objective | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §4.2 (documentation) | Simulation documentation | — | [`../../architecture/05-simulation-gazebo.md`](../../architecture/05-simulation-gazebo.md) | Compliant |
| §4.3 (credibility) | Fidelity claims documented | SYS-REQ-0003 | [`../../architecture/05-simulation-gazebo.md`](../../architecture/05-simulation-gazebo.md) + [deviations §1](../../standards/deviations.md) | **Partial** — fidelity level documented; quantitative credibility assessment deferred to Phase-C closure |
| §4.4 (V&V) | Simulation V&V | — | [`V&V §4`](V&V-Plan.md) fault-injection tests | Compliant |

## 4. ECSS — European Cooperation for Space Standardization

SAKURA-II *aligns* with ECSS for pedigree; formal ECSS compliance is not claimed (per [references.md §3](../../standards/references.md)).

### 4.1 ECSS-E-ST-10C Rev.1 — System Engineering

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §4.2 (requirements) | System requirements document | — | [`../requirements/SRD.md`](../requirements/SRD.md) | Compliant (structural alignment) |
| §4.5 (verification) | V&V plan | — | [`V&V-Plan.md`](V&V-Plan.md) | Compliant |

### 4.2 ECSS-E-ST-10-02C — Verification

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §5.2 (V&V planning) | Plan structure | — | [`V&V §2`](V&V-Plan.md) class taxonomy | Compliant |
| §5.3 (verification methods) | T/A/I/D methods | — | SRD method columns | Compliant |
| §5.4 (control) | Verification control | — | scenario YAML + [`traceability.md`](../requirements/traceability.md) | Compliant |

### 4.3 ECSS-E-ST-40C — Software Engineering

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §5 (processes) | SW lifecycle processes | — | [`../configuration/CM-Plan.md`](../configuration/CM-Plan.md), [`V&V-Plan.md`](V&V-Plan.md) | **Partial** — structural alignment; full tailoring deferred |
| §6 (V&V) | SW verification | — | [`V&V §5`](V&V-Plan.md) | Compliant |

### 4.4 ECSS-Q-ST-80C Rev.1 — Software Product Assurance

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §5 (PA activities) | Product assurance | — | [`V&V §5.2`](V&V-Plan.md) gates | Compliant |
| §6 (records) | Audit records | — | git history + [`decisions-log.md`](../../standards/decisions-log.md) | Compliant |

### 4.5 ECSS-E-ST-50-12C Rev.1 — SpaceWire

| Clause | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §4 (physical) | SpW character / EOP | FSW-REQ-0060 | [ICD-mcu-cfs §2.1](../../interfaces/ICD-mcu-cfs.md) | Compliant (SITL framing) |

## 5. Coding Standards

### 5.1 MISRA C:2012

SAKURA-II enforces MISRA required rules under `apps/**`. Deviations carry inline justification per [`.claude/rules/general.md`](../../../.claude/rules/general.md).

| Rule | Topic | Req | Status |
|---|---|---|---|
| Rule 8.11 | Explicit external linkage | FSW-REQ-0024 | **Partial** — justified deviation for `.critical_mem` attribute ([09 §5.1](../../architecture/09-failure-and-radiation.md)); inline comment required |
| Rule 18.8 | No VLAs | FSW-REQ-0021 | Compliant |
| Rule 21.3 | No `malloc`/`free` | SYS-REQ-0072, FSW-REQ-0020 | Compliant |
| Required rules (all) | As-is | FSW-REQ-0020..0024 | Compliant (enforced by cppcheck + review) |

### 5.2 DO-178C / ED-12C

Referenced for pedigree only; SAKURA-II does not execute DO-178C objectives.

| Area | Status |
|---|---|
| Software levels, objectives | **N/A** — pedigree cite only ([references.md §4](../../standards/references.md)) |

## 6. IEEE

### 6.1 IEEE 1012 — Independent V&V

| Objective | Topic | Req | V&V | Status |
|---|---|---|---|---|
| §5 (IV&V) | Independent verification | — | — | **Deferred** — single-contributor Phase B; tracked as program role ([V&V §10](V&V-Plan.md)) |

## 7. Summary

| Category | Compliant | Partial | Deferred | N/A |
|---|---|---|---|---|
| CCSDS | 14 | 2 | 1 | 0 |
| NASA | 7 | 3 | 1 | 0 |
| ECSS | 7 | 2 | 0 | 0 |
| Coding | 3 | 1 | 0 | 1 |
| IEEE | 0 | 0 | 1 | 0 |
| **Total** | **31** | **8** | **3** | **1** |

Eight Partial rows correspond to documented deviations in [`../../standards/deviations.md`](../../standards/deviations.md); three Deferred rows are tracked in [V&V-Plan §10](V&V-Plan.md) or SRD §10.

## 8. Maintenance

- Adding a new cited standard to [`references.md`](../../standards/references.md) requires a new row here (citation-integrity linter, planned).
- Adding a new SAKURA-II requirement that binds to a standards clause adds a row or updates the status column.
- Moving a row from Compliant → Partial requires an entry in [`../../standards/deviations.md`](../../standards/deviations.md).
- Moving a row from Deferred → Compliant / Partial requires a PR that updates the status and the V&V artefact pointer together.

## 9. What this matrix is NOT

- Not a certification package — SAKURA-II is a SITL demonstrator, not a certified flight artefact.
- Not a deviations log — [`../../standards/deviations.md`](../../standards/deviations.md) owns that.
- Not a test report — [`V&V-Plan`](V&V-Plan.md) + CI runs own that.
- Not a claim of formal compliance with any listed standard — see [`V&V-Plan §1.2`](V&V-Plan.md).
