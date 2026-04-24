//! CFDP Class 1 (unacknowledged) receiver implementation (§4.2–4.5).
//!
//! `Class1Receiver` maintains the active transaction table, drives timeouts via
//! [`CfdpProvider::poll`], validates CRC-32 IEEE 802.3 on EOF PDUs, and persists
//! received files to disk. It implements both [`CfdpReceiver`] and [`CfdpProvider`].
//!
//! # PDU parsing
//!
//! CFDP PDUs arrive as raw byte slices from the ingest pipeline (VC 2, after
//! `ApidRouter` dispatches `Route::CfdpPdu`). This module implements a minimal
//! CFDP fixed-header + directive parser per CCSDS 727.0-B-5 §5. Only the three
//! directive types needed for Class 1 downlink are handled: Metadata (0x07),
//! File-Data (implicit, `pdu_type` = 1), and End-of-File (0x04).
//!
//! # Why no `cfdp-core` `DestinationHandler`
//!
//! `cfdp-rs 0.3.0`'s `DestinationHandler` carries seven generic type parameters
//! and a callback-based `CfdpUser` trait that is incompatible with the frozen
//! `CfdpReceiver` signature (§4.2 / 07 §5.2). We therefore add `cfdp-core` as a
//! workspace dependency per the Phase 26 deliverable list; state-machine logic
//! is self-contained so the trait boundary remains stable for Class 2 addition.

use crate::cfdp::{
    CfdpError, CfdpProvider, CfdpReceiver, TransactionId, TransactionOutcome,
    CFDP_MAX_TRANSACTIONS, CFDP_TIMEOUT_OWLT_MULT,
};
use ccsds_wire::Cuc;
use crc::{Crc, CRC_32_ISO_HDLC};
use std::io::{Seek, SeekFrom, Write};
use std::{
    collections::HashMap,
    path::{Path, PathBuf},
    time::Duration,
};

// CRC-32 IEEE 802.3 (ISO-HDLC) algorithm — Q-C2.
const CRC32: Crc<u32> = Crc::<u32>::new(&CRC_32_ISO_HDLC);

// ---------------------------------------------------------------------------
// CFDP PDU fixed-header constants (CCSDS 727.0-B-5 §5.1)
// ---------------------------------------------------------------------------

/// Minimum bytes required to decode the CFDP fixed header (4 bytes).
const FIXED_HDR_LEN: usize = 4;

/// Bit mask for PDU type (bit 4 of byte 0): set = File Data, clear = Directive.
const PDU_TYPE_FILE_DATA_MASK: u8 = 0b0001_0000;

/// CFDP file-directive codes (first byte of the directive data field).
const DIR_EOF: u8 = 0x04;
const DIR_METADATA: u8 = 0x07;

/// EOF PDU condition code for "No error" (upper nibble of condition-code byte).
const COND_NO_ERROR: u8 = 0x00;

// ---------------------------------------------------------------------------
// Internal transaction state
// ---------------------------------------------------------------------------

/// Metadata extracted from the CFDP Metadata PDU.
struct FileMetadata {
    /// Final destination path on disk.
    dest_path: PathBuf,
    /// File size in bytes as declared in the Metadata PDU (logged for audit).
    file_size: u64,
}

/// Per-transaction in-flight state.
struct TransactionState {
    /// TAI coarse seconds when the transaction was opened (audit/log only).
    started_tai_coarse: u32,
    /// TAI coarse seconds of the most-recent PDU (drives timeout arithmetic).
    last_pdu_tai: u32,
    /// Metadata (set on first Metadata PDU; `None` until seen).
    metadata: Option<FileMetadata>,
    /// Path of the partial file being assembled.
    partial_path: PathBuf,
    /// Bytes written so far.
    bytes_written: u64,
    /// CRC-32 declared in the EOF PDU (`None` until EOF seen).
    expected_crc: Option<u32>,
}

// ---------------------------------------------------------------------------
// Class1Receiver
// ---------------------------------------------------------------------------

