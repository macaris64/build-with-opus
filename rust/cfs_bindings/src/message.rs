//! Safe wrappers around cFS Software Bus message types.
//!
//! Phase 12 locus. Exposes the minimum cFS message surface ground tooling
//! needs to decode packets captured from a cFS application without touching
//! raw bindgen output. Definition site:
//! `docs/architecture/06-ground-segment-rust.md §3.1, §3.2`.
//!
//! Conversion boundary per arch §3.2:
//! ```text
//! C host-endian struct  ←→  cfs_bindings::message  ←→  ccsds_wire::SpacePacket
//! ```
//!
//! Q-C8 note: `MsgId` uses bitwise OR on `u16` values (logical bit masking,
//! not byte-level endianness conversion). No little-endian byte-order calls
//! appear in this module — per the `scripts/grep-lints.sh` guard.
//!
//! Q-F3 note: `Vault<T>` wrapping lands in Phase 44. This phase establishes
//! the type boundary only.

use ccsds_wire::{Apid, CcsdsError, PacketType, SpacePacket};

/// cFE v1 Software Bus Message ID.
///
/// A 16-bit identifier derived from an 11-bit APID with a direction prefix:
/// - TC (command, ground → spacecraft): `0x1800 | APID`
/// - TM (telemetry, spacecraft → ground): `0x0800 | APID`
///
/// Source of truth: `docs/interfaces/apid-registry.md §cFE Message ID (MID) Scheme`.
///
/// Construction via [`MsgId::from_apid`] is infallible because [`Apid`]
/// already enforces the 11-bit range invariant. No byte-level endianness
/// conversion occurs here — the OR is a logical bit mask on a `u16`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct MsgId(u16);

impl MsgId {
    /// Encode an APID into a cFE MID.
    ///
    /// - `is_cmd = true`  → TC path: `0x1800 | apid.get()`
    /// - `is_cmd = false` → TM path: `0x0800 | apid.get()`
    ///
    /// Infallible: [`Apid`] already validates the 11-bit range; bitwise OR
    /// with a constant cannot overflow a `u16`.
    #[must_use]
    pub fn from_apid(apid: Apid, is_cmd: bool) -> Self {
        MsgId(if is_cmd { 0x1800 } else { 0x0800 } | apid.get())
    }

    /// Extract the 11-bit APID from this MID.
    ///
    /// # Errors
    ///
    /// Returns [`CcsdsError::ApidOutOfRange`] if the lower 11 bits of the
    /// raw MID exceed `Apid::MAX`. This is mathematically unreachable from a
    /// correctly-constructed [`MsgId`] but is kept fallible to route
    /// construction through the single auditable [`Apid::new`] path per Q-C8.
    pub fn apid(self) -> Result<Apid, CcsdsError> {
        Apid::new(self.0 & Apid::MAX)
    }

    /// Returns `true` if this MID encodes a command (TC) message.
    ///
    /// Tests bit 12 of the raw MID, which is `1` for TC and `0`
    /// for TM per the cFE v1 MID scheme.
    #[must_use]
    pub fn is_command(self) -> bool {
        (self.0 & 0x1000) != 0
    }

    /// Raw 16-bit MID value.
    ///
    /// Prefer [`MsgId::apid`] and [`MsgId::is_command`] for semantic access.
    #[must_use]
    pub fn raw(self) -> u16 {
        self.0
    }
}

/// Validated CCSDS Space Packet stored as owned bytes.
///
/// Wraps a `Vec<u8>` that has been parsed through
/// [`ccsds_wire::SpacePacket::parse`] at construction time. Once you hold
/// a `Message`, the bytes are structurally valid — no downstream code needs
/// to re-validate the header.
///
/// This is the owned-bytes conversion boundary described in
/// `docs/architecture/06-ground-segment-rust.md §3.2`.
#[derive(Debug)]
pub struct Message {
    bytes: Vec<u8>,
}

