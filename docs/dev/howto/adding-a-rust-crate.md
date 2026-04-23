# How-To: Add a Rust Crate to the Workspace

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Reference exemplars: [`rust/ground_station/`](../../../rust/ground_station/), [`rust/cfs_bindings/`](../../../rust/cfs_bindings/). Rules: [`../../../.claude/rules/security.md`](../../../.claude/rules/security.md), [`general.md`](../../../.claude/rules/general.md). Architecture: [`../../architecture/06-ground-segment-rust.md`](../../architecture/06-ground-segment-rust.md). Bibliography: [`../../standards/references.md`](../../standards/references.md).

This guide walks adding a new Cargo workspace member under [`rust/`](../../../rust/). Signpost style; rules live in `.claude/rules/`, architecture lives in `06-ground-segment-rust.md`.

## 1. When this applies

- Adding a new ground-segment crate (TM ingest, UI, CFDP implementation).
- Adding a new FFI binding layer (analogous to `cfs_bindings`).
- Adding the `ccsds_wire` crate ([Q-C8](../../standards/decisions-log.md) definition site).
- Adding the `vault` crate ([Q-F3](../../standards/decisions-log.md), [09 §5.2](../../architecture/09-failure-and-radiation.md)).

Do **not** use this guide for adding test-only utilities — those go under the owning crate's `tests/`, not as a workspace member.

## 2. Prerequisites

- `rustup` installed, stable channel active ([`../quickstart.md §1`](../quickstart.md)).
- `cargo-audit`, `cargo-tarpaulin` installed.
- You've read [`Cargo.toml`](../../../rust/ground_station/Cargo.toml) of one existing member.

## 3. Steps

### 3.1 Create the crate

```bash
cd rust
cargo new <crate_name> --lib   # or --bin for an executable
```

Most SAKURA-II crates are `--lib`. Binaries (e.g. `ground_station` as a UI) use `--bin`.

### 3.2 Wire into the workspace

Edit `rust/Cargo.toml` (workspace root) to add the new member:

```toml
[workspace]
members = [
    "cfs_bindings",
    "ground_station",
    "<crate_name>",
]
```

### 3.3 Crate-root lints

Per [`.claude/rules/general.md`](../../../.claude/rules/general.md) and [CLAUDE.md §Coding Standards](../../../CLAUDE.md), every crate has at its root (`src/lib.rs` or `src/main.rs`):

```rust
#![deny(clippy::all)]
```

Suppression requires an inline justification comment. Crate-wide `#[allow(...)]` without justification fails review.

### 3.4 Error handling discipline

Per [`.claude/rules/general.md`](../../../.claude/rules/general.md):

- `unwrap()` and `expect()` on `Result` / `Option` are banned outside `#[cfg(test)]`.
- Use `?` for propagation or `match` for explicit handling.
- `expect("<invariant>")` acceptable only for genuine invariants (e.g. `const`-initialised values) — document the invariant in the message.

### 3.5 Unsafe discipline

Per [`.claude/rules/security.md`](../../../.claude/rules/security.md): every `unsafe` block carries a `// SAFETY:` comment explaining the invariant being upheld.

```rust
// SAFETY: ptr is non-null and valid for `len` bytes — precondition of caller.
let slice = unsafe { std::slice::from_raw_parts(ptr, len) };
```

If you can't articulate the invariant, the `unsafe` is unjustified.

### 3.6 Radiation-sensitive state → `Vault<T>`

If the crate holds state that would cause mission-level consequences on a bit flip (CFDP transaction-id counters, filter tables, long-held config), wrap it per [`09 §5.2`](../../architecture/09-failure-and-radiation.md) and [`.claude/rules/general.md`](../../../.claude/rules/general.md). `Vault<T>` lands as a sibling crate ([`rust/vault/`](../../../rust/)); its trait shape is fixed in [09 §5.2](../../architecture/09-failure-and-radiation.md).

### 3.7 Dependency hygiene

Add only what you need. Avoid:
- Async runtimes for crates that don't need them (`tokio` pulls in a lot).
- Derive macros that bloat compile time unless the ergonomic win is real.
- Deps with unmaintained upstream (check `cargo audit`).

Per [`.claude/rules/security.md`](../../../.claude/rules/security.md): `cargo audit` must be green on HIGH/CRITICAL.

### 3.8 Tests

Per [`.claude/rules/testing.md`](../../../.claude/rules/testing.md):

- In-module `#[cfg(test)]` for unit tests.
- Integration tests under `tests/` at crate root.
- Parsers / encoders that accept unbounded input get `proptest` property tests.
- At least one failure-path test.

### 3.9 Lint + audit + coverage

Before commit:

```bash
cargo clippy --all-targets -- -D warnings
cargo test
cargo audit
cargo tarpaulin --out Html     # Linux x86_64 only; occasional
```

All must be green per [`.claude/rules/security.md`](../../../.claude/rules/security.md) and the PR gate in [CLAUDE.md](../../../CLAUDE.md).

### 3.10 Update architecture doc

If the crate is load-bearing (not a throwaway internal utility), add it to [`06-ground-segment-rust.md`](../../architecture/06-ground-segment-rust.md)'s crate-boundary discussion and cross-link from [REPO_MAP.md](../../REPO_MAP.md).

## 4. Checklist before PR

- [ ] Added to `rust/Cargo.toml` workspace members
- [ ] `#![deny(clippy::all)]` at crate root
- [ ] No `unwrap()` outside `#[cfg(test)]`
- [ ] Every `unsafe` has a `// SAFETY:` comment
- [ ] Radiation-sensitive state in `Vault<T>` (if applicable)
- [ ] `cargo clippy -- -D warnings` clean
- [ ] `cargo audit` clean (HIGH/CRITICAL)
- [ ] At least one failure-path test
- [ ] Cross-linked from `06-ground-segment-rust.md` if load-bearing

## 5. Troubleshooting

Clippy lints: [`../troubleshooting.md §3.1`](../troubleshooting.md). Audit failures: [`../troubleshooting.md §3.2`](../troubleshooting.md). `unsafe` without SAFETY: [`../troubleshooting.md §3.4`](../troubleshooting.md).

## 6. What this guide does NOT cover

- FFI design principles — see [`../../architecture/06-ground-segment-rust.md`](../../architecture/06-ground-segment-rust.md).
- CCSDS wire-format encoding — see [`07-comms-stack.md`](../../architecture/07-comms-stack.md) and (when authored) the `ccsds_wire` crate.
- Ferrocene / safety-critical compilation — tracked for program-surrogate maturity, out of scope for Phase B.
