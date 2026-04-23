# Build Runbook

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Authoritative commands: [`../../CLAUDE.md`](../../CLAUDE.md) — this runbook orchestrates them, never restates. Quickstart: [`quickstart.md`](quickstart.md). Troubleshooting: [`troubleshooting.md`](troubleshooting.md). Coding rules: [`../../.claude/rules/`](../../.claude/rules/). Repo layout: [`../REPO_MAP.md`](../REPO_MAP.md).

This doc fixes the **build discipline** across SAKURA-II's four stacks: when to run what, how to order builds for iteration vs clean slate, how the static-analysis loop fits, and what the PR-gate baseline is. It is the reference a contributor cites after reading [`quickstart.md`](quickstart.md) and needing to work efficiently.

If a command below disagrees with [`CLAUDE.md`](../../CLAUDE.md), `CLAUDE.md` wins and this doc is out-of-date.

## 1. Prerequisites

The [`quickstart.md §1`](quickstart.md) prerequisite table is authoritative. This section adds **build-time only** hints that don't belong in quickstart:

- **`ccache`** (optional) — drops rebuild time by 5–10×. Enable with `export CC="ccache gcc"` before `cmake -B build`.
- **`CMAKE_BUILD_PARALLEL_LEVEL`** — `cmake --build build` obeys this env var. Set to number of cores.
- **`RUSTFLAGS`** — avoid setting globally; crate-level rustflags belong in `.cargo/config.toml` when needed.
- **`ROS_LOG_DIR`** — point to `/tmp/ros` during bulk rebuilds to avoid filling `~/.ros/log`.

## 2. Four-stack overview

| Stack | Directory | Tool | Typical iteration build | Clean build |
|---|---|---|---|---|
| cFS / C | [`apps/**`](../../apps/) | CMake + CTest | `cmake --build build` | `rm -rf build && cmake -B build && cmake --build build` |
| ROS 2 | [`ros2_ws/`](../../ros2_ws/) | colcon | `colcon build --symlink-install --packages-select <pkg>` | `rm -rf ros2_ws/{build,install,log} && colcon build --symlink-install` |
| Rust | [`rust/`](../../rust/) | cargo | `cargo build -p <crate>` | `cargo clean && cargo build` |
| Gazebo | [`simulation/**`](../../simulation/) | CMake (under root build) | `cmake --build build --target <plugin>` | same as cFS clean |

Commands themselves are in [`CLAUDE.md §Build & Test Commands`](../../CLAUDE.md); this runbook exists to answer *when* and *why* to use each.

## 3. Build orderings

### 3.1 Sequential (most correct)

