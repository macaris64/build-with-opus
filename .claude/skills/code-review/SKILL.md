---
name: code-review
description: Perform a thorough code review of a branch or PR. Checks correctness, test coverage, performance, security, MISRA compliance, and adherence to project conventions. Use when asked to review code changes before merge.
argument-hint: "[branch-name-or-PR-number]"
allowed-tools: Bash(git diff *) Bash(git log *) Bash(gh pr *) Bash(cargo clippy *) Bash(cppcheck *) Read Grep Glob
model: claude-sonnet-4-6
effort: high
disable-model-invocation: true
---

Review the code changes in $ARGUMENTS.

## Step 1 — Get the diff

If it looks like a PR number (numeric):

```bash
gh pr view $ARGUMENTS --json title,body,additions,deletions,changedFiles,baseRefName
gh pr diff $ARGUMENTS
```

If it looks like a branch name:

```bash
git log main...$ARGUMENTS --oneline
git diff main...$ARGUMENTS
```

## Step 2 — Evaluate each changed file

For every changed file, assess:

**Correctness**
- Does the logic match the stated intent in the PR description or issue?
- Are edge cases handled: null/NULL pointers, empty buffers, zero, negative or maximum values?
- Are all error paths handled or explicitly documented as intentionally unhandled?

**Tests**
- Is there a test for the new behavior?
- Is there a test for the error/failure path?
- Are tests independent (no shared mutable state between cases)?

**Performance**
- Any unbounded loops over input that could stall a real-time task?
- Any hidden allocations in the FSW path (containers, string formatting)?

**Security**
- Any bounded-string functions (`snprintf`, `strncpy`) called with correct length arguments?
- Any raw array indexing without a preceding bounds check?
- (For deep security analysis, use `/security-review` instead)

**Convention adherence**
- Does the code follow the rules in `.claude/rules/`?
- Are naming conventions consistent with the surrounding code?

## Step 3 — MISRA C Checklist (for `.c` / `.h` files)

Flag any violation as Must Fix with the rule number:
- **Rule 8.1**: every function has an explicit return type declared
- **Rule 8.4**: an external object or function has a matching declaration visible
- **Rule 17.7**: return value of every non-void function is used or explicitly cast to void
- **Rule 18.8**: no variable-length arrays
- **Rule 21.3**: no use of `malloc`, `calloc`, `realloc`, or `free` family in `apps/`
- **Banned symbols**: `printf`, `OS_printf`, `sprintf`, `strcpy`, `strcat`, `gets` in `apps/` source

## Step 4 — Clippy / Rust Checklist (for `.rs` files)

Run and include output:
```bash
cargo clippy -- -D warnings 2>&1 | head -50
```
- `unsafe` block without `// SAFETY:` comment → **Must Fix**
- `unwrap()` on `Result` or `Option` outside `#[cfg(test)]` → **Must Fix**
- `#![allow(clippy::...)]` without a justification comment → **Should Fix**

## Output Format

```
## Summary
<2–3 sentence overview of the change and overall quality>

## Must Fix
- [file:line] Issue description + suggested fix

## Should Fix
- [file:line] Issue description + suggested direction

## Nits
- [file:line] Minor style or naming note

## Approved
Yes / No / Yes with minor fixes
```