impl Message {
    /// Construct a `Message` from owned bytes, validating them as a
    /// complete CCSDS Space Packet.
    ///
    /// # Errors
    ///
    /// Propagates any [`CcsdsError`] from [`SpacePacket::parse`]:
    /// [`CcsdsError::BufferTooShort`] if `bytes.len() < 16`,
    /// [`CcsdsError::LengthMismatch`] if the declared `data_length` does
    /// not match the buffer, or any header-level error.
    pub fn from_bytes(bytes: Vec<u8>) -> Result<Self, CcsdsError> {
        // Validate structure; the borrowed view is dropped at `?` before
        // `bytes` is moved into the struct — borrow ends at the semicolon.
        SpacePacket::parse(&bytes)?;
        Ok(Message { bytes })
    }

    /// Derive the cFE MID for this packet by re-parsing the primary header.
    ///
    /// # Errors
    ///
    /// Propagates [`CcsdsError`] from re-parsing. In practice unreachable
    /// after a successful [`Message::from_bytes`], but kept fallible so
    /// callers can stay `?`-composable.
    pub fn msg_id(&self) -> Result<MsgId, CcsdsError> {
        let pkt = SpacePacket::parse(&self.bytes)?;
        let is_cmd = pkt.primary.packet_type() == PacketType::Tc;
        Ok(MsgId::from_apid(pkt.primary.apid(), is_cmd))
    }

    /// Re-parse and return a zero-copy view over the stored bytes.
    ///
    /// # Errors
    ///
    /// Propagates [`CcsdsError`] from re-parsing. Unreachable after a
    /// successful [`Message::from_bytes`].
    pub fn as_space_packet(&self) -> Result<SpacePacket<'_>, CcsdsError> {
        SpacePacket::parse(&self.bytes)
    }

    /// Raw byte slice of the validated packet.
    #[must_use]
    pub fn as_bytes(&self) -> &[u8] {
        &self.bytes
    }
}

/// Minimum-size-validated byte buffer mapping to `CFE_SB_Buffer_t`.
///
/// Accepts any byte slice that is at least [`SpacePacket::HEADER_LEN`]
/// (16) bytes long, without requiring a fully-valid CCSDS header. Used
/// as a staging type when raw cFS SB buffer bytes arrive before they can
/// be completely decoded.
///
/// Convert to a fully-validated [`Message`] via [`SbBuffer::into_message`].
#[derive(Debug)]
pub struct SbBuffer {
    bytes: Vec<u8>,
}

impl SbBuffer {
    /// Construct an `SbBuffer` from owned bytes, checking only the minimum
    /// size invariant.
    ///
    /// Full header validity is not checked here — defer to
    /// [`SbBuffer::into_message`] for complete validation.
    ///
    /// # Errors
    ///
    /// Returns [`CcsdsError::BufferTooShort`] if
    /// `bytes.len() < SpacePacket::HEADER_LEN` (16 bytes).
    pub fn from_bytes(bytes: Vec<u8>) -> Result<Self, CcsdsError> {
        let need = SpacePacket::HEADER_LEN;
        let got = bytes.len();
        if got < need {
            return Err(CcsdsError::BufferTooShort { need, got });
        }
        Ok(SbBuffer { bytes })
    }

    /// Consume this buffer and attempt to construct a fully-validated
    /// [`Message`].
    ///
    /// # Errors
    ///
    /// Propagates any [`CcsdsError`] from [`Message::from_bytes`].
    pub fn into_message(self) -> Result<Message, CcsdsError> {
        Message::from_bytes(self.bytes)
    }

    /// Raw byte slice of the buffer.
    #[must_use]
    pub fn as_bytes(&self) -> &[u8] {
        &self.bytes
    }
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
    use ccsds_wire::{FuncCode, InstanceId, PacketBuilder};

    fn make_packet(is_cmd: bool, apid_raw: u16) -> Vec<u8> {
        let apid = Apid::new(apid_raw).unwrap();
        let fc = FuncCode::new(0x0001).unwrap();
        let iid = InstanceId::new(1).unwrap();
        if is_cmd {
            PacketBuilder::tc(apid)
                .func_code(fc)
                .instance_id(iid)
                .build()
                .unwrap()
        } else {
            PacketBuilder::tm(apid)
                .func_code(fc)
                .instance_id(iid)
                .build()
                .unwrap()
        }
    }