/// CFDP Class 1 (unacknowledged) receiver.
///
/// Construct with [`Class1Receiver::new`], supplying the one-way light time so
/// the timeout (`10 × OWLT`) can be enforced during [`CfdpProvider::poll`].
pub struct Class1Receiver {
    /// One-way light time; timeout = `CFDP_TIMEOUT_OWLT_MULT × owlt`.
    owlt: Duration,
    /// Active transaction table; capped at [`CFDP_MAX_TRANSACTIONS`].
    active: HashMap<TransactionId, TransactionState>,
    /// Directory where assembled files are written.
    output_dir: PathBuf,
    /// Monotonically increasing counter for ground-originated sends (Phase 28+ stub).
    next_tx_id: u64,
}

impl Class1Receiver {
    /// Construct a new `Class1Receiver`.
    ///
    /// * `owlt` — one-way light time; transaction timeout = `10 × owlt`.
    /// * `output_dir` — directory where completed files are written; must exist.
    #[must_use]
    pub fn new(owlt: Duration, output_dir: PathBuf) -> Self {
        Self {
            owlt,
            active: HashMap::new(),
            output_dir,
            next_tx_id: 1,
        }
    }

    // -----------------------------------------------------------------------
    // PDU parsing helpers (CCSDS 727.0-B-5 §5)
    // -----------------------------------------------------------------------

    /// Decode the CFDP fixed header and return
    /// `(is_file_data, entity_id_len, seqnum_len, variable_header_end_offset)`.
    fn decode_fixed_header(pdu: &[u8]) -> Result<(bool, usize, usize, usize), CfdpError> {
        let (&byte0, &byte3) = pdu.first().zip(pdu.get(3)).ok_or_else(|| {
            CfdpError::Parse(format!(
                "PDU too short: {} bytes (minimum {FIXED_HDR_LEN})",
                pdu.len()
            ))
        })?;

        let is_file_data = (byte0 & PDU_TYPE_FILE_DATA_MASK) != 0;
        let entity_id_len = usize::from((byte3 >> 4) & 0x07) + 1;
        let seqnum_len = usize::from(byte3 & 0x07) + 1;
        // Variable header layout: source_entity_id + seq_num + dest_entity_id
        let var_hdr_end = FIXED_HDR_LEN + entity_id_len + seqnum_len + entity_id_len;

        if pdu.len() < var_hdr_end {
            return Err(CfdpError::Parse(format!(
                "PDU too short for variable header: {} bytes (need {var_hdr_end})",
                pdu.len()
            )));
        }
        Ok((is_file_data, entity_id_len, seqnum_len, var_hdr_end))
    }

    /// Extract the transaction ID from the parsed CFDP variable header.
    ///
    /// ID is packed as `(source_entity_id << 32) | seq_num` for table-key uniqueness.
    fn extract_transaction_id(
        pdu: &[u8],
        entity_id_len: usize,
        seqnum_len: usize,
    ) -> Result<TransactionId, CfdpError> {
        let src_start = FIXED_HDR_LEN;
        let src_end = src_start + entity_id_len;
        let seq_end = src_end + seqnum_len;

        let src_bytes = pdu
            .get(src_start..src_end)
            .ok_or_else(|| CfdpError::Parse("PDU truncated in source entity ID".to_string()))?;
        let seq_bytes = pdu
            .get(src_end..seq_end)
            .ok_or_else(|| CfdpError::Parse("PDU truncated in transaction seq num".to_string()))?;

        let source_id = Self::read_be_u64(src_bytes);
        let seq_num = Self::read_be_u64(seq_bytes);
        Ok(TransactionId((source_id << 32) | seq_num))
    }

    /// Read a big-endian unsigned integer from `bytes` (1–8 bytes).
    fn read_be_u64(bytes: &[u8]) -> u64 {
        bytes
            .iter()
            .fold(0_u64, |acc, &b| (acc << 8) | u64::from(b))
    }

