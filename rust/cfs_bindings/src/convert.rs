//! FFI host ↔ `ccsds_wire` conversion helpers.
//!
//! Phase 14 locus. Q-C8 locus B: the only place in `cfs_bindings` where
//! host-side [`Message`] bytes are re-encoded through
//! [`ccsds_wire::PacketBuilder`] (outbound) or parsed from a raw BE buffer
//! (inbound). No `*_be_bytes` / `*_le_bytes` calls appear here — that detail
//! lives inside `ccsds_wire` locus A.
//!
//! Conversion boundary per `docs/architecture/06-ground-segment-rust.md §3.2`:
//! ```text
//! C host-endian bytes  →  to_c_message   →  Message
//! Message              →  from_c_message →  canonical BE Vec<u8>
//! ```
//!
//! Q-F3: `Vault<T>` hardening of radiation-sensitive state is out of scope
//! for this phase; it lands in Phase 44.

use ccsds_wire::{CcsdsError, PacketBuilder, PacketType};

use crate::message::Message;

/// Parse a raw big-endian CCSDS byte buffer into a validated [`Message`].
///
/// This is the inbound path: bytes arriving from a cFS software-bus capture
/// or a network socket enter `cfs_bindings` through this function. Full
/// structural validation is performed by [`ccsds_wire::SpacePacket::parse`]
/// inside [`Message::from_bytes`].
///
/// # Errors
///
/// Propagates any [`CcsdsError`] from [`Message::from_bytes`]:
/// [`CcsdsError::BufferTooShort`], [`CcsdsError::LengthMismatch`],
/// or any header-level error (invalid version, P-Field, APID, etc.).
pub fn to_c_message(buf: &[u8]) -> Result<Message, CcsdsError> {
    Message::from_bytes(buf.to_vec())
}

/// Re-encode a validated [`Message`] as canonical big-endian bytes via
/// [`ccsds_wire::PacketBuilder`].
///
/// This is the outbound path and **Q-C8 locus B**: every byte that leaves
/// this crate on the wire is produced by `PacketBuilder`, which is the
/// single authoritative BE-encoding path for `cfs_bindings`. The output
/// is byte-identical to the input for any well-formed packet (round-trip
/// property enforced by the test suite below).
///
/// # Errors
///
/// Propagates [`CcsdsError`] from [`Message::as_space_packet`] (unreachable
/// after a successful construction) and from [`PacketBuilder::build`]
/// ([`CcsdsError::LengthMismatch`] if user data exceeds the CCSDS cap).
pub fn from_c_message(msg: &Message) -> Result<Vec<u8>, CcsdsError> {
    let pkt = msg.as_space_packet()?;
    let builder = match pkt.primary.packet_type() {
        PacketType::Tc => PacketBuilder::tc(pkt.primary.apid()),
        PacketType::Tm => PacketBuilder::tm(pkt.primary.apid()),
    };
    builder
        .func_code(pkt.secondary.func_code())
        .instance_id(pkt.secondary.instance_id())
        .cuc(pkt.secondary.time())
        .sequence_count(pkt.primary.sequence_count())
        .user_data(pkt.user_data)
        .build()
}

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::doc_markdown
)]
mod tests {
    use super::*;
    use ccsds_wire::{Apid, Cuc, FuncCode, InstanceId, PacketBuilder, SequenceCount, SpacePacket};

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------

    fn tm_bytes(
        apid_raw: u16,
        seq_raw: u16,
        cuc: Cuc,
        fc_raw: u16,
        iid_raw: u8,
        payload: &[u8],
    ) -> Vec<u8> {
        PacketBuilder::tm(Apid::new(apid_raw).unwrap())
            .sequence_count(SequenceCount::new(seq_raw).unwrap())
            .cuc(cuc)
            .func_code(FuncCode::new(fc_raw).unwrap())
            .instance_id(InstanceId::new(iid_raw).unwrap())
            .user_data(payload)
            .build()
            .unwrap()
    }

    fn tc_bytes(apid_raw: u16, fc_raw: u16, iid_raw: u8) -> Vec<u8> {
        PacketBuilder::tc(Apid::new(apid_raw).unwrap())
            .func_code(FuncCode::new(fc_raw).unwrap())
            .instance_id(InstanceId::new(iid_raw).unwrap())
            .build()
            .unwrap()
    }

    // ------------------------------------------------------------------
    // to_c_message tests
    // ------------------------------------------------------------------

    /// G: a valid 20-byte TM buffer (16-byte header + 4-byte payload).
    /// W: to_c_message is called.
    /// T: Ok(Message) and as_bytes() equals the original buffer.
    #[test]
    fn test_to_c_message_accepts_valid_tm_buffer() {
        let payload = [0xDE_u8, 0xAD, 0xBE, 0xEF];
        let buf = tm_bytes(
            0x100,
            0x0001,
            Cuc { coarse: 0, fine: 0 },
            0x0042,
            1,
            &payload,
        );
        let copy = buf.clone();
        let msg = to_c_message(&buf).unwrap();
        assert_eq!(msg.as_bytes(), copy.as_slice());
    }

