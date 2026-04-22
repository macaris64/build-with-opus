# space-systems-boilerplate

Industry-standard Claude Code boilerplate for mission-critical space software. Demonstrates every Claude Code feature: skills, hooks, subagents, MCP servers, rules, output styles, and more. Fork this repo to bootstrap new projects targeting NASA cFS, Space ROS 2, Gazebo simulation, and Rust ground-support tools.

## Build & Test Commands

### cFS / C
- `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build` — full build
- `ctest --test-dir build --output-on-failure` — run all C unit tests
- `cppcheck --enable=all --std=c17 apps/` — static analysis

### ROS 2 / C++
- `colcon build --symlink-install` (from `ros2_ws/`) — build workspace
- `colcon test` (from `ros2_ws/`) — run ROS 2 package tests

### Rust
- `cargo build` — build all workspace members
- `cargo test` — run all Rust tests
- `cargo clippy -- -D warnings` — lint with zero tolerance
- `cargo audit` — dependency vulnerability scan
- `cargo tarpaulin --out Html` — coverage report (Linux x86_64)

## Stack

- Flight Software: C17, MISRA C:2012, NASA cFS (CFE, OSAL, PSP)
- Robotics: Space ROS 2 (ament/colcon), Lifecycle nodes, DDS QoS
- Simulation: Gazebo Harmonic plugins (C++17, ModelPlugin)
- Ground Software: Rust stable (Ferrocene-compatible target)
- Compiler flags: `-Wall -Wextra -Werror -pedantic` on all C/C++ targets

## Architecture

- `apps/` — cFS applications; one directory per app
  - `fsw/src/` — flight software source (C17, MISRA)
  - `fsw/unit-test/` — CMocka unit tests (100% branch coverage target)
- `ros2_ws/src/` — ROS 2 packages (C++17 lifecycle nodes, launch files)
- `simulation/` — Gazebo Harmonic plugins (C++ ModelPlugin subclasses)
- `rust/` — Cargo workspace; ground station tools and cFS FFI bindings
- `_defs/` — Mission-level CMake targets and compile-time configuration header
- `.claude/` — All Claude Code configuration (skills, agents, rules, hooks)

## Coding Standards

### C / FSW (applies under `apps/`)
- No dynamic memory allocation — `malloc`/`calloc`/`realloc`/`free` are banned in FSW
- All functions must have an explicit return type; all non-void functions return `int32_t` error codes
- No `printf` or `OS_printf` — use `CFE_EVS_SendEvent` for all runtime messages
- MISRA C:2012 required rules are enforced; deviations require an inline justification comment:
  `/* MISRA C:2012 Rule X.Y deviation: <reason> */`
- No variable-length arrays (MISRA Rule 18.8); stack depth must be statically bounded
- Bounds-check all array indices before use; use `snprintf`/`strncpy`, never `sprintf`/`strcpy`

### C++ / ROS 2 & Simulation (applies under `ros2_ws/` and `simulation/`)
- All nodes must subclass `rclcpp_lifecycle::LifecycleNode`; plain `rclcpp::Node` is banned
- Callbacks (subscription, timer, service) must not block
- Log via `get_logger()` only; `std::cout` and `printf` are banned
- QoS profiles defined as named constants at file scope; no inline `rclcpp::QoS(...)` at call sites

### Rust (applies under `rust/`)
- All code must pass `cargo clippy -- -D warnings` with zero suppressions (unless justified)
- `unsafe` blocks require a `// SAFETY:` comment explaining the invariant
- No `unwrap()` on `Result` or `Option` in non-test code — use `?` or explicit error handling
- `#![deny(clippy::all)]` at crate root

### General
- Prefer early returns; max 3 levels of nesting
- All magic numbers and magic strings become named constants
- Comments explain the *why* (regulatory constraints, timing invariants, hardware errata), not the *what*

## Conventions

- Commit format: `type(scope): description` (feat, fix, chore, docs, test, refactor)
- Branch names: `feat/short-description` or `fix/issue-number`
- Never commit `.env` files; use `.env.example` as the template
- Each cFS app must register at least one event type in its events header
- Every PR must pass `ctest` + `cargo test` + `cargo clippy` before requesting review

## PR & Review Process

- Use `/create-pr` skill when opening PRs
- Use the `@code-reviewer` agent for inline review (MISRA + Clippy focus) before requesting human review
- Use `/security-review` before merging anything that touches comms, crypto, or data buffers
- PRs require at least one approving review before merge to `main`

## MCP Servers (`.mcp.json`)

The project configures three team-shared MCP servers. On first run, Claude Code will prompt for approval:

- **github** — GitHub API access for issues, PRs, and code search
- **sentry** — Flight software anomaly tracking and performance monitoring
- **postgres** — Direct telemetry database queries (uses `DATABASE_URL` env var)

Set `DATABASE_URL` in your `.env` file before using the postgres server. Note: the postgres MCP server uses `npx @bytebase/dbhub` internally — Node.js is required for that server only.

## Claude Code Features in this Repo

| Feature | Location | Purpose |
|---|---|---|
| Skills | `.claude/skills/` | `/security-review`, `/fix-issue`, `/code-review`, `/create-pr`, `/update-deps` |
| Agents | `.claude/agents/` | `@code-reviewer`, `@researcher`, `@debugger`, `@architect` |
| Rules | `.claude/rules/` | Path-scoped conventions (cfs-apps, testing, ros2-nodes, security, general) |
| Hooks | `.claude/hooks/` | Auto-format on save (rustfmt/clang-format), secret protection, done notification |
| Commands | `.claude/commands/` | `/fix-issue`, `/standup` |
| Output styles | `.claude/output-styles/` | `concise`, `educational` |
| MCP servers | `.mcp.json` | GitHub, Sentry, Postgres |
| Settings | `.claude/settings.json` | Permissions, hooks config, model, statusLine |
