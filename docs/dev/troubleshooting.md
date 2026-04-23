# Troubleshooting

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Build commands: [`../../CLAUDE.md`](../../CLAUDE.md). Quickstart: [`quickstart.md`](quickstart.md). Coding rules: [`../../.claude/rules/`](../../.claude/rules/).

This is the per-stack troubleshooting reference. If a build or test fails and your fix isn't here, open a task and append the new entry after you resolve it — letting a problem recur silently is how knowledge rots.

Known-good commands are documented in [`CLAUDE.md`](../../CLAUDE.md); this doc only covers **what to do when they fail**.

## 1. cFS / C (`apps/**`)

### 1.1 `-Werror` fails on a warning

**Symptom**: `cmake --build build` stops with `error: ... [-Werror=...]`.

**Do NOT** suppress with `-Wno-*`. `-Wall -Wextra -Werror -pedantic` is the baseline per [CLAUDE.md §Coding Standards](../../CLAUDE.md); every suppression is a silent coverage hole.

**Fix**:
- Read the warning class. If the warning is real (shadowed name, unused variable, signed/unsigned comparison) — fix the code.
- If the warning is a toolchain artefact (new GCC minor version emits a rule older ones did not), open a task, record the toolchain version, and choose: patch the code, or narrow the flag to a specific file with an inline rationale.
- MISRA-class findings get an inline deviation comment per [`.claude/rules/general.md`](../../.claude/rules/general.md):
  ```c
  /* MISRA C:2012 Rule X.Y deviation: <reason tied to a constraint or invariant> */
  ```

### 1.2 `ctest` reports "No tests were found"

**Symptom**: `ctest --test-dir build --output-on-failure` reports zero tests.

**Checks** (in order):
1. Root `CMakeLists.txt` contains `include(CTest)` before any `add_test()`. The repo currently has it at [`CMakeLists.txt` line 13](../../CMakeLists.txt); confirm it didn't regress.
2. The app has a `fsw/unit-test/` directory with a `*_test.c` file per [.claude/rules/testing.md](../../.claude/rules/testing.md).
3. Target was built in `Debug` mode, not `Release` (test binaries are gated off in some profiles).

If the app has no unit tests, that is the bug. Target is **100 % branch coverage** per [CLAUDE.md](../../CLAUDE.md); add at least one failure-path test.

### 1.3 `cppcheck` reports new findings

**Symptom**: `cppcheck --enable=all --std=c17 apps/` shows findings not present before a commit.

**Rule**: zero new findings before merge per [.claude/rules/security.md](../../.claude/rules/security.md).

**Fix**:
- Memory or buffer findings — fix, don't silence. `strncpy`/`snprintf` with explicit length, bounds-check before index.
- Style-only findings (naming, inline suggestions) may be suppressed with a comment-scoped suppression if the style is intentional.
- Do not edit `.cppcheck-suppress` globally to hide a whole class; the file is CI-visible.

### 1.4 cFE unit-test link failures

**Symptom**: tests link against real cFE symbols instead of stubs.

**Fix**: per [.claude/rules/testing.md](../../.claude/rules/testing.md), CFE/OSAL stubs are compiled under `#ifdef UNIT_TEST`. Confirm the test target has `-DUNIT_TEST` or its equivalent CMake flag, and that stubs do not contain business logic — they intercept at the CFE/OSAL boundary only.

## 2. ROS 2 (`ros2_ws/**`)

### 2.1 `colcon build` "Package 'X' not found"

**Symptom**: colcon reports a missing dependency at configure.

**Fix** (most common to least):
1. Source the ROS 2 environment first: `source /opt/ros/<distro>/setup.bash` (or the Space ROS overlay).
2. If the distro is sourced but a workspace-local package is still missing, try `colcon build --symlink-install` from a fresh `ros2_ws/install/` — a stale install can shadow workspace packages.
3. If the missing package is a Space ROS-specific one, confirm the Space ROS overlay sourced over stock ROS 2.

### 2.2 DDS discovery / nodes don't see each other

**Symptom**: a publisher and subscriber on the same topic don't exchange data.

**Checks**:
1. `ROS_DOMAIN_ID` matches across processes. Default is `0`; container bring-up may set a non-default id.
2. Multicast is available on `lo`. Docker containers by default block multicast unless a user-defined network is created with `--driver bridge`.
3. QoS profiles match — publisher's reliability and durability are compatible with the subscriber's. Per [.claude/rules/ros2-nodes.md](../../.claude/rules/ros2-nodes.md) (QoS profiles must be named constants at file scope, not inline), check the file-scope constants in both ends.

### 2.3 Lifecycle transitions stuck on `configure` or `activate`

**Symptom**: node transitions to `ErrorProcessing` or stays at `inactive`.

**Fix**:
- Per [.claude/rules/general.md](../../.claude/rules/general.md) callbacks must not block. If `on_configure` waits on a service or timer, it can deadlock against the executor.
- Offload non-trivial work to a separate thread; `on_configure` should only acquire resources and set up subscriptions / publishers.

### 2.4 Logs go missing (`std::cout` / `printf` used)

**Rule**: `std::cout` and `printf` are banned in `ros2_ws/` per [.claude/rules/general.md](../../.claude/rules/general.md). Use `RCLCPP_INFO(get_logger(), ...)` and friends.

If you find `std::cout` during review, that's a correctness bug, not a style one — host logs are not captured by ROS 2 bag / ros2cli tooling.

## 3. Rust (`rust/**`)

### 3.1 `cargo clippy -- -D warnings` fails

**Symptom**: a clippy lint fires on new code.

**Rule**: zero warnings, zero suppressions without a justification per [.claude/rules/general.md](../../.claude/rules/general.md).

