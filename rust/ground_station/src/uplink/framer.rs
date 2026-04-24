//! `TcFramer` — TC SDLP frame encoder (CCSDS 232.0-B-3).
//!
//! Wraps a [`TcFrame`][super::cop1::TcFrame] in a 5-byte primary header and
//! appends a 2-byte FECF (CRC-16/IBM-3740). Output is ≤ [`TC_FRAME_MAX_BYTES`]
//! (512 B) per docs/architecture/07-comms-stack.md §3.3.
//!
//! # TC Transfer Frame Primary Header (5 bytes, big-endian bit fields)
//!
//! ```text
//! Byte 0: [7:6]=Version(00) [5]=Bypass [4]=CC [3:2]=Reserved(00) [1:0]=SCID[9:8]
//! Byte 1: [7:0]=SCID[7:0]
//! Byte 2: [7:2]=VC_ID[5:0] [1:0]=FrameLen-1[9:8]
//! Byte 3: [7:0]=FrameLen-1[7:0]
//! Byte 4: [7:0]=FrameSequenceNumber (V(S) for AD; 0 for BD/BC)
//! ```
//!
//! # Q-C8 compliance
//!
//! Header bit-fields are assembled with explicit bit-shift arithmetic (no
//! endian-conversion function). The 2-byte FECF is stored big-endian via
//! `u16::to_be_bytes()`, which is the single permitted endian-conversion site
//! per Q-C8 in this module.

use crc::{Crc, CRC_16_IBM_3740};

use super::cop1::{TcFrame, TcFrameType};
use super::TC_FRAME_MAX_BYTES;

// ---------------------------------------------------------------------------
// TcFramer
// ---------------------------------------------------------------------------

/// TC SDLP frame encoder.
pub struct TcFramer {
    /// 10-bit spacecraft ID from `_defs/mission_config.h` (value = 42).
    spacecraft_id: u16,
}

/// Errors returned by [`TcFramer::frame`].
#[derive(Debug, thiserror::Error)]
pub enum FramerError {
    /// Payload would produce a frame larger than [`TC_FRAME_MAX_BYTES`].
    ///
    /// Frame size = 5 (header) + payload + 2 (FECF). The limit is 512 B.
    #[error(
        "payload {0} B would produce a {total} B frame, exceeding TC_FRAME_MAX_BYTES ({TC_FRAME_MAX_BYTES})",
        total = 5 + .0 + 2
    )]
    PayloadTooLarge(usize),
}

impl TcFramer {
    /// Construct a framer with the given spacecraft ID (10-bit; value = 42).
    #[must_use]
    pub fn new(spacecraft_id: u16) -> Self {
        Self { spacecraft_id }
    }

