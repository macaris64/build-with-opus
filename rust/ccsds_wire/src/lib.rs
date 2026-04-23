//! `ccsds_wire` — Q-C8 locus A. The only pure-Rust CCSDS BE pack/unpack
//! crate in the workspace. Populated by Phases 02–09 per
//! `docs/architecture/06-ground-segment-rust.md §1.1, §2`. Phase 03 lands
//! the first public item: the sealed [`Apid`] newtype (arch §2.2).

// `forbid(unsafe_code)` escalates above the workspace `deny(unsafe_code)`
// so no child scope can re-enable unsafe via `#[allow]` — this is the BE
// codec's soundness anchor per arch §2.1. `deny(clippy::all)` is kept
// verbatim from arch §2.1 even though redundant with workspace policy.
// Arch §2.1 also mandates `deny(missing_docs)` + `warn(rust_2018_idioms)`;
// Phase 02 deferred those two because an empty crate has no items to bite.
// Phase 03 brings the first public item (`Apid`), so they land now.
#![forbid(unsafe_code)]
#![deny(clippy::all)]
#![deny(missing_docs)]
#![warn(rust_2018_idioms)]

pub mod apid;
pub mod error;
pub mod packet_type;
pub mod primitives;

pub use apid::Apid;
pub use error::CcsdsError;
pub use packet_type::PacketType;
pub use primitives::{FuncCode, InstanceId, PacketDataLength, SequenceCount};
