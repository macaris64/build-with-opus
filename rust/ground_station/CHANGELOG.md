# Changelog — ground_station

All notable changes to this crate are documented here. Format: [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.1.0] — 2026-04-25

Initial release. Satisfies the Phase C Step 2 acceptance gate (`docs/architecture/06-ground-segment-rust.md §12`).

### Added

- **AOS ingest pipeline** (Phases 22–25): `AosFramer` (FECF CRC-16, OCF/CLCW, `LinkState`), `VcDemultiplexer` (bounded mpsc, backpressure events), `SppDecoder` (CCSDS Space Packet decode, backpressure), `ApidRouter` (per-APID routing table, fault-inject APID rejection for 0x540–0x543 per Q-F2).
- **CFDP Class 1 receiver** (Phase 26): `CfdpProvider` / `CfdpReceiver` traits; `Class1Receiver` state machine using `cfdp-core`; TAI↔Unix epoch adapter.
- **MFileAssembler** (Phase 27): out-of-order multi-file reassembly with `BitSet`-based completion tracking and RAM cap enforcement.
- **TC uplink pipeline** (Phase 28): `TcBuilder` (10 `TcIntent` variants, safety checks), `Cop1Engine` (FOP-1 state machine, 5 states, window=15, max-retransmit=3, CLCW watch), `TcFramer` (SDLP, CRC-16/IBM-3740).
- **Operator-UI backend** (Phase 29): `axum` HTTP+WebSocket server, 7 telemetry response surfaces (HK, Events, CFDP, MFile, LinkState, Cop1, TimeAuthority), 8 REST routes + 1 WS route; `TaiUtcConverter` UTC boundary per Q-F4 / SYS-REQ-0061.
- **Compliance**: Q-C8 (zero little-endian byte-order calls outside approved loci), Q-F3 (Cop1Engine::vs marked as Vault<u8> reservation site for Phase C+), `#![deny(clippy::unwrap_used)]` at crate root.

### Coverage (Phase 30 gate)

| Module | Lines | Coverage |
|--------|-------|----------|
| `ccsds_wire` (all src) | 163/167 | **97.6 %** |
| `ground_station::{cfdp, ingest}` | 368/387 | **95.1 %** |
