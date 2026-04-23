//! CCSDS Space Packet — complete packet view over caller-owned bytes.
//!
//! Phase 09 locus. Composes [`PrimaryHeader`] + [`SecondaryHeader`] + a
//! zero-copy user-data slice into a single validated view. No heap
//! allocation on the hot path — suitable for embedded FSW and
//! high-throughput ground ingest alike.
//!
//! # Wire layout
//!
//! ```text
//! byte  0..=5  : PrimaryHeader   (6 B)
//! byte  6..=15 : SecondaryHeader (10 B)
//! byte 16..    : user_data       (0..N B)
//! ```
//!
//! # CCSDS length invariant
//!
//! `data_length` (bytes 4–5 of the primary header) equals
//! `total_packet_bytes − 7` per CCSDS 133.0-B-2.
//! [`SpacePacket::parse`] enforces
//! `data_length.get() as usize + 7 == buf.len()`.
//!
//! # Definition sites
//! - `docs/architecture/06-ground-segment-rust.md §2.6`
//! - `SYS-REQ-0020`, `SYS-REQ-0021`; `Q-C8`.

use crate::{CcsdsError, PrimaryHeader, SecondaryHeader};

/// Borrowed view over a complete CCSDS Space Packet.
///
/// All three fields are derived from the caller's buffer; no data is
/// copied. The lifetime `'a` ties every field to the same origin slice.
///
/// Construct via [`SpacePacket::parse`]. Public fields are exposed for
/// zero-cost downstream access (Phase 25 `ApidRouter` etc.).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SpacePacket<'a> {
    /// Decoded primary header (bytes 0..=5).
    pub primary: PrimaryHeader,
    /// Decoded secondary header (bytes 6..=15).
    pub secondary: SecondaryHeader,
    /// Zero-copy user-data slice starting at byte 16.
    pub user_data: &'a [u8],
}

impl<'a> SpacePacket<'a> {
    /// Total length of both headers in octets.
    ///
    /// Equal to [`PrimaryHeader::LEN`] + [`SecondaryHeader::LEN`] = 6 + 10 = 16.
    pub const HEADER_LEN: usize = PrimaryHeader::LEN + SecondaryHeader::LEN;

    /// Parse a complete Space Packet from `buf`.
    ///
    /// Errors propagate from sub-decoders before the length check so that
    /// a malformed-header error takes precedence over a `LengthMismatch`.
    ///
    /// # Errors
    ///
    /// - [`CcsdsError::BufferTooShort`] if `buf.len() < HEADER_LEN`.
    /// - Any [`CcsdsError`] propagated from [`PrimaryHeader::decode`] or
    ///   [`SecondaryHeader::decode`] (invalid version, P-Field, etc.).
    /// - [`CcsdsError::LengthMismatch`] when
    ///   `data_length.get() as usize + 7 != buf.len()`;
    ///   `declared` carries `data_length + 7`, `actual` carries `buf.len()`.
    pub fn parse(buf: &'a [u8]) -> Result<Self, CcsdsError> {
        if buf.len() < Self::HEADER_LEN {
            let need = Self::HEADER_LEN;
            let got = buf.len();
            return Err(CcsdsError::BufferTooShort { need, got });
        }
        let primary = PrimaryHeader::decode(buf)?;
        // buf.len() >= HEADER_LEN (16) > PrimaryHeader::LEN (6): split_at is safe.
        let (_, after_primary) = buf.split_at(PrimaryHeader::LEN);
        let secondary = SecondaryHeader::decode(after_primary)?;
        let declared_total = usize::from(primary.data_length().get()) + 7;
        if declared_total != buf.len() {
            let declared = declared_total;
            let actual = buf.len();
            return Err(CcsdsError::LengthMismatch { declared, actual });
        }
        // buf.len() >= HEADER_LEN (16): split_at is safe.
        let (_, user_data) = buf.split_at(Self::HEADER_LEN);
        Ok(SpacePacket {
            primary,
            secondary,
            user_data,
        })
    }

