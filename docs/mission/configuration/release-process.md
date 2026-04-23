# Release Process

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). CM policy: [`CM-Plan.md`](CM-Plan.md). V&V: [`../verification/V&V-Plan.md`](../verification/V&V-Plan.md). Compliance: [`../verification/compliance-matrix.md`](../verification/compliance-matrix.md). Traceability: [`../requirements/traceability.md`](../requirements/traceability.md). Deviations: [`../../standards/deviations.md`](../../standards/deviations.md).

This is the **operational runbook** for cutting a SAKURA-II release. Policy (what a release is, what can change, versioning rules) lives in [`CM-Plan.md`](CM-Plan.md); this doc is the sequence of steps.

A release cut takes ~30 minutes of wall time for a Developmental baseline, ~2 hours for a Phase-gate release (Phase A / B / C close).

## 1. Release Types

Per [`CM-Plan.md §2`](CM-Plan.md):

- **Developmental** — every merge to `master` (automatic; no runbook invocation).
- **Release** — tagged `vX.Y.Z`; runbook below.
- **Emergency** — hotfix off a release tag; abbreviated runbook in §9.

## 2. Pre-Release Gate

Before starting the runbook, confirm:

- [ ] `master` has no uncommitted or unpushed changes on any contributor's machine (Slack / standup check).
- [ ] No open PR is in a partially-merged state (GitHub UI).
- [ ] The current CI run on `master` is green (or CI is manual per [`../verification/V&V-Plan §7.1`](../verification/V&V-Plan.md); then the manual PR-gate checks must be green — see §3).
- [ ] [Milestone / release notes draft exists](#5-release-notes).
- [ ] Freeze announced if appropriate (Phase-gate releases only).

## 3. Local PR-Gate Sanity (manual until Phase-C CI)

Until `.github/workflows/` land ([V&V-Plan §7.2](../verification/V&V-Plan.md)), the release engineer runs the full baseline locally:

```bash
# From repo root
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure
cppcheck --enable=all --std=c17 apps/
cargo build
cargo test
cargo clippy --all-targets -- -D warnings
cargo audit
( cd ros2_ws && colcon build --symlink-install && colcon test )
python3 scripts/traceability-lint.py
```

All commands exit 0. If any fails, the release is **not cut**. Fix first.

## 4. Version Decision

Consult [`CM-Plan.md §3.1`](CM-Plan.md) for semver rules. The release engineer picks `MAJOR.MINOR.PATCH` based on changes since the last tag:

```bash
git log --oneline $(git describe --tags --abbrev=0)..HEAD
```

- Any ICD / requirement / SRD incompatibility → bump MAJOR.
- New requirement / new app / new scenario (backwards-compatible) → bump MINOR.
- Bugs / docs / lint → bump PATCH.

Phase-gate releases have pre-allocated versions:

| Gate | Version |
|---|---|
| Phase A close | `v0.1.0` |
| Phase B close | `v0.2.0` |
| Phase C close | `v1.0.0` |

## 5. Release Notes

Draft release notes before the tag cut. Format:

```markdown
# SAKURA-II vX.Y.Z

## Highlights
- <1–3 headline items>

## Requirements
- Added: SYS-REQ-####, ...
- Modified: SYS-REQ-#### (changelog: ...)
- Withdrawn: (per traceability.md §9)

## Documentation
- Added: docs/...
- Modified: ...

## Code
- apps/: <summary>
- ros2_ws/: <summary>
- rust/: <summary>
- simulation/: <summary>

## Compliance Matrix
- Newly Compliant rows: <list>
- Newly Partial rows (deviations): <list>
- Newly Deferred → Compliant: <list>

## Known issues / open items
- <bullet list>

## Contributors
- <names / handles>
```

Generated from `git log` + [`../requirements/traceability.md`](../requirements/traceability.md) diff + [`../verification/compliance-matrix.md`](../verification/compliance-matrix.md) diff since the last tag.

## 6. Tag Cut

Once §3 is green and §5 is drafted:

```bash
# From repo root, on master
git pull --ff-only
git tag -a vX.Y.Z -m "SAKURA-II vX.Y.Z"
git push origin vX.Y.Z
```

**Tags are immutable** per [`CM-Plan.md §6.5`](CM-Plan.md) — never `git tag -d` + re-push. If the tag is wrong, cut a new one at the next patch and document the skipped version in the release notes.

## 7. Compliance Snapshot (Phase-gate releases only)

For `vX.0.0` tags (Phase A / B / C close), commit a snapshot of the compliance matrix:

```bash
mkdir -p docs/mission/verification/snapshots
cp docs/mission/verification/compliance-matrix.md \
   docs/mission/verification/snapshots/compliance-matrix-vX.0.0.md
git add docs/mission/verification/snapshots/compliance-matrix-vX.0.0.md
git commit -m "chore(cm): compliance matrix snapshot for vX.0.0"
git push
```

The snapshot directory lands with the first Phase-gate release. Developmental / PATCH releases do **not** snapshot.

## 8. Publish

### 8.1 GitHub release

```bash
gh release create vX.Y.Z --title "SAKURA-II vX.Y.Z" \
    --notes-file release-notes-vX.Y.Z.md
```

Attach any relevant artefacts (per §8.3).

### 8.2 Docker images (when infra lands)

Per [`CM-Plan.md §4.3`](CM-Plan.md), Docker base images are digest-pinned. On release:

```bash
# [Phase B] — once Dockerfiles land
docker buildx bake --push --set "*.tags=sakura/orbiter:vX.Y.Z,..."
```

Image registry policy is open per [`CM-Plan.md §10`](CM-Plan.md). Until then, images are built locally.

### 8.3 Release artefacts

| Artefact | Source | Attached? |
|---|---|---|
| Release notes | manually drafted | Yes (release body) |
| Compliance snapshot (Phase-gate only) | `docs/mission/verification/snapshots/compliance-matrix-vX.Y.Z.md` | Yes (Phase-gate only) |
| Build outputs (binaries) | `build/`, `rust/target/release/`, `ros2_ws/install/` | No — downstream rebuilds |
| Docker images | registry | Link (when infra lands) |
| CFDP drop files from a representative SCN-NOM-01 run | `simulation/scenarios/scn-nom-01/` (Phase C when scenario infra lands) | Optional |

## 9. Emergency Hotfix

If a critical defect (security vuln, mission-stopping bug) lands in a released baseline:

```bash
git checkout -b hotfix/vX.Y.Z-fix vX.Y.Z   # branch from the release tag
# ... fix + test locally ...
git commit -m "fix: <short description>"
# Create an abbreviated PR — CI green, one reviewer sufficient.
# Once merged into the hotfix branch:
git tag -a vX.Y.Z-hotfix.N -m "Hotfix vX.Y.Z-hotfix.N"
git push origin hotfix/vX.Y.Z-fix vX.Y.Z-hotfix.N
# Cherry-pick to master:
git checkout master
git cherry-pick <fix-commit>
git push
```

Document the hotfix in the release notes of the parent release per [`CM-Plan.md §6.4`](CM-Plan.md).

## 10. Rollback

If a release turns out to be broken after the tag is pushed:

- **Do NOT** delete or move the tag.
- Cut a new patch release with the fix.
- The broken release stays in history; release notes call out the issue.

Consumers pinned to the broken tag see an advisory in the release notes and upgrade.

## 11. Release Engineer Checklist

Copy this into the release PR / issue:

- [ ] §2 Pre-release gate verified
- [ ] §3 PR-gate sanity run locally — all green
- [ ] §4 Version decision: `vX.Y.Z`
- [ ] §5 Release notes drafted
- [ ] §6 Tag cut + pushed
- [ ] §7 (Phase-gate only) Compliance snapshot committed
- [ ] §8.1 GitHub release published
- [ ] §8.2 Docker images pushed (when infra lands)
- [ ] CM audit run per [`CM-Plan §7.2`](CM-Plan.md)
- [ ] Announcement posted (Slack / mailing list / wherever the team congregates)

## 12. Open Items

- **CI automation** for §3 — Phase-C per [`../verification/V&V-Plan §7.2`](../verification/V&V-Plan.md).
- **Image-registry policy** — where Docker images are pushed, retention, signing (Phase C).
- **Release-note generation tooling** — scripts to diff `traceability.md` and `compliance-matrix.md` across tags.
- **SBOM generation** — software bill of materials for each release (Phase C stretch goal).
- **Signing** — tag / image signing with Sigstore or equivalent (Phase C stretch goal).

## 13. What this runbook is NOT

- Not the CM policy — [`CM-Plan.md`](CM-Plan.md) owns that.
- Not a changelog — release notes are the per-release record; [`CM-Plan.md §7`](CM-Plan.md) lists the full records ladder.
- Not a backup / DR plan — out of scope for this doc set.
- Not a deployment runbook for downstream missions — those fork at a release tag and follow [`../../dev/fork-bootstrap.md`](../../dev/fork-bootstrap.md).
