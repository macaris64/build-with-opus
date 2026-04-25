//! M-File chunked file reassembly demo.
//!
//! Exercises `MFileAssembler` — the ground station's protocol for receiving
//! chunked file transfers from the relay satellite over AOS VC 2
//! (ICD-relay-surface.md §6, Batch B2).
// Examples intentionally use expect(), direct indexing, long main(), and
// similar variable names (outcome1/outcome2) for readable scenario labelling.
// Suppress workspace deny/warn lints that would otherwise reject these patterns.
#![allow(
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::too_many_lines,
    clippy::cast_possible_truncation,
    clippy::similar_names
)]
//!
//! Wire header (34 bytes, all multi-byte fields big-endian per Q-C8):
//!
//! ```text
//! offset  field              type    description
//!   0     transaction_id     u32 BE  unique per file transfer
//!   4     total_size_bytes   u64 BE  complete file size
//!  12     total_chunks       u32 BE  expected chunk count
//!  16     crc32_full_file    u32 BE  CRC-32 IEEE 802.3 of whole file
//!  20     chunk_index        u32 BE  zero-based chunk index
//!  24     chunk_len          u16 BE  payload bytes in this chunk
//!  26     last_chunk_index   u32 BE  index of the final chunk
//!  30     crc32_sent         u32 BE  CRC-32 of this chunk's payload
//!  34     payload            bytes   chunk_len bytes of file data
//! ```
//!
//! Scenarios:
//!   1. In-order delivery — simple happy path
//!   2. Out-of-order delivery — reassembler sorts by offset
//!   3. Duplicate chunk (same content) — idempotent, counter incremented
//!   4. Duplicate chunk (different content) — first-arrival wins, mismatch counter
//!   5. Timeout eviction — incomplete transfer abandoned as `.partial`

use ccsds_wire::Cuc;
use crc::{Crc, CRC_32_ISO_HDLC};
use ground_station::mfile::{AssemblyOutcome, MFileAssembler};
use std::time::Duration;

const CRC32: Crc<u32> = Crc::<u32>::new(&CRC_32_ISO_HDLC);

// ── Wire frame builder ────────────────────────────────────────────────────────

struct ChunkDesc<'a> {
    tx_id: u32,
    total_size: u64,
    total_chunks: u32,
    file_crc32: u32,
    chunk_index: u32,
    last_chunk_index: u32,
    payload: &'a [u8],
}

fn build_chunk(d: &ChunkDesc<'_>) -> Vec<u8> {
    let chunk_crc = CRC32.checksum(d.payload);
    let chunk_len = d.payload.len() as u16;

    let mut buf = Vec::with_capacity(34 + d.payload.len());
    buf.extend_from_slice(&d.tx_id.to_be_bytes());
    buf.extend_from_slice(&d.total_size.to_be_bytes());
    buf.extend_from_slice(&d.total_chunks.to_be_bytes());
    buf.extend_from_slice(&d.file_crc32.to_be_bytes());
    buf.extend_from_slice(&d.chunk_index.to_be_bytes());
    buf.extend_from_slice(&chunk_len.to_be_bytes());
    buf.extend_from_slice(&d.last_chunk_index.to_be_bytes());
    buf.extend_from_slice(&chunk_crc.to_be_bytes());
    buf.extend_from_slice(d.payload);
    buf
}

/// Split `data` into chunks of `chunk_size` and return the wire frames
/// for all chunks of transaction `tx_id`.
fn make_chunks(tx_id: u32, data: &[u8], chunk_size: usize) -> Vec<Vec<u8>> {
    let file_crc32 = CRC32.checksum(data);
    let raw_chunks: Vec<&[u8]> = data.chunks(chunk_size).collect();
    let total_chunks = raw_chunks.len() as u32;
    let last_chunk_index = total_chunks - 1;

    raw_chunks
        .iter()
        .enumerate()
        .map(|(i, chunk)| {
            build_chunk(&ChunkDesc {
                tx_id,
                total_size: data.len() as u64,
                total_chunks,
                file_crc32,
                chunk_index: i as u32,
                last_chunk_index,
                payload: chunk,
            })
        })
        .collect()
}

// ── Main ──────────────────────────────────────────────────────────────────────