    // ------------------------------------------------------------------
    // MsgId tests
    // ------------------------------------------------------------------

    /// Given APID 0x100 and is_cmd=true,
    /// When from_apid is called,
    /// Then raw() == 0x1900 and is_command() == true.
    #[test]
    fn test_msgid_from_apid_tc_encodes_0x1800_prefix() {
        let apid = Apid::new(0x100).unwrap();
        let mid = MsgId::from_apid(apid, true);
        assert_eq!(mid.raw(), 0x1900);
        assert!(mid.is_command());
    }

    /// Given APID 0x100 and is_cmd=false,
    /// When from_apid is called,
    /// Then raw() == 0x0900 and is_command() == false.
    #[test]
    fn test_msgid_from_apid_tm_encodes_0x0800_prefix() {
        let apid = Apid::new(0x100).unwrap();
        let mid = MsgId::from_apid(apid, false);
        assert_eq!(mid.raw(), 0x0900);
        assert!(!mid.is_command());
    }

    /// Given a MsgId constructed from APID 0x100 TM,
    /// When apid() is called,
    /// Then the returned Apid equals Apid::new(0x100).
    #[test]
    fn test_msgid_apid_roundtrips() {
        let apid = Apid::new(0x100).unwrap();
        let mid = MsgId::from_apid(apid, false);
        assert_eq!(mid.apid().unwrap(), apid);
    }

    /// Given maximum valid APID (Idle = 0x7FF) and is_cmd=true,
    /// When from_apid is called,
    /// Then raw() == 0x1FFF and apid() roundtrips to Apid::IDLE.
    #[test]
    fn test_msgid_from_apid_max_apid() {
        let apid = Apid::IDLE;
        let mid = MsgId::from_apid(apid, true);
        assert_eq!(mid.raw(), 0x1FFF);
        assert_eq!(mid.apid().unwrap(), Apid::IDLE);
    }

    /// Given APID 0x000 and is_cmd=false,
    /// When from_apid is called,
    /// Then raw() == 0x0800.
    #[test]
    fn test_msgid_from_apid_zero_apid_tm() {
        let apid = Apid::new(0x000).unwrap();
        let mid = MsgId::from_apid(apid, false);
        assert_eq!(mid.raw(), 0x0800);
    }

    // ------------------------------------------------------------------
    // Message tests
    // ------------------------------------------------------------------

    /// Given a valid TM packet built via PacketBuilder,
    /// When Message::from_bytes is called,
    /// Then Ok is returned and as_bytes() reproduces the input.
    #[test]
    fn test_message_from_bytes_accepts_valid_tm_packet() {
        let raw = make_packet(false, 0x042);
        let copy = raw.clone();
        let msg = Message::from_bytes(raw).unwrap();
        assert_eq!(msg.as_bytes(), copy.as_slice());
    }

    /// Given a valid TC packet,
    /// When msg_id() is called,
    /// Then is_command() == true.
    #[test]
    fn test_message_msg_id_is_command_for_tc_packet() {
        let raw = make_packet(true, 0x042);
        let msg = Message::from_bytes(raw).unwrap();
        assert!(msg.msg_id().unwrap().is_command());
    }

    /// Given a valid TM packet,
    /// When msg_id() is called,
    /// Then is_command() == false.
    #[test]
    fn test_message_msg_id_is_not_command_for_tm_packet() {
        let raw = make_packet(false, 0x042);
        let msg = Message::from_bytes(raw).unwrap();
        assert!(!msg.msg_id().unwrap().is_command());
    }

    /// Given a buffer of 8 bytes (shorter than HEADER_LEN=16),
    /// When Message::from_bytes is called,
    /// Then Err(CcsdsError::BufferTooShort { need: 16, got: 8 }).
    #[test]
    fn test_message_from_bytes_rejects_too_short() {
        let raw = vec![0u8; 8];
        let err = Message::from_bytes(raw).unwrap_err();
        assert!(
            matches!(err, CcsdsError::BufferTooShort { need: 16, got: 8 }),
            "expected BufferTooShort, got {err:?}"
        );
    }

