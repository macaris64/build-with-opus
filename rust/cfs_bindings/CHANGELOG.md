# Changelog — cfs_bindings

All notable changes to this crate are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [1.0.0] — 2026-04-24

### Added
- Phase 12: safe CFE_MSG / CFE_SB wrappers (`message.rs`: `MsgId`, `Message`, `SbBuffer`)
- Phase 13: `_defs/mids.h` + `cfs_bindings::mids` MID macro constants
- Phase 14: FFI host ↔ `ccsds_wire` conversion helpers (`convert.rs`)
- Phase 16: round-trip integration test suite (`tests/roundtrip.rs` — 52 tests, 100 % branch coverage)

### Changed
- Phase 11: mission renamed from `SAMPLE_MISSION` to `SAKURA_II`; `SAKURA_II_SCID_BASE`
  fleet anchor added; `SPACECRAFT_ID = 42U` retained

### Compliance
- Q-C8 (Endianness): BE/LE conversion locus confined to `cfs_bindings` + `ccsds_wire` only;
  enforced by `scripts/grep-lints.sh`
- MISRA C:2012 baseline: `scripts/cppcheck-gate.sh` exits 0 (Phase 18)
- 100 % branch coverage gate: `scripts/coverage-gate.sh` exits 0 (Phase 19)
