---
name: create-pr
description: Create a well-structured pull request with a complete description, linked issues, test plan, and correct labels. Use whenever opening a PR — do not open PRs manually.
argument-hint: "[base-branch (default: main)]"
allowed-tools: Bash(git *) Bash(gh pr *) Bash(gh issue *) Bash(cmake *) Bash(ctest *) Bash(cargo *) Bash(colcon *) Bash(cppcheck *) Read Grep Glob
model: claude-sonnet-4-6
disable-model-invocation: true
---

Create a pull request targeting ${ARGUMENTS:-main}.

## Pre-flight checks

```bash
# C / cFS
cmake --build build && ctest --test-dir build --output-on-failure
cppcheck --enable=all --error-exitcode=1 apps/

# Rust
cargo test && cargo clippy -- -D warnings && cargo audit

# ROS 2 (if ros2_ws/ has changes)
cd ros2_ws && colcon build && colcon test
```

Fix any failures before proceeding. A PR with failing checks wastes reviewers' time.

## Gather context

```bash
git log ${ARGUMENTS:-main}...HEAD --oneline
git diff ${ARGUMENTS:-main}...HEAD --stat
```

Read the full diff to understand every change. Do not rely on commit messages alone.

## Compose the PR

**Title format:** `type(scope): concise description` — max 72 characters

**Body template:**

```
## What

<What this PR does and why — not how. 2–4 sentences.>

## How

<High-level approach, key design decisions, and alternatives considered and rejected>

## Testing

- [ ] C unit tests added/updated (`ctest` passes)
- [ ] Rust tests added/updated (`cargo test` passes)
- [ ] `cargo clippy -- -D warnings` — zero warnings
- [ ] `cppcheck` — zero new findings
- [ ] Manual smoke test: <describe the exact steps you took to verify the golden path>

## Checklist

- [ ] Follows coding conventions in CLAUDE.md
- [ ] MISRA deviations documented with inline justification comments
- [ ] No `unsafe` blocks without `// SAFETY:` comments
- [ ] No secrets or credentials in committed files
- [ ] Documentation updated if behavior changed
- [ ] Breaking changes documented (if any)

Closes #<issue-number>
```

## Open the PR

```bash
gh pr create \
  --title "<title>" \
  --body "$(cat <<'EOF'
<filled body from above>
EOF
)" \
  --base "${ARGUMENTS:-main}"
```

After creating, output the PR URL so the author can share it for review.
