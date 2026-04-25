//! Telecommand uplink pipeline (docs/architecture/06-ground-segment-rust.md §6).
//!
//! Three-stage pipeline:
//! `TcBuilder` (catalog validation) → `Cop1Engine` (FOP-1) → `TcFramer` (SDLP).
//!
//! Four-stage pipeline from operator intent to a physical TC frame on the wire:
//!
//! ```text
//! Operator intent ──► TcBuilder ──► Cop1Engine ──► TcFramer ──► UplinkSink
//!                                       ▲
//!                                       │ CLCW (watch channel from AosFramer)
//! ```
//!
//! # Stages
//!
//! - **`TcBuilder`** — Validates operator intent `(PKT-ID, params)` against the
//!   packet catalog and serialises to SPP bytes via `ccsds_wire::PacketBuilder`.
//! - **`Cop1Engine`** — FOP-1 state machine (CCSDS 232.1-B-2). Consumes CLCW
//!   feedback from the [`tokio::sync::watch`] channel published by
//!   `ingest::AosFramer` (§6.5). Window = [`COP1_WINDOW_SIZE`], timer T1 =
//!   `2 × (OWLT + 5 s)` from `mission.yaml`, max retransmissions =
//!   [`COP1_MAX_RETRANSMIT`].
//! - **`TcFramer`** — SDLP-frames a TC frame; output ≤ [`TC_FRAME_MAX_BYTES`]
//!   per docs/architecture/07-comms-stack.md §3.3.
//! - **`UplinkSink`** — tokio [`tokio::io::AsyncWrite`] to the radio simulator
//!   or hardware interface.
//!
//! # COP-1 Stale-CLCW Policy (§6.4)
//!
//! | Threshold    | Action                                    |
//! |--------------|-------------------------------------------|
//! | `3 × T1`    | Emit `COP1-CLCW-STALE` event              |
//! | `10 × T1`   | Transition to `RETRANSMIT_WITH_WAIT` state |

// ---------------------------------------------------------------------------
// COP-1 / SDLP constants (docs/architecture/06-ground-segment-rust.md §6.4,
// CCSDS 232.1-B-2 §6.3.1).
// ---------------------------------------------------------------------------

/// FOP-1 sliding-window size (CCSDS 232.1-B-2 §6.3.1).
pub const COP1_WINDOW_SIZE: u8 = 15;

/// Maximum TC frame size in bytes (docs/architecture/07-comms-stack.md §3.3).
pub const TC_FRAME_MAX_BYTES: usize = 512;

/// Maximum retransmissions per TC frame before COP-1 abandons the frame.
pub const COP1_MAX_RETRANSMIT: u8 = 3;

/// CLCW-stale threshold multiplier over T1 before emitting `COP1-CLCW-STALE`.
pub const COP1_STALE_WARN_MULT: u32 = 3;

/// CLCW-stale threshold multiplier over T1 before entering `RETRANSMIT_WITH_WAIT`.
pub const COP1_STALE_ABORT_MULT: u32 = 10;

pub mod builder;
pub mod cop1;
pub mod dispatcher;
pub mod framer;

pub use builder::{BuilderError, TcBuilder, TcIntent};
pub use cop1::{Cop1Engine, Cop1Error, Fop1State, TcFrame, TcFrameType};
pub use framer::{FramerError, TcFramer};
