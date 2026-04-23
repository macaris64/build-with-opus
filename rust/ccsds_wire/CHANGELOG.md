# Changelog — ccsds_wire

## [1.0.0] — 2026-04-23

### Added
- Property-based test battery (7 named tests) in `tests/proptests.rs`:
  `prop_cuc_roundtrip`, `prop_primary_header_roundtrip`,
  `prop_secondary_header_roundtrip`, `prop_space_packet_roundtrip`,
  `prop_rejects_short_buffer`, `prop_rejects_invalid_version`,
  `prop_rejects_invalid_pfield`.
- Known-answer test suite (5 KATs) in `tests/known_answers.rs`:
  `kat_pkt_tm_0100_0002`, `kat_pkt_tm_0101_0002`, `kat_pkt_tm_0110_0002`,
  `kat_pkt_tm_0400_0004`, `kat_pkt_tc_0184_8100`.
- CI grep-lint script at `scripts/grep-lints.sh` (Q-C8 LE guard,
  no-unwrap in production code, no-unsafe in ccsds_wire).

### Fixed
- Pre-existing `uninlined_format_args` in `src/builder.rs` test module.
- Pre-existing `indexing_slicing` warnings in `src/packet.rs` test module.
- Pre-existing `assertions_on_constants` in `rust/cfs_bindings/src/lib.rs`
  (replaced with `const { assert!(...) }` idiomatic form).

### Summary

MILESTONE 1/5: `ccsds_wire v1.0` — complete test coverage for the
CCSDS 133.0-B-2 BE codec stack (`Apid`, `SequenceCount`, `Cuc`,
`PrimaryHeader`, `SecondaryHeader`, `SpacePacket`, `PacketBuilder`).