    /// Given a valid packet with one extra trailing byte,
    /// When Message::from_bytes is called,
    /// Then Err(CcsdsError::LengthMismatch { .. }).
    #[test]
    fn test_message_from_bytes_rejects_length_mismatch() {
        let mut raw = make_packet(false, 0x042);
        // Append a byte not declared in data_length — parse must reject it.
        raw.push(0xFF);
        let err = Message::from_bytes(raw).unwrap_err();
        assert!(
            matches!(err, CcsdsError::LengthMismatch { .. }),
            "expected LengthMismatch, got {err:?}"
        );
    }

    /// Given a valid TM packet built with APID 0x042,
    /// When as_space_packet() is called,
    /// Then primary.apid() == Apid::new(0x042).
    #[test]
    fn test_message_as_space_packet_returns_correct_apid() {
        let raw = make_packet(false, 0x042);
        let msg = Message::from_bytes(raw).unwrap();
        let pkt = msg.as_space_packet().unwrap();
        assert_eq!(pkt.primary.apid(), Apid::new(0x042).unwrap());
    }

    // ------------------------------------------------------------------
    // SbBuffer tests
    // ------------------------------------------------------------------

    /// Given exactly 16 bytes (HEADER_LEN, arbitrary content),
    /// When SbBuffer::from_bytes is called,
    /// Then Ok is returned (minimum size only, no parse).
    #[test]
    fn test_sb_buffer_from_bytes_accepts_exactly_header_len() {
        let raw = vec![0u8; SpacePacket::HEADER_LEN];
        assert!(SbBuffer::from_bytes(raw).is_ok());
    }

    /// Given 15 bytes (one short of HEADER_LEN),
    /// When SbBuffer::from_bytes is called,
    /// Then Err(CcsdsError::BufferTooShort { need: 16, got: 15 }).
    #[test]
    fn test_sb_buffer_from_bytes_rejects_too_short() {
        let raw = vec![0u8; 15];
        let err = SbBuffer::from_bytes(raw).unwrap_err();
        assert!(
            matches!(err, CcsdsError::BufferTooShort { need: 16, got: 15 }),
            "expected BufferTooShort, got {err:?}"
        );
    }

    /// Given an SbBuffer containing a valid packet,
    /// When into_message() is called,
    /// Then Ok(Message) and as_bytes() matches the original bytes.
    #[test]
    fn test_sb_buffer_into_message_succeeds_for_valid_packet() {
        let raw = make_packet(false, 0x001);
        let copy = raw.clone();
        let buf = SbBuffer::from_bytes(raw).unwrap();
        let msg = buf.into_message().unwrap();
        assert_eq!(msg.as_bytes(), copy.as_slice());
    }

    /// Given an SbBuffer with a corrupt CCSDS version field,
    /// When into_message() is called,
    /// Then Err(CcsdsError::InvalidVersion(_)).
    #[test]
    fn test_sb_buffer_into_message_fails_for_invalid_header() {
        let mut raw = make_packet(false, 0x001);
        // CCSDS version occupies the top 3 bits of byte 0 (valid = 0b000).
        // Set them to 0b111 to force an InvalidVersion error.
        raw[0] |= 0b1110_0000;
        let buf = SbBuffer::from_bytes(raw).unwrap();
        let err = buf.into_message().unwrap_err();
        assert!(
            matches!(err, CcsdsError::InvalidVersion(_)),
            "expected InvalidVersion, got {err:?}"
        );
    }

    /// Given a valid SbBuffer,
    /// When as_bytes() is called,
    /// Then the returned slice equals the input bytes.
    #[test]
    fn test_sb_buffer_as_bytes_matches_input() {
        let raw = make_packet(true, 0x010);
        let copy = raw.clone();
        let buf = SbBuffer::from_bytes(raw).unwrap();
        assert_eq!(buf.as_bytes(), copy.as_slice());
    }
}
