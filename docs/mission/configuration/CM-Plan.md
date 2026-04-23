# Configuration Management Plan

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Release process: [`release-process.md`](release-process.md). Bibliography: [../../standards/references.md](../../standards/references.md). Decisions log: [../../standards/decisions-log.md](../../standards/decisions-log.md). V&V: [`../verification/V&V-Plan.md`](../verification/V&V-Plan.md). Requirements: [../requirements/SRD.md](../requirements/SRD.md).

This plan fixes **how SAKURA-II manages change** — baselines, version pinning, submodule hygiene, and the change-control process. Cadence and artefact generation live in [`release-process.md`](release-process.md); this plan is the policy.

Pedigree: aligned with [NPR 7150.2D](../../standards/references.md) and [ECSS-Q-ST-80C Rev.1](../../standards/references.md) structural expectations. SAKURA-II does not *claim conformance* per [`../verification/V&V-Plan §1.2`](../verification/V&V-Plan.md).

## 1. Scope

Applies to:

- All tracked sources under the repo root (`apps/`, `ros2_ws/`, `simulation/`, `rust/`, `_defs/`, `docs/`, `scripts/`, `.claude/`, top-level build files).
- The `.mcp.json` configuration.
- Submodules (future, once wired).

Does **not** apply to:

- Developer-local `.env` files (gitignored; individual responsibility).
- IDE / editor configuration (user-local).
- Host toolchain versions (covered by [`../../dev/quickstart.md §1`](../../dev/quickstart.md) as a prereq).

## 2. Baseline Definitions

A **baseline** is an immutable reference point in git. Three classes:

| Class | Source | Trigger | Retention |
|---|---|---|---|
| **Developmental** | Every merge to `master` | CI green + approved PR | Retained indefinitely |
| **Release** | Tagged `vX.Y.Z` per [`release-process.md`](release-process.md) | Phase-gate milestone | Retained indefinitely |
| **Emergency** | Hotfix branch; tagged `vX.Y.Z-hotfix.N` | Critical defect in an active Release | Retained indefinitely |

### 2.1 Baseline scope

A baseline captures **the full working tree at that commit**, not just a subset. No "partial baseline" where some files are tracked and others are detached.

### 2.2 What counts as a baseline event

| Event | Creates baseline? |
|---|---|
| Merge to `master` | Yes (Developmental) |
| Force-push to `master` | **Prohibited** — see §7 Change Control |
| Tag creation (`vX.Y.Z`) | Yes (Release) |
| Tag deletion / move | **Prohibited** — tags are immutable once pushed |
| Deleting `master` branch | **Prohibited** |

## 3. Version Identifiers

### 3.1 Repo version

