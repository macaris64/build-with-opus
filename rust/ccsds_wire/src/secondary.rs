//! CCSDS Space Packet Protocol **secondary header** (10 bytes, big-endian).
//!
//! Phase 08 locus. Rides immediately after the 6-byte [`crate::PrimaryHeader`]
//! on every SAKURA-II frame (the sec-hdr flag is pinned to `1` by arch §2.4
//! — the fleet never omits the time tag). Composes the Phase 06 [`crate::Cuc`]
//! codec with the Phase 04 [`crate::FuncCode`] and [`crate::InstanceId`]
//! sealed newtypes into a 80-bit on-wire frame.
//!
//! # Wire layout
//!
//! Per [arch §2.5](../../../docs/architecture/06-ground-segment-rust.md) and
//! [Q-C6](../../../docs/standards/decisions-log.md):
//!
//! ```text
//! byte 0..=6  : Cuc (7 B)           — P-Field 0x2F + 4 B coarse + 2 B fine
//! byte 7..=8  : FuncCode (u16 BE)   — nonzero; 0x0000 reserved
//! byte 9      : InstanceId (u8)     — nonzero; 0x00 reserved
//! ```
//!
//! # Guards
//!
//! [`SecondaryHeader::decode`] rejects non-conformant frames via
//! [`CcsdsError`]:
//!
//! | Condition | Error |
//! |---|---|
//! | `buf.len() < 10` | [`CcsdsError::BufferTooShort`] |
//! | `buf[0] != 0x2F` (inner CUC) | [`CcsdsError::InvalidPField`] |
//! | `buf[7..=8] == 0x0000` | [`CcsdsError::FuncCodeReserved`] |
//! | `buf[9] == 0x00` | [`CcsdsError::InstanceIdReserved`] |
//!
//! The inner `Cuc::decode_be` also surfaces `BufferTooShort`, but the outer
//! `buf.len() < 10` check makes that arm mathematically unreachable — kept in
//! the `?` chain to keep the single auditable construction path through
//! `Cuc::decode_be` (mirrors the Phase 07 `Apid::new` / `SequenceCount::new`
//! defensive pattern in [`crate::primary`]).
//!
//! # `time_suspect`
//!
//! Bit 0 of the 16-bit `func_code` is the **time-suspect flag** per
//! [arch 08 §4](../../../docs/architecture/08-timing-and-clocks.md). When a
//! tagging asset's time service drops to `DEGRADED` (loss-of-signal > 4 h
//! without a correlation frame), it sets this bit on every outgoing TM
//! packet. [`SecondaryHeader::time_suspect`] is a pure bit extraction —
//! semantic interpretation ("apply only to TM") is Phase 09's
//! `SpacePacket` concern, since `SecondaryHeader` does not know the
//! enclosing primary-header type discriminant.
//!
//! # Definition sites
//! - [`docs/architecture/06-ground-segment-rust.md §2.5`](../../../docs/architecture/06-ground-segment-rust.md) — shape + API
//! - [`docs/architecture/08-timing-and-clocks.md §4`](../../../docs/architecture/08-timing-and-clocks.md) — `time_suspect` semantics
//! - `SYS-REQ-0021`, `SYS-REQ-0034`; `Q-C6`, `Q-C8`, `Q-F4`.

use crate::{CcsdsError, Cuc, FuncCode, InstanceId};

/// CCSDS Space Packet Protocol secondary header.
///
/// Sealed struct — all fields are private. The only paths to a valid
/// `SecondaryHeader` are [`SecondaryHeader::new`] (from already-validated
/// sealed newtypes) and [`SecondaryHeader::decode`] (from wire bytes, which
/// enforces every §2.5 invariant at the type boundary). The only path back
/// to wire bytes is [`SecondaryHeader::encode`]. No public `u16`/`u32`
/// field exists — part of the Q-C8 grep-visible endianness-conversion
/// discipline.
///
/// `Copy` because the struct is plain-old-data (one `Cuc` = 6 bytes payload
/// + two sealed newtypes); cheap to pass by value.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SecondaryHeader {
    time: Cuc,
    func_code: FuncCode,
    instance_id: InstanceId,
}

impl SecondaryHeader {
    /// Encoded length on the wire, in octets. Fixed by arch §2.5
    /// (7 B CUC + 2 B `func_code` + 1 B `instance_id`).
    pub const LEN: usize = 10;

    /// Compose a secondary header from already-validated sealed newtypes.
    ///
    /// Infallible because every input type has already been validated at
    /// its own sealed boundary: `Cuc` is structurally valid by
    /// construction; [`FuncCode::new`] rejects `0x0000`;
    /// [`InstanceId::new`] rejects `0x00`. This is the constructor the
    /// Phase 09 `PacketBuilder` will call.
    #[must_use]
    pub const fn new(time: Cuc, func_code: FuncCode, instance_id: InstanceId) -> Self {
        Self {
            time,
            func_code,
            instance_id,
        }
    }

