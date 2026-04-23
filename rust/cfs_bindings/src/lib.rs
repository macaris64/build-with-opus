// Allow bindgen-generated names that don't follow Rust conventions.
// `clippy::all` is now denied at the workspace level (see /Cargo.toml
// [workspace.lints.clippy]), so the old crate-level `#![deny(clippy::all)]`
// is dropped as redundant.
#![allow(non_upper_case_globals, non_camel_case_types, non_snake_case)]
// bindgen may emit `unsafe extern "C"` blocks and raw pointer types from
// a richer mission_config.h; override the workspace-wide `unsafe_code =
// deny` for this crate only. Authorised by
// docs/architecture/06-ground-segment-rust.md §1.2.
#![allow(unsafe_code)]

//! Safe Rust wrappers around compile-time constants from `_defs/mission_config.h`.
//!
//! The raw bindgen output is included below; safe wrapper types are defined
//! in this module to give callers a Rust-idiomatic interface.

pub mod message;
pub use message::{Message, MsgId, SbBuffer};

include!(concat!(env!("OUT_DIR"), "/mission_config_bindings.rs"));

/// Mission configuration constants re-exported with idiomatic Rust names.
///
/// The `// SAFETY:` prefix is reserved for `unsafe` blocks per
/// `.claude/rules/general.md`; these are plain `pub const` re-exports of
/// integer values that bindgen generated from `#define` lines in
/// `_defs/mission_config.h`, so the rationale below is documentation, not
/// a safety invariant.
pub mod mission {
    // Bindgen `#define` constant — plain integer, no pointer indirection.
    pub const MAX_PIPES: u32 = super::SAKURA_II_MAX_PIPES;

    // Bindgen `#define` constant — same rationale as MAX_PIPES.
    pub const TASK_STACK_BYTES: u32 = super::SAKURA_II_TASK_STACK;

    // Bindgen `#define` constant — same rationale as MAX_PIPES.
    pub const SPACECRAFT_ID: u32 = super::SPACECRAFT_ID;

    // Bindgen `#define` constant — fleet-allocation anchor; instance N
    // derives SPACECRAFT_ID = SCID_BASE + N (see
    // docs/interfaces/apid-registry.md §Identifiers and Ranges).
    pub const SCID_BASE: u32 = super::SAKURA_II_SCID_BASE;

    // Canonical mission name. Bindgen 0.69 does not emit string `#define`s
    // as Rust constants, so the value is mirrored here and cross-checked
    // against `_defs/targets.cmake` by `tests::test_mission_name_is_sakura_ii`
    // to prevent drift.
    pub const MISSION_NAME: &str = "SAKURA_II";
}

#[cfg(test)]
mod tests {
    use super::mission;

    /// Embedded verbatim copy of `_defs/targets.cmake` at compile time.
    /// Used by `test_mission_name_is_sakura_ii` as a drift detector between
    /// the `CMake` mission name and the Rust re-export.
    const TARGETS_CMAKE: &str = include_str!("../../../_defs/targets.cmake");

    #[test]
    fn test_mission_name_is_sakura_ii() {
        /* Given _defs/targets.cmake sets `MISSION_NAME "SAKURA_II"` and
         *   cfs_bindings re-exports it as `mission::MISSION_NAME`.
         * When  a consumer reads `mission::MISSION_NAME`.
         * Then  the value is `"SAKURA_II"` and the embedded
         *   `_defs/targets.cmake` agrees (drift detector). */
        assert_eq!(mission::MISSION_NAME, "SAKURA_II");
        assert!(
            TARGETS_CMAKE.contains(r#"set(MISSION_NAME "SAKURA_II")"#),
            "_defs/targets.cmake must carry `set(MISSION_NAME \"SAKURA_II\")`"
        );
    }

    #[test]
    fn test_max_pipes_is_nonzero() {
        const {
            assert!(
                mission::MAX_PIPES > 0,
                "mission must allow at least one pipe"
            );
        };
    }

    #[test]
    fn test_task_stack_is_power_of_two() {
        let stack = mission::TASK_STACK_BYTES;
        assert!(
            stack.is_power_of_two(),
            "stack size should be a power of two for alignment"
        );
    }

    #[test]
    fn test_spacecraft_id_is_in_valid_apid_range() {
        /* CCSDS APIDs are 11-bit values (0–2047) */
        const {
            assert!(
                mission::SPACECRAFT_ID <= 2047,
                "spacecraft ID must fit in 11-bit APID field"
            );
        };
    }

    #[test]
    fn test_scid_base_anchors_spacecraft_id() {
        /* Single-instance repo: the deployed SCID is exactly the fleet
         * anchor. Multi-instance deployments will relax this to
         * `SPACECRAFT_ID >= SCID_BASE` and bound by a per-mission ceiling. */
        const {
            assert!(
                mission::SCID_BASE == mission::SPACECRAFT_ID,
                "single-instance SAKURA-II must deploy with SPACECRAFT_ID == SCID_BASE"
            );
        };
    }
}
