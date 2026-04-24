//! Mission File (M-File) assembler (docs/architecture/06-ground-segment-rust.md §7).
//!
//! Reassembles chunked file transfers delivered over AOS VC 2, either alongside
//! or instead of CFDP, per ICD-relay-surface.md §6 (Batch B2).
//!
//! # Wire Format (§7.3 — all multi-byte fields big-endian, Q-C8)
//!
//! | Offset | Field              | Type   | Description                         |
//! |-------:|--------------------|--------|-------------------------------------|
//! | 0      | `transaction_id`   | u32 BE | Unique per file transfer            |
//! | 4      | `total_size_bytes` | u64 BE | Complete file size in bytes         |
//! | 12     | `total_chunks`     | u32 BE | Expected chunk count                |
//! | 16     | `crc32_full_file`  | u32 BE | CRC-32 IEEE 802.3 of complete file  |
//! | 20     | `chunk_index`      | u32 BE | Zero-based index of this chunk      |
//! | 24     | `chunk_len`        | u16 BE | Payload bytes in this chunk         |
//! | 26     | `last_chunk_index` | u32 BE | Index of the final chunk            |
//! | 30     | `crc32_sent`       | u32 BE | CRC-32 of this chunk's payload      |
//! | 34     | payload            | bytes  | `chunk_len` bytes of file data      |
//!
//! # Assembly Strategy
//!
//! Chunks arrive out of order. Per-transaction state holds a
//! `BTreeMap<chunk_index, Vec<u8>>` for O(log n) insertion and a `BitSet` for
//! O(1) completion detection. On receipt of the last chunk, the full file is
//! flushed and `crc32_full_file` is verified.
//!
//! # Duplicate Handling
//!
//! - Same chunk index, same content → `dup_ok_total` incremented (idempotent).
//! - Same chunk index, differing content → `dup_mismatch_total` incremented +
//!   `MFILE-DUP-MISMATCH` event emitted (first-arrival wins).
//!
//! # Resource Bounds
//!
//! `MFILE_MAX_ASSEMBLY_RAM_MB` (from `mission.yaml`) caps total in-flight RAM.
//! On breach, the oldest transaction is evicted and `STORAGE-PRESSURE` is emitted.
//! Transactions that expire at `started_tai + MFILE_TIMEOUT_OWLT_MULT × OWLT`
//! emit `MFILE-INCOMPLETE` and are persisted as `<transaction_id>.partial`.
//!
//! Q-F3: `MFileAssembler` holds transient TM pipeline state — explicitly excluded
//! from `Vault<T>` per docs/architecture/09-failure-and-radiation.md §5.2.

use std::{collections::BTreeMap, fs, path::PathBuf, time::Duration};

use ccsds_wire::Cuc;
use crc::{Crc, CRC_32_ISO_HDLC};

// ---------------------------------------------------------------------------
// M-File protocol constants (§7.3).
// ---------------------------------------------------------------------------

/// Maximum chunk payload in bytes (matches [`crate::cfdp::CFDP_SEGMENT_SIZE_BYTES`]
/// to allow shared reassembly buffers where CFDP and M-File coexist on VC 2).
pub const MFILE_CHUNK_MAX_BYTES: usize = 1_024;

/// Timeout multiplier over one-way light time before a transaction is evicted
/// as incomplete (§7.3).
pub const MFILE_TIMEOUT_OWLT_MULT: u32 = 10;

/// Minimum wire header size in bytes (§7.3 wire table: last field at offset 30, 4 B wide).
const HEADER_LEN: usize = 34;

/// CRC-32 IEEE 802.3 algorithm instance.
const CRC32: Crc<u32> = Crc::<u32>::new(&CRC_32_ISO_HDLC);

// ---------------------------------------------------------------------------
// Wire-field helpers — use .get() so indexing_slicing is not triggered.
// ---------------------------------------------------------------------------

