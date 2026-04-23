//! MID constants mirrored from `_defs/mids.h` via bindgen.
//!
//! Source of truth: `docs/interfaces/apid-registry.md §cFE Message ID (MID) Scheme`.
//! Q-C8: all values are derived by logical bitwise OR — no byte-level endianness
//! conversion occurs here.

// bindgen emits SCREAMING_SNAKE_CASE for `#define` constants; allow the
// non-Rust-idiomatic names so consumers can match C macro names exactly.
#![allow(non_upper_case_globals)]

include!(concat!(env!("OUT_DIR"), "/mids_bindings.rs"));

#[cfg(test)]
mod tests {
    use super::*;

    const REGISTRY: &str = include_str!("../../../docs/interfaces/apid-registry.md");

    /// G: apid-registry.md allocates APID 0x100 to sample_app HK TM.
    /// W: SAMPLE_APP_HK_MID is derived via 0x0800 | 0x100.
    /// T: constant equals 0x0900 and registry still lists that APID entry.
    #[test]
    fn test_sample_app_hk_mid_matches_registry() {
        assert!(
            REGISTRY.contains("| `0x100` | `sample_app`"),
            "registry must allocate APID 0x100 to sample_app HK TM"
        );
        assert_eq!(SAMPLE_APP_HK_MID, 0x0900_u32);
    }

    /// G: apid-registry.md allocates APID 0x180 to sample_app TC.
    /// W: SAMPLE_APP_CMD_MID is derived via 0x1800 | 0x180.
    /// T: constant equals 0x1980 and registry still lists that APID entry.
    #[test]
    fn test_sample_app_cmd_mid_matches_registry() {
        assert!(
            REGISTRY.contains("| `0x180` | `sample_app`"),
            "registry must allocate APID 0x180 to sample_app TC"
        );
        assert_eq!(SAMPLE_APP_CMD_MID, 0x1980_u32);
    }

    /// G: orbiter_cdh is allocated APID 0x101 (TM) and 0x181 (TC) in the registry.
    /// W: MID constants are derived by formula.
    /// T: values equal 0x0901 and 0x1981 respectively.
    #[test]
    fn test_orbiter_cdh_mids_match_registry() {
        assert_eq!(ORBITER_CDH_HK_MID, 0x0901_u32);
        assert_eq!(ORBITER_CDH_CMD_MID, 0x1981_u32);
    }

    /// G: every orbiter TM MID has prefix 0x0800 and every TC MID has prefix 0x1800.
    /// W: we apply the MID formula check independently for each constant.
    /// T: high-12-bit mask equals 0x0800 (TM) or 0x1800 (TC) for all orbiter MIDs.
    #[test]
    fn test_all_orbiter_mids_follow_formula() {
        let tm_mids: &[u32] = &[
            SAMPLE_APP_HK_MID,
            ORBITER_CDH_HK_MID,
            ORBITER_ADCS_HK_MID,
            ORBITER_COMM_HK_MID,
            ORBITER_POWER_HK_MID,
            ORBITER_PAYLOAD_HK_MID,
        ];
        let tc_mids: &[u32] = &[
            SAMPLE_APP_CMD_MID,
            ORBITER_CDH_CMD_MID,
            ORBITER_ADCS_CMD_MID,
            ORBITER_COMM_CMD_MID,
            ORBITER_POWER_CMD_MID,
            ORBITER_PAYLOAD_CMD_MID,
        ];
        for mid in tm_mids {
            assert_eq!(
                mid & 0xF800,
                0x0800,
                "TM MID 0x{mid:04X} must have 0x0800 prefix"
            );
        }
        for mid in tc_mids {
            assert_eq!(
                mid & 0xF800,
                0x1800,
                "TC MID 0x{mid:04X} must have 0x1800 prefix"
            );
        }
    }

    /// G: every orbiter APID falls in the usable range 0x100–0x7FE (not reserved).
    /// W: we extract the 11-bit APID from each MID (value & 0x07FF).
    /// T: all APIDSs are >= 0x100 and <= 0x7FE.
    #[test]
    fn test_all_mids_use_non_reserved_apids() {
        let all_mids: &[u32] = &[
            SAMPLE_APP_HK_MID,
            SAMPLE_APP_CMD_MID,
            ORBITER_CDH_HK_MID,
            ORBITER_CDH_CMD_MID,
            ORBITER_ADCS_HK_MID,
            ORBITER_ADCS_CMD_MID,
            ORBITER_COMM_HK_MID,
            ORBITER_COMM_CMD_MID,
            ORBITER_POWER_HK_MID,
            ORBITER_POWER_CMD_MID,
            ORBITER_PAYLOAD_HK_MID,
            ORBITER_PAYLOAD_CMD_MID,
        ];
        for mid in all_mids {
            let apid = mid & 0x07FF;
            assert!(
                apid >= 0x100 && apid <= 0x7FE,
                "MID 0x{mid:04X} derives APID 0x{apid:03X} which is in the CCSDS-reserved range"
            );
        }
    }
}
