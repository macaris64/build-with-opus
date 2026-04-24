//! CFDP transfer layer (docs/architecture/06-ground-segment-rust.md §4).
//!
//! Implements CCSDS File Delivery Protocol (CFDP) Class 1 (unacknowledged)
//! for the Phase B downlink path. Class 2 (acknowledged) is deferred behind
//! the [`CfdpProvider`] trait boundary per Q-C3.
//!
//! # Trait Hierarchy (§4.2, Q-C3)
//!
//! ```text
//! CfdpReceiver  ──  on_pdu, finalize_transaction
//!      ▲
//! CfdpProvider  ──  send_file, cancel, poll, active_transactions
//! ```
//!
//! `Class1Receiver` implements both. Class 2 will add a second implementation
//! without modifying any call site.
//!
//! # Parameters (§4.3)
//!
//! | Parameter               | Value                             |
//! |-------------------------|-----------------------------------|
//! | Max concurrent tx       | [`CFDP_MAX_TRANSACTIONS`] = 16    |
//! | Transaction timeout     | `10 × OWLT` (from `mission.yaml`) |
//! | Segment size            | [`CFDP_SEGMENT_SIZE_BYTES`] = 1024|
//! | Checksum                | CRC-32 IEEE 802.3 (Q-C2)          |

pub mod adapter;
pub mod class1;

// ---------------------------------------------------------------------------
// CFDP protocol constants (§4.3).
// ---------------------------------------------------------------------------

/// Maximum simultaneously active CFDP transactions (§4.3).
pub const CFDP_MAX_TRANSACTIONS: usize = 16;

/// User-data payload per File Data PDU in bytes (§4.3).
pub const CFDP_SEGMENT_SIZE_BYTES: usize = 1_024;

/// Timeout multiplier over one-way light time for transaction abandonment (§4.3).
pub const CFDP_TIMEOUT_OWLT_MULT: u32 = 10;

// ---------------------------------------------------------------------------
// Core types — verbatim from §4.2 (Q-C3 definition site).
// ---------------------------------------------------------------------------

/// Opaque CFDP transaction identifier (host-endian; Q-C8 conversion at
/// `ccsds_wire` / `cfs_bindings` boundary only).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct TransactionId(pub u64);

/// Errors returned by the CFDP receive/send path (§4.2).
#[derive(Debug, thiserror::Error)]
pub enum CfdpError {
    /// Incoming PDU byte slice could not be parsed.
    #[error("pdu parse failed: {0}")]
    Parse(String),
    /// Referenced transaction is not in the active transaction table.
    #[error("unknown CFDP transaction: {0:?}")]
    UnknownTransaction(TransactionId),
    /// Active transaction table is at capacity ([`CFDP_MAX_TRANSACTIONS`]).
    #[error("CFDP transaction table full (capacity 16)")]
    CapacityExceeded,
    /// CRC-32 mismatch between EOF PDU and assembled file (Q-C2).
    #[error("crc-32 mismatch for transaction {0:?}")]
    CrcMismatch(TransactionId),
    /// No EOF PDU received within `CFDP_TIMEOUT_OWLT_MULT × OWLT` deadline.
    #[error("transaction {0:?} timed out")]
    Timeout(TransactionId),
    /// Underlying I/O failure during file assembly.
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
}

/// Outcome of a completed or abandoned CFDP transaction (§4.2).
#[derive(Debug)]
pub enum TransactionOutcome {
    /// File received, CRC-32 verified against the EOF PDU (Q-C2).
    Completed {
        /// Transaction that produced the file.
        id: TransactionId,
        /// Absolute path of the assembled output file.
        path: std::path::PathBuf,
        /// Total bytes written.
        bytes: u64,
    },
    /// Transaction abandoned due to timeout or integrity failure.
    Abandoned {
        /// Transaction that was abandoned.
        id: TransactionId,
        /// Human-readable abandonment reason.
        reason: String,
        /// Bytes received before abandonment (partial file retained as `<id>.partial`).
        bytes_received: u64,
    },
}

// ---------------------------------------------------------------------------
// Trait definitions (Q-C3 resolution).
// ---------------------------------------------------------------------------

/// Receive side of CFDP: accept incoming PDUs and finalise transactions.
///
/// Signature frozen by docs/architecture/07-comms-stack.md §5.2 — DO NOT CHANGE.
pub trait CfdpReceiver {
    /// Process a single raw CFDP PDU byte slice.
    ///
    /// # Errors
    ///
    /// Returns [`CfdpError::Parse`] if the PDU cannot be parsed, or
    /// [`CfdpError::CapacityExceeded`] if the active transaction table is full.
    fn on_pdu(&mut self, pdu: &[u8]) -> Result<(), CfdpError>;

    /// Flush transaction state, verify CRC-32, and return the final outcome.
    ///
    /// # Errors
    ///
    /// Returns [`CfdpError::UnknownTransaction`] if `id` is not in the active
    /// transaction table, or [`CfdpError::CrcMismatch`] on checksum failure.
    fn finalize_transaction(&mut self, id: TransactionId) -> Result<TransactionOutcome, CfdpError>;
}

/// Full CFDP provider: receive + send + transaction lifecycle (§4.2, Q-C3).
pub trait CfdpProvider: CfdpReceiver {
    /// Initiate an uplink file transfer to a remote CFDP entity (Phase 28+).
    ///
    /// # Errors
    ///
    /// Returns [`CfdpError::CapacityExceeded`] if the transaction table is full,
    /// or [`CfdpError::Io`] if `src` cannot be read.
    fn send_file(
        &mut self,
        src: &std::path::Path,
        destination_entity_id: u16,
        remote_path: &str,
    ) -> Result<TransactionId, CfdpError>;

    /// Cancel an active transaction.
    ///
    /// # Errors
    ///
    /// Returns [`CfdpError::UnknownTransaction`] if `id` is not active.
    fn cancel(&mut self, id: TransactionId) -> Result<(), CfdpError>;

    /// Drive timeout eviction and emit outcomes for completed/abandoned transactions.
    ///
    /// MUST be called at ≥ 1 Hz with the current TAI time (§4.3). Evicts any
    /// transaction whose age exceeds `CFDP_TIMEOUT_OWLT_MULT × owlt`.
    fn poll(&mut self, now: ccsds_wire::Cuc) -> Vec<TransactionOutcome>;

    /// Return all currently active transaction identifiers (for HK and UI).
    fn active_transactions(&self) -> Vec<TransactionId>;
}