When in doubt, use the sequence [`quickstart.md §11`](quickstart.md) walks through:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure
cargo build && cargo test && cargo clippy -- -D warnings
( cd ros2_ws && colcon build --symlink-install && colcon test )
```

Each step runs to completion before the next. If any fails, stop and fix before continuing. This is the PR-gate baseline per [`../../CLAUDE.md §PR & Review Process`](../../CLAUDE.md).

### 3.2 Parallel (faster on multi-core)

The four stacks are independent. If your terminal supports job control:

```bash
cmake --build build &
cargo build &
( cd ros2_ws && colcon build --symlink-install ) &
wait
```

Tests still serialise per stack. Don't parallelise tests across stacks — flaky output attribution gets hard.

### 3.3 Iteration (fast inner loop)

Editing a single cFS app:
```bash
cmake --build build --target <app_name>
ctest --test-dir build -R <app_name>
```

Editing a single Rust crate:
```bash
cargo build -p <crate>
cargo test -p <crate>
```

Editing one ROS 2 package:
```bash
( cd ros2_ws && colcon build --symlink-install --packages-select <pkg> )
( cd ros2_ws && colcon test --packages-select <pkg> )
```

Iteration loops skip clippy and audit — those belong in the pre-commit pass (§5), not the inner loop.

## 4. Build types and artefacts

### 4.1 CMake build types

| Type | Flags | Use |
|---|---|---|
| `Debug` | `-O0 -g` | Default; unit tests expect this |
| `RelWithDebInfo` | `-O2 -g` | Performance sanity check |
| `Release` | `-O2 -DNDEBUG` | Pre-release validation only |

All of the above retain `-Wall -Wextra -Werror -pedantic` per [`CLAUDE.md §Coding Standards`](../../CLAUDE.md). `-Werror` does not go away in `Release`.

### 4.2 Artefact locations

| Stack | Outputs | Intermediate / cache |
|---|---|---|
| cFS / C | `build/apps/<app>/` | `build/CMakeFiles/` |
| Gazebo | `build/simulation/*/lib*.so` | same |
| Rust | `rust/target/{debug,release}/` | `rust/target/debug/deps/` |
| ROS 2 | `ros2_ws/install/<pkg>/` | `ros2_ws/build/<pkg>/`, `ros2_ws/log/` |

Clean by deleting the output directory of the affected stack; never blow away the whole `build/` unless you're doing a cFS clean.

### 4.3 CI build expectation (target state)

CI (per [V&V-Plan §7.2](../mission/verification/V&V-Plan.md)) will run all four clean builds in parallel matrix jobs with `-DCMAKE_BUILD_TYPE=Debug`. No `ccache`. The PR-gate state is the full sequential build — treat your local iteration as optimisation on top of that.

## 5. Static-analysis loop

Every PR must pass static analysis before merge per [`../../CLAUDE.md §PR & Review Process`](../../CLAUDE.md) and [V&V-Plan §5.2](../mission/verification/V&V-Plan.md). Run these before requesting review:

### 5.1 cFS / C

```bash
cppcheck --enable=all --std=c17 apps/
```

**Zero new findings** per [`.claude/rules/security.md`](../../.claude/rules/security.md). Existing findings are either accepted (tracked) or treated as open bugs.

### 5.2 Rust

```bash
cargo clippy --all-targets -- -D warnings
cargo audit
```

- `-D warnings` is non-negotiable per [`.claude/rules/general.md`](../../.claude/rules/general.md).
- `cargo audit` must be green on HIGH/CRITICAL per [`.claude/rules/security.md`](../../.claude/rules/security.md).
- Each lint suppression requires a `#[allow(clippy::...)]` with an inline justification comment.

### 5.3 ROS 2 / C++

No enforced analyser in Phase B. clang-tidy integration is tracked as a Phase-C open item in [V&V-Plan §10](../mission/verification/V&V-Plan.md). Manual review against [`.claude/rules/ros2-nodes.md`](../../.claude/rules/ros2-nodes.md) is the baseline.

### 5.4 Coverage (occasional, not every PR)

```bash
cargo tarpaulin --out Html          # Rust; Linux x86_64 only
ctest --test-dir build               # C; use gcov / llvm-cov post-run
```

Target is 100 % branch coverage on C and Rust per [`.claude/rules/testing.md`](../../.claude/rules/testing.md). Missing coverage must be tracked in [`../standards/deviations.md`](../standards/deviations.md).

## 6. Incremental troubleshooting

When a build fails, match the error class against [`troubleshooting.md`](troubleshooting.md):

| Symptom | Section |
|---|---|
| `-Werror` kills a build | [`troubleshooting.md §1.1`](troubleshooting.md) |
| `ctest` finds no tests | [`troubleshooting.md §1.2`](troubleshooting.md) |
| `colcon build` missing dependency | [`troubleshooting.md §2.1`](troubleshooting.md) |
| `cargo clippy` fires | [`troubleshooting.md §3.1`](troubleshooting.md) |
| Gazebo plugin won't load | [`troubleshooting.md §4.1`](troubleshooting.md) |

Do not silence a failure to "make the build green." Track root cause.

## 7. Reset and recovery

### 7.1 Stale cache

CMake caches configure-time values. If toolchain versions change, or you edit `_defs/targets.cmake`, rerun:

```bash
cmake -B build --fresh
```

### 7.2 ROS 2 workspace confusion

If `colcon` reports mysterious package / overlay issues, nuke the workspace:

```bash
rm -rf ros2_ws/{build,install,log}
```

This is faster than debugging overlay ordering.

### 7.3 Rust target drift

`cargo` almost never needs a full clean; if you suspect stale artefacts after a toolchain update:

```bash
cargo clean
```

### 7.4 Git-worktree isolation

If you want to try a disruptive change without wrecking your working tree, use `git worktree add ../sakura-experiment <branch>` rather than stashing. Worktrees get independent `build/` / `target/` / `ros2_ws/build/` directories and isolate each other's CMake caches.

## 8. What this runbook is NOT

- Not a command reference. [`CLAUDE.md §Build & Test Commands`](../../CLAUDE.md) owns the commands.
- Not a coding rulebook. Rules live in [`../../.claude/rules/`](../../.claude/rules/).
- Not a Docker runbook. Container build and orchestration go in [`docker-runbook.md`](docker-runbook.md).
- Not a CI configuration. CI workflow is tracked as a Phase-C open item in [V&V-Plan §7](../mission/verification/V&V-Plan.md).
- Not a benchmark harness. Performance regressions are a separate discipline.
