//! CCSDS Unsegmented Code (CUC) 7-byte time-stamp codec.
//!
//! Definition sites:
//! - [`docs/architecture/06-ground-segment-rust.md §2.3`](../../../docs/architecture/06-ground-segment-rust.md)
//!   — struct shape + codec API.
//! - [`docs/architecture/08-timing-and-clocks.md §2`](../../../docs/architecture/08-timing-and-clocks.md)
//!   — P-Field `0x2F` fleet-wide, TAI/1958-01-01 epoch, leap-second-free.
//!
//! Decisions: [`Q-C6`](../../../docs/standards/decisions-log.md) pins the
//! P-Field; [`Q-C8`](../../../docs/standards/decisions-log.md) pins the wire
//! endianness to big-endian and constrains the conversion locus to this
//! crate + `cfs_bindings`.
//!
//! The P-Field byte `0x2F` encodes, per CCSDS Time Code Format 301.0-B-4
//! §3.2.2: TAI epoch, 4-octet coarse-time field, 2-octet fine-time field.
//! No leap-second bookkeeping — TAI is monotone — so the codec has no
//! clock semantics of its own; it is a pure byte layout.

use crate::CcsdsError;

/// CUC P-Field byte. Pinned to `0x2F` fleet-wide per
/// [`08 §2`](../../../docs/architecture/08-timing-and-clocks.md) and
/// [`Q-C6`](../../../docs/standards/decisions-log.md). `decode_be` rejects
/// any other value with `CcsdsError::InvalidPField`.
pub const P_FIELD: u8 = 0x2F;

/// Number of octets the CUC codec reads from / writes to the wire.
/// 1 B P-Field + 4 B coarse seconds + 2 B fine units.
pub const ENCODED_LEN: usize = 7;

/// CCSDS Unsegmented Code (CUC) time stamp.
///
/// - `coarse`: whole seconds since the TAI epoch 1958-01-01T00:00:00 (no
///   leap-second bookkeeping — TAI is monotone by construction).
/// - `fine`:   sub-second remainder in units of 2⁻¹⁶ s (≈ 15.26 µs).
///
/// The struct holds only the time value; the P-Field byte is a wire-format
/// detail enforced by [`Cuc::encode_be`] and [`Cuc::decode_be`], not a
/// field of the struct.
///
/// `Copy + Eq + Hash` — 6 bytes, trivially copyable; downstream pipelines
/// may hash by time stamp (e.g. deduplicating TM at the ground segment).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Cuc {
    /// Whole seconds since the TAI epoch 1958-01-01T00:00:00.
    pub coarse: u32,
    /// Sub-second remainder, in units of 2⁻¹⁶ s.
    pub fine: u16,
}

impl Cuc {
    /// Encode this time stamp into the caller's 7-byte buffer.
    ///
    /// Byte layout (big-endian, per Q-C8):
    ///
    /// | offset | field                     |
    /// |--------|---------------------------|
    /// | 0      | P-Field (`0x2F`)          |
    /// | 1..=4  | `coarse` (u32 BE)         |
    /// | 5..=6  | `fine`   (u16 BE)         |
    ///
    /// Caller owns the output buffer — stack-allocated, no heap, `no_std`-ready.
    pub fn encode_be(&self, out: &mut [u8; ENCODED_LEN]) {
        let [c0, c1, c2, c3] = self.coarse.to_be_bytes();
        let [f0, f1] = self.fine.to_be_bytes();
        *out = [P_FIELD, c0, c1, c2, c3, f0, f1];
    }

    /// Decode a CUC time stamp from the first 7 bytes of `buf`.
    ///
    /// Trailing bytes beyond offset 7 are ignored — the primary use case
    /// is decoding the CUC at the start of a longer secondary-header frame.
    ///
    /// # Errors
    ///
    /// - [`CcsdsError::BufferTooShort`] if `buf.len() < 7`.
    /// - [`CcsdsError::InvalidPField`] if `buf[0] != 0x2F`. The offending
    ///   byte is preserved in the error variant for operator diagnostics.
    pub fn decode_be(buf: &[u8]) -> Result<Self, CcsdsError> {
        let Some(head) = buf.first_chunk::<ENCODED_LEN>() else {
            return Err(CcsdsError::BufferTooShort {
                need: ENCODED_LEN,
                got: buf.len(),
            });
        };
        let [p, c0, c1, c2, c3, f0, f1] = *head;
        if p != P_FIELD {
            return Err(CcsdsError::InvalidPField(p));
        }
        Ok(Self {
            coarse: u32::from_be_bytes([c0, c1, c2, c3]),
            fine: u16::from_be_bytes([f0, f1]),
        })
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::panic, clippy::expect_used)] // test-only conveniences: workspace denies these for production code
mod tests {
    use super::{Cuc, P_FIELD};
    use crate::CcsdsError;

