# Quickstart — 10-minute Bring-up

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Repo layout: [../REPO_MAP.md](../REPO_MAP.md). Build commands: the authoritative source is [`CLAUDE.md`](../../CLAUDE.md); this doc orchestrates them.

Target: a fresh clone of `build-with-opus` goes from nothing to a successful multi-layer build in ≤ 10 minutes, on Linux, with documented fallbacks if any toolchain is missing. This is the Phase A **acceptance gate** — the gate closes when a developer who did not write the quickstart runs through it end-to-end without unplanned detours.

## 0. Status of this quickstart

Current Phase A scope: verify local builds of the three existing stacks (cFS/C, ROS 2, Rust) and confirm the Claude Code harness is discoverable. **Not yet in scope**: Docker orchestration, Gazebo live scenario, ground-station TM ingest — those land when [`docker-runbook.md`](docker-runbook.md), [`simulation-walkthrough.md`](simulation-walkthrough.md), and the `howto/*` guides land in Phase B.

Where a step is not yet executable against current repo content, it is marked **[Phase B]** and a placeholder command is given.

## 1. Prerequisites

Minimum Linux host. Versions below are known-good; newer is usually fine.

| Tool | Version | Purpose | Install hint |
|---|---|---|---|
| `git` | ≥ 2.30 | Clone + submodules (cFS added as submodule in Phase B) | system package manager |
| `cmake` | ≥ 3.20 | Root C/C++ build ([`CMakeLists.txt`](../../CMakeLists.txt) line 1) | system package manager |
| GCC or Clang (C17/C++17) | GCC ≥ 10, Clang ≥ 12 | `-Wall -Wextra -Werror -pedantic` must pass | system package manager |
| `ctest` | bundled with CMake | C unit tests | bundled |
| Rust toolchain (`rustup`) | stable, current | `rust/` workspace | `rustup.rs` |
| `cargo-audit`, `cargo-tarpaulin` | latest | Security + coverage (per `.claude/rules/security.md` and `.claude/rules/testing.md`) | `cargo install` |
| `cppcheck` | ≥ 2.10 | C static analysis gate | system package manager |
| `clang-format`, `rustfmt` | any | Triggered by `.claude/hooks/format-on-save.sh` on save | system package manager / `rustup component add rustfmt` |
| Space ROS 2 | current distro | `ros2_ws/` build + test | `space-ros.org` install guide |
| `colcon` | bundled with ROS 2 | ROS 2 workspace build | bundled |
| Gazebo Harmonic | current | Simulation plugins | `gazebo-harmonic` package or upstream |
| Docker + `docker compose` v2 | latest | [Phase B] orchestration | vendor install |
| Node.js | LTS | Only required by the Postgres MCP server via `npx @bytebase/dbhub` — not required for builds | `nvm` recommended |

If any of these are missing, the corresponding build step will report which toolchain it wanted; treat that as the actionable error. Do **not** skip a step by deleting it — open a task and document the gap.

## 2. Clone and enter the repo

```bash
git clone <your-fork-url> build-with-opus
cd build-with-opus
```

In Phase B the cFS submodule will be wired (under `apps/cfs/` or `third_party/cfs/` — CM decision pending). Until then, there is no `git submodule update --init` to run.

## 3. Claude Code harness sanity check

This repo is a Claude Code boilerplate. Confirm the harness is discoverable:

```bash
ls .claude/
# Expect: agents/ commands/ hooks/ rules/ skills/ settings.json output-styles/
cat .claude/settings.json | head -5
```

Optionally, when starting Claude Code in this directory, `.mcp.json` will prompt for approval of three MCP servers (`github`, `sentry`, `postgres`). Deny/approve at your discretion; **only the `postgres` server needs `DATABASE_URL` in a `.env` file** — set it only if you intend to use the telemetry DB. The `.env.example` template does not yet exist (tracked; pending Phase B infra). For now, create `.env` manually:

```bash
# Only needed if the Postgres MCP server is approved.
echo 'DATABASE_URL=postgres://user:pass@localhost:5432/telemetry' > .env
```

`.env` is gitignored. The [`protect-secrets.sh`](../../.claude/hooks/protect-secrets.sh) hook refuses any write to `.env` / `*.pem` / `*.key` from within Claude Code — this is intentional.

## 4. Build the C/cFS stack

From the repo root:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Expected output from [`_defs/targets.cmake`](../../_defs/targets.cmake):

```
-- Mission: SAKURA_II  SCID: 42  Apps: sample_app
```

