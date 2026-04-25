//! `ccsds_wire` — Q-C8 locus A. The only pure-Rust CCSDS BE pack/unpack
//! crate in the workspace. Populated by Phases 02–09 per
//! `docs/architecture/06-ground-segment-rust.md §1.1, §2`. Phase 03 lands
//! the first public item: the sealed [`Apid`] newtype (arch §2.2). Phase 04
//! extends the sealed-newtype pattern to `SequenceCount`, `PacketDataLength`,
//! `FuncCode`, `InstanceId`. Phase 05 adds the unified [`CcsdsError`] enum
//! (arch §2.8) and the [`PacketType`] discriminant (arch §2.4). Phase 06
//! adds the [`Cuc`] 7-byte BE time codec with fleet-pinned P-Field `0x2F`
//! (arch §2.3). Phase 07 adds the 6-byte [`PrimaryHeader`] BE codec (arch
//! §2.4). Phase 08 adds the 10-byte [`SecondaryHeader`] BE codec and the
//! `time_suspect` flag accessor (arch §2.5, §08-4).

// `forbid(unsafe_code)` escalates above the workspace `deny(unsafe_code)`
// so no child scope can re-enable unsafe via `#[allow]` — this is the BE
// codec's soundness anchor per arch §2.1. `deny(clippy::all)` is kept
// verbatim from arch §2.1 even though redundant with workspace policy.
// Arch §2.1 also mandates `deny(missing_docs)` + `warn(rust_2018_idioms)`;
// Phase 02 deferred those two because an empty crate has no items to bite.
// Phase 03 brings the first public item (`Apid`), so they land now.
#![forbid(unsafe_code)]
#![deny(clippy::all)]
#![deny(clippy::unwrap_used)]
#![deny(missing_docs)]
#![warn(rust_2018_idioms)]

pub mod apid;
pub mod builder;
pub mod cuc;
pub mod error;
pub mod packet;
pub mod packet_type;
pub mod primary;
pub mod primitives;
pub mod secondary;

pub use apid::Apid;
pub use builder::PacketBuilder;
pub use cuc::{Cuc, P_FIELD};
pub use error::CcsdsError;
pub use packet::SpacePacket;
pub use packet_type::PacketType;
pub use primary::PrimaryHeader;
pub use primitives::{FuncCode, InstanceId, PacketDataLength, SequenceCount};
pub use secondary::SecondaryHeader;