    /// G: a buffer of 8 bytes (shorter than HEADER_LEN=16).
    /// W: to_c_message is called.
    /// T: Err(CcsdsError::BufferTooShort { need: 16, got: 8 }).
    #[test]
    fn test_to_c_message_rejects_too_short() {
        let err = to_c_message(&[0u8; 8]).unwrap_err();
        assert!(
            matches!(err, CcsdsError::BufferTooShort { need: 16, got: 8 }),
            "expected BufferTooShort, got {err:?}"
        );
    }

    /// G: a buffer whose data_length field declares a longer packet than the
    ///    actual buffer.
    /// W: to_c_message is called.
    /// T: Err(CcsdsError::LengthMismatch { .. }).
    #[test]
    fn test_to_c_message_rejects_length_mismatch() {
        let mut buf = tm_bytes(
            0x100,
            0x0001,
            Cuc { coarse: 0, fine: 0 },
            0x0042,
            1,
            &[0xAA],
        );
        // Append an extra byte not reflected in data_length — SpacePacket::parse rejects.
        buf.push(0xFF);
        let err = to_c_message(&buf).unwrap_err();
        assert!(
            matches!(err, CcsdsError::LengthMismatch { .. }),
            "expected LengthMismatch, got {err:?}"
        );
    }

    // ------------------------------------------------------------------
    // from_c_message tests
    // ------------------------------------------------------------------

    /// G: a TM Message with APID=0x100, seq=0x0010, Cuc{42,7}, fc=0x0042,
    ///    iid=3, payload=[0xDE,0xAD,0xBE,0xEF].
    /// W: from_c_message is called.
    /// T: Ok(bytes) and SpacePacket::parse of those bytes reproduces every field.
    #[test]
    fn test_from_c_message_tm_round_trip() {
        let cuc = Cuc {
            coarse: 42,
            fine: 7,
        };
        let payload = [0xDE_u8, 0xAD, 0xBE, 0xEF];
        let orig = tm_bytes(0x100, 0x0010, cuc, 0x0042, 3, &payload);
        let msg = Message::from_bytes(orig).unwrap();

        let bytes = from_c_message(&msg).unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();

        assert_eq!(pkt.primary.apid(), Apid::new(0x100).unwrap());
        assert_eq!(pkt.primary.packet_type(), PacketType::Tm);
        assert_eq!(
            pkt.primary.sequence_count(),
            SequenceCount::new(0x0010).unwrap()
        );
        assert_eq!(pkt.secondary.func_code(), FuncCode::new(0x0042).unwrap());
        assert_eq!(pkt.secondary.instance_id(), InstanceId::new(3).unwrap());
        assert_eq!(pkt.secondary.time(), cuc);
        assert_eq!(pkt.user_data, &payload);
    }

    /// G: a TC Message with APID=0x184, fc=0x8100, iid=1.
    /// W: from_c_message is called.
    /// T: packet_type()==Tc and all fields match.
    #[test]
    fn test_from_c_message_tc_round_trip() {
        let orig = tc_bytes(0x184, 0x8100, 1);
        let msg = Message::from_bytes(orig).unwrap();

        let bytes = from_c_message(&msg).unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();

        assert_eq!(pkt.primary.packet_type(), PacketType::Tc);
        assert_eq!(pkt.primary.apid(), Apid::new(0x184).unwrap());
        assert_eq!(pkt.secondary.func_code(), FuncCode::new(0x8100).unwrap());
        assert_eq!(pkt.secondary.instance_id(), InstanceId::new(1).unwrap());
    }

    /// G: a TM Message with no payload (header-only, 16 bytes).
    /// W: from_c_message is called.
    /// T: Ok(bytes) of length 16; SpacePacket::parse gives empty user_data.
    #[test]
    fn test_from_c_message_empty_user_data() {
        let orig = tm_bytes(0x100, 0, Cuc { coarse: 0, fine: 0 }, 0x0001, 1, &[]);
        let msg = Message::from_bytes(orig).unwrap();

        let bytes = from_c_message(&msg).unwrap();
        assert_eq!(bytes.len(), 16);
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.user_data, &[] as &[u8]);
    }

    // ------------------------------------------------------------------
    // Full C → bytes → C round-trip (equality on raw bytes)
    // ------------------------------------------------------------------

    /// G: a Message constructed from PacketBuilder output.
    /// W: from_c_message → to_c_message.
    /// T: the final Message's as_bytes() is byte-identical to the original.
    #[test]
    fn test_round_trip_bytes_are_identical() {
        let payload = [0x01_u8, 0x02, 0x03];
        let orig = tm_bytes(
            0x101,
            0x0020,
            Cuc {
                coarse: 1_000_000,
                fine: 500,
            },
            0x0003,
            2,
            &payload,
        );
        let orig_copy = orig.clone();
        let msg = Message::from_bytes(orig).unwrap();

        let bytes = from_c_message(&msg).unwrap();
        let msg2 = to_c_message(&bytes).unwrap();

        assert_eq!(msg2.as_bytes(), orig_copy.as_slice());
    }
}