fn read_u32_be(buf: &[u8], offset: usize) -> Option<u32> {
    buf.get(offset..offset + 4)
        .and_then(|s| s.try_into().ok())
        .map(u32::from_be_bytes)
}

fn read_u64_be(buf: &[u8], offset: usize) -> Option<u64> {
    buf.get(offset..offset + 8)
        .and_then(|s| s.try_into().ok())
        .map(u64::from_be_bytes)
}

fn read_u16_be(buf: &[u8], offset: usize) -> Option<u16> {
    buf.get(offset..offset + 2)
        .and_then(|s| s.try_into().ok())
        .map(u16::from_be_bytes)
}

// ---------------------------------------------------------------------------
// Public types.
// ---------------------------------------------------------------------------

/// Outcome produced when a transaction reaches a terminal state.
#[derive(Debug)]
pub enum AssemblyOutcome {
    /// All chunks received, full-file CRC-32 verified, file written to disk.
    Complete {
        transaction_id: u32,
        path: PathBuf,
        bytes: u64,
    },
    /// Transaction timed out or had a full-file CRC-32 mismatch; partial file on disk.
    Abandoned {
        transaction_id: u32,
        partial_path: PathBuf,
        /// Chunk indices in `[0, total_chunks)` that were never received.
        gap_count: usize,
    },
}

/// Errors returned by [`MFileAssembler::on_chunk`].
#[derive(Debug, thiserror::Error)]
pub enum MFileError {
    #[error("raw buffer too short for M-File header (need {HEADER_LEN} bytes)")]
    TooShort,

    #[error("chunk_len {got} exceeds MFILE_CHUNK_MAX_BYTES ({MFILE_CHUNK_MAX_BYTES})")]
    ChunkLenExceeded { got: u16 },

    #[error(
        "chunk CRC-32 mismatch for tx {transaction_id}: \
         expected {expected:#010x}, got {got:#010x}"
    )]
    ChunkCrcMismatch {
        transaction_id: u32,
        expected: u32,
        got: u32,
    },

    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
}

// ---------------------------------------------------------------------------
// Internal helpers.
// ---------------------------------------------------------------------------

/// Minimal bit-array backed by `Vec<u64>` words.
/// O(1) set and completion-check without external dependencies.
///
/// Invariant: `words.len() == (len as usize).div_ceil(64)`.
/// All `idx` arguments must satisfy `idx < len`; callers are responsible.
struct BitSet {
    words: Vec<u64>,
    len: u32,
    set_count: u32,
}

impl BitSet {
    fn new(len: u32) -> Self {
        let word_count = (len as usize).div_ceil(64);
        Self {
            words: vec![0u64; word_count],
            len,
            set_count: 0,
        }
    }

    /// Sets bit `idx`. Returns `true` if newly set (false = duplicate).
    // Indexing is bounds-safe: word = idx/64 < words.len() by construction (see struct invariant).
    #[allow(clippy::indexing_slicing)]
    fn set(&mut self, idx: u32) -> bool {
        let word = (idx / 64) as usize;
        let bit = idx % 64;
        let mask = 1u64 << bit;
        if self.words[word] & mask == 0 {
            self.words[word] |= mask;
            self.set_count += 1;
            true
        } else {
            false
        }
    }

    // Indexing is bounds-safe: same invariant as set().
    #[allow(clippy::indexing_slicing)]
    fn is_set(&self, idx: u32) -> bool {
        let word = (idx / 64) as usize;
        let bit = idx % 64;
        self.words[word] & (1u64 << bit) != 0
    }

    fn is_full(&self) -> bool {
        self.set_count == self.len
    }
}

/// Per-transaction reassembly state.
struct TxState {
    chunks: BTreeMap<u32, Vec<u8>>,
    received: BitSet,
    total_chunks: u32,
    total_size_bytes: u64,
    crc32_full_file: u32,
    /// CUC coarse-second deadline; evicted when `poll` observes `now.coarse > this`.
    timeout_tai_coarse: u32,
    /// RAM held by received chunks for this transaction.
    in_flight_bytes: usize,
}

