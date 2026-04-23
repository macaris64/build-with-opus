//! CCSDS Space Packet Protocol **primary header** (6 bytes, big-endian).
//!
//! Phase 07 locus. First end-to-end BE codec in the crate — composes the
//! sealed newtypes from Phases 03–04 ([`crate::Apid`],
//! [`crate::SequenceCount`], [`crate::PacketDataLength`]) and the
//! [`crate::PacketType`] discriminant from Phase 05 into a 48-bit on-wire
//! frame, then enforces the §2.4 decode-time guards against it.
//!
//! # Wire layout
//!
//! Per [arch §2.4](../../../docs/architecture/06-ground-segment-rust.md) and
//! [07 §2](../../../docs/architecture/07-comms-stack.md) (both derived from
//! CCSDS 133.0-B-2):
//!
//! ```text
//! byte 0: VVV T S AAA                byte 1: aaaaaaaa
//!         │││ │ │ │                          │
//!         │││ │ │ └─ APID hi 3 bits          └─ APID lo 8 bits   (11 bits total)
//!         │││ │ └─── Sec-Hdr Flag (MUST be 1 — SAKURA-II never omits time)
//!         │││ └───── Type bit (0 = TM, 1 = TC)
//!         └┴┴──────── Version (MUST be 0b000 — CCSDS v1)
//!
//! byte 2: FF SSSSSS                  byte 3: ssssssss
//!         │  │                               │
//!         │  └─ Seq count hi 6 bits          └─ Seq count lo 8 bits  (14 bits total)
//!         └──── Seq flags (MUST be 0b11 — standalone; no segmentation)
//!
//! byte 4: LLLLLLLL                   byte 5: llllllll   (Packet Data Length, u16 BE, raw)
//! ```
//!
//! # Guards
//!
//! [`PrimaryHeader::decode`] rejects non-conformant frames via
//! [`CcsdsError`]:
//!
//! | Condition | Error |
//! |---|---|
//! | `buf.len() < 6` | [`CcsdsError::BufferTooShort`] |
//! | Version `[7:5]` ≠ `0b000` **or** sec-hdr flag `[3]` ≠ `1` | [`CcsdsError::InvalidVersion`] |
//! | Seq-flags `[15:14]` ≠ `0b11` | [`CcsdsError::SequenceFlagsNotStandalone`] |
//!
//! The sec-hdr-flag check reuses [`CcsdsError::InvalidVersion`] rather than
//! introducing a new variant — the Phase 05 `CcsdsError` enum is frozen at
//! nine variants (arch §2.8). The payload `top4` carries the observed
//! version-plus-sec-hdr nibble (type bit masked out) so operator logs can
//! distinguish "bad version" from "missing sec-hdr flag" post-hoc.
//!
//! APID 11-bit and sequence-count 14-bit range invariants are enforced at
//! the sealed-newtype boundary by [`crate::Apid::new`] /
//! [`crate::SequenceCount::new`]. Because the decode path masks before
//! constructing (`& 0x07FF` and `& 0x3FFF` respectively), those error arms
//! are mathematically unreachable from wire bytes — but kept in the
//! `Result` chain so the single auditable construction path is `Apid::new`
//! (no unchecked constructor is introduced).
//!
//! # Definition sites
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
/// `Copy` because the struct is 8 bytes of plain-old-data (three sealed
/// newtypes backed by `u16` + one enum `#[repr(u8)]`); cheap to pass by
/// value.
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
    /// its own sealed boundary ([`Apid::new`], [`SequenceCount::new`],
    /// [`PacketDataLength::new`]); `packet_type` is a typed enum, not a
    /// raw bit. This is the constructor the Phase 09 `PacketBuilder` will
    /// call.
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

    /// Encode this header into the caller's 6-byte buffer, big-endian.
    ///
    /// Byte layout (BE, per Q-C8) matches the module-level diagram.
    /// Every bit position is either pinned to a spec-locked constant
    /// (version `0b000`, sec-hdr `1`, seq-flags `0b11`) or sourced from
    /// a sealed-newtype accessor whose invariants are already enforced.
    ///
    /// Caller owns the output buffer — stack-allocated, no heap,
    /// `no_std`-ready.
    pub fn encode(&self, out: &mut [u8; Self::LEN]) {
        let apid_raw = self.apid.get(); // ≤ 0x7FF by sealed newtype
        let seq_raw = self.sequence_count.get(); // ≤ 0x3FFF by sealed newtype

        // Packet identification (bytes 0-1, 16 bits BE):
        //   bits 15..13 = version (0b000, pinned)
        //   bit  12     = type (TM=0, TC=1)
        //   bit  11     = sec-hdr flag (0b1, pinned — `0x0800`)
        //   bits 10..0  = apid (11 bits)
        let pkt_id: u16 =
            0x0800 | (u16::from(self.packet_type.as_bit()) << 12) | (apid_raw & 0x07FF);
        let [b0, b1] = pkt_id.to_be_bytes();

        // Packet sequence control (bytes 2-3, 16 bits BE):
        //   bits 15..14 = seq flags (0b11, pinned — `0xC000`)
        //   bits 13..0  = seq count (14 bits)
        let pkt_seq: u16 = 0xC000 | (seq_raw & 0x3FFF);
        let [b2, b3] = pkt_seq.to_be_bytes();

        // Packet data length (bytes 4-5, 16 bits BE, raw).
        let [b4, b5] = self.data_length.get().to_be_bytes();

        *out = [b0, b1, b2, b3, b4, b5];
    }

    /// Decode a primary header from the first 6 bytes of `buf`.
    ///
    /// Trailing bytes beyond offset 6 are ignored — the normal call site
    /// is the head of a longer 16-byte secondary-header-included frame,
    /// or the full `SpacePacket` handled by Phase 09's `SpacePacket::parse`.
    ///
    /// # Errors
    ///
    /// - [`CcsdsError::BufferTooShort`] if `buf.len() < 6`.
    /// - [`CcsdsError::InvalidVersion`] if the version bits `[7:5]` of
    ///   byte 0 are non-zero **or** the sec-hdr flag bit `[3]` is zero.
    ///   The payload is the 4-bit `top_nibble_without_type` so callers
    ///   can distinguish bad-version from missing-sec-hdr in operator logs.
    /// - [`CcsdsError::SequenceFlagsNotStandalone`] if the top-2 bits of
    ///   byte 2 are not `0b11`. Payload is the observed 2-bit value.
    /// - [`CcsdsError::ApidOutOfRange`] / [`CcsdsError::SequenceCountOutOfRange`]:
    ///   mathematically unreachable (the decode path pre-masks these
    ///   fields to their respective widths before constructing the sealed
    ///   newtype), kept in the `Result` chain to keep the single
    ///   auditable construction path through `Apid::new` /
    ///   `SequenceCount::new`.
    pub fn decode(buf: &[u8]) -> Result<Self, CcsdsError> {
        let Some(head) = buf.first_chunk::<{ Self::LEN }>() else {
            return Err(CcsdsError::BufferTooShort {
                need: Self::LEN,
                got: buf.len(),
            });
        };
        let [b0, b1, b2, b3, b4, b5] = *head;

        // Byte 0: VVV T S AAA.
        // Compose top-nibble-without-type = [ver2, ver1, ver0, sec-hdr] and
        // compare to the spec-locked pattern 0b0001. Any deviation maps to
        // `InvalidVersion(top4)`, preserving the raw observed nibble for
        // operator diagnostics (§5.b of the Phase 07 plan).
        let top4 = ((b0 >> 4) & 0b1110) | ((b0 >> 3) & 0b0001);
        if top4 != 0b0001 {
            return Err(CcsdsError::InvalidVersion(top4));
        }
        let packet_type = PacketType::from_bit((b0 >> 4) & 0b0000_0001);
        let apid_raw = (u16::from(b0 & 0b0000_0111) << 8) | u16::from(b1);
        // apid_raw ≤ 0x7FF by mask → Apid::new error arm unreachable; `?`
        // keeps the single auditable construction path (§5 table).
        let apid = Apid::new(apid_raw)?;

        // Bytes 2-3: FF | seq-count (14 bits).
        let seq_flags = (b2 >> 6) & 0b0000_0011;
        if seq_flags != 0b0000_0011 {
            return Err(CcsdsError::SequenceFlagsNotStandalone(seq_flags));
        }
        let seq_raw = (u16::from(b2 & 0b0011_1111) << 8) | u16::from(b3);
        // seq_raw ≤ 0x3FFF by mask → SequenceCount::new error arm unreachable.
        let sequence_count = SequenceCount::new(seq_raw)?;

        // Bytes 4-5: data length (u16 BE, raw). LengthMismatch semantics
        // are Phase 09's `SpacePacket::parse` concern.
        let data_length = PacketDataLength::new(u16::from_be_bytes([b4, b5]));

        Ok(Self {
            apid,
            packet_type,
            sequence_count,
            data_length,
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

    // Test-only helper: build a conformant 6-byte wire buffer from
    // primitives. Keeps the individual tests focused on the one field
    // they stress.
    fn wire(
        version: u8,
        type_bit: u8,
        sec_hdr: u8,
        apid: u16,
        seq_flags: u8,
        seq_count: u16,
        data_length: u16,
    ) -> [u8; 6] {
        let b0 = ((version & 0b0000_0111) << 5)
            | ((type_bit & 0b0000_0001) << 4)
            | ((sec_hdr & 0b0000_0001) << 3)
            | ((apid >> 8) as u8 & 0b0000_0111);
        let b1 = (apid & 0x00FF) as u8;
        let b2 = ((seq_flags & 0b0000_0011) << 6) | ((seq_count >> 8) as u8 & 0b0011_1111);
        let b3 = (seq_count & 0x00FF) as u8;
        let [b4, b5] = data_length.to_be_bytes();
        [b0, b1, b2, b3, b4, b5]
    }

    // --- RED (first failing test of Phase 07) ------------------------------
    // Given: a 6-byte buffer whose version field (byte 0, bits [7:5]) is
    //        0b001 instead of the spec-required 0b000. Sec-hdr flag is set,
    //        type is TM, APID is 0x100, seq-flags are 0b11, seq-count is 0,
    //        data_length is 0.
    // When:  we decode the buffer through the sealed boundary.
    // Then:  we get Err(InvalidVersion(0b0011)) — bits [ver_hi, ver_mid,
    //        ver_lo, sec_hdr] = 0b0011, preserving the offending nibble.
    #[test]
    fn test_primary_decode_returns_err_when_version_is_non_zero() {
        let buf = wire(0b001, 0, 1, 0x100, 0b11, 0, 0);
        let result = PrimaryHeader::decode(&buf);
        assert_eq!(result, Err(CcsdsError::InvalidVersion(0b0011)));
    }

    // --- §5.b: sec-hdr flag = 0 also fires InvalidVersion ------------------
    // Given: a 6-byte buffer whose sec-hdr flag (byte 0, bit 3) is 0 but
    //        every other field is valid (version=000, type=0, apid=0x100,
    //        seq-flags=0b11, seq-count=0, data_length=0).
    // When:  we decode.
    // Then:  Err(InvalidVersion(0b0000)) — top4 = [0,0,0,0] signaling the
    //        missing sec-hdr. Distinct value from the Red test (0b0011) so
    //        the two cases stay separable in operator logs.
    #[test]
    fn test_primary_decode_returns_err_when_sec_hdr_flag_is_zero() {
        let buf = wire(0b000, 0, 0, 0x100, 0b11, 0, 0);
        let result = PrimaryHeader::decode(&buf);
        assert_eq!(result, Err(CcsdsError::InvalidVersion(0b0000)));
    }

    // --- BufferTooShort across the full 0..6 length range ------------------
    // Given: every buffer length strictly less than the 6-byte header.
    // When:  we decode.
    // Then:  Err(BufferTooShort { need: 6, got: len }) — never reads byte 0.
    #[test]
    fn test_primary_decode_returns_err_when_buffer_is_too_short() {
        for len in 0..PrimaryHeader::LEN {
            let buf = vec![0u8; len];
            assert_eq!(
                PrimaryHeader::decode(&buf),
                Err(CcsdsError::BufferTooShort {
                    need: PrimaryHeader::LEN,
                    got: len,
                }),
                "len={len}",
            );
        }
    }

    // --- SequenceFlagsNotStandalone for every non-conformant pattern -------
    // Given: a 6-byte buffer with a valid packet-id but seq-flags ≠ 0b11
    //        (tries 0b00, 0b01, 0b10).
    // When:  we decode.
    // Then:  Err(SequenceFlagsNotStandalone(flags)) preserving the observed
    //        raw 2-bit value.
    #[test]
    fn test_primary_decode_returns_err_when_sequence_flags_are_not_0b11() {
        for flags in [0b00u8, 0b01, 0b10] {
            let buf = wire(0b000, 0, 1, 0x100, flags, 0x1234, 0xABCD);
            assert_eq!(
                PrimaryHeader::decode(&buf),
                Err(CcsdsError::SequenceFlagsNotStandalone(flags)),
                "flags=0b{flags:02b}",
            );
        }
    }

    // --- Happy path: TM at every field's lower bound -----------------------
    // Given: TM packet with APID=0, seq-count=0, data_length=0.
    // When:  encoded and immediately decoded.
    // Then:  the round-trip reproduces every accessor value; byte 0 top
    //        nibble is exactly 0b0000_1000 (version=000, type=0, sec-hdr=1).
    #[test]
    fn test_primary_encode_decode_roundtrip_tm_min_bounds() {
        let original = PrimaryHeader::new(
            Apid::new(0x000).unwrap(),
            PacketType::Tm,
            SequenceCount::new(0).unwrap(),
            PacketDataLength::new(0),
        );
        let mut buf = [0xFFu8; 6]; // pre-fill 0xFF to prove encode overwrites every byte
        original.encode(&mut buf);
        assert_eq!(buf, [0b0000_1000, 0x00, 0b1100_0000, 0x00, 0x00, 0x00]);

        let decoded = PrimaryHeader::decode(&buf).unwrap();
        assert_eq!(decoded, original);
    }

    // --- Happy path: TC at every field's upper bound -----------------------
    // Given: TC packet with APID=0x7FF (Idle), seq-count=0x3FFF,
    //        data_length=u16::MAX.
    // When:  encoded and immediately decoded.
    // Then:  round-trip is lossless; every accessor equals the input.
    #[test]
    fn test_primary_encode_decode_roundtrip_tc_max_bounds() {
        let original = PrimaryHeader::new(
            Apid::new(0x7FF).unwrap(),
            PacketType::Tc,
            SequenceCount::new(SequenceCount::MAX).unwrap(),
            PacketDataLength::new(u16::MAX),
        );
        let mut buf = [0u8; 6];
        original.encode(&mut buf);
        let decoded = PrimaryHeader::decode(&buf).unwrap();
        assert_eq!(decoded, original);
        assert_eq!(decoded.apid(), Apid::IDLE);
        assert_eq!(decoded.packet_type(), PacketType::Tc);
        assert_eq!(decoded.sequence_count().get(), 0x3FFF);
        assert_eq!(decoded.data_length().get(), u16::MAX);
    }

    // --- Apid::IDLE sentinel round-trips unchanged -------------------------
    // Given: `Apid::IDLE` placed inside a primary header.
    // When:  encoded and decoded.
    // Then:  the decoded APID equals `Apid::IDLE` — ensures the idle-fill
    //        sentinel isn't mangled by the 11-bit mask/shift.
    #[test]
    fn test_primary_encode_decode_roundtrip_apid_idle() {
        let original = PrimaryHeader::new(
            Apid::IDLE,
            PacketType::Tm,
            SequenceCount::new(0).unwrap(),
            PacketDataLength::new(0),
        );
        let mut buf = [0u8; 6];
        original.encode(&mut buf);
        let decoded = PrimaryHeader::decode(&buf).unwrap();
        assert_eq!(decoded.apid(), Apid::IDLE);
    }

    // --- Explicit BE byte layout assertion ---------------------------------
    // Given: TM packet with APID=0x123, seq-count=0x2A5F, data_length=0x1234
    //        (every byte distinct so mis-ordered BE writes are caught).
    // When:  encoded.
    // Then:  the 6 output bytes match the hand-computed BE layout:
    //          b0 = 0000_1_001 = 0x09  (ver=000 | type=0 | sec-hdr=1 | apid-hi=001)
    //          b1 = 0x23              (apid-lo)
    //          b2 = 11_101010 = 0xEA  (flags=11 | seq-count-hi=101010)
    //          b3 = 0x5F              (seq-count-lo)
    //          b4 = 0x12              (data-length-hi)
    //          b5 = 0x34              (data-length-lo)
    #[test]
    fn test_primary_encode_bytes_match_be_layout() {
        let hdr = PrimaryHeader::new(
            Apid::new(0x123).unwrap(),
            PacketType::Tm,
            SequenceCount::new(0x2A5F).unwrap(),
            PacketDataLength::new(0x1234),
        );
        let mut buf = [0u8; 6];
        hdr.encode(&mut buf);
        assert_eq!(buf, [0x09, 0x23, 0xEA, 0x5F, 0x12, 0x34]);
    }

    // --- Decode preserves the Type bit in both directions ------------------
    // Given: two valid 6-byte buffers differing only in the Type bit.
    // When:  decoded.
    // Then:  `packet_type()` reports `Tm` for the first, `Tc` for the
    //        second — confirms the Type bit isn't silently masked during
    //        the InvalidVersion check (which masks the Type bit out of
    //        `top4`).
    #[test]
    fn test_primary_decode_preserves_type_bit() {
        let telemetry = wire(0b000, 0, 1, 0x100, 0b11, 0, 0);
        let telecommand = wire(0b000, 1, 1, 0x180, 0b11, 0, 0);
        assert_eq!(
            PrimaryHeader::decode(&telemetry).unwrap().packet_type(),
            PacketType::Tm,
        );
        assert_eq!(
            PrimaryHeader::decode(&telecommand).unwrap().packet_type(),
            PacketType::Tc,
        );
    }

    // --- Length constant locked against silent edits -----------------------
    #[test]
    fn test_primary_len_const_is_six() {
        assert_eq!(PrimaryHeader::LEN, 6);
    }
}