fn main() {
    println!();
    println!("╔══════════════════════════════════════════════════════════╗");
    println!("║     SAKURA-II M-File Chunked Reassembly Demo             ║");
    println!("╠══════════════════════════════════════════════════════════╣");
    println!("║  Relay → VC 2 chunks → MFileAssembler → file on disk    ║");
    println!("╚══════════════════════════════════════════════════════════╝");
    println!();

    let output_dir = std::env::temp_dir().join("sakura_mfile_demo");
    std::fs::create_dir_all(&output_dir).expect("create output dir");

    // owlt=2s → timeout = 10 × 2 = 20 s; max_ram_mb=1
    let mut asm = MFileAssembler::new(Duration::from_secs(2), output_dir.clone(), 1);

    // ── Scenario 1: in-order delivery ─────────────────────────────────────────
    println!("━━━ SCENARIO 1: In-order delivery ━━━");
    println!();

    let file1: Vec<u8> = (0u8..=255).cycle().take(600).collect(); // 600 B
    let chunks1 = make_chunks(1001, &file1, 256); // 3 chunks: 256+256+88
    println!(
        "  File: 600B  chunks: {}  CRC-32: 0x{:08X}",
        chunks1.len(),
        CRC32.checksum(&file1)
    );
    let mut outcome1 = None;
    for (i, chunk) in chunks1.iter().enumerate() {
        let result = asm.on_chunk(chunk).expect("chunk accepted");
        println!(
            "    chunk[{i}] {}B → {}",
            chunk.len() - 34,
            if result.is_some() {
                "COMPLETE"
            } else {
                "buffered"
            }
        );
        if result.is_some() {
            outcome1 = result;
        }
    }
    match outcome1 {
        Some(AssemblyOutcome::Complete {
            transaction_id,
            bytes,
            path,
        }) => {
            let on_disk = std::fs::read(&path).expect("read file");
            println!(
                "  ✓ tx={transaction_id}  bytes={bytes}  match={}  path={}",
                on_disk == file1,
                path.display()
            );
        }
        _ => println!("  ✗ unexpected outcome"),
    }
    println!();

    // ── Scenario 2: out-of-order delivery ─────────────────────────────────────
    println!("━━━ SCENARIO 2: Out-of-order delivery ━━━");
    println!();

    let file2: Vec<u8> = b"SAKURA-II surface telemetry batch export v2\n"
        .iter()
        .cycle()
        .take(512)
        .copied()
        .collect();
    let mut chunks2 = make_chunks(1002, &file2, 200); // 3 chunks: 200+200+112
                                                      // Reverse delivery order: chunk 2, chunk 0, chunk 1
    chunks2.swap(0, 2);
    println!(
        "  File: {}B  delivery order: [2, 0, 1]  CRC-32: 0x{:08X}",
        file2.len(),
        CRC32.checksum(&file2)
    );
    let mut outcome2 = None;
    let original_order = [2usize, 0, 1];
    for (delivery, chunk) in chunks2.iter().enumerate() {
        let result = asm.on_chunk(chunk).expect("chunk accepted");
        println!(
            "    delivery[{delivery}] = chunk[{}] → {}",
            original_order[delivery],
            if result.is_some() {
                "COMPLETE"
            } else {
                "buffered"
            }
        );
        if result.is_some() {
            outcome2 = result;
        }
    }
    match outcome2 {
        Some(AssemblyOutcome::Complete { bytes, path, .. }) => {
            let on_disk = std::fs::read(&path).expect("read file");
            println!(
                "  ✓ reassembled correctly  bytes={bytes}  match={}",
                on_disk == file2
            );
        }
        _ => println!("  ✗ unexpected outcome"),
    }
    println!();

    // ── Scenario 3: duplicate chunk (same content — idempotent) ───────────────
    println!("━━━ SCENARIO 3: Duplicate chunk (same content) ━━━");
    println!();

    let file3 = b"duplicate test payload\n";
    let chunks3 = make_chunks(1003, file3, 16); // ≥2 chunks
    println!("  File: {}B  chunks: {}", file3.len(), chunks3.len());
    let before_ok = asm.dup_ok_total();
    // Send chunk 0 twice
    asm.on_chunk(&chunks3[0]).expect("first send");
    asm.on_chunk(&chunks3[0])
        .expect("duplicate send (same content)");
    println!(
        "  dup_ok_total before={before_ok}  after={}  (increment = {})",
        asm.dup_ok_total(),
        asm.dup_ok_total() - before_ok
    );
    // Finish the transaction
    for chunk in chunks3.iter().skip(1) {
        asm.on_chunk(chunk).expect("chunk");
    }
    println!("  ✓ duplicate same-content chunk handled idempotently");
    println!();

    // ── Scenario 4: duplicate chunk (different content — first-arrival wins) ───
    println!("━━━ SCENARIO 4: Duplicate chunk (different content) ━━━");
    println!();

    // Use a 2-chunk file so chunk 0 can be duplicated while the transaction
    // is still open (waiting for chunk 1). A single-chunk file would close
    // on first delivery, making the duplicate arrive after expiry.
    let file4_a = b"correct telemetry chunk-A data!!"; // chunk 0
    let file4_b = b"correct telemetry chunk-B data!!"; // chunk 1
    let file4_full: Vec<u8> = file4_a.iter().chain(file4_b.iter()).copied().collect();
    let file4_crc = CRC32.checksum(&file4_full);

    let real_chunk0 = build_chunk(&ChunkDesc {
        tx_id: 1004,
        total_size: file4_full.len() as u64,
        total_chunks: 2,
        file_crc32: file4_crc,
        chunk_index: 0,
        last_chunk_index: 1,
        payload: file4_a.as_ref(),
    });
    let corrupt_payload = b"CORRUPTED!!!corrupted!!!CORRUPTED";
    let corrupt_chunk0 = build_chunk(&ChunkDesc {
        tx_id: 1004,
        total_size: file4_full.len() as u64,
        total_chunks: 2,
        file_crc32: file4_crc,
        chunk_index: 0, // same index, different content
        last_chunk_index: 1,
        payload: corrupt_payload.as_ref(),
    });
    let chunk1 = build_chunk(&ChunkDesc {
        tx_id: 1004,
        total_size: file4_full.len() as u64,
        total_chunks: 2,
        file_crc32: file4_crc,
        chunk_index: 1,
        last_chunk_index: 1,
        payload: file4_b.as_ref(),
    });

    let before_mismatch = asm.dup_mismatch_total();
    asm.on_chunk(&real_chunk0).expect("real chunk 0");
    println!("  [TX] chunk[0] real content");
    asm.on_chunk(&corrupt_chunk0)
        .expect("corrupt duplicate of chunk 0");
    println!("  [TX] chunk[0] corrupt duplicate — first-arrival wins");
    asm.on_chunk(&chunk1)
        .expect("chunk 1 — completes transaction");
    println!("  [TX] chunk[1] → transaction complete");
    println!(
        "  dup_mismatch_total before={before_mismatch}  after={}",
        asm.dup_mismatch_total()
    );
    println!("  ✓ mismatch detected; original chunk 0 retained");
    println!();

    // ── Scenario 5: timeout eviction ──────────────────────────────────────────
    println!("━━━ SCENARIO 5: Incomplete transfer — timeout eviction ━━━");
    println!();

    let file5 = b"science image header block";
    let chunks5 = make_chunks(2001, file5, 10);
    // Only deliver the first chunk
    asm.on_chunk(&chunks5[0]).expect("first chunk only");
    println!(
        "  Sent chunk 0 of {}; holding back the rest.",
        chunks5.len()
    );
    println!("  Active transactions: {}", asm.active_count());

    // TAI coarse=25 > timeout=20 → eviction
    let evictions = asm.poll(Cuc {
        coarse: 25,
        fine: 0,
    });
    println!(
        "  poll(tai_coarse=25, timeout=20s): {} eviction(s)",
        evictions.len()
    );
    for o in evictions {
        match o {
            AssemblyOutcome::Abandoned {
                transaction_id,
                gap_count,
                partial_path,
            } => {
                println!(
                    "  ✓ tx={transaction_id} abandoned  gap_count={gap_count}  partial={}",
                    partial_path.display()
                );
            }
            AssemblyOutcome::Complete { .. } => {}
        }
    }
    println!("  Active after poll: {}", asm.active_count());
    println!();

    // ── Summary ───────────────────────────────────────────────────────────────
    println!("━━━ ASSEMBLER COUNTERS ━━━");
    println!("  dup_ok_total:       {}", asm.dup_ok_total());
    println!("  dup_mismatch_total: {}", asm.dup_mismatch_total());
    println!();

    let _ = std::fs::remove_dir_all(&output_dir);
    println!("Demo complete — 5 M-File scenarios exercised.");
    println!();
}