    /// CUC time stamp (7 B on-wire, P-Field pinned to `0x2F`).
    #[must_use]
    pub const fn time(&self) -> Cuc {
        self.time
    }

    /// Function code (nonzero `u16`, sealed).
    #[must_use]
    pub const fn func_code(&self) -> FuncCode {
        self.func_code
    }

    /// Instance ID (nonzero `u8`, sealed).
    #[must_use]
    pub const fn instance_id(&self) -> InstanceId {
        self.instance_id
    }

    /// Time-suspect flag — bit 0 of `func_code`.
    ///
    /// `true` when the tagging asset's time service is `DEGRADED` (LOS > 4 h
    /// without a correlation frame, per arch 08 §4). Downstream — log writer
    /// prefixes TAI with `~`; operator UI renders a "suspect" badge. The
    /// packet is still processed normally — time-suspect is a hint, not a
    /// drop condition.
    ///
    /// Pure bit extraction: the accessor does not know the enclosing
    /// `PrimaryHeader` type, so applying the TM-only semantic is the caller's
    /// job (Phase 09 `SpacePacket`).
    #[must_use]
    pub const fn time_suspect(&self) -> bool {
        self.func_code.get() & 0x0001 != 0
    }

    /// Encode this header into the caller's 10-byte buffer, big-endian.
    ///
    /// Caller owns the output buffer — stack-allocated, no heap,
    /// `no_std`-ready. Every byte of `out` is overwritten.
    pub fn encode(&self, out: &mut [u8; Self::LEN]) {
        // Delegate the 7-byte CUC slice to `Cuc::encode_be` so the P-Field
        // pinning (Q-C6) stays in exactly one place.
        let mut cuc_buf = [0u8; 7];
        self.time.encode_be(&mut cuc_buf);
        out[0..7].copy_from_slice(&cuc_buf);

        // Func code: u16 BE into bytes 7..=8.
        let [f0, f1] = self.func_code.get().to_be_bytes();
        out[7] = f0;
        out[8] = f1;

        // Instance ID: u8 into byte 9.
        out[9] = self.instance_id.get();
    }

    /// Decode a secondary header from the first 10 bytes of `buf`.
    ///
    /// Trailing bytes beyond offset 10 are ignored — the normal call site is
    /// Phase 09's `SpacePacket::parse`, which hands over the sub-slice
    /// starting at the end of the primary header.
    ///
    /// # Errors
    ///
    /// - [`CcsdsError::BufferTooShort`] if `buf.len() < 10`.
    /// - [`CcsdsError::InvalidPField`] if `buf[0] != 0x2F`
    ///   (propagated from [`Cuc::decode_be`]).
    /// - [`CcsdsError::FuncCodeReserved`] if `buf[7..=8]` is `0x0000`.
    /// - [`CcsdsError::InstanceIdReserved`] if `buf[9]` is `0x00`.
    pub fn decode(buf: &[u8]) -> Result<Self, CcsdsError> {
        let Some(head) = buf.first_chunk::<{ Self::LEN }>() else {
            return Err(CcsdsError::BufferTooShort {
                need: Self::LEN,
                got: buf.len(),
            });
        };
        // `time` propagates `InvalidPField` unchanged; its `BufferTooShort`
        // arm is unreachable because we already checked buf.len() >= 10.
        let time = Cuc::decode_be(&head[0..7])?;

        let func_code = FuncCode::new(u16::from_be_bytes([head[7], head[8]]))?;
        let instance_id = InstanceId::new(head[9])?;

        Ok(Self {
            time,
            func_code,
            instance_id,
        })
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::panic, clippy::expect_used)]
mod tests {
    use super::*;

    // --- Red: happy-path roundtrip at the lower bounds ---------------------
    // Given:  SecondaryHeader with time=Cuc{0,0}, func_code=0x0002 (smallest
    //         nonzero with the time-suspect bit clear — `0x0001` would
    //         spuriously set time_suspect), instance_id=1 (min nonzero).
    // When:   encoded into a 10-byte buffer and immediately decoded.
    // Then:   round-trip is lossless; byte 0 is the pinned P-Field 0x2F;
    //         bytes 7..=8 = [0x00, 0x02] (func_code BE); byte 9 = 0x01;
    //         time_suspect() == false.
    #[test]
    fn test_secondary_roundtrip_minimum_bounds_preserves_all_fields() {
        let original = SecondaryHeader::new(
            Cuc { coarse: 0, fine: 0 },
            FuncCode::new(0x0002).unwrap(),
            InstanceId::new(1).unwrap(),
        );
        let mut buf = [0xFFu8; 10]; // pre-fill to prove encode overwrites every byte
        original.encode(&mut buf);

        assert_eq!(buf[0], 0x2F, "P-Field pinned per Q-C6");
        assert_eq!(&buf[7..=8], &[0x00, 0x02], "func_code BE layout");
        assert_eq!(buf[9], 0x01, "instance_id byte");

        let decoded = SecondaryHeader::decode(&buf).unwrap();
        assert_eq!(decoded, original);
        assert!(!decoded.time_suspect());
    }

