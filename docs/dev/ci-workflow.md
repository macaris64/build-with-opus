# CI Workflow Specification

> **Status: spec-only — implementation deferred.** This document fixes the job matrix and gate policy; `.github/workflows/` does not yet exist. Track it as an open Phase-C-plus item in [`../README.md`](../README.md).

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Bound requirements: [SYS-REQ-0070 through SYS-REQ-0074](../mission/requirements/SRD.md). V&V plan: [`../mission/verification/V&V-Plan.md §5.2, §7.2`](../mission/verification/V&V-Plan.md). Release process: [`../mission/configuration/release-process.md`](../mission/configuration/release-process.md). Linter specs gated here: [`howto/authoring-a-repo-linter.md`](howto/authoring-a-repo-linter.md), [`linter-specs/apid-mid-lint.md`](linter-specs/apid-mid-lint.md), [`linter-specs/citation-lint.md`](linter-specs/citation-lint.md).

This is the design spec for the `.github/workflows/` PR-gate. It fixes **which jobs run, on which triggers, with which pass/fail semantics** — the YAML that implements it is deliberately out of scope of this document, and lands in a follow-up PR that cites this spec as its contract.

## 1. When this applies

The PR-gate runs for any pull request that touches:

- `apps/**` — cFS flight software (C, CMocka tests)
- `ros2_ws/**` — Space ROS 2 workspace (C++17 lifecycle nodes)
- `rust/**` — Cargo workspace (ground station, `cfs_bindings`)
- `simulation/**` — Gazebo Harmonic plugins
- `_defs/**` — mission-level CMake + compile-time headers
- `scripts/**` — repo linters and tooling
- `docs/**` — all documentation

Which is to say: every PR. Path-filters narrow *individual jobs* (§2), not the workflow itself.

## 2. Jobs

One subsection per job. Every job runs in parallel unless it declares a dependency. Default runner is `ubuntu-22.04` unless noted.

### 2.1 `cfs-ctest`

- **Trigger:** paths touching `apps/**`, `_defs/**`, or `CMakeLists.txt`.
- **Command:** `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build --output-on-failure`
- **Source of truth:** [`../../CLAUDE.md §Build & Test Commands`](../../CLAUDE.md).
- **Gate:** exit 0 and no regressions in test count. Coverage measurement (`gcov` / `llvm-cov --show-branches`) is a separate downstream job (§2.4), not inline here.
- **Binds:** [`V&V-Plan.md §2.1`](../mission/verification/V&V-Plan.md), [SYS-REQ-0070](../mission/requirements/SRD.md).

### 2.2 `ros2-colcon`

- **Trigger:** paths touching `ros2_ws/**`.
- **Command (run inside `ros2_ws/`):** `colcon build --symlink-install && colcon test && colcon test-result --verbose`
- **Container:** official Space ROS image pinned in a repo-level `.ci/ros2.Dockerfile` (image tag policy per [CM-Plan](../mission/configuration/CM-Plan.md)).
- **Gate:** zero failing test artefacts from `colcon test-result`.
- **Binds:** [`V&V-Plan.md §2.1`](../mission/verification/V&V-Plan.md); indirectly [SYS-REQ-0070](../mission/requirements/SRD.md) via C++ compile flags per [`.claude/rules/ros2-nodes.md`](../../.claude/rules/ros2-nodes.md).

### 2.3 `rust-cargo`

- **Trigger:** paths touching `rust/**` or the root `Cargo.toml` / `Cargo.lock`.
- **Commands (sequential within the job):**
  - `cargo test --workspace --all-features`
  - `cargo clippy --workspace --all-targets -- -D warnings`
  - `cargo audit --deny warnings`
- **Gate:** all three exit 0. `cargo clippy` tolerates zero warnings, `cargo audit` tolerates zero HIGH/CRITICAL advisories. Suppressions in code require the `// SAFETY:` / clippy-allow justification comment per [`.claude/rules/general.md`](../../.claude/rules/general.md) and [`.claude/rules/security.md`](../../.claude/rules/security.md).
- **Binds:** [SYS-REQ-0071](../mission/requirements/SRD.md) (`cargo clippy -D warnings`), [SYS-REQ-0073](../mission/requirements/SRD.md) (`cargo audit`), [`V&V-Plan.md §5.2`](../mission/verification/V&V-Plan.md).

### 2.4 `cppcheck`

- **Trigger:** paths touching `apps/**`.
- **Command:** `cppcheck --enable=all --std=c17 --error-exitcode=1 apps/`
- **Gate:** zero new findings. Baseline suppressions, if any, live in a repo-committed `.cppcheck-suppressions` file with inline justification per [`.claude/rules/security.md`](../../.claude/rules/security.md).
- **Binds:** [SYS-REQ-0072](../mission/requirements/SRD.md), [`V&V-Plan.md §5.2`](../mission/verification/V&V-Plan.md).