    /// Encode a [`TcFrame`] as a CCSDS TC SDLP frame byte buffer.
    ///
    /// # Errors
    ///
    /// Returns [`FramerError::PayloadTooLarge`] if
    /// `5 + payload.len() + 2 > TC_FRAME_MAX_BYTES`.
    pub fn frame(&self, tc_frame: &TcFrame) -> Result<Vec<u8>, FramerError> {
        let payload_len = tc_frame.payload.len();
        let total_len = 5 + payload_len + 2; // header + data + FECF

        if total_len > TC_FRAME_MAX_BYTES {
            return Err(FramerError::PayloadTooLarge(payload_len));
        }

        // total_len ≤ TC_FRAME_MAX_BYTES (512) is guaranteed by the check above;
        // 511 < u16::MAX, so narrowing is always safe here.
        #[allow(clippy::cast_possible_truncation)]
        let frame_len_m1 = (total_len - 1) as u16; // FrameLength - 1 (10-bit field)

        // Bypass and Control-Command flags per frame type.
        let (bypass, cc): (u8, u8) = match tc_frame.frame_type {
            TcFrameType::TypeAd => (0, 0), // sequence-controlled data
            TcFrameType::TypeBd => (1, 0), // bypass data (emergency)
            TcFrameType::TypeBc => (1, 1), // bypass control (Set V(R))
        };

        let scid = self.spacecraft_id; // 10-bit; SCID[9:8] in low 2 bits of byte 0
        let vc = tc_frame.vc_id & 0x3F; // 6-bit VC ID

        // Assemble 5-byte primary header via explicit bit arithmetic (Q-C8).
        let b0 = (bypass << 5) | (cc << 4) | ((scid >> 8) as u8 & 0x03);
        let b1 = (scid & 0xFF) as u8;
        let b2 = (vc << 2) | ((frame_len_m1 >> 8) as u8 & 0x03);
        let b3 = (frame_len_m1 & 0xFF) as u8;
        let b4 = tc_frame.sequence;

        let mut out = Vec::with_capacity(total_len);
        out.extend_from_slice(&[b0, b1, b2, b3, b4]);
        out.extend_from_slice(&tc_frame.payload);

        // FECF: CRC-16/IBM-3740 over header + data (Q-C8: stored BE).
        let crc_engine = Crc::<u16>::new(&CRC_16_IBM_3740);
        let fecf = crc_engine.checksum(&out);
        out.extend_from_slice(&fecf.to_be_bytes());

        Ok(out)
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
    clippy::indexing_slicing
)]
mod tests {
    use super::*;
    use crate::uplink::cop1::{TcFrame, TcFrameType};

    const SCID: u16 = 42; // mission_config.h

    fn framer() -> TcFramer {
        TcFramer::new(SCID)
    }

    fn ad_frame(payload: Vec<u8>, sequence: u8) -> TcFrame {
        TcFrame {
            vc_id: 0,
            frame_type: TcFrameType::TypeAd,
            sequence,
            payload,
        }
    }

    // ── F1 ──────────────────────────────────────────────────────────────────
    // Given: AD frame with 10-byte payload.
    // When: frame() is called.
    // Then: output ≤ 512 B; bypass_flag=0 in byte 0; FECF validates.
    #[test]
    fn f1_ad_frame_within_size_limit_with_valid_fecf() {
        let payload = vec![0xAA; 10];
        let tc = ad_frame(payload, 0);
        let bytes = framer().frame(&tc).unwrap();

        assert!(bytes.len() <= TC_FRAME_MAX_BYTES);
        assert_eq!(bytes.len(), 5 + 10 + 2); // header + data + FECF

        // Bypass bit (bit 5 of byte 0) must be 0 for AD.
        assert_eq!(bytes[0] & 0x20, 0, "bypass flag must be 0 for TypeAd");

        // Verify FECF: CRC over all bytes except the trailing 2.
        let crc_engine = Crc::<u16>::new(&CRC_16_IBM_3740);
        let computed = crc_engine.checksum(&bytes[..bytes.len() - 2]);
        let stored = u16::from_be_bytes(bytes[bytes.len() - 2..].try_into().unwrap());
        assert_eq!(computed, stored, "FECF mismatch");
    }

    // ── F2 ──────────────────────────────────────────────────────────────────
    // Given: BD frame on VC 7.
    // When: frame() is called.
    // Then: bypass_flag=1 (bit 5 of byte 0); VC ID encoded correctly.
    #[test]
    fn f2_bd_frame_sets_bypass_flag_and_vc7() {
        let tc = TcFrame {
            vc_id: 7,
            frame_type: TcFrameType::TypeBd,
            sequence: 0,
            payload: vec![0xFF],
        };
        let bytes = framer().frame(&tc).unwrap();

        // Bypass bit (bit 5) must be 1 for BD.
        assert_eq!(bytes[0] & 0x20, 0x20, "bypass flag must be 1 for TypeBd");
        // CC bit (bit 4) must be 0 for data (not control).
        assert_eq!(bytes[0] & 0x10, 0, "CC flag must be 0 for TypeBd");
        // VC ID in byte 2 bits 7:2 = 7 → 0b000111_xx = 0x1C | frame_len bits.
        assert_eq!(bytes[2] >> 2, 7, "VC ID must be 7 in byte 2 bits [7:2]");
    }