    /// Parse a Metadata PDU `data_field` (starting at directive code) and
    /// return the extracted [`FileMetadata`].
    fn parse_metadata_pdu(data_field: &[u8]) -> Result<FileMetadata, CfdpError> {
        // data_field layout (§5.2.5):
        //   [0]      directive code 0x07 (already verified by caller)
        //   [1]      segmentation_control | reserved
        //   [2..6]   file size (u32 BE, small-file mode)
        //   [6..]    source filename (NUL-terminated) + dest filename (NUL-terminated)
        let size_bytes: [u8; 4] = data_field
            .get(2..6)
            .and_then(|s| s.try_into().ok())
            .ok_or_else(|| CfdpError::Parse("Metadata PDU data field too short".to_string()))?;
        let file_size = u64::from(u32::from_be_bytes(size_bytes));

        let names_start = 6_usize;
        let names = data_field
            .get(names_start..)
            .ok_or_else(|| CfdpError::Parse("Metadata PDU missing filenames".to_string()))?;

        // Source filename: NUL-terminated (may be empty → just the NUL byte).
        let src_end = names.iter().position(|&b| b == 0).ok_or_else(|| {
            CfdpError::Parse("Metadata: unterminated source filename".to_string())
        })?;

        // Destination filename: NUL-terminated, immediately after source NUL.
        let dest_slice = names.get(src_end + 1..).ok_or_else(|| {
            CfdpError::Parse("Metadata: no destination filename field".to_string())
        })?;
        let dest_end = dest_slice.iter().position(|&b| b == 0).ok_or_else(|| {
            CfdpError::Parse("Metadata: unterminated destination filename".to_string())
        })?;

        let dest_name =
            std::str::from_utf8(dest_slice.get(..dest_end).ok_or_else(|| {
                CfdpError::Parse("Metadata: dest filename slice error".to_string())
            })?)
            .map_err(|e| CfdpError::Parse(format!("Metadata: invalid dest filename UTF-8: {e}")))?;

        Ok(FileMetadata {
            dest_path: PathBuf::from(dest_name),
            file_size,
        })
    }

    /// Parse an EOF PDU `data_field` and return `(condition_code, crc32, file_size)`.
    fn parse_eof_pdu(data_field: &[u8]) -> Result<(u8, u32, u64), CfdpError> {
        // data_field layout (§5.2.4):
        //   [0]      directive code 0x04 (already verified by caller)
        //   [1]      condition_code(4) | spare(4)
        //   [2..6]   CRC-32 (u32 BE)
        //   [6..10]  file size (u32 BE, small-file mode)
        let cond_byte = data_field
            .get(1)
            .copied()
            .ok_or_else(|| CfdpError::Parse("EOF PDU too short for condition code".to_string()))?;
        let crc_bytes: [u8; 4] = data_field
            .get(2..6)
            .and_then(|s| s.try_into().ok())
            .ok_or_else(|| CfdpError::Parse("EOF PDU too short for CRC-32".to_string()))?;
        let size_bytes: [u8; 4] = data_field
            .get(6..10)
            .and_then(|s| s.try_into().ok())
            .ok_or_else(|| CfdpError::Parse("EOF PDU too short for file size".to_string()))?;

        let cond_code = (cond_byte >> 4) & 0x0F;
        let crc32 = u32::from_be_bytes(crc_bytes);
        let file_size = u64::from(u32::from_be_bytes(size_bytes));
        Ok((cond_code, crc32, file_size))
    }

