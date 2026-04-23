//! Sized newtypes for CCSDS header primitives.
//!
//! Phase 04 locus. Replicates the sealing pattern established by
//! `crate::apid::Apid` (Phase 03) for the four remaining size-constrained
//! primitives the primary and secondary headers need. Each type's
//! `.get()` accessor is the sole path back to its raw integer — keeping
//! that path narrow is what makes every endianness-conversion site
//! grep-visible per Q-C8 (`docs/standards/decisions-log.md`).
//!
//! Definition sites:
//! - `docs/architecture/06-ground-segment-rust.md §2.2` — per-field invariants
//! - `docs/architecture/07-comms-stack.md §2` — primary-header layout (`SeqCount` 14-bit, `PacketDataLength` raw)
//! - `SYS-REQ-0026` — instance multiplicity via `InstanceId`, not by burning extra APIDs
//!
//! The three placeholder error enums in this module (`SequenceCountError`,
//! `FuncCodeError`, `InstanceIdError`) are folded into the unified
//! `crate::CcsdsError` by Phase 05 (arch §2.8). `PacketDataLength` has no
//! placeholder — its constructor is infallible because the only invariant
//! (buffer length vs declared length) is a decode-time concern and
//! surfaces as `CcsdsError::LengthMismatch` in Phase 07.

// --- SequenceCount ----------------------------------------------------------

/// `SequenceCount` construction error.
///
/// Temporary module-local enum per the Phase 04 Phase Card: carries only
/// `#[derive(Debug)]` (plus `PartialEq`/`Eq` so tests can `assert_eq!`).
/// Phase 05 folds this variant into `crate::CcsdsError`; see decisions-log
/// ticket for the §2.8 enum gap flagged from Phase 04.
#[derive(Debug, PartialEq, Eq)]
pub enum SequenceCountError {
    /// Raw value exceeded the 14-bit sequence-count ceiling (`> 0x3FFF`).
    /// The raw input is preserved so callers can log or surface it.
    SequenceCountOutOfRange(u16),
}

/// 14-bit CCSDS primary-header sequence count (`0x0000..=0x3FFF`).
///
/// Sealed newtype. The inner `u16` is private, so the only path to a valid
/// `SequenceCount` is [`SequenceCount::new`], which enforces the 14-bit
/// invariant at the type boundary. `0x0000` is the valid count emitted
/// immediately after boot — there is no reserved sentinel in this range.
///
/// `Copy + Eq + Hash` — downstream routing may bucket packets by
/// (`Apid`, `SequenceCount`) pair; the derives land now to avoid churn.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SequenceCount(u16);

impl SequenceCount {
    /// Maximum valid sequence count — the 14-bit ceiling (`0x3FFF`).
    pub const MAX: u16 = 0x3FFF;

    /// Construct a `SequenceCount` from a raw `u16`.
    ///
    /// # Errors
    ///
    /// Returns [`SequenceCountError::SequenceCountOutOfRange`] if
    /// `v > 0x3FFF`. The error carries `v` unchanged.
    pub const fn new(v: u16) -> Result<Self, SequenceCountError> {
        if v > Self::MAX {
            Err(SequenceCountError::SequenceCountOutOfRange(v))
        } else {
            Ok(SequenceCount(v))
        }
    }

    /// Raw 14-bit value. Call sites are auditable via `rg '\.get\(\)'`
    /// — part of the Q-C8 grep-visible endianness-conversion discipline.
    #[must_use]
    pub const fn get(self) -> u16 {
        self.0
    }
}

// --- PacketDataLength -------------------------------------------------------

/// CCSDS primary-header packet data length (raw 16-bit).
///
/// Sealed newtype. Construction is infallible because the only invariant
/// in arch §2.2 — "declared length matches the actual buffer length" —
/// is a decode-time concern. The mismatch surfaces as
/// `CcsdsError::LengthMismatch { declared, actual }` from
/// `PrimaryHeader::decode` (Phase 07), not from this constructor.
///
/// Per CCSDS 133.0-B-2, the on-wire value is `total_packet_length - 7`;
/// this newtype stores the raw field as written on the wire.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct PacketDataLength(u16);

impl PacketDataLength {
    /// Maximum representable value — the full 16-bit domain is valid
    /// (`u16::MAX`). No intrinsic per-value invariant exists for this field.
    pub const MAX: u16 = u16::MAX;

    /// Construct a `PacketDataLength` from a raw `u16`. Cannot fail.
    #[must_use]
    pub const fn new(v: u16) -> Self {
        PacketDataLength(v)
    }

    /// Raw 16-bit value. Call sites are auditable via `rg '\.get\(\)'`
    /// — part of the Q-C8 grep-visible endianness-conversion discipline.
    #[must_use]
    pub const fn get(self) -> u16 {
        self.0
    }
}

// --- FuncCode ---------------------------------------------------------------