Semantic versioning per [semver.org](https://semver.org):

- **MAJOR** — incompatible ICD, requirement, or SRD change.
- **MINOR** — new requirement / new app / new scenario (backwards-compatible).
- **PATCH** — bug fix, doc typo, lint fix.

Current: pre-1.0 (`0.Y.Z`) during Phase B / early Phase C. `1.0.0` cuts when Phase C closes with a complete compliance matrix.

### 3.2 Per-artefact versioning

| Artefact | Version source |
|---|---|
| Top-level repo | `git describe --tags` |
| cFS apps | Each `<app>_version.h` (per [01 §5](../../architecture/01-orbiter-cfs.md)) |
| ROS 2 packages | `package.xml` `<version>` tag |
| Rust crates | `Cargo.toml` `version` field |
| Docker images | Tagged identically to the repo release (e.g. `sakura/cfs-orbiter:v1.2.3`) |

Per-artefact versions move in lockstep with the repo version by default; drift is allowed but must be justified in the PR description.

## 4. Submodule Policy

### 4.1 cFS / third-party source

Per [`../../standards/references.md §5`](../../standards/references.md), `nasa/cFS` will be wired as a git submodule in Phase B. Submodule discipline:

- **Pinned revision** — never tracked against a branch; always a specific SHA.
- **Update requires PR** — a submodule bump is an independent PR; no drive-by submodule moves.
- **Changelog entry** — each bump records the upstream version and any breaking changes in the PR description.

### 4.2 Rust / ROS 2 dependencies

- Rust: `Cargo.lock` is committed for binary crates; library crates may choose per crate-level policy. `cargo update` is a deliberate PR, not a background task.
- ROS 2: `package.xml` exact-version pins discouraged; use range specifiers aligned with the Space ROS distro.

### 4.3 Docker base images

Base images (e.g. Ubuntu LTS) are pinned by digest (`sha256:...`) in the `FROM` line (Phase-B infra PR when the Dockerfiles land).

## 5. Configuration Surfaces (the four, plus CM additions)

Per [`../../architecture/10-scaling-and-config.md §1`](../../architecture/10-scaling-and-config.md) there are four runtime / build configuration surfaces. CM adds three more for programmatic discipline:

| Surface | Mutator | Change cadence |
|---|---|---|
| Docker compose profiles | PR to `docker-compose.yml` + overlays | Release |
| `_defs/mission.yaml` | PR | Per-mission (cloned on fork) |
| cFS C headers under `_defs/` | PR | Rebuild |
| ROS 2 launch files | PR | Release |
| **Git tags** (CM) | `release-process.md` runbook | Release |
| **`.mcp.json`** (CM) | PR | Rarely |
| **`.claude/settings.json`** (CM) | PR | Per-policy |

## 6. Change Control

### 6.1 Ordinary changes

Every change lands via a PR that:

1. Has a descriptive title (`feat(scope): ...`, `fix(scope): ...`, `chore(scope): ...`, `docs(scope): ...`).
2. Passes all PR-gate checks per [`../../CLAUDE.md §PR & Review Process`](../../../CLAUDE.md) and [`../verification/V&V-Plan §5.4`](../verification/V&V-Plan.md).
3. Has at least one approving review.
4. Touches at most one concern (scope-bounded).

### 6.2 Requirements changes

A change that modifies an existing `XXX-REQ-####`:

- **MUST** preserve the ID (requirements are immutable per [`../../README.md`](../../README.md) Contributing Conventions).
- **MUST** update [`../requirements/traceability.md`](../requirements/traceability.md) in the same PR.
- **MUST** run [`scripts/traceability-lint.py`](../../../scripts/traceability-lint.py) green.
- **MUST** either update or widen the V&V artefact pointer; narrowing requires a deviation entry.

Withdrawing a requirement: mark `withdrawn: ReasonCode` in traceability per [`../requirements/traceability.md §9`](../requirements/traceability.md); do not delete the row or reuse the ID.

### 6.3 ICD / architecture changes

A change that modifies an ICD or an architecture doc's load-bearing text:

- **MUST** cross-cite the Q-\* decision it hinges on (add a row to [`../../standards/decisions-log.md`](../../standards/decisions-log.md) if the decision is new).
- **MUST** update the Propagation Manifest in [`../../standards/decisions-log.md`](../../standards/decisions-log.md) if new forward-ref sites arise.
- **MUST** propagate to every citing doc in the same PR.

### 6.4 Emergency hotfixes

If a critical defect lands in a released baseline:

1. Branch from the release tag: `git checkout -b hotfix/vX.Y.Z-fix <tag>`.
2. Fix + PR + review (abbreviated acceptable; one reviewer + CI).
3. Tag `vX.Y.Z-hotfix.N`.
4. Cherry-pick forward to `master` in a follow-up PR.

Hotfixes are tracked in the release notes of the parent release.

### 6.5 Prohibited operations

Per [`../../CLAUDE.md Git Safety Protocol`](../../../CLAUDE.md):

- Force-push to `master` or any release tag.
- `git push --force` / `--force-with-lease` to shared branches.
- `git rebase -i` over shared history.
- Tag deletion / move.
- Git config changes in CI scripts.
- Skipping hooks (`--no-verify`) without recorded justification.

## 7. Auditing

### 7.1 What lands where

| Record | Location |
|---|---|
| Decision rationale | [`../../standards/decisions-log.md`](../../standards/decisions-log.md) |
| Deviation from a cited standard | [`../../standards/deviations.md`](../../standards/deviations.md) |
| Requirement changes | git history of the SRDs + [`../requirements/traceability.md`](../requirements/traceability.md) |
| V&V results | per-test artefact + CI run history |
| CM events (baselines, tags) | git log / release notes |

### 7.2 Periodic audit (Phase C)

At each release cut:

1. Run [`scripts/traceability-lint.py`](../../../scripts/traceability-lint.py) — must be green.
2. Confirm every `XXX-REQ-####` has a V&V artefact pointer.
3. Confirm [`../../standards/deviations.md`](../../standards/deviations.md) rows all name a review signer.
4. Confirm no "Deferred" row in [`../verification/compliance-matrix.md`](../verification/compliance-matrix.md) is past its intended sunset.

Audit results attach to the release notes per [`release-process.md`](release-process.md).

## 8. Submodules, Forks, and Downstream

### 8.1 This repo as boilerplate

SAKURA-II is a **fork-to-bootstrap** boilerplate per [`../../../CLAUDE.md`](../../../CLAUDE.md) and [`../../dev/fork-bootstrap.md`](../../dev/fork-bootstrap.md). A downstream mission:

- Forks at a tagged release.
- Follows [`../../dev/fork-bootstrap.md`](../../dev/fork-bootstrap.md) to rename codename, reset SCID, etc.
- Archives the upstream SAKURA-II `decisions-log.md` and starts fresh.
- Retains its own CM history from fork onward.

### 8.2 Contributing upstream

Downstream missions contributing fixes or features back to SAKURA-II:

- Submit a PR against `master`.
- PR must be free of mission-specific identifiers (no leftover mission-name, SCID, or APID values from the downstream).
- Reviewer checks that the change generalises across the boilerplate.

## 9. Phase-Gate Reviews (CM events)

Per the three-phase release posture ([`../../README.md`](../../README.md)):

| Gate | Entry criteria | CM artefact |
|---|---|---|
| Phase A close | Quickstart runnable, CI baseline green | `v0.1.0` tag |
| Phase B close | All segment / interface docs ✅; [Q-F3](../../standards/decisions-log.md) resolved | `v0.2.0` tag |
| Phase C close | SRDs + traceability + compliance matrix + CM plan all green; no Deferred rows past sunset | `v1.0.0` tag |

Each gate event generates a release note per [`release-process.md`](release-process.md) and a compliance-matrix snapshot committed to `docs/mission/verification/snapshots/` (convention — directory lands with the first snapshot).

## 10. Open Items

- Wire the cFS submodule under `apps/cfs/` or `third_party/cfs/` (Phase B infra; CM decision pending).
- Stand up `.github/workflows/` to mechanise the PR-gate checks per [`../verification/V&V-Plan §7.2`](../verification/V&V-Plan.md).
- Author a `standards/deviations.md` PR template so new deviations land consistently.
- Define the archival location for CFDP-received files during release demos.

## 11. What this plan is NOT

- Not a release runbook — [`release-process.md`](release-process.md) owns operational steps.
- Not a coding rulebook — rules live in [`../../../.claude/rules/`](../../../.claude/rules/).
- Not a project-management / scrum artefact — sprint cadence, backlog management, and staffing are out of scope.
- Not a security policy — supply-chain security (signing, provenance) is tracked in [`release-process.md`](release-process.md) and [`../../../.claude/rules/security.md`](../../../.claude/rules/security.md).