SCID must match [`_defs/mission_config.h`](../../_defs/mission_config.h); fleet SCID allocation anchors on `SAKURA_II_SCID_BASE` (see [../interfaces/apid-registry.md](../interfaces/apid-registry.md) §Identifiers). The APID/MID linter (`scripts/apid_mid_lint.py`) enforces the registry ↔ header consistency.

Run C unit tests:

```bash
ctest --test-dir build --output-on-failure
```

All tests under `apps/*/fsw/unit-test/` should pass. Failing at this stage is a blocker for Phase A sign-off.

Static analysis (one-shot sanity check; the full gate is in CI once wired):

```bash
cppcheck --enable=all --std=c17 apps/
```

`cppcheck` should produce **zero findings** per [`.claude/rules/security.md`](../../.claude/rules/security.md) §Memory & Buffer Safety.

## 5. Build the Rust stack

```bash
cargo build
cargo test
cargo clippy --all-targets -- -D warnings
```

`-D warnings` is non-negotiable per [`.claude/rules/general.md`](../../.claude/rules/general.md) Rust section. If clippy reports suppressions, each one must carry a justification comment per the same rule.

Security + coverage check (occasional, not every build):

```bash
cargo audit
cargo tarpaulin --out Html
```

`cargo audit` must be green on HIGH/CRITICAL per [`.claude/rules/security.md`](../../.claude/rules/security.md) §Rust.

## 6. Build the ROS 2 workspace

```bash
cd ros2_ws
colcon build --symlink-install
colcon test
cd ..
```

`--symlink-install` lets you iterate on Python / launch files without rebuilding. Tests run the `rover_teleop` / `rover_bringup` unit tests.

Lifecycle-node check (per [`.claude/rules/ros2-nodes.md`](../../.claude/rules/ros2-nodes.md)): any new node must subclass `rclcpp_lifecycle::LifecycleNode`. Violations are caught by review, not by build.

## 7. Build the Gazebo plugin

```bash
cmake --build build --target gazebo_rover_plugin
```

(Already built by the root `cmake --build build` in step 4; this target-specific invocation is for when you change the plugin in isolation.)

## 8. **[Phase B]** Bring up the minimal docker-compose profile

Placeholder until `docker-compose.yml` and the `minimal` profile land:

```bash
# docker compose --profile minimal up     # [Phase B]
```

When this command exists, it should spin up: 1 orbiter container, 1 relay container, 1 land rover container, 1 UAV container, 1 cryobot container, 1 Gazebo container, 1 ground-station container. The operator UI reachable at `http://localhost:8080` shows TM arriving within ~10 minutes of simulated light-time — expected behavior, documented in [`../mission/conops/ConOps.md`](../mission/conops/ConOps.md) §4.

## 9. **[Phase B]** Run the end-to-end scenario

Placeholder:

```bash
# make scenario SCN-NOM-01     # [Phase B]
```

Replays the nominal surface-ops HK scenario from [`../mission/conops/ConOps.md`](../mission/conops/ConOps.md) §4. Expected result: rover HK arriving at the ground station with end-to-end latency within the modeled light-time band, no SPP sequence-counter gaps.

## 10. Troubleshooting

Top gotchas; full list in [`troubleshooting.md`](troubleshooting.md) (planned):

- **`-Werror` fails on a harmless warning.** Do not suppress. Fix the warning or, if the warning is a genuine toolchain artefact, open a task and record it — never sprinkle `-Wno-*` on the tree.
- **`ctest` reports `No tests were found`.** Ensure `include(CTest)` in the root `CMakeLists.txt` ran before any `add_test()` was issued (it does, in [`CMakeLists.txt`](../../CMakeLists.txt) line 13). If an app has no unit tests yet, that is the bug — per [`.claude/rules/testing.md`](../../.claude/rules/testing.md) the 100% branch-coverage target requires at least one test per app.
- **`colcon build` fails with "Package 'X' not found".** Run `source /opt/ros/<distro>/setup.bash` in the shell first, or use the ROS 2 container image.
- **MCP server prompts on every session.** That is correct — MCP servers in `.mcp.json` require per-scope approval. You can allow them in `.claude/settings.local.json` if you trust the server config.

## 11. What success looks like for Phase A gate

Run these four commands cleanly, in this order, from a fresh clone:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure
cargo build && cargo test && cargo clippy -- -D warnings
( cd ros2_ws && colcon build --symlink-install && colcon test )
```

If all four exit 0, Phase A scaffolding is working. Log the timing — if the walk-through took more than 10 active minutes (excluding the first ROS 2 install which is one-time), update this doc rather than accepting the drift.