    /// Total wire length: `HEADER_LEN + user_data.len()`.
    #[must_use]
    pub fn total_len(&self) -> usize {
        Self::HEADER_LEN + self.user_data.len()
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used,
    clippy::cast_possible_truncation,
    clippy::indexing_slicing
)]
mod tests {
    use super::*;
    use crate::{Apid, Cuc, FuncCode, InstanceId, PacketDataLength, PacketType, SequenceCount};

    // Test-only helper: build valid wire bytes for a complete Space Packet.
    // Sets data_length correctly so parse() accepts the buffer.
    fn make_packet(packet_type: PacketType, payload: &[u8]) -> Vec<u8> {
        // data_length = SecondaryHeader::LEN + payload.len() - 1
        //             = 9 + payload.len()  (since SecondaryHeader::LEN = 10)
        let raw_dl = (SecondaryHeader::LEN + payload.len() - 1) as u16;
        let primary = PrimaryHeader::new(
            Apid::new(0x100).unwrap(),
            packet_type,
            SequenceCount::new(0).unwrap(),
            PacketDataLength::new(raw_dl),
        );
        let secondary = SecondaryHeader::new(
            Cuc { coarse: 0, fine: 0 },
            FuncCode::new(0x0001).unwrap(),
            InstanceId::new(1).unwrap(),
        );
        let mut pb = [0u8; PrimaryHeader::LEN];
        let mut sb = [0u8; SecondaryHeader::LEN];
        primary.encode(&mut pb);
        secondary.encode(&mut sb);
        let mut out = Vec::with_capacity(SpacePacket::HEADER_LEN + payload.len());
        out.extend_from_slice(&pb);
        out.extend_from_slice(&sb);
        out.extend_from_slice(payload);
        out
    }

    // Same as make_packet but allows overriding data_length independently of
    // payload, so we can construct LengthMismatch scenarios.
    fn make_packet_with_dl(packet_type: PacketType, data_length: u16, payload: &[u8]) -> Vec<u8> {
        let primary = PrimaryHeader::new(
            Apid::new(0x100).unwrap(),
            packet_type,
            SequenceCount::new(0).unwrap(),
            PacketDataLength::new(data_length),
        );
        let secondary = SecondaryHeader::new(
            Cuc { coarse: 0, fine: 0 },
            FuncCode::new(0x0001).unwrap(),
            InstanceId::new(1).unwrap(),
        );
        let mut pb = [0u8; PrimaryHeader::LEN];
        let mut sb = [0u8; SecondaryHeader::LEN];
        primary.encode(&mut pb);
        secondary.encode(&mut sb);
        let mut out = Vec::with_capacity(SpacePacket::HEADER_LEN + payload.len());
        out.extend_from_slice(&pb);
        out.extend_from_slice(&sb);
        out.extend_from_slice(payload);
        out
    }

    // --- BufferTooShort for every length 0..HEADER_LEN ---------------------
    // Given: buffers with lengths 0..15.
    // When:  SpacePacket::parse().
    // Then:  Err(BufferTooShort { need: 16, got: len }).
    #[test]
    fn test_parse_buf_too_short() {
        for len in 0..SpacePacket::HEADER_LEN {
            let buf = vec![0u8; len];
            assert_eq!(
                SpacePacket::parse(&buf),
                Err(CcsdsError::BufferTooShort {
                    need: SpacePacket::HEADER_LEN,
                    got: len,
                }),
                "len={len}",
            );
        }
    }

    // --- LengthMismatch: buffer longer than declared ------------------------
    // Given: a valid 24-byte buffer whose data_length declares total = 16.
    // When:  SpacePacket::parse().
    // Then:  Err(LengthMismatch { declared: 16, actual: 24 }).
    #[test]
    fn test_parse_length_mismatch_actual_too_long() {
        // data_length = 9 → declared_total = 16; buf.len() = 24
        let mut buf = make_packet(PacketType::Tm, &[]);
        assert_eq!(buf.len(), 16);
        buf.extend_from_slice(&[0u8; 8]); // now 24 bytes
        assert_eq!(
            SpacePacket::parse(&buf),
            Err(CcsdsError::LengthMismatch {
                declared: 16,
                actual: 24,
            }),
        );
    }

