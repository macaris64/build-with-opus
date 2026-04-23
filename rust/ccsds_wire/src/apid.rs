//! `Apid` — 11-bit CCSDS Application Process Identifier (`0x000..=0x7FF`).
//!
//! Sealed newtype, Phase 03 locus. Pattern replicated by Phase 04 for the
//! sized newtypes (`SequenceCount`, `PacketDataLength`, `FuncCode`,
//! `InstanceId`). Definition sites:
//! - `docs/architecture/06-ground-segment-rust.md §2.2` — invariants
//! - `docs/interfaces/apid-registry.md` — `0x7FF` reserved as Idle / fill
//! - `SYS-REQ-0026` — class-based allocation; instance multiplicity via
//!   the secondary-header `instance_id`, never by burning extra APIDs

/// APID construction error.
///
/// Temporary module-local enum per the Phase 03 Phase Card: carries only
/// `#[derive(Debug)]` (plus `PartialEq`/`Eq` so tests can `assert_eq!`).
/// Phase 05 folds this variant into the unified `crate::CcsdsError` enum
/// (arch §2.8), at which point `Apid::new` returns `Result<Self, CcsdsError>`
/// and this type is deleted.
#[derive(Debug, PartialEq, Eq)]
pub enum ApidError {
    /// Raw value exceeded the 11-bit APID ceiling (`> 0x7FF`). The raw input
    /// is preserved so callers can log or surface the offending value.
    ApidOutOfRange(u16),
}

/// 11-bit CCSDS Application Process Identifier.
///
/// Sealed newtype. The inner `u16` is private, so the only path to a valid
/// `Apid` is [`Apid::new`], which enforces the `0x000..=0x7FF` invariant at
/// the type boundary. Once you hold an `Apid`, the 11-bit range is a
/// compile-time guarantee — no downstream code needs to re-validate.
///
/// The only path back to a raw `u16` is [`Apid::get`]; keeping that access
/// path narrow is what makes every endianness-conversion site grep-visible
/// per Q-C8 (`docs/standards/decisions-log.md`).
///
/// `Copy + Eq + Hash` — `Apid` is used as a routing-table key (APID-indexed
/// packet dispatch) in downstream phases, so the derives land now.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Apid(u16);

impl Apid {
    /// Maximum valid APID — the 11-bit ceiling (`0x7FF`).
    pub const MAX: u16 = 0x7FF;

    /// CCSDS idle / fill sentinel.
    ///
    /// Reserved by `apid-registry.md` (§ "APID Allocation", row `0x7FF`) and
    /// by arch §2.2. Equal to `Apid::new(0x7FF).unwrap()` — exposed as an
    /// associated `const` so it can appear in other `const` contexts without
    /// a `Result` unwrap.
    pub const IDLE: Apid = Apid(Self::MAX);

    /// Construct an `Apid` from a raw `u16`.
    ///
    /// # Errors
    ///
    /// Returns [`ApidError::ApidOutOfRange`] if `v > 0x7FF`. The error
    /// carries `v` unchanged so the caller can log it verbatim.
    pub const fn new(v: u16) -> Result<Self, ApidError> {
        if v > Self::MAX {
            Err(ApidError::ApidOutOfRange(v))
        } else {
            Ok(Apid(v))
        }
    }

    /// Raw 11-bit value.
    ///
    /// By-value (not `&self`) because `Apid` is `Copy`. Call sites are
    /// auditable via `rg '\.get\(\)'` — part of the Q-C8 grep-visible
    /// endianness-conversion discipline.
    #[must_use]
    pub const fn get(self) -> u16 {
        self.0
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::panic, clippy::expect_used)] // test-only conveniences: workspace denies these for production code
mod tests {
    use super::*;

    // --- RED test (Phase 03 Phase Card DoD, line 1) ------------------------
    // Given: an APID value one past the 11-bit ceiling.
    // When:  constructed through the sealed boundary.
    // Then:  construction fails with `ApidError::ApidOutOfRange(v)`
    //        carrying the offending raw value (per arch §2.8 ultimate
    //        target `CcsdsError::ApidOutOfRange(u16)`; landed locally
    //        as `ApidError` this phase, folded in Phase 05).
    #[test]
    fn test_new_returns_err_when_apid_exceeds_11_bit_range() {
        let raw: u16 = 0x800;
        let result = Apid::new(raw);
        assert_eq!(result, Err(ApidError::ApidOutOfRange(0x800)));
    }

    // --- DoD line 2: `Apid::new(0x7FF) == Apid::IDLE` ------------------------
    #[test]
    fn test_idle_constant_equals_new_0x7ff() {
        let via_ctor = Apid::new(0x7FF).unwrap();
        assert_eq!(via_ctor, Apid::IDLE);
    }

    // --- Happy path: lower boundary ------------------------------------------
    // Given: APID value 0x000 (minimum valid).
    // When:  constructed.
    // Then:  Ok, and .get() roundtrips the raw value.
    #[test]
    fn test_new_returns_ok_when_apid_is_zero() {
        let a = Apid::new(0x000).unwrap();
        assert_eq!(a.get(), 0x000);
    }

    // --- Happy path: upper boundary ------------------------------------------
    #[test]
    fn test_new_returns_ok_when_apid_is_max_boundary() {
        let a = Apid::new(0x7FF).unwrap();
        assert_eq!(a.get(), 0x7FF);
    }

    // --- Happy path: mid-range (catches any off-by-one in the comparison) ---
    #[test]
    fn test_new_returns_ok_for_mid_range_apid() {
        let a = Apid::new(0x123).unwrap();
        assert_eq!(a.get(), 0x123);
    }

    // --- Happy path: one below ceiling ---------------------------------------
    // Explicitly distinct from the 0x7FF boundary — catches a `>=` vs `>`
    // comparison bug in `new`.
    #[test]
    fn test_new_returns_ok_for_one_below_ceiling() {
        let a = Apid::new(0x7FE).unwrap();
        assert_eq!(a.get(), 0x7FE);
    }

    // --- Error path: u16::MAX (far out of range) -----------------------------
    // Confirms the error path also holds for inputs well past 0x800.
    #[test]
    fn test_new_returns_err_when_apid_is_u16_max() {
        let result = Apid::new(u16::MAX);
        assert_eq!(result, Err(ApidError::ApidOutOfRange(u16::MAX)));
    }

    // --- Accessor: IDLE constant reports raw 0x7FF --------------------------
    #[test]
    fn test_idle_get_equals_0x7ff() {
        assert_eq!(Apid::IDLE.get(), 0x7FF);
    }

    // --- MAX constant matches the documented invariant ----------------------
    #[test]
    fn test_max_constant_is_0x7ff() {
        assert_eq!(Apid::MAX, 0x7FF);
    }

    // --- Derive smoke test: Copy + Eq + Hash usable as map key -------------
    // This is what downstream routing tables depend on (arch §2.2: APID is a
    // routing-table key). If `Hash`/`Eq` were ever removed the compile would
    // break here with a clearer signal than a downstream crate failure.
    #[test]
    fn test_apid_usable_as_map_key() {
        use std::collections::HashMap;
        let mut m: HashMap<Apid, &str> = HashMap::new();
        m.insert(Apid::new(0x100).unwrap(), "orbiter tm");
        m.insert(Apid::IDLE, "idle");
        assert_eq!(m.get(&Apid::new(0x100).unwrap()), Some(&"orbiter tm"));
        assert_eq!(m.get(&Apid::IDLE), Some(&"idle"));
    }
}
