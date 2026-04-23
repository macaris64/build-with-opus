//! CCSDS Space Packet Protocol **primary header** (6 bytes, big-endian).
//!
//! Phase 07 locus. This file is the first end-to-end BE codec in the crate —
//! it composes the sealed newtypes from Phases 03–04 ([`crate::Apid`],
//! [`crate::SequenceCount`], [`crate::PacketDataLength`]) and the
//! [`crate::PacketType`] discriminant from Phase 05 into a 48-bit on-wire
//! frame, then enforces the §2.4 decode-time guards against it.
//!
//! **Red-step stub.** The public shape below matches the Phase 07 plan
//! (plan §4). `encode` is a no-op and `decode` returns a sentinel
//! [`CcsdsError::BufferTooShort`] unconditionally so the Red test in this
//! module's `tests` sub-module fails loudly via `assert_eq!` (the decoder
//! reports the wrong error variant). The Green commit replaces the stub
//! body with the real bit-packed codec.
//!
//! Definition sites:
//! - [`docs/architecture/06-ground-segment-rust.md §2.4`](../../../docs/architecture/06-ground-segment-rust.md) — shape + guards
//! - [`docs/architecture/06-ground-segment-rust.md §2.9`](../../../docs/architecture/06-ground-segment-rust.md) — silent-LE guard
//! - [`docs/architecture/07-comms-stack.md §2`](../../../docs/architecture/07-comms-stack.md) — primary-header field table
//! - [`docs/interfaces/apid-registry.md`](../../../docs/interfaces/apid-registry.md) — primary-header value table
//! - `SYS-REQ-0020`, `SYS-REQ-0022`; `Q-C8`.

use crate::{Apid, CcsdsError, PacketDataLength, PacketType, SequenceCount};

/// CCSDS Space Packet Protocol primary header.
///
/// Sealed struct — all fields are private. The only paths to a valid
/// `PrimaryHeader` are [`PrimaryHeader::new`] (from already-validated
/// sealed newtypes) and [`PrimaryHeader::decode`] (from wire bytes, which
/// enforces every §2.4 invariant at the type boundary). The only path back
/// to wire bytes is [`PrimaryHeader::encode`]. No public `u16`/`u32` field
/// exists — part of the Q-C8 grep-visible endianness-conversion discipline
/// (§2.9). Downstream code never re-validates these invariants.
///
/// `Copy` because the struct is 12 bytes of plain-old-data (two sealed
/// newtypes backed by `u16`, one backed by `u16`, one enum `#[repr(u8)]`);
/// cheap to pass by value.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct PrimaryHeader {
    apid: Apid,
    packet_type: PacketType,
    sequence_count: SequenceCount,
    data_length: PacketDataLength,
}

impl PrimaryHeader {
    /// Encoded length on the wire, in octets. Fixed by CCSDS 133.0-B-2.
    pub const LEN: usize = 6;

    /// Compose a primary header from already-validated sealed newtypes.
    ///
    /// Infallible because every input type has already been validated at
    /// its own sealed boundary (`Apid::new`, `SequenceCount::new`,
    /// `PacketDataLength::new`); `packet_type` is a typed enum, not a raw
    /// bit. This is the constructor the Phase 09 `PacketBuilder` will call.
    #[must_use]
    pub const fn new(
        apid: Apid,
        packet_type: PacketType,
        sequence_count: SequenceCount,
        data_length: PacketDataLength,
    ) -> Self {
        Self {
            apid,
            packet_type,
            sequence_count,
            data_length,
        }
    }

    /// APID (11-bit, sealed).
    #[must_use]
    pub const fn apid(&self) -> Apid {
        self.apid
    }

    /// Packet type discriminant (TM = 0, TC = 1).
    #[must_use]
    pub const fn packet_type(&self) -> PacketType {
        self.packet_type
    }

    /// Sequence count (14-bit, sealed).
    #[must_use]
    pub const fn sequence_count(&self) -> SequenceCount {
        self.sequence_count
    }

    /// Packet Data Length as written on the wire (raw `total_length − 7`).
    #[must_use]
    pub const fn data_length(&self) -> PacketDataLength {
        self.data_length
    }

    /// Encode this header into the caller's 6-byte buffer (Green-step stub).
    pub fn encode(&self, _out: &mut [u8; Self::LEN]) {
        // Red-step stub: the Green commit replaces this with the real
        // bit-packed BE layout.
    }

    /// Decode a primary header from the first 6 bytes of `buf` (Red-step stub).
    ///
    /// # Errors
    ///
    /// Always returns a fixed [`CcsdsError::BufferTooShort`] in the
    /// Red-step stub so the first Red test in this module asserts against
    /// the wrong variant. The Green commit replaces this body with the
    /// real decoder that enforces every §2.4 guard.
    pub fn decode(_buf: &[u8]) -> Result<Self, CcsdsError> {
        Err(CcsdsError::BufferTooShort {
            need: Self::LEN,
            got: 0,
        })
    }
}

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used,
    // Field-boundary binary literals (e.g. `0b0010_1_001` = version|type|sec-hdr|apid-hi)
    // use unequal group widths deliberately to mirror the bit-packed layout of §3
    // (plan) / arch §2.4 — the grouping is the whole point.
    clippy::unusual_byte_groupings,
)]
mod tests {
    use super::*;

    // --- RED (first failing test of Phase 07) ------------------------------
    // Given: a 6-byte buffer whose version field (byte 0, bits [7:5]) is
    //        0b001 instead of the spec-required 0b000. The sec-hdr flag is
    //        set, type is TM, APID is 0x100, seq-flags are 0b11, seq-count
    //        is 0, data_length is 0 — every OTHER field is valid so the
    //        failure is attributable only to the version guard.
    // When:  we decode the buffer through the sealed boundary.
    // Then:  we get Err(InvalidVersion(top4)), where top4 is the observed
    //        top-nibble-after-masking-the-type-bit. For this buffer that is
    //        0b0011 = 0x3 (version bits 0b001 shifted down by 4, plus
    //        sec-hdr bit 1 in bit 0 of the nibble), preserving the
    //        offending raw value for operator diagnostics per §2.8's
    //        "raw value preserved" discipline.
    //
    // Fails against the Red-step stub because `decode` returns
    // `Err(BufferTooShort { need: 6, got: 0 })` — a different variant.
    #[test]
    fn test_primary_decode_returns_err_when_version_is_non_zero() {
        let mut buf = [0u8; 6];
        buf[0] = 0b0010_1_001; // version=001, type=0, sec-hdr=1, apid-hi=001
        buf[1] = 0x00; // apid-lo=0x00 → apid = 0x100
        buf[2] = 0b11_000000; // seq-flags=0b11, seq-count hi = 0
        buf[3] = 0x00; // seq-count lo = 0
        buf[4] = 0x00;
        buf[5] = 0x00;

        let result = PrimaryHeader::decode(&buf);

        assert_eq!(result, Err(CcsdsError::InvalidVersion(0b0011)));
    }
}