/// `FuncCode` construction error.
///
/// Temporary module-local enum per the Phase 04 Phase Card. Phase 05 folds
/// this variant into `crate::CcsdsError::FuncCodeReserved` (arch §2.8).
#[derive(Debug, PartialEq, Eq)]
pub enum FuncCodeError {
    /// Raw value was `0x0000`, which is reserved by arch §2.2 and must
    /// not be used as a live function code.
    FuncCodeReserved,
}

/// CCSDS secondary-header function code (nonzero `u16`).
///
/// Sealed newtype. The inner `u16` is private; `0x0000` is reserved and
/// rejected at the type boundary, so downstream code that holds a
/// `FuncCode` has a compile-time guarantee the value is nonzero.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct FuncCode(u16);

impl FuncCode {
    /// Maximum valid function code. Any nonzero `u16` is valid, so the
    /// ceiling is `u16::MAX`.
    pub const MAX: u16 = u16::MAX;

    /// Construct a `FuncCode` from a raw `u16`.
    ///
    /// # Errors
    ///
    /// Returns [`FuncCodeError::FuncCodeReserved`] if `v == 0x0000`.
    pub const fn new(v: u16) -> Result<Self, FuncCodeError> {
        if v == 0 {
            Err(FuncCodeError::FuncCodeReserved)
        } else {
            Ok(FuncCode(v))
        }
    }

    /// Raw value. Call sites are auditable via `rg '\.get\(\)'`
    /// — part of the Q-C8 grep-visible endianness-conversion discipline.
    #[must_use]
    pub const fn get(self) -> u16 {
        self.0
    }
}

// --- InstanceId -------------------------------------------------------------

/// `InstanceId` construction error.
///
/// Temporary module-local enum per the Phase 04 Phase Card. Phase 05 folds
/// this variant into `crate::CcsdsError::InstanceIdReserved` (arch §2.8).
#[derive(Debug, PartialEq, Eq)]
pub enum InstanceIdError {
    /// Raw value was `0`, which is reserved by arch §2.2 as the broadcast
    /// sentinel and must not be used as a unit-addressed instance id.
    InstanceIdReserved,
}

/// CCSDS secondary-header instance id (`1..=255`).
///
/// Sealed newtype. `0` is reserved as the broadcast sentinel per arch §2.2
/// and is rejected at the type boundary.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct InstanceId(u8);

impl InstanceId {
    /// Maximum valid instance id (`255`).
    pub const MAX: u8 = u8::MAX;

    /// Minimum valid instance id (`1`). `0` is reserved for broadcast.
    pub const MIN: u8 = 1;

    /// Construct an `InstanceId` from a raw `u8`.
    ///
    /// # Errors
    ///
    /// Returns [`InstanceIdError::InstanceIdReserved`] if `v == 0`.
    pub const fn new(v: u8) -> Result<Self, InstanceIdError> {
        if v == 0 {
            Err(InstanceIdError::InstanceIdReserved)
        } else {
            Ok(InstanceId(v))
        }
    }

    /// Raw 8-bit value. Call sites are auditable via `rg '\.get\(\)'`
    /// — part of the Q-C8 grep-visible endianness-conversion discipline.
    #[must_use]
    pub const fn get(self) -> u8 {
        self.0
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::panic, clippy::expect_used)] // test-only conveniences: workspace denies these for production code
mod tests {
    use super::*;

    // === SequenceCount =====================================================

    // --- RED (first failing test of Phase 04) ------------------------------
    // Given: a sequence-count value one past the 14-bit ceiling.
    // When:  constructed through the sealed boundary.
    // Then:  construction fails with
    //        `SequenceCountError::SequenceCountOutOfRange(v)` carrying the
    //        offending raw value (arch §2.2; Phase 05 will re-surface this
    //        as `CcsdsError::SequenceCountOutOfRange(u16)` pending the
    //        §2.8 enum gap flagged in `docs/standards/decisions-log.md`).
    #[test]
    fn test_sequence_count_new_returns_err_when_exceeds_14_bit_range() {
        let raw: u16 = 0x4000;
        let result = SequenceCount::new(raw);
        assert_eq!(
            result,
            Err(SequenceCountError::SequenceCountOutOfRange(0x4000))
        );
    }

    // --- Happy path: lower boundary (first packet after boot) --------------
    #[test]
    fn test_sequence_count_new_returns_ok_when_zero() {
        let sc = SequenceCount::new(0x0000).unwrap();
        assert_eq!(sc.get(), 0x0000);
    }

    // --- Happy path: upper boundary ----------------------------------------
    #[test]
    fn test_sequence_count_new_returns_ok_at_max_boundary() {
        let sc = SequenceCount::new(0x3FFF).unwrap();
        assert_eq!(sc.get(), 0x3FFF);
    }

    // --- Happy path: one below ceiling (catches `>=` vs `>` in `new`) ------
    #[test]
    fn test_sequence_count_new_returns_ok_one_below_ceiling() {
        let sc = SequenceCount::new(0x3FFE).unwrap();
        assert_eq!(sc.get(), 0x3FFE);
    }