### 2.5 `traceability-lint`

- **Trigger:** paths touching `docs/mission/requirements/**` or `docs/mission/verification/V&V-Plan.md`.
- **Command:** `python3 scripts/traceability-lint.py --repo-root .`
- **Gate:** exit 0 per [`../../scripts/traceability-lint.py`](../../scripts/traceability-lint.py) rules 1–4.
- **Binds:** [SYS-REQ-0080](../mission/requirements/SRD.md).

### 2.6 `apid-mid-lint`

- **Trigger:** every PR (no path-filter — cost is negligible; see [`linter-specs/apid-mid-lint.md §8`](linter-specs/apid-mid-lint.md)).
- **Command:** `python3 scripts/apid_mid_lint.py --repo-root .`
- **Gate:** exit 0 per [`linter-specs/apid-mid-lint.md §3`](linter-specs/apid-mid-lint.md) rules R1–R5.
- **Script status:** **not yet implemented.** Spec-only until the follow-up PR lands.

### 2.7 `citation-lint`

- **Trigger:** paths touching `docs/**/*.md`.
- **Command:** `python3 scripts/citation_lint.py --repo-root .`
- **Gate:** exit 0 per [`linter-specs/citation-lint.md §3`](linter-specs/citation-lint.md) rules R1–R5. R6 warnings appear in the job log but do not fail the gate.
- **Script status:** **not yet implemented.** Spec-only until the follow-up PR lands.

### 2.8 `link-check`

- **Trigger:** paths touching `docs/**/*.md` or any root-level README (`README.md`, `apps/README.md`, `ros2_ws/README.md`, `rust/README.md`, `simulation/README.md`).
- **Tool:** `markdown-link-check` (Node-based) pinned in `.ci/package.json`.
- **Command:** `find docs -name "*.md" -print0 | xargs -0 -I{} markdown-link-check --config .ci/markdown-link-check.json {}`
- **Gate:** zero broken intra-repo links. External links are retried once; network flakes do not fail the gate (configured via the JSON config).
- **Binds:** [`V&V-Plan.md §7.2`](../mission/verification/V&V-Plan.md) Link-check row.

### 2.9 `plantuml-parse`

- **Trigger:** paths touching `docs/**/*.puml`.
- **Command:** `plantuml -checkonly docs/architecture/diagrams/*.puml`
- **Gate:** exit 0 on every `.puml` source. No image is rendered in this job — the SVG render is a separate job in the release workflow (not here).
- **Binds:** [`V&V-Plan.md §7.2`](../mission/verification/V&V-Plan.md) PlantUML parse row.

### 2.10 `mermaid-parse`

- **Trigger:** paths touching `docs/**/*.md`.
- **Tool:** `@mermaid-js/mermaid-cli` (`mmdc`) pinned in `.ci/package.json`.
- **Command:** a small helper script that extracts every fenced ```` ```mermaid ```` block into a temp file and invokes `mmdc -i <tmp> -o /dev/null`. The helper lives at `scripts/mermaid-parse.sh` (planned).
- **Gate:** every block parses.
- **Binds:** [`V&V-Plan.md §7.2`](../mission/verification/V&V-Plan.md) Mermaid parse row.

## 3. Triggers

The workflow file declares three event triggers:

| Event | Purpose | Notes |
|---|---|---|
| `pull_request` | Primary PR gate | Runs on all branches. Required checks are enforced via branch protection on `main`. |
| `push` to `main` | Post-merge safety net | Catches anything the PR-gate missed (rare — branch protection keeps this from mattering, but it keeps the badge on the README honest). |
| `workflow_dispatch` | Manual re-run | For re-running after infrastructure flakes (e.g. CCSDS public index HTTP 503). |

No `schedule` trigger. Cron-triggered CI produces noise proportional to the cron frequency and adds no signal not already produced by PRs.

## 4. Concurrency

```yaml
concurrency:
  group: ci-${{ github.workflow }}-${{ github.event.pull_request.number || github.ref }}
  cancel-in-progress: true
