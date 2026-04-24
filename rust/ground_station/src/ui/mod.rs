//! Operator UI backend (docs/architecture/06-ground-segment-rust.md §10).
//!
//! Serves ground-station state over WebSocket + REST to the operator frontend.
//! Framework selection, authentication, colour palette, and deployment topology
//! are out of scope for Phase B (see `docs/dev/ground-ui.md`, planned).
//!
//! # Surfaces
//!
//! 1. **HK dashboard** — per-asset, per-APID, latest-N housekeeping frames.
//! 2. **Event stream** — rolling event log, filterable by severity and APID.
//! 3. **CFDP transaction list** — active + completed transactions with progress %.
//! 4. **M-File transaction list** — per-transaction chunk-gap visualisation.
//! 5. **Link state** — `Aos` / `Los` / `Degraded` with last-frame timestamp.
//! 6. **COP-1 state** — FOP-1 state machine, window occupancy, retransmit counters.
//! 7. **Time authority** — TAI offset, drift budget (µs/day), sync-packet age.
//!
//! # Time Representation
//!
//! All serialised timestamps use UTC milliseconds in ISO-8601 format.
//! TAI-to-UTC conversion happens at this boundary; internal pipeline state is
//! TAI throughout (Q-F4, docs/architecture/08-timing-and-clocks.md §3–4).
