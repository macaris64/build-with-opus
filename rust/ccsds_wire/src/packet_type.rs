//! `PacketType` — CCSDS primary-header Type bit (TM = 0, TC = 1).
//!
//! Definition site: [`docs/architecture/06-ground-segment-rust.md §2.4`]
//! (../../../docs/architecture/06-ground-segment-rust.md). Used by
//! `PrimaryHeader` (Phase 07) to report the Type bit as a typed enum
//! rather than a raw `u8`, consistent with the Q-C8 grep-visible
//! accessor discipline on the sized newtypes.

/// CCSDS primary-header Type discriminant.
///
/// The Type bit is position `[4]` of byte 0 in the 6-byte primary header.
/// `Tm` (telemetry) is the spacecraft-to-ground direction; `Tc` (telecommand)
/// is the ground-to-spacecraft direction. The discriminant values are
/// normative — they match the on-wire bit value so `as_bit` is a cheap
/// reinterpretation rather than a branch.
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum PacketType {
    /// Telemetry (spacecraft → ground). Type bit = `0`.
    Tm = 0,
    /// Telecommand (ground → spacecraft). Type bit = `1`.
    Tc = 1,
}

impl PacketType {
    /// Build a `PacketType` from the on-wire Type bit.
    ///
    /// Only the low bit of `b` is inspected; any other bits are masked
    /// away. This keeps the signature infallible (no `Result`) while
    /// matching the workspace `deny(panic, unwrap_used, expect_used)`
    /// policy — callers pass the byte they extracted from the header
    /// without needing to pre-mask.
    #[must_use]
    pub const fn from_bit(b: u8) -> PacketType {
        if b & 1 == 0 {
            PacketType::Tm
        } else {
            PacketType::Tc
        }
    }

    /// Return the on-wire Type bit for this variant (`0` for `Tm`,
    /// `1` for `Tc`). The `#[repr(u8)]` discriminant makes this a
    /// no-op cast at the machine-code level.
    #[must_use]
    pub const fn as_bit(self) -> u8 {
        self as u8
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::panic, clippy::expect_used)] // test-only conveniences: workspace denies these for production code
mod tests {
    use super::*;

    // --- RED: from_bit + as_bit round-trip for the two spec values --------
    #[test]
    fn test_from_bit_zero_maps_to_tm() {
        assert_eq!(PacketType::from_bit(0), PacketType::Tm);
    }

    #[test]
    fn test_from_bit_one_maps_to_tc() {
        assert_eq!(PacketType::from_bit(1), PacketType::Tc);
    }

    #[test]
    fn test_as_bit_tm_is_zero() {
        assert_eq!(PacketType::Tm.as_bit(), 0);
    }

    #[test]
    fn test_as_bit_tc_is_one() {
        assert_eq!(PacketType::Tc.as_bit(), 1);
    }

    // --- Round-trip under mask-low-bit contract ----------------------------
    // `from_bit` inspects only the low bit, so `from_bit(x).as_bit() == x & 1`
    // must hold for *any* `u8` input. Exercise the boundary values plus
    // u8::MAX (all bits set) and a mid-range value to catch a regression
    // where an implementation might accidentally treat a non-0/1 input as
    // a third state.
    #[test]
    fn test_from_bit_masks_low_bit_for_any_u8() {
        for &b in &[0u8, 1, 2, 3, 0x7F, 0x80, 0xFE, u8::MAX] {
            assert_eq!(PacketType::from_bit(b).as_bit(), b & 1);
        }
    }
}