    // --- Error path: u16::MAX (far out of range, raw preserved) ------------
    #[test]
    fn test_sequence_count_new_returns_err_when_u16_max() {
        let result = SequenceCount::new(u16::MAX);
        assert_eq!(
            result,
            Err(SequenceCountError::SequenceCountOutOfRange(u16::MAX))
        );
    }

    // --- Constant matches the documented invariant -------------------------
    #[test]
    fn test_sequence_count_max_constant_is_0x3fff() {
        assert_eq!(SequenceCount::MAX, 0x3FFF);
    }

    // === PacketDataLength ==================================================
    // No reserved-rejection test: arch §2.2 admits the full 16-bit domain;
    // the buffer-vs-declared-length check is Phase 07's decoder concern.

    // --- Happy path: lower boundary ----------------------------------------
    #[test]
    fn test_packet_data_length_new_accepts_zero() {
        let pdl = PacketDataLength::new(0x0000);
        assert_eq!(pdl.get(), 0x0000);
    }

    // --- Happy path: upper boundary ----------------------------------------
    #[test]
    fn test_packet_data_length_new_accepts_u16_max() {
        let pdl = PacketDataLength::new(u16::MAX);
        assert_eq!(pdl.get(), u16::MAX);
    }

    // --- Happy path: mid-range roundtrip -----------------------------------
    #[test]
    fn test_packet_data_length_new_roundtrips_mid_range() {
        let pdl = PacketDataLength::new(0x0100);
        assert_eq!(pdl.get(), 0x0100);
    }

    // --- Constant matches the documented invariant -------------------------
    #[test]
    fn test_packet_data_length_max_constant_is_u16_max() {
        assert_eq!(PacketDataLength::MAX, u16::MAX);
    }

    // === FuncCode ==========================================================

    // --- Error path: reserved 0x0000 rejected ------------------------------
    // Given: function code 0x0000 (reserved by arch §2.2).
    // When:  constructed through the sealed boundary.
    // Then:  rejected with `FuncCodeError::FuncCodeReserved`.
    #[test]
    fn test_func_code_new_returns_err_when_zero() {
        let result = FuncCode::new(0);
        assert_eq!(result, Err(FuncCodeError::FuncCodeReserved));
    }

    // --- Happy path: lower boundary (0x0001 is the smallest valid) ---------
    #[test]
    fn test_func_code_new_returns_ok_when_one() {
        let fc = FuncCode::new(0x0001).unwrap();
        assert_eq!(fc.get(), 0x0001);
    }

    // --- Happy path: upper boundary (u16::MAX is valid) --------------------
    #[test]
    fn test_func_code_new_returns_ok_at_u16_max() {
        let fc = FuncCode::new(u16::MAX).unwrap();
        assert_eq!(fc.get(), u16::MAX);
    }

    // --- Happy path: mid-range ---------------------------------------------
    #[test]
    fn test_func_code_new_returns_ok_for_mid_range() {
        let fc = FuncCode::new(0x1234).unwrap();
        assert_eq!(fc.get(), 0x1234);
    }

    // --- Error variant identity --------------------------------------------
    // Locks the variant name so a Phase 05 rename (to
    // `CcsdsError::FuncCodeReserved`) shows up as a single-site diff.
    #[test]
    fn test_func_code_error_variant_equals_reserved() {
        let err = FuncCode::new(0).unwrap_err();
        assert_eq!(err, FuncCodeError::FuncCodeReserved);
    }

    // === InstanceId ========================================================

    // --- Error path: reserved 0 rejected -----------------------------------
    // Given: instance id 0 (broadcast sentinel per arch §2.2).
    // When:  constructed through the sealed boundary.
    // Then:  rejected with `InstanceIdError::InstanceIdReserved`.
    #[test]
    fn test_instance_id_new_returns_err_when_zero() {
        let result = InstanceId::new(0);
        assert_eq!(result, Err(InstanceIdError::InstanceIdReserved));
    }

    // --- Happy path: lower boundary (MIN = 1) ------------------------------
    #[test]
    fn test_instance_id_new_returns_ok_at_min_boundary() {
        let iid = InstanceId::new(1).unwrap();
        assert_eq!(iid.get(), 1);
        assert_eq!(InstanceId::MIN, 1);
    }

    // --- Happy path: upper boundary (MAX = 255) ----------------------------
    #[test]
    fn test_instance_id_new_returns_ok_at_max_boundary() {
        let iid = InstanceId::new(255).unwrap();
        assert_eq!(iid.get(), 255);
        assert_eq!(InstanceId::MAX, u8::MAX);
    }

    // --- Happy path: mid-range ---------------------------------------------
    #[test]
    fn test_instance_id_new_returns_ok_for_mid_range() {
        let iid = InstanceId::new(128).unwrap();
        assert_eq!(iid.get(), 128);
    }

    // --- Error variant identity --------------------------------------------
    // Locks the variant name so a Phase 05 rename (to
    // `CcsdsError::InstanceIdReserved`) shows up as a single-site diff.
    #[test]
    fn test_instance_id_error_variant_equals_reserved() {
        let err = InstanceId::new(0).unwrap_err();
        assert_eq!(err, InstanceIdError::InstanceIdReserved);
    }
}
