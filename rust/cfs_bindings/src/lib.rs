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
    pub const MAX_PIPES: u32 = super::SAMPLE_MISSION_MAX_PIPES;

    // Bindgen `#define` constant — same rationale as MAX_PIPES.
    pub const TASK_STACK_BYTES: u32 = super::SAMPLE_MISSION_TASK_STACK;

    // Bindgen `#define` constant — same rationale as MAX_PIPES.
    pub const SPACECRAFT_ID: u32 = super::SPACECRAFT_ID;
}

#[cfg(test)]
mod tests {
    use super::mission;

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
}