    // --- LengthMismatch: buffer shorter than declared ----------------------
    // Given: a valid 16-byte header with data_length = 17 (declares 24 total)
    //        but buf.len() == 16.
    // When:  SpacePacket::parse().
    // Then:  Err(LengthMismatch { declared: 24, actual: 16 }).
    #[test]
    fn test_parse_length_mismatch_actual_too_short() {
        // data_length = 17 → declared_total = 24; payload = &[] → buf.len() = 16
        let buf = make_packet_with_dl(PacketType::Tm, 17, &[]);
        assert_eq!(buf.len(), 16);
        assert_eq!(
            SpacePacket::parse(&buf),
            Err(CcsdsError::LengthMismatch {
                declared: 24,
                actual: 16,
            }),
        );
    }

    // --- Propagate primary-header error (invalid version) ------------------
    // Given: a 16-byte buffer whose version field (byte 0 bits [7:5]) is 0b001.
    // When:  SpacePacket::parse().
    // Then:  Err(InvalidVersion(_)) — primary-header error takes precedence.
    #[test]
    fn test_parse_propagates_primary_header_error() {
        let mut buf = make_packet(PacketType::Tm, &[]);
        // Set version bits to 0b001 (byte 0 bits [7:5] = 0b0010_xxxx)
        buf[0] = (buf[0] & 0x1F) | 0b0010_0000;
        assert!(
            matches!(SpacePacket::parse(&buf), Err(CcsdsError::InvalidVersion(_))),
            "expected InvalidVersion",
        );
    }

    // --- Propagate secondary-header error (bad P-Field) --------------------
    // Given: a 16-byte buffer with a valid primary header but P-Field at
    //        byte 6 is 0x00 instead of 0x2F.
    // When:  SpacePacket::parse().
    // Then:  Err(InvalidPField(0x00)).
    #[test]
    fn test_parse_propagates_secondary_header_error() {
        let mut buf = make_packet(PacketType::Tm, &[]);
        buf[6] = 0x00; // corrupt the CUC P-Field
        assert_eq!(
            SpacePacket::parse(&buf),
            Err(CcsdsError::InvalidPField(0x00)),
        );
    }

    // --- Happy path: 24-byte TM packet -------------------------------------
    // Given: a valid 24-byte TM packet with 8-byte user data.
    // When:  SpacePacket::parse().
    // Then:  all fields match; total_len() == 24; user_data.len() == 8.
    #[test]
    fn test_parse_valid_24_byte_tm_packet() {
        let payload = [0x01u8, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08];
        let buf = make_packet(PacketType::Tm, &payload);
        let pkt = SpacePacket::parse(&buf).unwrap();
        assert_eq!(pkt.user_data, &payload);
        assert_eq!(pkt.total_len(), 24);
        assert_eq!(pkt.primary.packet_type(), PacketType::Tm);
        assert_eq!(pkt.primary.apid(), Apid::new(0x100).unwrap());
        assert_eq!(pkt.secondary.func_code(), FuncCode::new(0x0001).unwrap());
        assert_eq!(pkt.secondary.instance_id(), InstanceId::new(1).unwrap());
    }

    // --- Happy path: 16-byte packet with empty user data -------------------
    // Given: a valid 16-byte packet (zero-length payload).
    //        data_length = 9, declared_total = 16 == buf.len().
    // When:  SpacePacket::parse().
    // Then:  user_data is an empty slice; total_len() == 16.
    #[test]
    fn test_parse_valid_empty_user_data() {
        let buf = make_packet(PacketType::Tm, &[]);
        assert_eq!(buf.len(), 16);
        let pkt = SpacePacket::parse(&buf).unwrap();
        assert_eq!(pkt.user_data, &[] as &[u8]);
        assert_eq!(pkt.total_len(), 16);
    }

    // --- HEADER_LEN constant locked against silent edits -------------------
    #[test]
    fn test_header_len_constant_is_sixteen() {
        assert_eq!(SpacePacket::HEADER_LEN, 16);
    }
}