    // ── F3 ──────────────────────────────────────────────────────────────────
    // Given: payload of 506 bytes (5 + 506 + 2 = 513 > 512).
    // When: frame() is called.
    // Then: FramerError::PayloadTooLarge is returned.
    #[test]
    fn f3_oversized_payload_returns_error() {
        let tc = ad_frame(vec![0u8; 506], 0); // 5 + 506 + 2 = 513 > 512
        let err = framer().frame(&tc).unwrap_err();
        assert!(matches!(err, FramerError::PayloadTooLarge(506)));
    }

    // ── F4 ──────────────────────────────────────────────────────────────────
    // Given: AD frame with sequence=42.
    // When: framed.
    // Then: byte 4 of the header equals 42.
    #[test]
    fn f4_sequence_number_encoded_in_byte4() {
        let tc = ad_frame(vec![0x00], 42);
        let bytes = framer().frame(&tc).unwrap();
        assert_eq!(bytes[4], 42, "frame sequence number must appear in byte 4");
    }

    // ── F5 ──────────────────────────────────────────────────────────────────
    // Given: BC frame (TypeBc).
    // When: framed.
    // Then: bypass=1 (bit 5) AND CC=1 (bit 4) in byte 0.
    #[test]
    fn f5_bc_frame_sets_bypass_and_cc_flags() {
        let tc = TcFrame {
            vc_id: 0,
            frame_type: TcFrameType::TypeBc,
            sequence: 0,
            payload: vec![0x00],
        };
        let bytes = framer().frame(&tc).unwrap();
        assert_eq!(bytes[0] & 0x20, 0x20, "bypass must be 1 for TypeBc");
        assert_eq!(bytes[0] & 0x10, 0x10, "CC must be 1 for TypeBc");
    }

    // ── F6 ──────────────────────────────────────────────────────────────────
    // Spacecraft ID (42 = 0x2A) is encoded across bytes 0 and 1.
    #[test]
    fn f6_spacecraft_id_encoded_correctly() {
        let tc = ad_frame(vec![0x00], 0);
        let bytes = framer().frame(&tc).unwrap();
        // SCID = 42 = 0x02A → bits [9:8] = 0 (in byte 0 bits [1:0]);
        //                      bits [7:0] = 0x2A (in byte 1).
        let scid_hi = u16::from(bytes[0] & 0x03);
        let scid_lo = u16::from(bytes[1]);
        let scid = (scid_hi << 8) | scid_lo;
        assert_eq!(scid, 42);
    }

    // ── F7 ──────────────────────────────────────────────────────────────────
    // Frame length field: FrameLen - 1 encoded in bytes 2 (bits 1:0) and 3.
    #[test]
    fn f7_frame_length_field_is_total_minus_one() {
        let payload = vec![0u8; 20];
        let tc = ad_frame(payload, 0);
        let bytes = framer().frame(&tc).unwrap();
        let total = bytes.len(); // 5 + 20 + 2 = 27
        #[allow(clippy::cast_possible_truncation)] // total ≤ 27 here; cast is safe
        let frame_len_m1 = (total - 1) as u16; // 26

        let encoded_hi = u16::from(bytes[2] & 0x03);
        let encoded_lo = u16::from(bytes[3]);
        let encoded = (encoded_hi << 8) | encoded_lo;
        assert_eq!(encoded, frame_len_m1);
    }

    // ── F8 ──────────────────────────────────────────────────────────────────
    // Maximum-sized valid payload (505 bytes → total = 512, exactly at limit).
    #[test]
    fn f8_max_payload_fits_exactly() {
        let tc = ad_frame(vec![0u8; 505], 0); // 5 + 505 + 2 = 512
        let bytes = framer().frame(&tc).unwrap();
        assert_eq!(bytes.len(), TC_FRAME_MAX_BYTES);
    }
}
