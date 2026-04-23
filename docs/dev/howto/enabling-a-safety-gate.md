# How-To: Enable a Safety / Security Gate

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Rules: [`../../../.claude/rules/security.md`](../../../.claude/rules/security.md), [`general.md`](../../../.claude/rules/general.md). V&V plan: [`../../mission/verification/V&V-Plan.md §5`](../../mission/verification/V&V-Plan.md). Skills: [`../../../.claude/skills/`](../../../.claude/skills/). Failure model: [`../../architecture/09-failure-and-radiation.md`](../../architecture/09-failure-and-radiation.md).

This is a cross-stack signpost for the static-analysis, audit, and review gates SAKURA-II runs before merge. Commands live in [`CLAUDE.md`](../../../CLAUDE.md); enforcement policy lives in [`V&V-Plan §5.2`](../../mission/verification/V&V-Plan.md) and [`.claude/rules/security.md`](../../../.claude/rules/security.md). This guide tells you when each gate applies and how to wire a new one.

## 1. When this applies

- Adding a new PR check (lint, audit, secret scan).
- Enabling an existing gate on new code (`cppcheck` over a newly-authored app).
- Running the pre-merge sanity pass locally.

## 2. Existing gates (as of Phase B)

| Gate | Scope | Command | Enforcement |
|---|---|---|---|
| `cppcheck --enable=all` | `apps/**` (cFS / C) | `cppcheck --enable=all --std=c17 apps/` | Zero new findings ([`.claude/rules/security.md`](../../../.claude/rules/security.md)) |
| `cargo clippy -- -D warnings` | `rust/**` | `cargo clippy --all-targets -- -D warnings` | Zero warnings ([`.claude/rules/general.md`](../../../.claude/rules/general.md)) |
| `cargo audit` | `rust/**` dependency tree | `cargo audit` | Zero HIGH / CRITICAL ([`.claude/rules/security.md`](../../../.claude/rules/security.md)) |
| MISRA deviation comments | `apps/**` | grep for `MISRA C:2012 Rule` | Every deviation has justification ([`.claude/rules/general.md`](../../../.claude/rules/general.md)) |
| `-Werror -Wall -Wextra -pedantic` | `apps/**`, `simulation/**` | CMake flags | Zero warnings ([`CLAUDE.md §Coding Standards`](../../../CLAUDE.md)) |
| Unit tests (ctest / colcon / cargo) | All | [`CLAUDE.md §Build & Test`](../../../CLAUDE.md) | All green ([`V&V-Plan §5.4`](../../mission/verification/V&V-Plan.md)) |
| [`/security-review`](../../../.claude/skills/) skill | Comms, crypto, data buffers | Invoke skill | Required before merge on security-sensitive changes ([CLAUDE.md](../../../CLAUDE.md)) |
| [`/code-review`](../../../.claude/skills/) skill | Any PR | Invoke skill | Recommended pre-human-review ([CLAUDE.md](../../../CLAUDE.md)) |

## 3. What each gate protects

### 3.1 `cppcheck`

Memory and buffer safety — catches `strcpy` / `strcat` / `sprintf`, out-of-bounds indices, VLAs ([`.claude/rules/security.md`](../../../.claude/rules/security.md) §Memory & Buffer Safety).

### 3.2 `cargo clippy`

Rust idiom + safety lints — catches unnecessary `unwrap()`, incorrect trait impls, missing docs on public APIs. With `-D warnings` it converts every lint to an error ([`.claude/rules/general.md`](../../../.claude/rules/general.md)).

### 3.3 `cargo audit`

Supply-chain vulnerabilities — catches HIGH/CRITICAL CVEs in dependencies. Per [`.claude/rules/security.md`](../../../.claude/rules/security.md), open a tracking issue if a patched version doesn't exist.

### 3.4 MISRA deviations

Per [`.claude/rules/general.md`](../../../.claude/rules/general.md): every required-rule deviation has an inline comment:

```c
/* MISRA C:2012 Rule X.Y deviation: <reason tied to a constraint> */
```

No justification → no deviation. No deviation → fix the code.

### 3.5 Secret protection hook

[`protect-secrets.sh`](../../../.claude/hooks/protect-secrets.sh) refuses writes to `.env` / `*.pem` / `*.key` from within Claude Code. This is a hook, not a PR gate — it runs at edit time. See [`../troubleshooting.md §5.3`](../troubleshooting.md).

## 4. Running the full gate locally

Before requesting review:

```bash
# cFS / C
cppcheck --enable=all --std=c17 apps/
cmake --build build && ctest --test-dir build --output-on-failure

# Rust
cargo clippy --all-targets -- -D warnings
cargo test
cargo audit

# ROS 2
( cd ros2_ws && colcon build --symlink-install && colcon test )

# Skills (if applicable)
# /security-review    — on comms, crypto, buffer-touching changes
# /code-review        — MISRA + clippy focused review
```

All must be green. This mirrors the PR-gate baseline in [`../build-runbook.md §3.1`](../build-runbook.md).

## 5. Enabling a new gate

To wire a new gate (e.g. `clang-tidy` for ROS 2):

1. **Pick the enforcement point**: hook, Makefile target, or Phase-C CI workflow ([V&V-Plan §7.2](../../mission/verification/V&V-Plan.md)).
2. **Add the command to [`CLAUDE.md §Build & Test Commands`](../../../CLAUDE.md)** so it is discoverable.
3. **Update the rule**: if the gate enforces a policy, add it to the appropriate [`.claude/rules/*.md`](../../../.claude/rules/) file.
4. **Update [V&V-Plan §5.2](../../mission/verification/V&V-Plan.md)** to reflect the new gate.
5. **Deviations**: if a known-good rule has to be skipped, row in [`../../standards/deviations.md`](../../standards/deviations.md).

### 5.1 Hooks vs CI

Per [CLAUDE.md §Claude Code Features](../../../CLAUDE.md) and [V&V-Plan §7](../../mission/verification/V&V-Plan.md):

- Hooks are **fast, local, advisory** (format-on-save, secret-protect).
- CI is **slow, remote, authoritative** (full build + test + lint matrix). Phase-C target.
- Don't put slow gates in hooks (CI matrix belongs in CI).
- Don't make CI gates optional (per [CLAUDE.md §PR & Review Process](../../../CLAUDE.md), gates are mandatory).

## 6. Radiation-sensitive state and Q-F3

Per [`09 §5`](../../architecture/09-failure-and-radiation.md) and [`.claude/rules/cfs-apps.md`](../../../.claude/rules/cfs-apps.md), radiation-sensitive state in `apps/**` carries `__attribute__((section(".critical_mem")))` with a MISRA Rule 8.11 deviation comment. The Rust analogue is `Vault<T>` ([`09 §5.2`](../../architecture/09-failure-and-radiation.md)).

If you add radiation-sensitive state, the deviation rule applies. If you add a crate that holds such state, `Vault<T>` applies. Either way, [`/code-review`](../../../.claude/skills/) checks placement.

## 7. What this guide does NOT cover

- Formal-tier certification artifacts (DO-178C, full ECSS-Q-ST-80C) — out of scope per [V&V-Plan §1.2](../../mission/verification/V&V-Plan.md).
- Hardware qualification (radiation test campaigns, thermal vacuum) — out of scope.
- Penetration testing of the ground station — tracked as a Phase-C activity under `mission/verification/`.