    // --- Happy path: upper bounds, time_suspect bit propagates -------------
    // Given:  SecondaryHeader with time=Cuc{u32::MAX,u16::MAX},
    //         func_code=0xFFFF (bit 0 set → time_suspect), instance_id=255.
    // When:   encoded and decoded.
    // Then:   round-trip is lossless; time_suspect() == true.
    #[test]
    fn test_secondary_roundtrip_maximum_bounds_preserves_all_fields() {
        let original = SecondaryHeader::new(
            Cuc {
                coarse: u32::MAX,
                fine: u16::MAX,
            },
            FuncCode::new(u16::MAX).unwrap(),
            InstanceId::new(u8::MAX).unwrap(),
        );
        let mut buf = [0u8; 10];
        original.encode(&mut buf);

        let decoded = SecondaryHeader::decode(&buf).unwrap();
        assert_eq!(decoded, original);
        assert_eq!(decoded.time().coarse, u32::MAX);
        assert_eq!(decoded.time().fine, u16::MAX);
        assert_eq!(decoded.func_code().get(), u16::MAX);
        assert_eq!(decoded.instance_id().get(), u8::MAX);
        assert!(decoded.time_suspect());
    }

    // --- time_suspect: bit 0 set → true ------------------------------------
    // Given:  func_code = 0x0001 (only bit 0 set).
    // When:   constructed via `new`.
    // Then:   time_suspect() == true. Isolates bit 0 as the flag bit.
    #[test]
    fn test_secondary_time_suspect_true_when_func_code_bit0_set() {
        let hdr = SecondaryHeader::new(
            Cuc { coarse: 0, fine: 0 },
            FuncCode::new(0x0001).unwrap(),
            InstanceId::new(1).unwrap(),
        );
        assert!(hdr.time_suspect());
    }

    // --- time_suspect: bit 0 clear → false (even on non-trivial func_code) -
    // Given:  func_code = 0x0002 (bit 1 set, bit 0 clear).
    // When:   constructed.
    // Then:   time_suspect() == false. Confirms only bit 0 is probed;
    //         higher-order bits don't leak into the flag.
    #[test]
    fn test_secondary_time_suspect_false_when_func_code_bit0_clear() {
        let hdr = SecondaryHeader::new(
            Cuc { coarse: 0, fine: 0 },
            FuncCode::new(0x0002).unwrap(),
            InstanceId::new(1).unwrap(),
        );
        assert!(!hdr.time_suspect());
    }

    // --- BufferTooShort across the full 0..10 length range ----------------
    // Given:  every buffer length strictly less than the 10-byte header.
    // When:   we decode.
    // Then:   Err(BufferTooShort { need: 10, got: len }) — never reads byte 0
    //         (so a zero-length buffer does not panic).
    #[test]
    fn test_secondary_decode_returns_err_when_buffer_is_too_short() {
        for len in 0..SecondaryHeader::LEN {
            let buf = vec![0x2Fu8; len]; // even a well-formed prefix fails
            assert_eq!(
                SecondaryHeader::decode(&buf),
                Err(CcsdsError::BufferTooShort {
                    need: SecondaryHeader::LEN,
                    got: len,
                }),
                "len={len}",
            );
        }
    }

    // --- InvalidPField propagated from inner Cuc decode --------------------
    // Given:  a 10-byte buffer whose P-Field byte is not 0x2F.
    // When:   we decode.
    // Then:   Err(InvalidPField(byte)). Sweeps {0x00, 0x1F, 0x2E, 0x30, 0xFF}
    //         to cover all-zero, off-by-one-low, off-by-one-high, and all-ones.
    #[test]
    fn test_secondary_decode_propagates_invalid_pfield_from_cuc() {
        for bad in [0x00u8, 0x1F, 0x2E, 0x30, 0xFF] {
            // fill with valid func_code (0x00_01) and instance_id (0x01) so
            // the only reason for failure is the P-Field
            let buf = [bad, 0, 0, 0, 0, 0, 0, 0x00, 0x01, 0x01];
            assert_eq!(
                SecondaryHeader::decode(&buf),
                Err(CcsdsError::InvalidPField(bad)),
                "bad=0x{bad:02X}",
            );
        }
    }

