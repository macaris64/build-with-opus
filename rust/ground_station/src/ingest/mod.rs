//! Telemetry ingest pipeline (docs/architecture/06-ground-segment-rust.md §5).
//!
//! Five-stage pipeline from a raw AOS byte stream to routed CCSDS space packets:
//!
//! ```text
//! UDP/TCP ──► AosFramer ──► VcDemultiplexer ──► SppDecoder(×4) ──► ApidRouter
//!                                                                        │
//!                                              ┌────────────────────────┤
//!                                              ▼                        ▼
//!                                         HK sink              EventLog sink
//!                                         CFDP sink            RoverForward sink
//! ```
//!
//! # Stages
//!
//! - **`AosFramer`** — Frame sync, FECF CRC-16/CCITT-FALSE validation, OCF
//!   extraction. Publishes CLCW to a [`tokio::sync::watch`] channel consumed by
//!   the COP-1 engine in [`crate::uplink`] (§6.5).
//! - **`VcDemultiplexer`** — Dispatches [`AosFrame`] to per-VC bounded mpsc
//!   channels (VC 0/1/2/3/63). Unknown VC → discard + `EVENT-AOS-UNKNOWN-VC`.
//! - **`SppDecoder`** — Decodes `M_PDU` bytes into `SpacePacket` via `ccsds_wire`.
//!   One decoder task per VC. `CcsdsError` → discard + `EVENT-SPP-DECODE-FAIL`.
//! - **`ApidRouter`** — Routes packets to HK / `EventLog` / CFDP / `RoverForward`
//!   sinks. MUST reject APIDs `0x540`–`0x543` on RF (§8.2, Q-F2) and emit
//!   `INGEST-FORBIDDEN-APID`.
//! - **Sinks** — HK ring buffer (latest-N per APID), persistent event log,
//!   [`crate::cfdp`] `Class1Receiver`, rover-forward archive.
//!
//! # Backpressure
//!
//! On [`tokio::sync::mpsc::Sender::try_send`] full, the upstream stage increments
//! `<stage>_dropped_total` and emits `EVENT-INGEST-BACKPRESSURE` (rate-limited
//! to 1 Hz maximum per stage). See §5.3.

// ---------------------------------------------------------------------------
// Channel capacities (docs/architecture/06-ground-segment-rust.md §5.3).
// These constants are the single authoritative source — never inline a literal.
// ---------------------------------------------------------------------------

/// Bounded channel capacity: raw byte stream → [`VcDemultiplexer`] (§5.3).
pub const AOS_TO_DEMUX_CAP: usize = 64;

/// Bounded channel capacity: [`VcDemultiplexer`] → per-VC [`SppDecoder`] (§5.3).
pub const DEMUX_TO_SPP_CAP: usize = 128;

/// Bounded channel capacity: [`SppDecoder`] → [`ApidRouter`] (§5.3).
pub const SPP_TO_ROUTER_CAP: usize = 128;

/// Bounded channel capacity: [`ApidRouter`] → HK sink (§5.3).
pub const ROUTER_TO_HK_CAP: usize = 256;

/// Bounded channel capacity: [`ApidRouter`] → event-log sink (§5.3).
///
/// Larger than other sinks because the persistent I/O writer may stall briefly.
pub const ROUTER_TO_EVENT_CAP: usize = 1_024;

/// Bounded channel capacity: [`ApidRouter`] → [`crate::cfdp`] sink (§5.3).
pub const ROUTER_TO_CFDP_CAP: usize = 256;

/// Bounded channel capacity: [`ApidRouter`] → rover-forward archive (§5.3).
pub const ROUTER_TO_ROVER_CAP: usize = 256;

pub mod decoder;
pub mod demux;
pub mod framer;

// ---------------------------------------------------------------------------
// Inter-stage frame / packet carrier types.
// ---------------------------------------------------------------------------

/// Carrier for a single decoded AOS transfer frame (Q-C4: fixed 1024 B on wire).
///
/// Produced by [`framer::AosFramer`] and consumed by the `VcDemultiplexer`
/// (Phase 23). `data_field` is a zero-copy [`bytes::Bytes`] slice.
#[derive(Debug, Clone)]
pub struct AosFrame {
    /// Virtual Channel ID (6-bit, 0–63), extracted from AOS primary header.
    pub vc_id: u8,
    /// Operational Control Field / CLCW (4 bytes), present when `OCF_FLAG=1`
    /// in the AOS primary header.
    pub ocf: Option<[u8; 4]>,
    /// Data field bytes: 1012 B when OCF present, 1016 B when absent.
    pub data_field: bytes::Bytes,
}

/// Routing decision produced by [`ApidRouter`] for each decoded space packet.
///
/// Routing table lives in §5.4; fault-injection APID rejection in §8.2 / Q-F2.
pub enum Route {
    /// Housekeeping telemetry — forwarded to the HK ring-buffer sink.
    Hk,
    /// Event record — appended to the persistent event log.
    EventLog,
    /// CFDP protocol data unit — forwarded to [`crate::cfdp::Class1Receiver`].
    CfdpPdu,
    /// Rover telemetry — re-serialised and stored in the rover-forward archive.
    RoverForward,
    /// Idle fill — discarded silently.
    IdleFill,
    /// Rejected packet (includes forbidden APIDs `0x540`–`0x543` per Q-F2).
    Rejected {
        /// Raw APID value that triggered rejection.
        apid: u16,
    },
}
