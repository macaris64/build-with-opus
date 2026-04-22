---
name: fix-issue
description: Fix a GitHub issue end-to-end. Reads the issue, implements the fix, writes tests, and prepares a commit. Use when asked to address a specific GitHub issue number.
argument-hint: "[issue-number]"
allowed-tools: Bash(gh issue *) Bash(git *) Bash(cmake *) Bash(ctest *) Bash(cargo *) Bash(colcon *) Bash(cppcheck *) Read Write Edit Grep Glob
model: claude-sonnet-4-6
effort: high
disable-model-invocation: true
---

Fix GitHub issue #$ARGUMENTS end-to-end.

## Step 1 — Understand the issue

```bash
gh issue view $ARGUMENTS
```

Read the full issue body, comments, and any linked issues before writing a single line of code.

## Step 2 — Explore the relevant code

Use Grep and Glob to locate the code related to the issue. Read the surrounding context — not just the file mentioned in the issue, but its callers and callee stubs too. Use `@researcher` if the scope is unclear.

## Step 3 — Create a branch

```bash
git checkout -b fix/$ARGUMENTS
```

## Step 4 — Implement the fix

Follow CLAUDE.md coding standards. Keep the change focused — fix only what the issue describes. Do not refactor unrelated code in the same commit.

## Step 5 — Write or update tests

Every bug fix needs a test that would have caught the bug originally. For cFS issues, add a CMocka test case. For Rust issues, add a `#[test]` in the relevant `#[cfg(test)]` module. For ROS 2 issues, add a `gtest` case.

## Step 6 — Verify

```bash
# C / cFS changes
cmake --build build && ctest --test-dir build --output-on-failure
cppcheck --enable=all --error-exitcode=1 apps/

# Rust changes
cargo test && cargo clippy -- -D warnings

# ROS 2 changes (if ros2_ws/ was modified)
cd ros2_ws && colcon build && colcon test
```

All checks must pass before committing.

## Step 7 — Commit

```bash
git add -p
git commit -m "fix: resolve #$ARGUMENTS - <one-line description of what was wrong>"
```

## Step 8 — Comment on the issue

```bash
gh issue comment $ARGUMENTS --body "Fixed in commit $(git rev-parse --short HEAD). <2-3 sentence summary of root cause and what changed>"
```
