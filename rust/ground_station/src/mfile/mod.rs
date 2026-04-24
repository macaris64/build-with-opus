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

// ---------------------------------------------------------------------------
// M-File protocol constants (§7.3).
// ---------------------------------------------------------------------------

/// Maximum chunk payload in bytes (matches [`crate::cfdp::CFDP_SEGMENT_SIZE_BYTES`]
/// to allow shared reassembly buffers where CFDP and M-File coexist on VC 2).
pub const MFILE_CHUNK_MAX_BYTES: usize = 1_024;

/// Timeout multiplier over one-way light time before a transaction is evicted
/// as incomplete (§7.3).
pub const MFILE_TIMEOUT_OWLT_MULT: u32 = 10;