```

A new push to the same PR cancels the in-flight run. Queued runs from different PRs do not contend — they are separate groups.

## 5. Required checks → requirements map

The following jobs are marked **required** in branch protection for `main`. A red required-check blocks merge per [`../../CLAUDE.md §PR & Review Process`](../../CLAUDE.md).

| Required job | Binds to |
|---|---|
| `cfs-ctest` | [SYS-REQ-0070](../mission/requirements/SRD.md) |
| `rust-cargo` (`test` + `clippy` + `audit`) | [SYS-REQ-0071](../mission/requirements/SRD.md), [SYS-REQ-0073](../mission/requirements/SRD.md) |
| `cppcheck` | [SYS-REQ-0072](../mission/requirements/SRD.md) |
| `ros2-colcon` | [`V&V-Plan.md §2.1`](../mission/verification/V&V-Plan.md) |
| `traceability-lint` | [SYS-REQ-0080](../mission/requirements/SRD.md) |
| `apid-mid-lint` (once implemented) | [`apid-registry.md §Change Control`](../interfaces/apid-registry.md) |
| `citation-lint` (once implemented) | [`references.md` preamble](../standards/references.md) |
| `link-check` | [`V&V-Plan.md §7.2`](../mission/verification/V&V-Plan.md) |
| `plantuml-parse` | [`V&V-Plan.md §7.2`](../mission/verification/V&V-Plan.md) |
| `mermaid-parse` | [`V&V-Plan.md §7.2`](../mission/verification/V&V-Plan.md) |

[SYS-REQ-0074](../mission/requirements/SRD.md) (branch-coverage target) is satisfied by `cargo tarpaulin` + `gcov`, which run in a separate **informational** coverage job (not yet in this spec). Until that job lands, coverage is tracked manually — an open item carried from [`V&V-Plan.md §10`](../mission/verification/V&V-Plan.md).

## 6. Secrets and tokens

- `GITHUB_TOKEN` — provided by GitHub, used for PR status checks and commenting. No additional scope required.
- `CODECOV_TOKEN` — **optional**, only required if the informational coverage job uploads to Codecov. The PR-gate does not require it.

Explicitly **not** required in CI:

- GitHub MCP, Sentry MCP, Postgres MCP tokens — those are dev-side only per [`mcp-setup.md`](mcp-setup.md).
- `DATABASE_URL` (local-only per [`mcp-setup.md`](mcp-setup.md)) — CI does not hit the telemetry database.
- Any private key or certificate — per [`.claude/rules/security.md`](../../.claude/rules/security.md), secrets never appear in CI scripts or logs.

## 7. Caching

Cache keys follow the `actions/cache` conventions. Invalidation is automatic on lockfile change.

| Cache | Key | Path |
|---|---|---|
| Cargo registry | `${{ runner.os }}-cargo-${{ hashFiles('rust/Cargo.lock') }}` | `~/.cargo/registry`, `~/.cargo/git` |
| Cargo target dir | `${{ runner.os }}-cargo-target-${{ hashFiles('rust/Cargo.lock') }}-${{ hashFiles('rust/**/*.rs') }}` | `rust/target` |
| colcon build | `${{ runner.os }}-colcon-${{ hashFiles('ros2_ws/**/package.xml') }}` | `ros2_ws/build`, `ros2_ws/install` |
| cFS build | `${{ runner.os }}-cfs-${{ hashFiles('apps/**/CMakeLists.txt', '_defs/**') }}` | `build/` |
| Node tooling (`markdown-link-check`, `mmdc`) | `${{ runner.os }}-node-${{ hashFiles('.ci/package-lock.json') }}` | `~/.npm`, `node_modules` |

## 8. Failure policy

- Any required-checks job red → PR cannot merge (enforced by branch protection, not by the workflow itself).
- A single retry is permitted for network-flake failures (`markdown-link-check`, `cargo audit`'s advisory fetch). Retries beyond one require a `workflow_dispatch` re-run.
- A flaky test that passes on retry is still a failure — the gate does not auto-retry test jobs. File an issue, fix the flake.

## 9. What this spec does NOT cover

- **Release tagging / deployment** — lives in [`release-process.md`](../mission/configuration/release-process.md). The release workflow is a separate `.github/workflows/release.yml` file that runs on tag push, renders PlantUML to SVG, and produces the CM baseline artefact per [`CM-Plan.md`](../mission/configuration/CM-Plan.md).
- **HPSC cross-build** — deferred per [Q-H8](../standards/decisions-log.md). When that unblocks, a new job `cfs-ctest-hpsc` joins §2; no other changes to this workflow.
- **Scale-5 end-to-end tests** — planned per [`V&V-Plan.md §2.3`](../mission/verification/V&V-Plan.md) and [`10-scaling-and-config.md §2`](../architecture/10-scaling-and-config.md). Lives in a dedicated scheduled workflow (`scale-5-e2e.yml`), not here — it would dominate the PR-gate latency.
- **Dependency update automation** — Dependabot / Renovate configuration is out of scope; author when needed.
- **Performance regression testing** — separate discipline tracked as a future open item.

## 10. Implementation follow-up

A follow-up PR lands `.github/workflows/ci.yml` implementing this spec. That PR's checklist:

1. Cite this document in the PR description.
2. Include a test PR (draft, against the same branch) that deliberately breaks each required check to prove the gate fails red.
3. Enable branch protection on `main` with the required-checks list from §5.
4. Update [`../README.md` "Next Steps"](../README.md) to move "`.github/workflows/` CI" from "Design-spec complete, implementation deferred" to done.
