//! `ccsds_wire` — Q-C8 locus A. The only pure-Rust CCSDS BE pack/unpack
//! crate in the workspace. Populated by Phases 02–09 per
//! `docs/architecture/06-ground-segment-rust.md §1.1, §2`. Phase 01 only
//! reserves the workspace-members slot so `cargo build --workspace`
//! succeeds; no public types land here yet.

// `forbid(unsafe_code)` escalates above the workspace `deny(unsafe_code)`
// so no child scope can re-enable unsafe via `#[allow]` — this is the BE
// codec's soundness anchor per arch §2.1. `deny(clippy::all)` is kept
// verbatim from arch §2.1 even though redundant with workspace policy.
// Arch §2.1 also mandates `deny(missing_docs)` + `warn(rust_2018_idioms)`;
// those land alongside the first public item (they would fire on empty
// crate roots but have no items to bite).
#![forbid(unsafe_code)]
#![deny(clippy::all)]
