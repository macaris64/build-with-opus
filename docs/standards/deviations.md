# Knowing Deviations from Cited Standards

> Every reference in [references.md](references.md) is normative unless listed here. This file records **deliberate** deviations with rationale and review scope. If a behavior appears to deviate and is not in this file, it is a bug — open a PR to add an entry OR fix the behavior.

Compliance matrix: [`../mission/verification/compliance-matrix.md`](../mission/verification/compliance-matrix.md). Rows marked **Partial** in the matrix correspond one-to-one with entries here. Phase-C **Deferred** rows (not-yet-implemented rather than deliberately different) are tracked in [V&V-Plan §10](../mission/verification/V&V-Plan.md) and SRD §10, not here.

## Format

Each entry must include:

- **ID**: `D-###` — monotonically assigned, never reused.
- **Standard & clause**: the cited ID and section (e.g. `CCSDS 133.0-B-2 §4.1.3.2`).
- **Deviation**: what SAKURA-II does differently.
- **Rationale**: why (project constraint, SITL simplification, HPSC target difference, etc.).
- **Scope**: which docs/code are affected.
- **Review**: who signed off and when.
- **Sunset**: a trigger or date by which the deviation is revisited (where applicable).

## Current Deviations

### D-001 — Reed-Solomon channel coding is simulated, not computed

- **Standard & clause**: CCSDS 131.0-B-4 §4 (Reed-Solomon (255, 223)).
- **Deviation**: SAKURA-II does not compute R-S symbols at the AOS boundary. The `clock_link_model` container inflates / corrects bit errors at configurable rates, functionally substituting for R-S while running ~1000× faster than symbol-accurate encoding.
- **Rationale**: SITL performance. Symbol-accurate R-S at 1 Mbps downlink × 34 containers (Scale-5) would dominate sim cost. Above the link-physics layer per [SYS-REQ-0003](../mission/requirements/SRD.md).
- **Scope**: [`../architecture/07-comms-stack.md §3`](../architecture/07-comms-stack.md); `clock_link_model` implementation (Phase B); [`../mission/verification/compliance-matrix.md §2.5`](../mission/verification/compliance-matrix.md).
- **Review**: project engineer, Phase B1 close (accepted as load-bearing SITL simplification).
- **Sunset**: promotes to full compliance **only** if a future integration test replays recorded RF symbols. Until then, the deviation stands.

### D-002 — MISRA C:2012 Rule 8.11 deviation for `.critical_mem` placement

- **Standard & clause**: MISRA C:2012 Rule 8.11 — "The declaration of an array with external linkage should include an explicit size."
- **Deviation**: Radiation-sensitive static variables under `apps/**` carry `__attribute__((section(".critical_mem")))` to force linker placement. The attribute declaration pattern triggers Rule 8.11 flags under some static-analyser configurations. Each occurrence carries an inline justification comment.
- **Rationale**: EDAC/scrubber attachment surface per [Q-F3](decisions-log.md) and [`../architecture/09-failure-and-radiation.md §5.1`](../architecture/09-failure-and-radiation.md). The placement is required for the Phase-B-plus EDAC seam to be a local change rather than a repo-wide refactor.
- **Scope**: every `.critical_mem`-annotated static under `apps/**`; inline justification comment `/* MISRA C:2012 Rule 8.11 deviation: .critical_mem placement per Q-F3. */` is mandatory per [`.claude/rules/cfs-apps.md`](../../.claude/rules/cfs-apps.md).
- **Review**: project engineer, Phase C Phase-gate review (accepted as load-bearing for EDAC architecture).
- **Sunset**: promotes to full compliance if the linker script gains an alternative placement mechanism that does not require the attribute (none known at authoring time).

---

## Candidate deviations flagged for future review

These are not deviations today, but they are positions where a future choice may conflict with a cited standard. Tracked here so they are not forgotten.

1. **Leap-second handling is a policy choice, not a deviation.** [08-timing-and-clocks.md](../architecture/08-timing-and-clocks.md) §1 states SAKURA-II uses TAI internally and converts at UI boundaries only. CCSDS 301.0-B-4 / 302.0-B-1 do not mandate a specific policy — they define the formats — so this is a *policy choice* rather than a deviation. Recorded here for visibility. Entered as **Partial** in [`../mission/verification/compliance-matrix.md §2.7`](../mission/verification/compliance-matrix.md) to flag that a reviewer may expect a specific leap-second model.

2. **CFDP Class 2 is architected-for but not implemented.** Phase B ships CFDP Class 1 only. CCSDS 727.0-B-5 lists Class 2 as a profile, not a mandatory feature of every CFDP implementation — so this is **not** a deviation. Recorded for visibility because a cursory reader of [07-comms-stack.md](../architecture/07-comms-stack.md) §5 might expect Class 2. Per [Q-C3](decisions-log.md), the upgrade surface is the `CfdpProvider` trait in `rust/ground_station/src/cfdp/` (wrapping the `cfdp-core` crate); Class 1 today and Class 2 later both implement it — so the seam is present in code even while Class 2 behavior is not. Entered as **Deferred** in [compliance-matrix §2.6](../mission/verification/compliance-matrix.md).

3. **Independent V&V (IEEE 1012)** is deferred.** SAKURA-II runs with a single-contributor review posture in Phase B; the independent-verification role is a program-surrogate artefact that lands when the project grows. Not a deviation — IEEE 1012 is referenced for pedigree, not applied. Tracked in [V&V-Plan §10](../mission/verification/V&V-Plan.md) and [compliance-matrix §6.1](../mission/verification/compliance-matrix.md).

4. **Software-classification (NPR 7150.2D §3.2) is deferred.** The software-class designation (Class A / B / C) is a program-tier decision that SAKURA-II does not claim. Entered as **Deferred** in [compliance-matrix §3.2](../mission/verification/compliance-matrix.md).

---

## Relationship to other artefacts

- [`../mission/verification/compliance-matrix.md`](../mission/verification/compliance-matrix.md) — every row marked **Partial** must cite an entry here by ID.
- [decisions-log.md](decisions-log.md) — Q-\* entries are **choices between options the standard allows**; entries here are **we do X where the standard says Y**. The two files never overlap by design.
- [V&V-Plan §5.1](../mission/verification/V&V-Plan.md) — coverage targets; rows that are *knowingly* missed record a deviation here.

## Adding a new deviation

1. Assign the next `D-###`.
2. Populate all required fields above.
3. Update the corresponding row in [`../mission/verification/compliance-matrix.md`](../mission/verification/compliance-matrix.md) to **Partial** and cite `D-###`.
4. If the deviation is MISRA-class or `.critical_mem`-class, confirm the inline justification convention in [`.claude/rules/`](../../.claude/rules/) is being followed.
5. Obtain a review signer and fill the **Review** field before merge.

## Sunset maintenance

At each release cut per [`../mission/configuration/release-process.md §11`](../mission/configuration/release-process.md), confirm:

- No deviation's **Sunset** trigger has fired without promotion.
- Every deviation's **Review** field names a real reviewer.
- Compliance matrix rows match this file's IDs one-to-one.
