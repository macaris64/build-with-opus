//! CFDP Class 1 file downlink demo.
//!
//! Simulates a complete CCSDS File Delivery Protocol (CFDP 727.0-B-5)
//! unacknowledged (Class 1) downlink session.
//!
//! ```text
//! SPACECRAFT                            GROUND
//!   │──── Metadata PDU (0x07) ─────────▶│  file name, declared size
//!   │──── File Data PDU (offset=0) ────▶│  chunk 1
//!   │──── File Data PDU (offset=512) ──▶│  chunk 2
//!   │──── EOF PDU (0x04) ──────────────▶│  CRC-32, final size
//!   │                         finalize_transaction()
//!   │                         CRC-32 verified → file on disk
//! ```
//!
//! Also exercises the CRC mismatch error path, out-of-order PDU delivery,
//! and transaction timeout eviction.

use std::time::Duration;

use ccsds_wire::Cuc;
use crc::{Crc, CRC_32_ISO_HDLC};
use ground_station::cfdp::{
    class1::Class1Receiver, CfdpError, CfdpProvider, CfdpReceiver, TransactionId,
    TransactionOutcome,
};

const CRC32: Crc<u32> = Crc::<u32>::new(&CRC_32_ISO_HDLC);

// ── PDU builders (CCSDS 727.0-B-5 §5) ────────────────────────────────────────
//
// entity_id_len=1, seqnum_len=1 → variable header = 3 bytes (src+seq+dst).
// Fixed header byte 3 = 0x00 encodes both lengths as 1.

fn cfdp_header(pdu_type_bit: u8, src: u8, seq: u8) -> Vec<u8> {
    vec![
        (pdu_type_bit & 1) << 4, // byte 0: bit4 = PDU type (0=Directive, 1=FileData)
        0x00,                    // byte 1: data_field_length high (unused by receiver)
        0x00,                    // byte 2: data_field_length low
        0x00,                    // byte 3: entity_id_len-1=0, seqnum_len-1=0
        src,                     // variable header: source entity ID (1 B)
        seq,                     // variable header: transaction sequence number (1 B)
        0x01,                    // variable header: destination entity ID = 1 (ground)
    ]
}

/// Metadata Directive PDU (directive code 0x07, §5.2.5).
fn metadata_pdu(src: u8, seq: u8, file_size: u32, dest_name: &str) -> Vec<u8> {
    let mut p = cfdp_header(0, src, seq);
    p.push(0x07); // directive code
    p.push(0x00); // segmentation control
    p.extend_from_slice(&file_size.to_be_bytes());
    p.push(0x00); // source filename = empty (NUL only)
    p.extend_from_slice(dest_name.as_bytes());
    p.push(0x00); // dest filename NUL terminator
    p
}

/// File Data PDU (PDU type=1, §5.3.2 small-file mode).
fn file_data_pdu(src: u8, seq: u8, offset: u32, data: &[u8]) -> Vec<u8> {
    let mut p = cfdp_header(1, src, seq);
    p.extend_from_slice(&offset.to_be_bytes());
    p.extend_from_slice(data);
    p
}

/// EOF Directive PDU (directive code 0x04, §5.2.4).
fn eof_pdu(src: u8, seq: u8, crc32: u32, file_size: u32) -> Vec<u8> {
    let mut p = cfdp_header(0, src, seq);
    p.push(0x04); // directive code
    p.push(0x00); // condition_code=0 (No error), spare=0
    p.extend_from_slice(&crc32.to_be_bytes());
    p.extend_from_slice(&file_size.to_be_bytes());
    p
}

/// Build a TransactionId the same way Class1Receiver does internally.
fn tx_id(src_entity: u8, seq_num: u8) -> TransactionId {
    TransactionId((u64::from(src_entity) << 32) | u64::from(seq_num))
}

// ── Main ──────────────────────────────────────────────────────────────────────