    /// Parse a File Data PDU `data_field` and return `(offset, data_slice)`.
    fn parse_file_data_pdu(data_field: &[u8]) -> Result<(u64, &[u8]), CfdpError> {
        // data_field layout (§5.3.2, small-file mode):
        //   [0..4]  offset (u32 BE)
        //   [4..]   file data
        let offset_bytes: [u8; 4] = data_field
            .get(..4)
            .and_then(|s| s.try_into().ok())
            .ok_or_else(|| CfdpError::Parse("File Data PDU too short for offset".to_string()))?;
        let offset = u64::from(u32::from_be_bytes(offset_bytes));
        let chunk = data_field.get(4..).ok_or_else(|| {
            CfdpError::Parse("File Data PDU has no data after offset".to_string())
        })?;
        Ok((offset, chunk))
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    fn timeout_secs(&self) -> u64 {
        self.owlt
            .as_secs()
            .saturating_mul(u64::from(CFDP_TIMEOUT_OWLT_MULT))
    }

    fn partial_path_for(&self, id: TransactionId) -> PathBuf {
        self.output_dir.join(format!("{}.partial", id.0))
    }

    fn new_state(&self, id: TransactionId) -> TransactionState {
        TransactionState {
            started_tai_coarse: 0,
            last_pdu_tai: 0,
            metadata: None,
            partial_path: self.partial_path_for(id),
            bytes_written: 0,
            expected_crc: None,
        }
    }

    /// Compute CRC-32 IEEE 802.3 over a file on disk.
    fn file_crc32(path: &Path) -> Result<u32, CfdpError> {
        use std::io::Read;
        let mut file = std::fs::File::open(path)?;
        let mut digest = CRC32.digest();
        let mut buf = [0u8; 8192];
        loop {
            let n = file.read(&mut buf)?;
            if n == 0 {
                break;
            }
            digest.update(buf.get(..n).unwrap_or(&buf));
        }
        Ok(digest.finalize())
    }
}

// ---------------------------------------------------------------------------
// CfdpReceiver impl
// ---------------------------------------------------------------------------

impl CfdpReceiver for Class1Receiver {
    fn on_pdu(&mut self, pdu: &[u8]) -> Result<(), CfdpError> {
        let (is_file_data, entity_id_len, seqnum_len, var_hdr_end) =
            Self::decode_fixed_header(pdu)?;

        let tx_id = Self::extract_transaction_id(pdu, entity_id_len, seqnum_len)?;

        let data_field = pdu.get(var_hdr_end..).ok_or_else(|| {
            CfdpError::Parse("PDU has no data field after variable header".to_string())
        })?;

        if is_file_data {
            // File Data PDU — open transaction on first segment if needed.
            if !self.active.contains_key(&tx_id) {
                if self.active.len() >= CFDP_MAX_TRANSACTIONS {
                    return Err(CfdpError::CapacityExceeded);
                }
                self.active.insert(tx_id, self.new_state(tx_id));
            }

            let (offset, chunk) = Self::parse_file_data_pdu(data_field)?;

            let state = self
                .active
                .get_mut(&tx_id)
                .ok_or(CfdpError::CapacityExceeded)?;

            let mut file = std::fs::OpenOptions::new()
                .create(true)
                .write(true)
                // truncate(false): we write at a specific offset, not from the start.
                .truncate(false)
                .open(&state.partial_path)?;
            file.seek(SeekFrom::Start(offset))?;
            file.write_all(chunk)?;
            // usize ≤ u64 on all supported targets; cast is lossless.
            state.bytes_written = state.bytes_written.saturating_add(chunk.len() as u64);
        } else {
            // File Directive PDU.
            let dir_code = data_field
                .first()
                .copied()
                .ok_or_else(|| CfdpError::Parse("Empty directive data field".to_string()))?;

            match dir_code {
                DIR_METADATA => {
                    if !self.active.contains_key(&tx_id) {
                        if self.active.len() >= CFDP_MAX_TRANSACTIONS {
                            return Err(CfdpError::CapacityExceeded);
                        }
                        self.active.insert(tx_id, self.new_state(tx_id));
                    }
                    let meta = Self::parse_metadata_pdu(data_field)?;
                    let state = self
                        .active
                        .get_mut(&tx_id)
                        .ok_or(CfdpError::CapacityExceeded)?;
                    state.metadata = Some(meta);
                }
                DIR_EOF => {
                    let (cond_code, crc32, _eof_file_size) = Self::parse_eof_pdu(data_field)?;
                    let state = self
                        .active
                        .get_mut(&tx_id)
                        .ok_or(CfdpError::UnknownTransaction(tx_id))?;
                    // Non-zero condition code means the sender declared an error.
                    state.expected_crc = if cond_code == COND_NO_ERROR {
                        Some(crc32)
                    } else {
                        None
                    };
                }
                _ => {
                    log::debug!(
                        "CFDP: ignoring unknown directive 0x{dir_code:02X} for tx {tx_id:?}"
                    );
                }
            }
        }
        Ok(())
    }