// ---------------------------------------------------------------------------
// Public assembler.
// ---------------------------------------------------------------------------

/// Out-of-order chunk reassembler for M-File transfers (§7).
pub struct MFileAssembler {
    owlt_secs: u32,
    output_dir: PathBuf,
    max_ram_bytes: usize,
    active: BTreeMap<u32, TxState>,
    total_ram_bytes: usize,
    dup_ok_total: u64,
    dup_mismatch_total: u64,
}

impl MFileAssembler {
    /// Creates a new assembler.
    ///
    /// - `owlt`: one-way light time; transaction timeout = `owlt × MFILE_TIMEOUT_OWLT_MULT`.
    /// - `output_dir`: directory where completed and partial files are written.
    /// - `max_ram_mb`: total RAM cap for all in-flight chunks combined (megabytes).
    #[must_use]
    pub fn new(owlt: Duration, output_dir: PathBuf, max_ram_mb: usize) -> Self {
        #[allow(clippy::cast_possible_truncation)]
        let owlt_secs = owlt.as_secs() as u32;
        Self {
            owlt_secs,
            output_dir,
            max_ram_bytes: max_ram_mb * 1_024 * 1_024,
            active: BTreeMap::new(),
            total_ram_bytes: 0,
            dup_ok_total: 0,
            dup_mismatch_total: 0,
        }
    }

    /// Processes one raw M-File wire frame.
    ///
    /// Returns `Ok(Some(outcome))` when the transaction reaches a terminal state
    /// (complete or abandoned on full-file CRC mismatch), `Ok(None)` otherwise.
    ///
    /// # Errors
    ///
    /// Returns [`MFileError`] on header parse failures or per-chunk CRC mismatch.
    pub fn on_chunk(&mut self, raw: &[u8]) -> Result<Option<AssemblyOutcome>, MFileError> {
        // --- 1. Parse header (§7.3) ----------------------------------------
        if raw.len() < HEADER_LEN {
            return Err(MFileError::TooShort);
        }
        let transaction_id = read_u32_be(raw, 0).ok_or(MFileError::TooShort)?;
        let total_size_bytes = read_u64_be(raw, 4).ok_or(MFileError::TooShort)?;
        let total_chunks = read_u32_be(raw, 12).ok_or(MFileError::TooShort)?;
        let crc32_full_file = read_u32_be(raw, 16).ok_or(MFileError::TooShort)?;
        let chunk_index = read_u32_be(raw, 20).ok_or(MFileError::TooShort)?;
        let chunk_len = read_u16_be(raw, 24).ok_or(MFileError::TooShort)?;
        // last_chunk_index (bytes 26-30): parsed for wire-format compliance;
        // completion is derived from the BitSet so it need not be stored.
        let _last_chunk_index = read_u32_be(raw, 26).ok_or(MFileError::TooShort)?;
        let crc32_sent = read_u32_be(raw, 30).ok_or(MFileError::TooShort)?;

        if usize::from(chunk_len) > MFILE_CHUNK_MAX_BYTES {
            return Err(MFileError::ChunkLenExceeded { got: chunk_len });
        }

        // --- 2. Per-chunk CRC-32 check ------------------------------------
        let payload_end = HEADER_LEN + usize::from(chunk_len);
        let payload = raw
            .get(HEADER_LEN..payload_end)
            .ok_or(MFileError::TooShort)?;
        let computed = CRC32.checksum(payload);
        if computed != crc32_sent {
            return Err(MFileError::ChunkCrcMismatch {
                transaction_id,
                expected: crc32_sent,
                got: computed,
            });
        }

        // --- 3. Upsert transaction state ------------------------------------
        if !self.active.contains_key(&transaction_id) {
            let timeout = self.owlt_secs.saturating_mul(MFILE_TIMEOUT_OWLT_MULT);
            self.active.insert(
                transaction_id,
                TxState {
                    chunks: BTreeMap::new(),
                    received: BitSet::new(total_chunks),
                    total_chunks,
                    total_size_bytes,
                    crc32_full_file,
                    timeout_tai_coarse: timeout,
                    in_flight_bytes: 0,
                },
            );
            self.enforce_ram_cap();
        }

        // --- 4. Duplicate detection ----------------------------------------
        let Some(tx) = self.active.get_mut(&transaction_id) else {
            // Transaction was evicted by RAM-cap enforcement.
            return Ok(None);
        };

        if tx.received.is_set(chunk_index) {
            if tx.chunks.get(&chunk_index).map(Vec::as_slice) == Some(payload) {
                self.dup_ok_total += 1;
            } else {
                self.dup_mismatch_total += 1;
                log::warn!(
                    "MFILE-DUP-MISMATCH: tx={transaction_id} chunk={chunk_index} \
                     — differing content received; first-arrival kept"
                );
            }
            return Ok(None);
        }

        // --- 5. Insert chunk -----------------------------------------------
        let chunk_bytes = usize::from(chunk_len);
        tx.received.set(chunk_index);
        tx.in_flight_bytes += chunk_bytes;
        self.total_ram_bytes += chunk_bytes;
        tx.chunks.insert(chunk_index, payload.to_vec());

        // --- 6. Completion check -------------------------------------------
        if tx.received.is_full() {
            return Ok(Some(self.finalize(transaction_id)?));
        }

        Ok(None)
    }