fn main() {
    println!();
    println!("╔══════════════════════════════════════════════════════════╗");
    println!("║     SAKURA-II CFDP Class 1 File Downlink Demo            ║");
    println!("╠══════════════════════════════════════════════════════════╣");
    println!("║  Metadata → FileData × N → EOF → finalize               ║");
    println!("╚══════════════════════════════════════════════════════════╝");
    println!();

    let output_dir = std::env::temp_dir().join("sakura_cfdp_demo");
    std::fs::create_dir_all(&output_dir).expect("create output dir");

    // OWLT = 5 s; timeout = 10 × 5 = 50 s.
    let mut rx = Class1Receiver::new(Duration::from_secs(5), output_dir.clone());

    // ── Transfer 1: happy path (two-chunk file) ────────────────────────────────
    println!("━━━ TRANSFER 1: Science telemetry file (happy path) ━━━");
    println!();

    let payload: Vec<u8> = (0u16..1024).map(|i| (i & 0xFF) as u8).collect();
    let file_size = payload.len() as u32;
    let crc32 = CRC32.checksum(&payload);
    println!("  File: orbit_dump_001.bin  size={file_size}B  CRC-32=0x{crc32:08X}");
    println!();

    rx.on_pdu(&metadata_pdu(1, 1, file_size, "orbit_dump_001.bin"))
        .expect("Metadata PDU");
    println!("  [RX] Metadata PDU  src=1 seq=1  declared={file_size}B  dest=orbit_dump_001.bin");
    println!(
        "       Active transactions: {}",
        rx.active_transactions().len()
    );

    let (chunk1, chunk2) = payload.split_at(512);
    rx.on_pdu(&file_data_pdu(1, 1, 0, chunk1))
        .expect("FileData 1");
    println!("  [RX] File Data PDU  offset=0    len=512B");
    rx.on_pdu(&file_data_pdu(1, 1, 512, chunk2))
        .expect("FileData 2");
    println!("  [RX] File Data PDU  offset=512  len=512B");

    rx.on_pdu(&eof_pdu(1, 1, crc32, file_size))
        .expect("EOF PDU");
    println!("  [RX] EOF PDU  crc32=0x{crc32:08X}  file_size={file_size}B");
    println!();

    let id1 = tx_id(1, 1);
    match rx.finalize_transaction(id1).expect("finalize") {
        TransactionOutcome::Completed { path, bytes, .. } => {
            let on_disk = std::fs::read(&path).expect("read assembled file");
            println!("  ✓ COMPLETE  path={}  bytes={bytes}", path.display());
            println!("    Byte-for-byte match: {}", on_disk == payload);
        }
        TransactionOutcome::Abandoned { reason, .. } => {
            println!("  ✗ ABANDONED: {reason}");
        }
    }
    println!();

    // ── Transfer 2: CRC mismatch (bit error on downlink) ──────────────────────
    println!("━━━ TRANSFER 2: Corrupt downlink (CRC mismatch) ━━━");
    println!();

    let data2 = b"orbital parameters block";
    let correct_crc = CRC32.checksum(data2.as_ref());
    let wrong_crc: u32 = 0xDEAD_BEEF;
    println!("  Correct CRC: 0x{correct_crc:08X}  Declared (corrupt): 0x{wrong_crc:08X}");

    rx.on_pdu(&metadata_pdu(2, 1, data2.len() as u32, "orb_params.bin"))
        .expect("Metadata");
    rx.on_pdu(&file_data_pdu(2, 1, 0, data2.as_ref()))
        .expect("FileData");
    rx.on_pdu(&eof_pdu(2, 1, wrong_crc, data2.len() as u32))
        .expect("EOF with wrong CRC");

    let id2 = tx_id(2, 1);
    match rx.finalize_transaction(id2) {
        Err(CfdpError::CrcMismatch(_)) => {
            let partial = output_dir.join(format!("{}.partial", id2.0));
            println!("  ✓ CrcMismatch detected — .partial file retained for forensics");
            println!("    path: {}", partial.display());
        }
        other => println!("  Unexpected: {other:?}"),
    }
    println!();

    // ── Transfer 3: out-of-order PDU delivery ──────────────────────────────────
    println!("━━━ TRANSFER 3: Out-of-order PDU delivery ━━━");
    println!();

    let content = b"ADCS wheel speed log v1.0\n";
    let crc3 = CRC32.checksum(content.as_ref());
    // FileData can precede Metadata — the receiver opens the transaction on
    // first FileData and accepts Metadata whenever it arrives.
    println!("  Delivering: FileData → Metadata → EOF  (Metadata late)");

    rx.on_pdu(&file_data_pdu(3, 1, 0, content.as_ref()))
        .expect("FileData first");
    println!(
        "  [RX] File Data PDU  offset=0  len={}B  (opens transaction)",
        content.len()
    );

    rx.on_pdu(&metadata_pdu(3, 1, content.len() as u32, "wheel_log.bin"))
        .expect("Metadata late");
    println!("  [RX] Metadata PDU  (arrived after FileData — receiver merges)");

    rx.on_pdu(&eof_pdu(3, 1, crc3, content.len() as u32))
        .expect("EOF last");
    println!("  [RX] EOF PDU");
    println!();

    let id3 = tx_id(3, 1);
    match rx.finalize_transaction(id3).expect("finalize out-of-order") {
        TransactionOutcome::Completed { path, bytes, .. } => {
            println!(
                "  ✓ COMPLETE despite reordering  bytes={bytes}  path={}",
                path.display()
            );
        }
        TransactionOutcome::Abandoned { reason, .. } => {
            println!("  ✗ ABANDONED: {reason}");
        }
    }
    println!();

    // ── Transfer 4: timeout eviction ──────────────────────────────────────────
    println!("━━━ TRANSFER 4: Incomplete transfer — timeout eviction ━━━");
    println!();

    let mut rx2 = Class1Receiver::new(Duration::from_secs(1), output_dir.clone());
    rx2.on_pdu(&metadata_pdu(9, 1, 256, "lost.bin"))
        .expect("Metadata");
    println!("  Opened transfer (owlt=1s → timeout=10s); no FileData sent.");
    println!("  Active transactions: {}", rx2.active_transactions().len());

    // TAI coarse=11: age = 11-0 = 11 > 10 s → eviction
    let outcomes = rx2.poll(Cuc {
        coarse: 11,
        fine: 0,
    });
    println!("  poll(tai_coarse=11): {} eviction(s)", outcomes.len());
    for o in outcomes {
        match o {
            TransactionOutcome::Abandoned {
                id,
                reason,
                bytes_received,
            } => {
                println!(
                    "  ✓ tx={id:?} abandoned — reason: {reason}  bytes_received={bytes_received}"
                );
            }
            TransactionOutcome::Completed { .. } => {}
        }
    }
    println!("  Active after poll: {}", rx2.active_transactions().len());
    println!();

    let _ = std::fs::remove_dir_all(&output_dir);

    let transfers_completed: usize = 4;
    println!("Demo complete — {transfers_completed} CFDP scenarios exercised.");
    println!();
}