    // --- FuncCodeReserved when bytes 7..=8 are 0x0000 ----------------------
    // Given:  an otherwise-valid 10-byte buffer whose func_code bytes are 0.
    // When:   we decode.
    // Then:   Err(FuncCodeReserved). Confirms `FuncCode::new`'s zero-rejection
    //         fires inside the decode path.
    #[test]
    fn test_secondary_decode_returns_err_when_func_code_is_zero() {
        let buf = [0x2F, 0, 0, 0, 0, 0, 0, 0x00, 0x00, 0x01];
        assert_eq!(
            SecondaryHeader::decode(&buf),
            Err(CcsdsError::FuncCodeReserved),
        );
    }

    // --- InstanceIdReserved when byte 9 is 0x00 ----------------------------
    // Given:  an otherwise-valid 10-byte buffer whose instance_id byte is 0.
    // When:   we decode.
    // Then:   Err(InstanceIdReserved). Confirms `InstanceId::new`'s
    //         zero-rejection fires inside the decode path.
    #[test]
    fn test_secondary_decode_returns_err_when_instance_id_is_zero() {
        let buf = [0x2F, 0, 0, 0, 0, 0, 0, 0x00, 0x01, 0x00];
        assert_eq!(
            SecondaryHeader::decode(&buf),
            Err(CcsdsError::InstanceIdReserved),
        );
    }

    // --- Explicit BE byte layout assertion ---------------------------------
    // Given:  time=Cuc{0x1234_5678, 0xAABB}, func_code=0xC0DE,
    //         instance_id=0x42 — every byte distinct so mis-ordered BE writes
    //         are caught.
    // When:   encoded.
    // Then:   the 10 output bytes match the hand-computed BE layout:
    //           b0        = 0x2F                       (P-Field)
    //           b1..=b4   = 0x12, 0x34, 0x56, 0x78     (coarse BE)
    //           b5..=b6   = 0xAA, 0xBB                 (fine BE)
    //           b7..=b8   = 0xC0, 0xDE                 (func_code BE)
    //           b9        = 0x42                       (instance_id)
    #[test]
    fn test_secondary_encode_bytes_match_be_layout() {
        let hdr = SecondaryHeader::new(
            Cuc {
                coarse: 0x1234_5678,
                fine: 0xAABB,
            },
            FuncCode::new(0xC0DE).unwrap(),
            InstanceId::new(0x42).unwrap(),
        );
        let mut buf = [0u8; 10];
        hdr.encode(&mut buf);
        assert_eq!(
            buf,
            [0x2F, 0x12, 0x34, 0x56, 0x78, 0xAA, 0xBB, 0xC0, 0xDE, 0x42],
        );
    }

    // --- Length constant locked against silent edits -----------------------
    #[test]
    fn test_secondary_len_const_is_ten() {
        assert_eq!(SecondaryHeader::LEN, 10);
    }

    // --- Accessors return constructor inputs unchanged --------------------
    // Given:  distinct input values for each field.
    // When:   constructed via `new`.
    // Then:   each accessor returns exactly its input — guards against
    //         swapped assignments in `new`.
    #[test]
    fn test_secondary_accessors_return_constructor_inputs() {
        let t = Cuc {
            coarse: 0xDEAD_BEEF,
            fine: 0xCAFE,
        };
        let fc = FuncCode::new(0x1234).unwrap();
        let id = InstanceId::new(0x77).unwrap();
        let hdr = SecondaryHeader::new(t, fc, id);
        assert_eq!(hdr.time(), t);
        assert_eq!(hdr.func_code(), fc);
        assert_eq!(hdr.instance_id(), id);
    }

    // --- time_suspect bit-isolation sweep ----------------------------------
    // Given:  func_code values that exercise every combination of "bit 0
    //         set or clear" across high/low byte patterns.
    // When:   time_suspect() is queried.
    // Then:   it returns exactly `(func_code & 1) != 0` — no leakage from
    //         any other bit.
    #[test]
    fn test_secondary_time_suspect_sweeps_low_bit_patterns() {
        let cases: [(u16, bool); 6] = [
            (0x0001, true),
            (0x0002, false),
            (0x00FF, true),
            (0xFFFE, false),
            (0xFFFF, true),
            (0x8000, false),
        ];
        for (fc_raw, expected) in cases {
            let hdr = SecondaryHeader::new(
                Cuc { coarse: 0, fine: 0 },
                FuncCode::new(fc_raw).unwrap(),
                InstanceId::new(1).unwrap(),
            );
            assert_eq!(hdr.time_suspect(), expected, "func_code=0x{fc_raw:04X}");
        }
    }
}
