# rust/ — Ground Segment & FFI Bindings

Rust stable, Cargo workspace. Ground-station TM ingest / TC uplink / CFDP, plus FSW-adjacent FFI for ground-side packet decoding.

## Layout

```
rust/
├── ground_station/         — TM ingest, TC uplink, CFDP (Class 1 now, Class 2 later), operator UI
├── cfs_bindings/           — bindgen FFI from Rust to cFS headers (ground-side decoders)
└── ccsds_wire/             — (planned) pure-Rust CCSDS pack/unpack (BE wire)
```

Per [Q-C3](../docs/standards/decisions-log.md) the CFDP implementation hides behind a single `CfdpProvider` trait so Class 2 can land later without caller-facing changes. Per [Q-C8](../docs/standards/decisions-log.md) `ccsds_wire` + `cfs_bindings` are the **only** Rust-side BE↔LE conversion loci — no ad-hoc `from_be_bytes` / `from_le_bytes` anywhere else.

## Build & Test

From the repo root:

```bash
cargo build --workspace
cargo test --workspace
cargo clippy --workspace -- -D warnings
cargo audit
```

Optional:

```bash
cargo tarpaulin --workspace --out Html     # coverage (Linux x86_64)
cargo fmt --all -- --check                  # formatting check
```

## Where to read more

- **Architecture** — [`../docs/architecture/06-ground-segment-rust.md`](../docs/architecture/06-ground-segment-rust.md) (crate boundaries, ingest/uplink pipelines, `CfdpProvider` trait, acceptance gate).
- **Protocol stack** — [`../docs/architecture/07-comms-stack.md`](../docs/architecture/07-comms-stack.md) (CCSDS layers, VC plan).
- **Coding rules** — [`../.claude/rules/general.md`](../.claude/rules/general.md) (Rust section), [`../.claude/rules/security.md`](../.claude/rules/security.md) (`unsafe`, `cargo audit`).

## Hard rules (non-exhaustive — see `.claude/rules/security.md`)

- Every `unsafe` block has a `// SAFETY:` comment explaining the invariant.
- No `unwrap()` on `Result` / `Option` outside `#[cfg(test)]`.
- `#![deny(clippy::all)]` at every crate root; suppressions need justification comments.
- `cargo audit` shows zero HIGH / CRITICAL advisories in CI.
- No new `u32::from_le_bytes` / `u16::from_le_bytes` in `rust/` outside `ccsds_wire` and `cfs_bindings` per [Q-C8](../docs/standards/decisions-log.md).
