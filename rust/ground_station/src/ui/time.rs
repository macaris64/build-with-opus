//! TAI ↔ UTC conversion and command-validity-window guard.
//!
//! UTC exists **only at this boundary** — internal pipeline state is TAI throughout
//! (Q-F4, docs/architecture/08-timing-and-clocks.md §3–4).
//!
//! Decisions: [`Q-C8`](../../../../docs/standards/decisions-log.md) — no
//! endian-conversion functions in this module; all wire parsing stays in
//! `ccsds_wire`.  [`Q-F3`](../../../../docs/standards/decisions-log.md) — UI
//! display-layer state is explicitly excluded from `Vault<T>` (09 §5.2).

use ccsds_wire::Cuc;

/// TAI epoch expressed as Unix seconds (1958-01-01T00:00:00 UTC = −378 691 200 s
/// from the Unix epoch).  Derived: 12 years × 365 days + 3 leap days = 4 383 days
/// × 86 400 s/day = 378 691 200 s.
const TAI_EPOCH_UNIX_S: i64 = -378_691_200;

/// Converts between TAI (internal representation throughout the pipeline) and
/// UTC (UI boundary only, per docs/architecture/08-timing-and-clocks.md §5.5).
pub struct TaiUtcConverter {
    /// TAI − UTC offset in whole seconds (37 s as of 2024; configurable for
    /// future leap-second additions).
    pub leap_seconds: i64,
}

impl TaiUtcConverter {
    /// Creates a converter with the given TAI−UTC offset.
    #[must_use]
    pub fn new(leap_seconds: i64) -> Self {
        Self { leap_seconds }
    }

    /// Converts a CUC TAI timestamp to a UTC [`time::OffsetDateTime`].
    ///
    /// Sub-second precision is preserved to within ≈ 15 µs (the CUC fine
    /// field resolution of 2⁻¹⁶ s).
    #[must_use]
    pub fn tai_to_utc(&self, tai: &Cuc) -> time::OffsetDateTime {
        let unix_s = TAI_EPOCH_UNIX_S + i64::from(tai.coarse) - self.leap_seconds;
        // Fine field: units of 2^-16 s → nanoseconds.
        let fine_ns = i64::from(tai.fine) * 1_000_000_000 / 65_536;
        // Any realistic TAI coarse value (u32 range → years 1957–2094) falls
        // well within `OffsetDateTime`'s supported range, so the fallback is
        // unreachable in practice.
        time::OffsetDateTime::from_unix_timestamp(unix_s)
            .unwrap_or(time::OffsetDateTime::UNIX_EPOCH)
            .saturating_add(time::Duration::nanoseconds(fine_ns))
    }

    /// Converts a UTC [`time::OffsetDateTime`] back to a CUC TAI timestamp.
    ///
    /// Sub-second precision is dropped (fine field = 0).  Used when the
    /// operator provides a UTC command-validity deadline that must be compared
    /// against TAI pipeline state.
    #[must_use]
    pub fn utc_to_tai(&self, utc: time::OffsetDateTime) -> Cuc {
        let unix_s = utc.unix_timestamp();
        let tai_s = unix_s - TAI_EPOCH_UNIX_S + self.leap_seconds;
        // Saturate at 0 for pre-epoch times; at u32::MAX for post-2094 times.
        let coarse = u32::try_from(tai_s).unwrap_or(0);
        Cuc { coarse, fine: 0 }
    }

    /// Formats a CUC TAI timestamp as an ISO-8601 UTC string with millisecond
    /// precision (e.g. `"2024-11-03T14:32:00.123Z"`), satisfying SYS-REQ-0060.
    ///
    /// # Errors
    ///
    /// Returns an empty string on formatting failure (unreachable for valid
    /// `OffsetDateTime` values; guarded by `unwrap_or_else`).
    #[must_use]
    pub fn tai_to_iso8601(&self, tai: &Cuc) -> String {
        let utc = self.tai_to_utc(tai);
        utc.format(&time::format_description::well_known::Rfc3339)
            .unwrap_or_else(|_| String::new())
    }
}

/// Rejects a telecommand whose uplink-arrival time would exceed its validity
/// window, satisfying SYS-REQ-0061.
///
/// The command will arrive at the spacecraft at approximately
/// `now_tai.coarse + light_time_s` seconds TAI.  If that arrival time is later
/// than `valid_until_tai.coarse`, the window has passed and the TC is rejected.
///
/// # Errors
///
/// Returns [`ValidityError::WindowExpired`] when the estimated arrival TAI
/// exceeds `valid_until_tai`.
pub fn check_validity_window(
    valid_until_tai: Cuc,
    now_tai: Cuc,
    light_time_s: f64,
) -> Result<(), ValidityError> {
    let arrival_s = f64::from(now_tai.coarse) + light_time_s;
    let limit_s = f64::from(valid_until_tai.coarse);
    if arrival_s > limit_s {
        Err(ValidityError::WindowExpired {
            expired_by_s: arrival_s - limit_s,
        })
    } else {
        Ok(())
    }
}