    fn finalize_transaction(&mut self, id: TransactionId) -> Result<TransactionOutcome, CfdpError> {
        let state = self
            .active
            .remove(&id)
            .ok_or(CfdpError::UnknownTransaction(id))?;

        // Compute CRC-32 over the assembled partial file.
        let actual_crc = Self::file_crc32(&state.partial_path)?;

        if let Some(expected) = state.expected_crc {
            if actual_crc != expected {
                // Retain .partial file for forensics (§4.3).
                return Err(CfdpError::CrcMismatch(id));
            }
        }

        // Resolve final output path; log declared vs actual size for audit.
        let final_path = match &state.metadata {
            Some(m) => {
                log::debug!(
                    "CFDP tx {id:?}: declared size {} B, received {} B",
                    m.file_size,
                    state.bytes_written
                );
                if m.dest_path.is_absolute() {
                    m.dest_path.clone()
                } else {
                    self.output_dir.join(&m.dest_path)
                }
            }
            None => self.output_dir.join(format!("{}.bin", id.0)),
        };

        std::fs::rename(&state.partial_path, &final_path)?;

        Ok(TransactionOutcome::Completed {
            id,
            path: final_path,
            bytes: state.bytes_written,
        })
    }
}

// ---------------------------------------------------------------------------
// CfdpProvider impl
// ---------------------------------------------------------------------------

impl CfdpProvider for Class1Receiver {
    fn send_file(
        &mut self,
        _src: &Path,
        _destination_entity_id: u16,
        _remote_path: &str,
    ) -> Result<TransactionId, CfdpError> {
        // Stub — uplink Class 1 send is implemented in Phase 28.
        let id = TransactionId(self.next_tx_id);
        self.next_tx_id += 1;
        Ok(id)
    }

    fn cancel(&mut self, id: TransactionId) -> Result<(), CfdpError> {
        let state = self
            .active
            .remove(&id)
            .ok_or(CfdpError::UnknownTransaction(id))?;
        log::info!(
            "CFDP-TX-CANCELLED: tx {id:?}, {} bytes received, partial at {}",
            state.bytes_written,
            state.partial_path.display()
        );
        Ok(())
    }

    fn poll(&mut self, now: Cuc) -> Vec<TransactionOutcome> {
        let timeout = self.timeout_secs();
        let now_tai = u64::from(now.coarse);

        let timed_out: Vec<TransactionId> = self
            .active
            .iter()
            .filter_map(|(&id, state)| {
                let age = now_tai.saturating_sub(u64::from(state.last_pdu_tai));
                if age > timeout {
                    Some(id)
                } else {
                    None
                }
            })
            .collect();

        let mut outcomes = Vec::with_capacity(timed_out.len());
        for id in timed_out {
            if let Some(state) = self.active.remove(&id) {
                log::warn!(
                    "CFDP-TX-ABANDONED: tx {id:?} (started TAI {}) timed out after {timeout}s, {} bytes received",
                    state.started_tai_coarse,
                    state.bytes_written,
                );
                outcomes.push(TransactionOutcome::Abandoned {
                    id,
                    reason: format!("timeout after {timeout}s"),
                    bytes_received: state.bytes_written,
                });
            }
        }
        outcomes
    }

    fn active_transactions(&self) -> Vec<TransactionId> {
        self.active.keys().copied().collect()
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::cast_possible_truncation,
    clippy::missing_panics_doc,
    clippy::missing_errors_doc
)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::TempDir;

    fn make_rx(owlt_secs: u64) -> (Class1Receiver, TempDir) {
        let dir = TempDir::new().unwrap();
        let rx = Class1Receiver::new(Duration::from_secs(owlt_secs), dir.path().to_path_buf());
        (rx, dir)
    }