    /// Drives timeout eviction. Call at ≥ 1 Hz with the current TAI time.
    pub fn poll(&mut self, now: Cuc) -> Vec<AssemblyOutcome> {
        let now_coarse = now.coarse;
        let timed_out: Vec<u32> = self
            .active
            .iter()
            .filter(|(_, tx)| now_coarse > tx.timeout_tai_coarse)
            .map(|(&id, _)| id)
            .collect();

        let mut outcomes = Vec::new();
        for id in timed_out {
            match self.abandon(id, "MFILE-INCOMPLETE: timeout") {
                Ok(outcome) => outcomes.push(outcome),
                Err(e) => log::error!("MFILE: I/O error during timeout eviction of tx={id}: {e}"),
            }
        }
        outcomes
    }

    /// Number of transactions currently being assembled.
    #[must_use]
    pub fn active_count(&self) -> usize {
        self.active.len()
    }

    /// Total count of duplicate chunks whose content matched the first arrival.
    #[must_use]
    pub fn dup_ok_total(&self) -> u64 {
        self.dup_ok_total
    }

    /// Total count of duplicate chunks whose content differed from the first arrival.
    #[must_use]
    pub fn dup_mismatch_total(&self) -> u64 {
        self.dup_mismatch_total
    }

    // -----------------------------------------------------------------------
    // Private helpers.
    // -----------------------------------------------------------------------

    /// Concatenates all received chunks in order, verifies full-file CRC-32, and
    /// writes the output file.  Returns `Complete` on CRC match, `Abandoned` on mismatch.
    fn finalize(&mut self, transaction_id: u32) -> Result<AssemblyOutcome, MFileError> {
        let tx = self
            .active
            .remove(&transaction_id)
            .ok_or(MFileError::TooShort)?;
        self.total_ram_bytes = self.total_ram_bytes.saturating_sub(tx.in_flight_bytes);

        let capacity = usize::try_from(tx.total_size_bytes).unwrap_or(0);
        let mut data: Vec<u8> = Vec::with_capacity(capacity);
        for chunk in tx.chunks.values() {
            data.extend_from_slice(chunk);
        }

        let computed = CRC32.checksum(&data);
        if computed != tx.crc32_full_file {
            log::warn!(
                "MFILE: full-file CRC mismatch tx={transaction_id}: \
                 expected={:#010x} got={:#010x}; persisting partial",
                tx.crc32_full_file,
                computed
            );
            let partial_path = self.partial_path(transaction_id);
            fs::write(&partial_path, &data)?;
            return Ok(AssemblyOutcome::Abandoned {
                transaction_id,
                partial_path,
                gap_count: 0,
            });
        }

        let path = self.complete_path(transaction_id);
        fs::write(&path, &data)?;
        // usize → u64: usize ≤ u64 on all supported targets, never truncates.
        #[allow(clippy::cast_possible_truncation)]
        let bytes = data.len() as u64;
        Ok(AssemblyOutcome::Complete {
            transaction_id,
            path,
            bytes,
        })
    }