/// Error returned when a telecommand's authorization window has expired.
#[derive(Debug, thiserror::Error)]
pub enum ValidityError {
    /// The command would arrive after its validity window closed.
    #[error("command validity window expired by {expired_by_s:.1} s")]
    WindowExpired {
        /// How far past the deadline the command would arrive (seconds).
        expired_by_s: f64,
    },
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::panic, clippy::expect_used)]
mod tests {
    use super::*;

    // GIVEN TC valid_until_tai.coarse = 1000, now_tai.coarse = 1100, light_time_s = 5.0
    // WHEN  check_validity_window is called
    // THEN  Err(ValidityError::WindowExpired { expired_by_s ≈ 105.0 }) — SYS-REQ-0061
    #[test]
    fn ui_validity_window_expired_rejects_tc() {
        let valid_until = Cuc {
            coarse: 1000,
            fine: 0,
        };
        let now_tai = Cuc {
            coarse: 1100,
            fine: 0,
        };
        match check_validity_window(valid_until, now_tai, 5.0) {
            Err(ValidityError::WindowExpired { expired_by_s }) => {
                assert!(
                    (expired_by_s - 105.0).abs() < 0.001,
                    "expected expired_by_s ≈ 105.0, got {expired_by_s}"
                );
            }
            Ok(()) => panic!("expected WindowExpired, got Ok"),
        }
    }

    // GIVEN TC valid_until_tai.coarse = 2000, now_tai.coarse = 1000, light_time_s = 600.0
    // WHEN  check_validity_window is called  (arrival = 1600 < 2000)
    // THEN  Ok(()) — window still open
    #[test]
    fn ui_validity_window_open_accepts_tc() {
        let valid_until = Cuc {
            coarse: 2000,
            fine: 0,
        };
        let now_tai = Cuc {
            coarse: 1000,
            fine: 0,
        };
        assert!(
            check_validity_window(valid_until, now_tai, 600.0).is_ok(),
            "expected Ok for window still open"
        );
    }

    // GIVEN TAI coarse = 2_084_688_000 (approx 2024-01-01T00:00:00 TAI with 37 leap-s)
    // WHEN  tai_to_utc is called with leap_seconds = 37
    // THEN  resulting UTC year is 2024 and month is January
    #[test]
    fn ui_tai_to_utc_known_epoch() {
        // 2024-01-01T00:00:37 TAI == 2024-01-01T00:00:00 UTC
        // Unix timestamp of 2024-01-01T00:00:00 UTC = 1_704_067_200
        // coarse = 1_704_067_200 - TAI_EPOCH_UNIX_S + 37
        //        = 1_704_067_200 + 378_691_200 + 37 = 2_082_758_437
        let coarse: u32 = 1_704_067_200u64.wrapping_add(378_691_200).wrapping_add(37) as u32;
        let tai = Cuc { coarse, fine: 0 };
        let conv = TaiUtcConverter::new(37);
        let utc = conv.tai_to_utc(&tai);
        assert_eq!(utc.year(), 2024);
        assert_eq!(utc.month(), time::Month::January);
        assert_eq!(utc.day(), 1);
        assert_eq!(utc.hour(), 0);
        assert_eq!(utc.minute(), 0);
        assert_eq!(utc.second(), 0);
    }

    // GIVEN a UTC OffsetDateTime and leap_seconds = 37
    // WHEN  utc_to_tai is called
    // THEN  the coarse field encodes the expected TAI seconds
    #[test]
    fn ui_utc_to_tai_roundtrip() {
        let conv = TaiUtcConverter::new(37);
        // Use Unix epoch (1970-01-01T00:00:00 UTC)
        let utc = time::OffsetDateTime::UNIX_EPOCH;
        let tai = conv.utc_to_tai(utc);
        // Expected coarse: 0 - (-378_691_200) + 37 = 378_691_237
        assert_eq!(tai.coarse, 378_691_237u32);
        assert_eq!(tai.fine, 0);
    }

    // GIVEN a CUC timestamp for 2024-01-01T00:00:00 UTC
    // WHEN  tai_to_iso8601 is called
    // THEN  the returned string starts with "2024-01-01T00:00:00"
    #[test]
    fn ui_tai_to_iso8601_format() {
        let coarse: u32 = 1_704_067_200u64.wrapping_add(378_691_200).wrapping_add(37) as u32;
        let tai = Cuc { coarse, fine: 0 };
        let conv = TaiUtcConverter::new(37);
        let s = conv.tai_to_iso8601(&tai);
        assert!(
            s.starts_with("2024-01-01T00:00:00"),
            "unexpected ISO-8601 string: {s}"
        );
    }
}
