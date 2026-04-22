---
name: update-deps
description: Safely update Rust dependencies and audit for vulnerabilities. For C/CMake dependencies, checks cppcheck for new findings after any version bump. Use for routine dependency maintenance.
allowed-tools: Bash(cargo *) Bash(cmake *) Bash(ctest *) Bash(cppcheck *) Bash(rosdep *) Bash(git *) Read Write
model: claude-sonnet-4-6
disable-model-invocation: true
---

Safely update project dependencies.

## Step 1 — Baseline audit

```bash
cargo audit
cargo outdated
```

Record the baseline vulnerability count and list of outdated crates before making any changes.

## Step 2 — Rust patch updates (safest)

```bash
cargo update
cargo test
cargo clippy -- -D warnings
```

If all checks pass:

```bash
git add Cargo.lock
git commit -m "chore(deps): cargo update (patch)"
```

If tests fail: `git checkout -- Cargo.lock` and investigate before proceeding.

## Step 3 — Rust minor/major updates (one crate at a time)

For each outdated crate flagged by `cargo outdated`:
1. Check the crate's CHANGELOG or release notes for breaking changes
2. Update the version in the relevant `Cargo.toml`
3. Run: `cargo build && cargo test && cargo clippy -- -D warnings`
4. If green, commit: `git commit -m "chore(deps): upgrade <crate> to vX.Y.Z"`
5. If red, revert and open a separate issue for that crate

## Step 4 — C/CMake dependency pins

CMake `FetchContent` or `ExternalProject` dependencies are pinned by git hash or version tag in `CMakeLists.txt` files. For each pin to update:
1. Find the new stable tag or commit hash in the upstream repo
2. Update the hash/tag in the relevant `CMakeLists.txt`
3. Run: `cmake --build build && ctest --test-dir build --output-on-failure`
4. Run: `cppcheck --enable=all --error-exitcode=1 apps/` — zero new findings required

## Step 5 — ROS 2 system dependencies

```bash
rosdep update
rosdep install --from-paths ros2_ws/src --ignore-src -r
cd ros2_ws && colcon build && colcon test
```

## Step 6 — Final audit

```bash
cargo audit
```

## Summary output

Provide a brief summary:
- Rust crates updated (count by patch / minor / major)
- CMake pins updated (count)
- Final test status for each language layer
- Outstanding HIGH/CRITICAL vulnerabilities (if any) — open issues for those
- Crates/pins skipped due to breaking changes — note which ones need follow-up