**Fix**:
- Apply clippy's suggestion if it is behaviourally equivalent.
- If the lint is wrong for your case, suppress **locally** with an inline comment:
  ```rust
  #[allow(clippy::too_many_arguments)] // SAFETY: FFI shim, argument set pinned by C header
  ```
- Never blanket-suppress at the crate root. `#![deny(clippy::all)]` is the baseline per the same rule.

### 3.2 `cargo audit` reports HIGH or CRITICAL

**Symptom**: an advisory fires on a dependency.

**Fix** (per [.claude/rules/security.md](../../.claude/rules/security.md)):
1. Update the dependency if a patched version exists — `cargo update -p <crate>` or a `Cargo.toml` bump.
2. If no patched version exists, open a tracking issue and record it in [`../standards/deviations.md`](../standards/deviations.md).
3. Do not pass `--deny warnings` through `cargo audit` configuration to silence it; the CI gate explicitly checks for HIGH/CRITICAL.

### 3.3 `unwrap()` sneaks in

**Rule**: no `unwrap()` on `Result` / `Option` outside `#[cfg(test)]` per [.claude/rules/general.md](../../.claude/rules/general.md).

**Fix**: use `?` or explicit `match`. `expect("<invariant>")` is acceptable only when the invariant is genuinely infallible (e.g. a `const`-initialised value); write the invariant in the message.

### 3.4 `unsafe` without a SAFETY comment

**Rule**: every `unsafe` block requires a `// SAFETY:` comment per [.claude/rules/security.md](../../.claude/rules/security.md).

**Fix**: add the comment. The comment must state the invariant being upheld, not just "this is safe." If you can't articulate the invariant, the `unsafe` is unjustified.

## 4. Gazebo (`simulation/**`)

### 4.1 Plugin fails to load (`Failed to load plugin`)

**Symptom**: Gazebo logs `[Err] [Plugin.hh:...] Failed to load plugin ...`.

**Checks**:
1. Plugin shared library is on `LD_LIBRARY_PATH`, **or** the SDF `<plugin filename="...">` uses an absolute path.
2. Plugin was built against the same Gazebo major version as the running instance — Harmonic is the pinned version per [05 §2](../architecture/05-simulation-gazebo.md) and [standards/references.md](../standards/references.md).
3. `GZ_REGISTER_MODEL_PLUGIN(...)` line is present at file scope in the `.cpp`.

### 4.2 `OnUpdate()` causes physics lag

**Rule**: `OnUpdate()` must not block or allocate on the hot path per [05 §3.1](../architecture/05-simulation-gazebo.md) and [.claude/rules/general.md](../../.claude/rules/general.md).

**Fix**: pre-reserve vectors in `Load()`; move file or socket I/O to a sidecar thread; cache `rclcpp::Node` references — do not reconstruct per tick.

### 4.3 Harmonic vs Classic confusion

SAKURA-II uses **Gazebo Harmonic**, not Gazebo Classic. APIs differ. If a copy-pasted example uses `gazebo::common::Plugin` (Classic) instead of `gz::sim::System` or a Harmonic `ModelPlugin`, it will compile against the wrong headers. The canonical shape is [`simulation/gazebo_rover_plugin/include/rover_drive_plugin.h`](../../simulation/gazebo_rover_plugin/include/rover_drive_plugin.h).

## 5. Claude Code harness

### 5.1 MCP server prompts every session

**Symptom**: on each Claude Code launch, the three servers in `.mcp.json` request approval.

**Fix**: this is expected. If you trust the servers, allow-list them in `.claude/settings.local.json` (user-level settings). See [`mcp-setup.md`](mcp-setup.md) for the per-server operational notes.

### 5.2 Postgres MCP server can't reach the DB

**Symptom**: `postgres` MCP server errors out during startup.

**Fix**:
1. Confirm `DATABASE_URL` is set in `.env`. `.env` is gitignored and the [`protect-secrets.sh`](../../.claude/hooks/protect-secrets.sh) hook refuses writes from within Claude Code — if you need to edit it, do it from a terminal.
2. Confirm Node.js is installed (the server uses `npx @bytebase/dbhub`).
3. If you don't intend to use the server, deny it on first prompt or remove `DATABASE_URL` from `.env`.

### 5.3 Hook blocks a legitimate operation

**Symptom**: [`protect-secrets.sh`](../../.claude/hooks/protect-secrets.sh) or [`format-on-save.sh`](../../.claude/hooks/format-on-save.sh) refuses a save.

**Fix**: hooks are safety nets. Investigate **why** before bypassing:
- `protect-secrets.sh` blocked a `.env` / `*.pem` / `*.key` write — the hook is doing its job; edit from a terminal.
- `format-on-save.sh` failed a format — confirm `clang-format` / `rustfmt` is installed per [`quickstart.md §1`](quickstart.md).

Never edit hook scripts to skip a check in-band; edit the source file instead.

## 6. Skills to run when stuck

- [`/security-review`](../../.claude/skills/) — for anything touching comms, crypto, or data buffers. Also the right tool before merging touching `apps/**` or `rust/**`.
- [`/code-review`](../../.claude/skills/) — MISRA + clippy focused review. Run before requesting a human review.
- [`/fix-issue`](../../.claude/skills/) — guided fix flow for a tracked issue; orchestrates build + test + lint.

See [`../../.claude/skills/`](../../.claude/skills/) for the full skill catalogue.

## 7. When this doc is wrong

Inaccuracies here are bugs. If you run a command that should work per this doc and it doesn't, either:
- The command is broken — fix the code, not this doc.
- The environment assumption is wrong — tighten the assumption here (and in [`quickstart.md §1`](quickstart.md) if it's a prereq).
- A new gotcha — append a subsection, even if short. Three reports of the same mystery cost more than one short troubleshooting entry.