    /// Build a minimal CFDP PDU: fixed header (4 B) + variable header (3 B:
    /// 1-byte source ID, 1-byte seq num, 1-byte dest ID) + `data_field`.
    ///
    /// `is_file_data=false` → file directive PDU; `is_file_data=true` → file data PDU.
    fn build_pdu(is_file_data: bool, source_id: u8, seq_num: u8, data_field: &[u8]) -> Vec<u8> {
        let mut pdu = Vec::new();
        // Byte 0: version(000) | pdu_type | direction(0) | tx_mode(1=unack) | crc(0) | large(0)
        let byte0: u8 = if is_file_data {
            0b0001_0100
        } else {
            0b0000_0100
        };
        pdu.push(byte0);
        // Bytes 1-2: PDU data field length (variable header 3 B + data field).
        let pdu_data_len = u16::try_from(3 + data_field.len()).unwrap();
        pdu.extend_from_slice(&pdu_data_len.to_be_bytes());
        // Byte 3: seg_ctrl(0) | entity_id_len_minus1(000=1 byte) | rsv(0) | seqnum_len_minus1(000=1 byte)
        pdu.push(0b0000_0000u8);
        pdu.push(source_id); // source entity ID (1 byte)
        pdu.push(seq_num); // transaction seq num (1 byte)
        pdu.push(0x01); // destination entity ID (1 byte)
        pdu.extend_from_slice(data_field);
        pdu
    }

    fn metadata_pdu(source_id: u8, seq_num: u8, file_size: u32, dest: &str) -> Vec<u8> {
        let mut df = Vec::new();
        df.push(DIR_METADATA);
        df.push(0x00); // segmentation control
        df.extend_from_slice(&file_size.to_be_bytes());
        df.push(0x00); // source filename: empty (NUL only)
        df.extend_from_slice(dest.as_bytes());
        df.push(0x00); // destination filename NUL terminator
        build_pdu(false, source_id, seq_num, &df)
    }

    fn file_data_pdu(source_id: u8, seq_num: u8, offset: u32, data: &[u8]) -> Vec<u8> {
        let mut df = Vec::new();
        df.extend_from_slice(&offset.to_be_bytes());
        df.extend_from_slice(data);
        build_pdu(true, source_id, seq_num, &df)
    }

    fn eof_pdu(source_id: u8, seq_num: u8, crc32: u32, file_size: u32) -> Vec<u8> {
        let mut df = Vec::new();
        df.push(DIR_EOF);
        df.push(COND_NO_ERROR << 4); // condition code + spare
        df.extend_from_slice(&crc32.to_be_bytes());
        df.extend_from_slice(&file_size.to_be_bytes());
        build_pdu(false, source_id, seq_num, &df)
    }

    fn make_tx_id(source_id: u8, seq_num: u8) -> TransactionId {
        TransactionId((u64::from(source_id) << 32) | u64::from(seq_num))
    }

    // -----------------------------------------------------------------------
    // RED test — Given  a freshly constructed Class1Receiver with owlt = 1 s
    //            When   on_pdu is called with an empty byte slice
    //            Then   Err(CfdpError::Parse(_)) is returned
    //            And    active_transactions() is empty
    // -----------------------------------------------------------------------
    #[test]
    fn rejects_empty_pdu() {
        let (mut rx, _dir) = make_rx(1);
        assert!(matches!(rx.on_pdu(&[]), Err(CfdpError::Parse(_))));
        assert!(rx.active_transactions().is_empty());
    }

    // -----------------------------------------------------------------------
    // Given  a 3-byte PDU (too short for fixed header)
    // When   on_pdu is called
    // Then   Err(CfdpError::Parse(_)) is returned
    // -----------------------------------------------------------------------
    #[test]
    fn rejects_truncated_fixed_header() {
        let (mut rx, _dir) = make_rx(1);
        assert!(matches!(
            rx.on_pdu(&[0x04, 0x00, 0x03]),
            Err(CfdpError::Parse(_))
        ));
    }

    // -----------------------------------------------------------------------
    // Given  a valid Metadata PDU for tx (src=1, seq=1)
    // When   on_pdu is called
    // Then   the transaction becomes active
    // -----------------------------------------------------------------------
    #[test]
    fn metadata_pdu_opens_transaction() {
        let (mut rx, _dir) = make_rx(1);
        rx.on_pdu(&metadata_pdu(1, 1, 5, "out.bin")).unwrap();
        let active = rx.active_transactions();
        assert_eq!(active.len(), 1);
        assert!(active.contains(&make_tx_id(1, 1)));
    }

