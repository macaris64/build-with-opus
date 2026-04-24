//! TAI ↔ Unix epoch adapter for the `cfdp-core` boundary (§4.5).
//!
//! CCSDS timestamps in this codebase use TAI epoch 1958-01-01 00:00:00 (per
//! [`ccsds_wire::Cuc`]). The CFDP layer operates on durations, not absolute
//! timestamps, so only the coarse-seconds field is used for timeout arithmetic.
//!
//! This is the **only** file in the ground segment that performs TAI ↔ Unix
//! epoch translation (Q-C8 equivalent for time: one conversion site, nowhere else).

use ccsds_wire::Cuc;

/// Seconds between TAI epoch (1958-01-01) and Unix epoch (1970-01-01).
///
/// Leap years in \[1958, 1970): 1960, 1964, 1968 → 3 leap years.
/// Regular years: 9 × 365 = 3 285 days. Leap years: 3 × 366 = 1 098 days.
/// Total: 4 383 days × 86 400 s/day = 378 691 200 s.
pub const TAI_UNIX_OFFSET_SECS: u64 = 378_691_200;

/// Convert a CUC coarse-seconds field (TAI epoch 1958) to Unix seconds.
///
/// Returns `0` if the CUC timestamp predates the Unix epoch (which cannot
/// happen in normal operations — guarded with `saturating_sub`).
#[must_use]
pub fn tai_to_unix_secs(cuc: Cuc) -> u64 {
    u64::from(cuc.coarse).saturating_sub(TAI_UNIX_OFFSET_SECS)
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::expect_used, clippy::panic)]
mod tests {
    use super::*;
    use ccsds_wire::Cuc;

    // Given  a CUC with coarse = TAI_UNIX_OFFSET_SECS (exactly the Unix epoch)
    // When   tai_to_unix_secs is called
    // Then   the result is 0
    #[test]
    fn unix_epoch_maps_to_zero() {
        // TAI_UNIX_OFFSET_SECS fits in u32 (378_691_200 < 2^32).
        let coarse = u32::try_from(TAI_UNIX_OFFSET_SECS).unwrap();
        let cuc = Cuc { coarse, fine: 0 };
        assert_eq!(tai_to_unix_secs(cuc), 0);
    }

    // Given  a CUC representing 2000-01-01 00:00:00 TAI (coarse only)
    // When   tai_to_unix_secs is called
    // Then   the result matches the known coarse Unix timestamp for that epoch
    //
    // Unix 2000-01-01 00:00:00 UTC = 946_684_800 s.
    // TAI coarse = 946_684_800 + 378_691_200 = 1_325_376_000 (fits in u32).
    #[test]
    fn known_date_roundtrip() {
        let unix_2000: u64 = 946_684_800;
        let tai_coarse = unix_2000 + TAI_UNIX_OFFSET_SECS;
        let coarse = u32::try_from(tai_coarse).unwrap();
        let cuc = Cuc { coarse, fine: 0 };
        assert_eq!(tai_to_unix_secs(cuc), unix_2000);
    }

    // Given  a CUC with coarse = 0 (before Unix epoch)
    // When   tai_to_unix_secs is called
    // Then   saturating_sub returns 0 (no underflow)
    #[test]
    fn pre_unix_epoch_saturates_to_zero() {
        let cuc = Cuc { coarse: 0, fine: 0 };
        assert_eq!(tai_to_unix_secs(cuc), 0);
    }

    // Given  a CUC with coarse = u32::MAX
    // When   tai_to_unix_secs is called
    // Then   no panic occurs (u64 widening prevents overflow)
    #[test]
    fn max_coarse_no_overflow() {
        let cuc = Cuc {
            coarse: u32::MAX,
            fine: u16::MAX,
        };
        let result = tai_to_unix_secs(cuc);
        // u32::MAX as u64 = 4_294_967_295; minus 378_691_200 = 3_916_276_095
        assert_eq!(result, u64::from(u32::MAX) - TAI_UNIX_OFFSET_SECS);
    }
}
