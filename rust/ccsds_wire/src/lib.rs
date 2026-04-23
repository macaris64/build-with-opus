//! `ccsds_wire` — Q-C8 locus A. The only pure-Rust CCSDS BE pack/unpack
//! crate in the workspace. Populated by Phases 02–09 per
//! `docs/architecture/06-ground-segment-rust.md §1.1, §2`. Phase 01 only
//! reserves the workspace-members slot so `cargo build --workspace`
//! succeeds; no public types land here yet.

#![forbid(unsafe_code)]