    // -----------------------------------------------------------------------
    // Given  a valid File Data PDU (no preceding Metadata)
    // When   on_pdu is called
    // Then   the transaction is opened and data is written to disk
    // -----------------------------------------------------------------------
    #[test]
    fn file_data_pdu_opens_transaction() {
        let (mut rx, _dir) = make_rx(1);
        rx.on_pdu(&file_data_pdu(1, 2, 0, b"hello")).unwrap();
        assert_eq!(rx.active_transactions().len(), 1);
    }

    // -----------------------------------------------------------------------
    // Given  16 concurrent transactions are open
    // When   a 17th Metadata PDU arrives
    // Then   Err(CfdpError::CapacityExceeded) is returned
    // -----------------------------------------------------------------------
    #[test]
    fn seventeenth_transaction_rejected() {
        let (mut rx, _dir) = make_rx(1);
        for seq in 1u8..=16 {
            rx.on_pdu(&metadata_pdu(1, seq, 0, "f.bin")).unwrap();
        }
        assert_eq!(rx.active_transactions().len(), 16);
        assert!(matches!(
            rx.on_pdu(&metadata_pdu(1, 17, 0, "g.bin")),
            Err(CfdpError::CapacityExceeded)
        ));
    }

    // -----------------------------------------------------------------------
    // Happy-path end-to-end: Metadata → FileData → EOF → finalize
    //
    // Given  a complete Class 1 downlink sequence for a small file
    // When   on_pdu is called for each PDU and finalize_transaction follows
    // Then   TransactionOutcome::Completed is returned and the file exists
    // -----------------------------------------------------------------------
    #[test]
    fn happy_path_complete_transfer() {
        let (mut rx, _dir) = make_rx(1);
        let payload: &[u8] = b"SAKURA-II telemetry data";
        let file_size = payload.len() as u32;
        let crc32 = CRC32.checksum(payload);

        rx.on_pdu(&metadata_pdu(2, 1, file_size, "tlm.bin"))
            .unwrap();
        rx.on_pdu(&file_data_pdu(2, 1, 0, payload)).unwrap();
        rx.on_pdu(&eof_pdu(2, 1, crc32, file_size)).unwrap();

        let id = make_tx_id(2, 1);
        match rx.finalize_transaction(id).unwrap() {
            TransactionOutcome::Completed { path, bytes, .. } => {
                assert!(path.exists());
                assert_eq!(bytes, u64::from(file_size));
                assert_eq!(fs::read(&path).unwrap(), payload);
            }
            TransactionOutcome::Abandoned { .. } => panic!("expected Completed"),
        }
    }

    // -----------------------------------------------------------------------
    // Given  an EOF PDU with a wrong CRC-32
    // When   finalize_transaction is called
    // Then   Err(CfdpError::CrcMismatch(_)) is returned
    // And    the .partial file is retained on disk (forensics, §4.3)
    // -----------------------------------------------------------------------
    #[test]
    fn crc_mismatch_returns_error_and_retains_partial() {
        let (mut rx, dir) = make_rx(1);
        let payload: &[u8] = b"bad data";
        let wrong_crc: u32 = 0xDEAD_BEEF;

        rx.on_pdu(&metadata_pdu(3, 1, payload.len() as u32, "x.bin"))
            .unwrap();
        rx.on_pdu(&file_data_pdu(3, 1, 0, payload)).unwrap();
        rx.on_pdu(&eof_pdu(3, 1, wrong_crc, payload.len() as u32))
            .unwrap();

        let id = make_tx_id(3, 1);
        assert!(matches!(
            rx.finalize_transaction(id),
            Err(CfdpError::CrcMismatch(_))
        ));
        // .partial file must still exist after CRC failure.
        assert!(dir.path().join(format!("{}.partial", id.0)).exists());
    }

    // -----------------------------------------------------------------------
    // Given  finalize_transaction called for an unknown ID
    // Then   Err(CfdpError::UnknownTransaction(_)) is returned
    // -----------------------------------------------------------------------
    #[test]
    fn finalize_unknown_transaction() {
        let (mut rx, _dir) = make_rx(1);
        assert!(matches!(
            rx.finalize_transaction(TransactionId(9999)),
            Err(CfdpError::UnknownTransaction(_))
        ));
    }

