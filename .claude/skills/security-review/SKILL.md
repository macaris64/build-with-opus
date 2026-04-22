---
name: security-review
description: Audit code changes for security vulnerabilities. Use when reviewing PRs, after adding new dependencies, or when modifying communication handlers, crypto, or data buffer processing. Pass a branch name, commit range, or leave empty to review HEAD~1.
argument-hint: "[branch-or-commit-range]"
allowed-tools: Bash(git diff *) Bash(git log *) Bash(cargo audit *) Bash(cppcheck *) Read Grep Glob
model: claude-opus-4-7
effort: high
disable-model-invocation: true
---

Run a security audit on the specified changes.

## Setup

```bash
git diff ${ARGUMENTS:-HEAD~1}
```

Check for dependency changes:

```bash
git diff ${ARGUMENTS:-HEAD~1} -- Cargo.toml Cargo.lock
```

If Rust dependencies were added or upgraded:

```bash
cargo audit --deny warnings
```

Check for C/CMake dependency changes:

```bash
git diff ${ARGUMENTS:-HEAD~1} -- CMakeLists.txt apps/*/CMakeLists.txt
```

If C deps changed, run static analysis:

```bash
cppcheck --enable=all --std=c17 --error-exitcode=1 apps/
```

## Review Checklist

Work through each category systematically. For every finding, note the file and line number.

### Buffer Safety (C/C++)
- Are all string operations using bounded functions (`snprintf`, `strncpy` with explicit length)?
- Are all array indices validated against the array bounds before use?
- Are there any calls to `strcpy`, `strcat`, `sprintf`, `gets`, or `printf` in `apps/` source?
- Does `cppcheck` report zero buffer-related findings on changed files?

### Integer Safety (C/C++)
- Are arithmetic results used as array indices or allocation sizes checked for overflow before use?
- Is unsigned subtraction guarded with a `>=` check before computing `a - b`?
- Are there any integer casts that could silently truncate a value?

### Memory Safety
- Does any FSW code under `apps/` call `malloc`, `calloc`, `realloc`, or `free`? (Must Fix)
- Does any ROS 2 callback use `new`/`delete` in the data path? (Should Fix — use smart pointers)
- Any Rust `unsafe` block without a `// SAFETY:` comment? (Must Fix)

### Rust-Specific
- Does `cargo audit` show zero HIGH/CRITICAL advisories?
- Are there any `unwrap()` or `expect()` calls on `Result`/`Option` in non-test production paths?
- Are all `#![allow(clippy::...)]` suppressions justified with a comment?

### Secrets & Credentials
- Are any API keys, tokens, passwords, or private keys hardcoded or logged?
- Are `.env` patterns followed? (no secrets in committed files)
- Are secrets excluded from error messages and telemetry responses? (`[REDACTED]` placeholder used?)

### Command / Data Injection (Ground Software)
- Is any user-supplied or network-supplied data used in shell commands without sanitization?
- Is any telemetry data used in SQL queries? Use parameterized queries if so.
- Is any external input inserted into log messages in a way that could cause log injection?

### Dependencies
- Did this PR add or upgrade packages? Flag any HIGH/CRITICAL `cargo audit` findings.

## Output Format

Structure findings as:

```
## CRITICAL — Exploitable now; block merge
[file:line] Description and exploit scenario

## HIGH — Fix before merge
[file:line] Description

## MEDIUM — Fix in follow-up ticket
[file:line] Description

## INFO — Suggestions for improvement
[file:line] Description

## Clean — Categories with no findings
(list categories that were reviewed and found clean)
```