    /// Persists received chunks as a `.partial` file, counts gaps, and removes
    /// the transaction from the active table.
    fn abandon(
        &mut self,
        transaction_id: u32,
        reason: &str,
    ) -> Result<AssemblyOutcome, MFileError> {
        let Some(tx) = self.active.remove(&transaction_id) else {
            return Ok(AssemblyOutcome::Abandoned {
                transaction_id,
                partial_path: self.partial_path(transaction_id),
                gap_count: 0,
            });
        };
        self.total_ram_bytes = self.total_ram_bytes.saturating_sub(tx.in_flight_bytes);

        // u32 → usize: u32 fits in usize on all 32-bit and 64-bit targets.
        #[allow(clippy::cast_possible_truncation)]
        let gap_count = tx.total_chunks as usize - tx.received.set_count as usize;
        log::warn!("{reason}: tx={transaction_id} gap_count={gap_count}");

        let mut data: Vec<u8> = Vec::new();
        for chunk in tx.chunks.values() {
            data.extend_from_slice(chunk);
        }
        let partial_path = self.partial_path(transaction_id);
        fs::write(&partial_path, &data)?;

        Ok(AssemblyOutcome::Abandoned {
            transaction_id,
            partial_path,
            gap_count,
        })
    }

    fn complete_path(&self, transaction_id: u32) -> PathBuf {
        self.output_dir.join(format!("{transaction_id}.bin"))
    }

    fn partial_path(&self, transaction_id: u32) -> PathBuf {
        self.output_dir.join(format!("{transaction_id}.partial"))
    }