    // -----------------------------------------------------------------------
    // Given  an active transaction whose last_pdu_tai = 0
    // When   poll is called with now.coarse = 11 and owlt = 1 s (timeout 10 s)
    // Then   the transaction is evicted with an Abandoned outcome
    // -----------------------------------------------------------------------
    #[test]
    fn poll_evicts_timed_out_transaction() {
        let (mut rx, _dir) = make_rx(1); // timeout = 10 s
        rx.on_pdu(&metadata_pdu(4, 1, 0, "t.bin")).unwrap();
        assert_eq!(rx.active_transactions().len(), 1);

        // age = 11 - 0 = 11 > 10 → timeout
        let outcomes = rx.poll(Cuc {
            coarse: 11,
            fine: 0,
        });
        assert_eq!(outcomes.len(), 1);
        assert!(matches!(
            outcomes.first(),
            Some(TransactionOutcome::Abandoned { .. })
        ));
        assert!(rx.active_transactions().is_empty());
    }

    // -----------------------------------------------------------------------
    // Given  an active transaction within the timeout window
    // When   poll is called
    // Then   no eviction occurs
    // -----------------------------------------------------------------------
    #[test]
    fn poll_does_not_evict_fresh_transaction() {
        let (mut rx, _dir) = make_rx(60); // timeout = 600 s
        rx.on_pdu(&metadata_pdu(5, 1, 0, "u.bin")).unwrap();

        // age = 5 - 0 = 5 < 600 → no eviction
        let outcomes = rx.poll(Cuc { coarse: 5, fine: 0 });
        assert!(outcomes.is_empty());
        assert_eq!(rx.active_transactions().len(), 1);
    }

    // -----------------------------------------------------------------------
    // poll on an empty transaction table must not panic
    // -----------------------------------------------------------------------
    #[test]
    fn poll_empty_table_no_panic() {
        let (mut rx, _dir) = make_rx(1);
        assert!(rx
            .poll(Cuc {
                coarse: 9999,
                fine: 0
            })
            .is_empty());
    }

    // -----------------------------------------------------------------------
    // cancel returns UnknownTransaction for a non-existent ID
    // -----------------------------------------------------------------------
    #[test]
    fn cancel_unknown_transaction() {
        let (mut rx, _dir) = make_rx(1);
        assert!(matches!(
            rx.cancel(TransactionId(42)),
            Err(CfdpError::UnknownTransaction(_))
        ));
    }

    // -----------------------------------------------------------------------
    // cancel removes the active transaction from the table
    // -----------------------------------------------------------------------
    #[test]
    fn cancel_removes_active_transaction() {
        let (mut rx, _dir) = make_rx(1);
        rx.on_pdu(&metadata_pdu(6, 1, 0, "c.bin")).unwrap();
        rx.cancel(make_tx_id(6, 1)).unwrap();
        assert!(rx.active_transactions().is_empty());
    }

    // -----------------------------------------------------------------------
    // send_file stub must return Ok and not panic (Phase 28 stub)
    // -----------------------------------------------------------------------
    #[test]
    fn send_file_stub_returns_ok() {
        let (mut rx, dir) = make_rx(1);
        let src = dir.path().join("dummy.bin");
        fs::write(&src, b"data").unwrap();
        assert!(rx.send_file(&src, 0x01, "remote/file.bin").is_ok());
    }

    // -----------------------------------------------------------------------
    // active_transactions returns all open transaction IDs
    // -----------------------------------------------------------------------
    #[test]
    fn active_transactions_lists_all_open() {
        let (mut rx, _dir) = make_rx(1);
        rx.on_pdu(&metadata_pdu(7, 1, 0, "a.bin")).unwrap();
        rx.on_pdu(&metadata_pdu(7, 2, 0, "b.bin")).unwrap();
        let active = rx.active_transactions();
        assert_eq!(active.len(), 2);
        assert!(active.contains(&make_tx_id(7, 1)));
        assert!(active.contains(&make_tx_id(7, 2)));
    }
}
