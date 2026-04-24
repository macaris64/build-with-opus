//! CFDP transfer layer (docs/architecture/06-ground-segment-rust.md §4).
//!
//! Implements CCSDS File Delivery Protocol (CFDP) Class 1 (unacknowledged)
//! for the Phase B downlink path. Class 2 (acknowledged) is deferred behind
//! this [`CfdpProvider`] trait boundary per Q-C3.
//!
//! # Trait Hierarchy (§4.2, Q-C3)
//!
//! ```text
//! CfdpReceiver  ──  on_pdu, finalize_transaction
//!      ▲
//! CfdpProvider  ──  send_file, cancel, active_transactions
//! ```
//!
//! `Class1Receiver` (Phase 25+) implements `CfdpProvider`. Class 2 will add a
//! second implementation against the same trait without changing call sites.
//!
//! # Parameters (§4.3)
//!
//! | Parameter               | Value                             |
//! |-------------------------|-----------------------------------|
//! | Max concurrent tx       | [`CFDP_MAX_TRANSACTIONS`] = 16    |
//! | Transaction timeout     | `10 × OWLT` (from `mission.yaml`) |
//! | Segment size            | [`CFDP_SEGMENT_SIZE_BYTES`] = 1024|
//! | Checksum                | CRC-32 IEEE 802.3 (Q-C2)          |

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
// Core types.
// ---------------------------------------------------------------------------

/// Opaque CFDP transaction identifier (wire encoding: `u32` big-endian per §4.2).
///
/// Big-endian encoding is enforced at the `ccsds_wire` + `cfs_bindings` boundary
/// only (Q-C8); this in-memory type is host-endian.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct TransactionId(pub u32);

/// Outcome of a completed or abandoned CFDP transaction.
#[derive(Debug)]
pub enum TransactionOutcome {
    /// File received and CRC-32 verified against the EOF PDU (Q-C2).
    Delivered {
        /// Transaction that produced the file.
        id: TransactionId,
        /// Absolute path of the assembled output file.
        path: std::path::PathBuf,
        /// CRC-32 IEEE 802.3 of the complete file, as carried in the EOF PDU.
        crc32: u32,
    },
    /// Transaction abandoned due to timeout or integrity failure.
    Abandoned {
        /// Transaction that was abandoned.
        id: TransactionId,
        /// Reason for abandonment.
        reason: AbandonReason,
        /// Bytes received before abandonment (partial file retained as `<id>.partial`).
        bytes_received: u64,
    },
}

/// Reason a CFDP transaction was abandoned.
#[derive(Debug)]
pub enum AbandonReason {
    /// No EOF PDU received within `CFDP_TIMEOUT_OWLT_MULT × OWLT` deadline.
    Timeout,
    /// CRC-32 in the EOF PDU did not match the reassembled file (Q-C2).
    CrcMismatch,
    /// Caller invoked [`CfdpProvider::cancel`].
    Cancelled,
}

/// Errors returned by the CFDP receive/send path.
#[derive(Debug, thiserror::Error)]
pub enum CfdpError {
    /// Referenced transaction is not in the active transaction table.
    #[error("unknown CFDP transaction: {0:?}")]
    UnknownTransaction(TransactionId),
    /// Incoming PDU byte slice could not be parsed.
    #[error("malformed CFDP PDU: {0}")]
    MalformedPdu(String),
    /// Active transaction table is at capacity ([`CFDP_MAX_TRANSACTIONS`]).
    #[error("CFDP transaction table full")]
    TableFull,
    /// Underlying I/O failure during file assembly or persistence.
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
}

// ---------------------------------------------------------------------------
// Trait definitions (Q-C3 resolution — establishes the Class 1 / Class 2
// boundary so call sites never depend on a concrete implementation).
// ---------------------------------------------------------------------------

/// Receive side of CFDP: accept incoming PDUs and finalise transactions.
///
/// Implemented by `Class1Receiver` (Phase 25+). Class 2 will provide a second
/// implementation without modifying any call site (Q-C3).
pub trait CfdpReceiver {
    /// Process a single raw CFDP PDU byte slice.
    ///
    /// # Errors
    ///
    /// Returns [`CfdpError::MalformedPdu`] if the PDU cannot be parsed, or
    /// [`CfdpError::TableFull`] if the active transaction table is at capacity.
    fn on_pdu(&mut self, pdu: &[u8]) -> Result<(), CfdpError>;

    /// Flush transaction state, verify CRC-32, and return the final outcome.
    ///
    /// # Errors
    ///
    /// Returns [`CfdpError::UnknownTransaction`] if `id` is not in the active
    /// transaction table.
    fn finalize_transaction(&mut self, id: TransactionId) -> Result<TransactionOutcome, CfdpError>;
}

/// Full CFDP provider: receive + send + transaction lifecycle.
///
/// `send_file` is a no-op stub in Phase B (downlink-only ground station).
/// Uplink support lands in Phase 28+.
pub trait CfdpProvider: CfdpReceiver {
    /// Initiate an uplink file transfer to a remote CFDP entity (Phase 28+).
    ///
    /// # Errors
    ///
    /// Returns [`CfdpError::TableFull`] if the transaction table is at capacity,
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

    /// Return all currently active transaction identifiers.
    fn active_transactions(&self) -> Vec<TransactionId>;
}