    /// Evicts the oldest transaction (smallest `timeout_tai_coarse`) when total
    /// in-flight RAM exceeds `max_ram_bytes`.
    fn enforce_ram_cap(&mut self) {
        if self.total_ram_bytes <= self.max_ram_bytes {
            return;
        }
        let oldest_id = self
            .active
            .iter()
            .min_by_key(|(_, tx)| tx.timeout_tai_coarse)
            .map(|(&id, _)| id);

        if let Some(id) = oldest_id {
            log::warn!("STORAGE-PRESSURE: evicting oldest tx={id} to stay within RAM cap");
            if let Err(e) = self.abandon(id, "STORAGE-PRESSURE eviction") {
                log::error!("MFILE: I/O error during RAM eviction of tx={id}: {e}");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Tests.
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::cast_possible_truncation,
    clippy::missing_panics_doc,
    clippy::missing_errors_doc,
    clippy::indexing_slicing
)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    // -----------------------------------------------------------------------
    // Helpers.
    // -----------------------------------------------------------------------

    fn make_assembler(owlt_secs: u64, max_ram_mb: usize) -> (MFileAssembler, TempDir) {
        let dir = TempDir::new().unwrap();
        let asm = MFileAssembler::new(
            Duration::from_secs(owlt_secs),
            dir.path().to_owned(),
            max_ram_mb,
        );
        (asm, dir)
    }

    fn make_frame(
        transaction_id: u32,
        total_size_bytes: u64,
        total_chunks: u32,
        crc32_full_file: u32,
        chunk_index: u32,
        last_chunk_index: u32,
        payload: &[u8],
    ) -> Vec<u8> {
        let chunk_len = payload.len() as u16;
        let crc32_sent = CRC32.checksum(payload);
        let mut frame = Vec::with_capacity(HEADER_LEN + payload.len());
        frame.extend_from_slice(&transaction_id.to_be_bytes());
        frame.extend_from_slice(&total_size_bytes.to_be_bytes());
        frame.extend_from_slice(&total_chunks.to_be_bytes());
        frame.extend_from_slice(&crc32_full_file.to_be_bytes());
        frame.extend_from_slice(&chunk_index.to_be_bytes());
        frame.extend_from_slice(&chunk_len.to_be_bytes());
        frame.extend_from_slice(&last_chunk_index.to_be_bytes());
        frame.extend_from_slice(&crc32_sent.to_be_bytes());
        frame.extend_from_slice(payload);
        frame
    }

    fn single_chunk_frame(transaction_id: u32, payload: &[u8]) -> Vec<u8> {
        let crc32_full = CRC32.checksum(payload);
        make_frame(
            transaction_id,
            payload.len() as u64,
            1,
            crc32_full,
            0,
            0,
            payload,
        )
    }

    // -----------------------------------------------------------------------
    // 1. Single-chunk happy path (Red → Green anchor).
    // -----------------------------------------------------------------------

    #[test]
    fn single_chunk_happy_path() {
        let (mut asm, _dir) = make_assembler(1, 16);
        let payload: &[u8] = b"SAKURA-II surface asset data";
        let frame = single_chunk_frame(1, payload);

        let outcome = asm.on_chunk(&frame).unwrap();
        match outcome.unwrap() {
            AssemblyOutcome::Complete {
                bytes,
                path,
                transaction_id,
            } => {
                assert_eq!(transaction_id, 1);
                assert_eq!(bytes, payload.len() as u64);
                assert!(path.exists());
                assert_eq!(fs::read(&path).unwrap(), payload);
            }
            AssemblyOutcome::Abandoned { .. } => panic!("expected Complete"),
        }
        assert_eq!(asm.active_count(), 0);
    }

    // -----------------------------------------------------------------------
    // 2. Multi-chunk in-order.
    // -----------------------------------------------------------------------

    #[test]
    fn multi_chunk_in_order() {
        let (mut asm, _dir) = make_assembler(1, 16);
        let chunks: &[&[u8]] = &[b"chunk-0-data", b"chunk-1-data", b"chunk-2-data"];
        let full: Vec<u8> = chunks.iter().flat_map(|c| c.iter().copied()).collect();
        let crc32_full = CRC32.checksum(&full);
        let total = chunks.len() as u32;
        let total_size = full.len() as u64;

        for (i, &chunk) in chunks.iter().enumerate() {
            let frame = make_frame(2, total_size, total, crc32_full, i as u32, total - 1, chunk);
            let outcome = asm.on_chunk(&frame).unwrap();
            if i < chunks.len() - 1 {
                assert!(outcome.is_none(), "not complete until last chunk");
            } else {
                match outcome.unwrap() {
                    AssemblyOutcome::Complete { bytes, path, .. } => {
                        assert_eq!(bytes, total_size);
                        assert_eq!(fs::read(&path).unwrap(), full);
                    }
                    AssemblyOutcome::Abandoned { .. } => panic!("expected Complete"),
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // 3. Multi-chunk out-of-order.
    // -----------------------------------------------------------------------

    #[test]
    fn multi_chunk_out_of_order() {
        let (mut asm, _dir) = make_assembler(1, 16);
        let chunks: &[&[u8]] = &[b"alpha", b"beta-", b"gamma"];
        let full: Vec<u8> = chunks.iter().flat_map(|c| c.iter().copied()).collect();
        let crc32_full = CRC32.checksum(&full);
        let total = chunks.len() as u32;
        let total_size = full.len() as u64;

        // Send in reverse order: 2, 1, 0.
        for i in [2u32, 1, 0] {
            let frame = make_frame(
                3,
                total_size,
                total,
                crc32_full,
                i,
                total - 1,
                chunks[i as usize],
            );
            let outcome = asm.on_chunk(&frame).unwrap();
            if i > 0 {
                assert!(outcome.is_none());
            } else {
                match outcome.unwrap() {
                    AssemblyOutcome::Complete { bytes, path, .. } => {
                        assert_eq!(bytes, total_size);
                        assert_eq!(fs::read(&path).unwrap(), full);
                    }
                    AssemblyOutcome::Abandoned { .. } => panic!("expected Complete"),
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // 4. Duplicate chunk — matching content.
    // -----------------------------------------------------------------------

    #[test]
    fn duplicate_chunk_matching_content() {
        let (mut asm, _dir) = make_assembler(1, 16);
        let payload = b"idempotent";
        let crc32_full = CRC32.checksum(payload);
        let frame = make_frame(4, payload.len() as u64, 2, crc32_full, 0, 1, payload);

        asm.on_chunk(&frame).unwrap();
        let result = asm.on_chunk(&frame).unwrap();
        assert!(result.is_none());
        assert_eq!(asm.dup_ok_total(), 1);
        assert_eq!(asm.dup_mismatch_total(), 0);
    }

    // -----------------------------------------------------------------------
    // 5. Duplicate chunk — differing content (first-arrival wins).
    // -----------------------------------------------------------------------

    #[test]
    fn duplicate_chunk_differing_content() {
        let (mut asm, _dir) = make_assembler(1, 16);
        let original = b"first-content-!!";
        let impostor = b"tampered-content";
        let crc32_full = CRC32.checksum(original);

        let frame1 = make_frame(5, original.len() as u64, 2, crc32_full, 0, 1, original);
        let frame2 = make_frame(5, original.len() as u64, 2, crc32_full, 0, 1, impostor);

        asm.on_chunk(&frame1).unwrap();
        let result = asm.on_chunk(&frame2).unwrap();
        assert!(result.is_none());
        assert_eq!(asm.dup_mismatch_total(), 1);
        assert_eq!(asm.dup_ok_total(), 0);

        // First-arrival content must be retained.
        let tx = asm.active.get(&5).unwrap();
        assert_eq!(
            tx.chunks.get(&0).map(Vec::as_slice),
            Some(original.as_slice())
        );
    }

    // -----------------------------------------------------------------------
    // 6. Timeout eviction via poll.
    // -----------------------------------------------------------------------

    #[test]
    fn timeout_eviction_via_poll() {
        let (mut asm, _dir) = make_assembler(1, 16);
        // Two-chunk transfer; only chunk 0 arrives.
        let payload = b"partial-data";
        let frame = make_frame(6, payload.len() as u64 * 2, 2, 0xDEAD_BEEF, 0, 1, payload);
        asm.on_chunk(&frame).unwrap();
        assert_eq!(asm.active_count(), 1);

        // owlt=1 s → timeout_tai_coarse = 10. Advance past it.
        let now = Cuc {
            coarse: 11,
            fine: 0,
        };
        let outcomes = asm.poll(now);
        assert_eq!(outcomes.len(), 1);
        match &outcomes[0] {
            AssemblyOutcome::Abandoned {
                transaction_id,
                partial_path,
                gap_count,
            } => {
                assert_eq!(*transaction_id, 6);
                assert!(partial_path.exists());
                assert_eq!(*gap_count, 1); // chunk 1 never arrived
            }
            AssemblyOutcome::Complete { .. } => panic!("expected Abandoned"),
        }
        assert_eq!(asm.active_count(), 0);
    }

    // -----------------------------------------------------------------------
    // 7. RAM-cap breach — oldest transaction evicted.
    // -----------------------------------------------------------------------

    #[test]
    fn ram_cap_breach_evicts_oldest() {
        // 0 MB cap → any real chunk will exceed it.
        let (mut asm, _dir) = make_assembler(1, 0);

        let p1 = b"first-tx-data";
        let p2 = b"second-tx-data";
        let f1 = make_frame(10, p1.len() as u64, 2, CRC32.checksum(p1), 0, 1, p1);
        let f2 = make_frame(11, p2.len() as u64, 2, CRC32.checksum(p2), 0, 1, p2);

        asm.on_chunk(&f1).unwrap(); // inserts tx=10
        asm.on_chunk(&f2).unwrap(); // inserts tx=11, RAM cap triggers eviction of tx=10

        assert!(!asm.active.contains_key(&10), "oldest must be evicted");
        assert!(asm.active.contains_key(&11), "newest must survive");
    }

    // -----------------------------------------------------------------------
    // 8. raw.len() < 34 → TooShort.
    // -----------------------------------------------------------------------

    #[test]
    fn too_short_header() {
        let (mut asm, _dir) = make_assembler(1, 16);
        let short = [0u8; 10];
        assert!(matches!(asm.on_chunk(&short), Err(MFileError::TooShort)));
    }

    // -----------------------------------------------------------------------
    // 9. chunk_len > MFILE_CHUNK_MAX_BYTES → ChunkLenExceeded.
    // -----------------------------------------------------------------------

    #[test]
    fn chunk_len_exceeded() {
        let (mut asm, _dir) = make_assembler(1, 16);
        let mut frame = vec![0u8; HEADER_LEN];
        let over: u16 = (MFILE_CHUNK_MAX_BYTES + 1) as u16;
        frame[24..26].copy_from_slice(&over.to_be_bytes());
        assert!(matches!(
            asm.on_chunk(&frame),
            Err(MFileError::ChunkLenExceeded { got: 1025 })
        ));
    }

    // -----------------------------------------------------------------------
    // 10. Per-chunk CRC-32 mismatch.
    // -----------------------------------------------------------------------

    #[test]
    fn per_chunk_crc_mismatch() {
        let (mut asm, _dir) = make_assembler(1, 16);
        let payload = b"real data";
        let crc32_full = CRC32.checksum(payload);
        let mut frame = make_frame(20, payload.len() as u64, 1, crc32_full, 0, 0, payload);
        // Corrupt the crc32_sent field (bytes 30..34).
        frame[30..34].copy_from_slice(&0x0BAD_C0DE_u32.to_be_bytes());

        assert!(matches!(
            asm.on_chunk(&frame),
            Err(MFileError::ChunkCrcMismatch { .. })
        ));
    }

    // -----------------------------------------------------------------------
    // 11. Full-file CRC-32 mismatch → Abandoned with .partial file.
    // -----------------------------------------------------------------------

    #[test]
    fn full_file_crc_mismatch() {
        let (mut asm, _dir) = make_assembler(1, 16);
        let payload = b"some data";
        // Intentionally wrong full-file CRC.
        let bad_crc32_full = 0xFFFF_FFFFu32;
        let frame = make_frame(30, payload.len() as u64, 1, bad_crc32_full, 0, 0, payload);

        let outcome = asm.on_chunk(&frame).unwrap();
        match outcome.unwrap() {
            AssemblyOutcome::Abandoned {
                transaction_id,
                partial_path,
                ..
            } => {
                assert_eq!(transaction_id, 30);
                assert!(partial_path.exists());
            }
            AssemblyOutcome::Complete { .. } => panic!("expected Abandoned on CRC mismatch"),
        }
    }

    // -----------------------------------------------------------------------
    // 12. active_count / counter accessors.
    // -----------------------------------------------------------------------

    #[test]
    fn counter_accessors() {
        let (mut asm, _dir) = make_assembler(1, 16);
        assert_eq!(asm.active_count(), 0);
        assert_eq!(asm.dup_ok_total(), 0);
        assert_eq!(asm.dup_mismatch_total(), 0);

        let payload = b"data";
        let frame = make_frame(
            99,
            payload.len() as u64,
            2,
            CRC32.checksum(payload),
            0,
            1,
            payload,
        );
        asm.on_chunk(&frame).unwrap();
        assert_eq!(asm.active_count(), 1);
    }
}
