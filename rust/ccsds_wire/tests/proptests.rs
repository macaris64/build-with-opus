//! Property-based tests for `ccsds_wire`. Populated by Phases 03–09 per
//! `IMPLEMENTATION_GUIDE.md`. Phase 02 shipped this file empty so the
//! integration-test harness wiring was in place before any test body landed.
//! Phase 06 lands the first body: a single `prop_cuc_roundtrip` scaffold
//! covering the CUC codec's encode/decode invariant. Phase 10 milestone
//! expands the battery (e.g. `prop_rejects_invalid_pfield`,
//! `prop_rejects_short_buffer`) once the primary-header codec is online.

// Test-only conveniences: workspace denies these for production code.
#![allow(clippy::unwrap_used, clippy::panic, clippy::expect_used)]

use ccsds_wire::{Cuc, P_FIELD};
use proptest::prelude::*;

proptest! {
    /// For any `(coarse, fine)` pair in the full `u32 × u16` domain:
    /// `Cuc::decode_be(encode_be(x)) == x` and the encoded P-Field byte is
    /// `0x2F`. Covers Q-C8 (BE wire) and Q-C6 (P-Field pinning) together.
    #[test]
    fn prop_cuc_roundtrip(coarse in any::<u32>(), fine in any::<u16>()) {
        let original = Cuc { coarse, fine };
        let mut buf = [0u8; 7];
        original.encode_be(&mut buf);
        prop_assert_eq!(buf[0], P_FIELD);
        let decoded = Cuc::decode_be(&buf).unwrap();
        prop_assert_eq!(decoded, original);
    }
}
