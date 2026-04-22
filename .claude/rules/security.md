---
# No paths field — security rules always apply, every session
---

# Security Guidelines

## Memory & Buffer Safety (C/C++)
- Never use `strcpy`, `strcat`, `sprintf`, or `gets` — use `strncpy`, `strncat`, `snprintf` with explicit length limits
- All array indices must be bounds-checked before use; out-of-bounds access is undefined behavior
- No variable-length arrays (VLAs) in FSW — stack depth must be statically bounded (MISRA C:2012 Rule 18.8)
- No dynamic allocation (`malloc`/`new`) in FSW flight path; document any sim-only or test-only exceptions
- `cppcheck --enable=all` must show zero new findings before a PR is merged

## Integer Safety (C/C++)
- Arithmetic results used as array indices or buffer sizes must be checked for overflow and underflow before use
- Unsigned subtraction that could wrap to a large value must be guarded: check `a >= b` before computing `a - b`

## Rust
- Every `unsafe` block must have a `// SAFETY:` comment explaining the invariant being upheld
- `cargo audit` must show zero HIGH or CRITICAL advisories; open a tracking issue for anything that cannot be updated immediately
- `#![allow(clippy::...)]` suppressions require a comment explaining why the suppression is justified

## Secrets & Credentials
- Never log tokens, passwords, API keys, command arguments that may contain sensitive values, or PII; use `[REDACTED]` placeholder
- Keep all secrets in environment variables; never hardcode credentials — not even in test files
- Secrets in error messages and API responses must be stripped before returning to callers

## General
- Use `crypto.randomUUID()` or equivalent CSPRNG for any ID or token generation; never use a non-cryptographic RNG
- Private keys and certificates go in `.env`-referenced paths; never in source files
- `cargo audit --deny warnings` and `cppcheck` must both pass in CI before merge
