//! Unified `CcsdsError` enum for the `ccsds_wire` crate.
//!
//! Definition site: [`docs/architecture/06-ground-segment-rust.md §2.8`]
//! (../../../docs/architecture/06-ground-segment-rust.md). The enum is
//! intentionally exhaustive — adding a variant is a semver-breaking API
//! change that requires a doc update. The Phase 05 Definition-of-Done
//! enforces this via a grep guard on this file that must return zero matches.
//!
//! Phase 05 folded the module-local placeholder errors from Phases 03–04
//! into this enum. The 9th variant — `SequenceCountOutOfRange(u16)` —
//! resolves [`Q-C10`](../../../docs/standards/decisions-log.md) as
//! option (a): the 14-bit ceiling stays at the `SequenceCount::new`
//! boundary and surfaces through `CcsdsError`.

use thiserror::Error;

/// Every error surfaced by the `ccsds_wire` pack/unpack codecs.
///
/// Variants carry the offending raw value when that is useful for logging
/// or operator diagnostics (e.g. `ApidOutOfRange(u16)` preserves the raw
/// 16-bit input so the caller can report what actually arrived on the
/// wire). Variants without an associated value represent invariants whose
/// offending value is unambiguous from the variant name alone.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum CcsdsError {
    /// Buffer passed to a decoder was shorter than the declared or required
    /// length. `need` is the count of bytes the decoder expected; `got` is
    /// the length of the buffer actually provided.
    #[error("buffer too short: need {need}, got {got}")]
    BufferTooShort {
        /// Bytes the decoder required to make progress.
        need: usize,
        /// Bytes actually present in the input buffer.
        got: usize,
    },

    /// CCSDS primary-header version field (3-bit) was not `0b000`.
    /// SAKURA-II only supports Space Packet Protocol v1; any other value
    /// here indicates a malformed or non-CCSDS frame.
    #[error("invalid CCSDS version: {0}")]
    InvalidVersion(u8),

    /// CUC P-Field byte was not `0x2F`. SAKURA-II pins the P-Field to
    /// `0x2F` fleet-wide (TAI epoch, 4 B coarse + 2 B fine) per
    /// [`08 §2`](../../../docs/architecture/08-timing-and-clocks.md).
    #[error("invalid CUC P-Field: 0x{0:02X} (expected 0x2F)")]
    InvalidPField(u8),

    /// APID raw value exceeded the 11-bit ceiling (`> 0x7FF`). The raw
    /// input is preserved so callers can log the offending value verbatim.
    #[error("APID out of range: 0x{0:03X}")]
    ApidOutOfRange(u16),

    /// Sequence-count raw value exceeded the 14-bit ceiling (`> 0x3FFF`).
    /// Resolves `Q-C10` as option (a): the 14-bit guard stays at
    /// `SequenceCount::new` and surfaces through this variant.
    #[error("sequence count out of range: 0x{0:04X}")]
    SequenceCountOutOfRange(u16),

    /// Secondary-header instance id was `0`, which is reserved by arch
    /// §2.2 as the broadcast sentinel and must not appear in a
    /// unit-addressed frame.
    #[error("instance id 0 is reserved")]
    InstanceIdReserved,

    /// Declared `data_length` field does not match the actual buffer
    /// length. `declared` is `data_length + 7` per CCSDS 133.0-B-2;
    /// `actual` is the length of the buffer passed to the decoder.
    #[error("declared data_length {declared} does not match buffer length {actual}")]
    LengthMismatch {
        /// `data_length` field decoded from the primary header, plus 7.
        declared: usize,
        /// Actual length of the buffer passed to the decoder.
        actual: usize,
    },

    /// Primary-header sequence-flags bits were not `0b11`. SAKURA-II only
    /// emits standalone packets — no segmentation — so any other value
    /// indicates a non-conformant frame.
    #[error("sequence flags not standalone (0b11): 0b{0:02b}")]
    SequenceFlagsNotStandalone(u8),

    /// Secondary-header function code was `0x0000`, which is reserved by
    /// arch §2.2 and must not be used as a live function code.
    #[error("function code 0x0000 is reserved")]
    FuncCodeReserved,
}

#[cfg(test)]
mod tests {
    use super::*;

    // --- Display for the new 9th variant (Q-C10 resolution) ----------------
    // Given: the Q-C10 9th variant constructed with a raw rejected value.
    // When:  formatted via the `thiserror`-derived Display impl.
    // Then:  the format string matches §2.8 verbatim.
    //
    // This is the Phase 05 RED test: it does not compile against the
    // Phase 04 tree (no `CcsdsError` enum and no `SequenceCountOutOfRange`
    // variant), proving the fold is required.
    #[test]
    fn test_display_sequence_count_out_of_range_matches_spec() {
        let err = CcsdsError::SequenceCountOutOfRange(0x4000);
        assert_eq!(format!("{err}"), "sequence count out of range: 0x4000");
    }

    // --- Spot-check one struct-form variant's Display ---------------------
    #[test]
    fn test_display_buffer_too_short_includes_need_and_got() {
        let err = CcsdsError::BufferTooShort { need: 10, got: 3 };
        assert_eq!(format!("{err}"), "buffer too short: need 10, got 3");
    }

    // --- Eq holds across all variant shapes -------------------------------
    #[test]
    fn test_eq_holds_for_tuple_and_struct_variants() {
        assert_eq!(
            CcsdsError::ApidOutOfRange(0x800),
            CcsdsError::ApidOutOfRange(0x800)
        );
        assert_eq!(
            CcsdsError::BufferTooShort { need: 7, got: 0 },
            CcsdsError::BufferTooShort { need: 7, got: 0 }
        );
        assert_ne!(
            CcsdsError::ApidOutOfRange(0x800),
            CcsdsError::ApidOutOfRange(0x801)
        );
    }
}