    // GIVEN a non-zero Cuc { coarse: 0x1234_5678, fine: 0xAABB }
    // WHEN  we encode_be into a [u8; 7] buffer and then decode_be it back
    // THEN  the decoded Cuc equals the original, buf[0] == 0x2F,
    //       buf[1..5] is coarse big-endian, buf[5..7] is fine big-endian.
    #[test]
    fn test_encode_decode_roundtrip_nonzero() {
        let original = Cuc {
            coarse: 0x1234_5678,
            fine: 0xAABB,
        };
        let mut buf = [0u8; 7];
        original.encode_be(&mut buf);

        assert_eq!(buf[0], 0x2F, "P-Field must be 0x2F on the wire");
        assert_eq!(&buf[1..5], &[0x12, 0x34, 0x56, 0x78], "coarse BE layout");
        assert_eq!(&buf[5..7], &[0xAA, 0xBB], "fine BE layout");

        let decoded = Cuc::decode_be(&buf).expect("well-formed buffer must decode");
        assert_eq!(decoded, original);
    }

    // GIVEN the lower-bound value Cuc { 0, 0 }
    // WHEN  encoded
    // THEN  the on-wire bytes are [0x2F, 0,0,0,0, 0,0] and decode is lossless.
    #[test]
    fn test_roundtrip_all_zero() {
        let original = Cuc { coarse: 0, fine: 0 };
        let mut buf = [0xFFu8; 7]; // pre-fill with 0xFF to prove encode overwrites every byte
        original.encode_be(&mut buf);
        assert_eq!(buf, [0x2F, 0, 0, 0, 0, 0, 0]);
        assert_eq!(Cuc::decode_be(&buf).unwrap(), original);
    }

    // GIVEN the upper-bound value Cuc { u32::MAX, u16::MAX }
    // WHEN  encoded
    // THEN  the on-wire bytes are [0x2F, 0xFF×4, 0xFF×2] and decode is lossless.
    #[test]
    fn test_roundtrip_all_max() {
        let original = Cuc {
            coarse: u32::MAX,
            fine: u16::MAX,
        };
        let mut buf = [0u8; 7];
        original.encode_be(&mut buf);
        assert_eq!(buf, [0x2F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]);
        assert_eq!(Cuc::decode_be(&buf).unwrap(), original);
    }

    // GIVEN a 7-byte buffer whose first byte is not 0x2F
    // WHEN  we attempt to decode
    // THEN  we get Err(InvalidPField(byte)) preserving the offending byte.
    //       Exercises the 0x00, 0xFF, and off-by-one (0x2E, 0x30) rejection paths.
    #[test]
    fn test_decode_rejects_invalid_pfield() {
        for bad in [0x00u8, 0xFF, 0x2E, 0x30] {
            let buf = [bad, 0, 0, 0, 0, 0, 0];
            assert_eq!(Cuc::decode_be(&buf), Err(CcsdsError::InvalidPField(bad)));
        }
    }

    // GIVEN a buffer shorter than the 7-byte CUC frame
    // WHEN  we attempt to decode
    // THEN  we get Err(BufferTooShort { need: 7, got: buf.len() }).
    //       Exercises the length-guard branch before any byte is read
    //       (so buf[0] is never touched on a zero-length buffer).
    #[test]
    fn test_decode_rejects_short_buffer() {
        for len in 0..7 {
            let buf = vec![0x2Fu8; len];
            assert_eq!(
                Cuc::decode_be(&buf),
                Err(CcsdsError::BufferTooShort { need: 7, got: len }),
            );
        }
    }

    // GIVEN a buffer of >7 bytes whose first 7 bytes are a valid CUC frame
    // WHEN  we decode
    // THEN  we get Ok(Cuc { .. }) — trailing bytes are not read.
    //       This is the normal secondary-header decode path (10-byte header
    //       with CUC in the first 7 octets; func_code/instance_id follow).
    #[test]
    fn test_decode_accepts_buffer_with_trailing_bytes() {
        let buf = [0x2F, 0, 0, 0, 1, 0, 2, 0xDE, 0xAD, 0xBE, 0xEF];
        let decoded = Cuc::decode_be(&buf).unwrap();
        assert_eq!(decoded, Cuc { coarse: 1, fine: 2 });
    }

    // GIVEN the P_FIELD module constant
    // WHEN  compared against the numeric literal 0x2F
    // THEN  equality holds. Locks the fleet invariant against silent edits;
    //       if anyone retargets the P-Field, this test fails loudly.
    #[test]
    fn test_p_field_constant_is_0x2f() {
        assert_eq!(P_FIELD, 0x2F);
    }
}
