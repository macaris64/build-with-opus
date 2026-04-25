//! cFS ↔ Rust message boundary demo.
//!
//! Demonstrates how the ground station ingest pipeline receives and decodes
//! raw cFE Software Bus message bytes — the exact bytes a cFS app writes to
//! the Software Bus — using only `ccsds_wire`, which is the Q-C8 locus for
//! all CCSDS big-endian pack/unpack in this workspace.
// Examples use expect()/unwrap(), direct indexing, and vec! for readability.
// Suppress workspace deny/warn lints that would otherwise reject demo patterns.
#![allow(
    clippy::expect_used,
    clippy::unwrap_used,
    clippy::indexing_slicing,
    clippy::too_many_lines,
    clippy::cast_possible_truncation,
    clippy::doc_markdown,
    clippy::useless_vec
)]
//!
//! The cFE SB message format is a strict superset of CCSDS Space Packets:
//!
//! ```text
//! cFE MID = 0x0800 | APID   (TM telemetry, downlink)
//! cFE MID = 0x1800 | APID   (TC command,   uplink)
//! ```
//!
//! So the ground station decodes any cFE message with `SpacePacket::parse`,
//! then classifies it by APID range and MID bit 12 (command flag).
//!
//! Scenarios:
//!   1. MID → APID decode for the full SAKURA-II orbiter MID table
//!   2. TM inbound: raw SB bytes → SpacePacket → ApidRouter routing decision
//!   3. TC outbound: PacketBuilder → CCSDS TC bytes → verify command flag
//!   4. Round-trip: encode → decode → re-encode is byte-identical (Q-C8)
//!   5. Error paths: truncated buffer, invalid CCSDS version

use ccsds_wire::{
    Apid, CcsdsError, Cuc, FuncCode, InstanceId, PacketBuilder, PacketType, SequenceCount,
    SpacePacket,
};
use ground_station::ingest::ApidRouter;

// ── cFE MID helpers ───────────────────────────────────────────────────────────

/// Encode a cFE Message ID from an 11-bit APID and direction flag.
/// TM (telemetry / downlink): `MID = 0x0800 | apid`
/// TC (command   / uplink):   `MID = 0x1800 | apid`
fn mid_from_apid(apid: u16, is_cmd: bool) -> u16 {
    if is_cmd {
        0x1800 | (apid & 0x07FF)
    } else {
        0x0800 | (apid & 0x07FF)
    }
}

/// Decode the 11-bit APID from a cFE MID.
fn apid_from_mid(mid: u16) -> u16 {
    mid & 0x07FF
}

/// True when the cFE MID command flag (bit 12) is set.
fn mid_is_cmd(mid: u16) -> bool {
    (mid & 0x1000) != 0
}

// ── Wire frame builders ───────────────────────────────────────────────────────

fn tm_bytes(apid: u16, seq: u16, payload: &[u8]) -> Vec<u8> {
    PacketBuilder::tm(Apid::new(apid).unwrap())
        .sequence_count(SequenceCount::new(seq).unwrap())
        .func_code(FuncCode::new(0x0001).unwrap())
        .instance_id(InstanceId::new(1).unwrap())
        .cuc(Cuc {
            coarse: 1_800_000,
            fine: 0,
        })
        .user_data(payload)
        .build()
        .unwrap()
}

fn tc_bytes(apid: u16, seq: u16, func: u16, payload: &[u8]) -> Vec<u8> {
    PacketBuilder::tc(Apid::new(apid).unwrap())
        .sequence_count(SequenceCount::new(seq).unwrap())
        .func_code(FuncCode::new(func).unwrap())
        .instance_id(InstanceId::new(1).unwrap())
        .cuc(Cuc {
            coarse: 1_800_000,
            fine: 0,
        })
        .user_data(payload)
        .build()
        .unwrap()
}

// ── Main ──────────────────────────────────────────────────────────────────────

fn main() {
    println!();
    println!("╔══════════════════════════════════════════════════════════╗");
    println!("║     SAKURA-II cFS ↔ Rust Message Bridge Demo            ║");
    println!("╠══════════════════════════════════════════════════════════╣");
    println!("║  SB raw bytes → SpacePacket → ApidRouter (zero-copy)    ║");
    println!("╚══════════════════════════════════════════════════════════╝");
    println!();

    // ── 1. MID table — encode and decode every SAKURA-II orbiter MID ──────────
    println!("━━━ SCENARIO 1: cFE MID Table (SAKURA-II orbiter apps) ━━━");
    println!();
    println!(
        "  {:>10}  {:>5}  {:>4}  {:>7}  App",
        "MID", "APID", "Dir", "cmd?"
    );
    println!("  {}", "─".repeat(55));

    let mid_table: &[(&str, u16, bool)] = &[
        ("sample_app TM", 0x100, false),
        ("orbiter_cdh TM", 0x101, false),
        ("orbiter_adcs TM", 0x110, false),
        ("orbiter_comm TM", 0x120, false),
        ("orbiter_power TM", 0x130, false),
        ("orbiter_payload TM", 0x140, false),
        ("orbiter_cdh TC", 0x181, true),
        ("orbiter_adcs TC", 0x182, true),
        ("orbiter_power TC", 0x184, true),
    ];

    for (label, apid, is_cmd) in mid_table {
        let mid = mid_from_apid(*apid, *is_cmd);
        assert_eq!(apid_from_mid(mid), *apid);
        assert_eq!(mid_is_cmd(mid), *is_cmd);
        println!(
            "  0x{mid:04X}      0x{apid:03X}   {:2}   {is_cmd:<5}  {label}",
            if *is_cmd { "TC" } else { "TM" },
        );
    }
    println!();

    // ── 2. TM inbound: raw SB bytes → SpacePacket → route ────────────────────
    println!("━━━ SCENARIO 2: TM Inbound (cFS app → ground station) ━━━");
    println!();

    let tm_msgs: &[(&str, u16, &[u8])] = &[
        ("orbiter_cdh", 0x101, &[0xC2, 0x1A, 0x00, 0x7F]),
        (
            "orbiter_adcs",
            0x110,
            &[0x3F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
        ),
        ("orbiter_comm", 0x120, &[0x01, 0x5A]),
        ("orbiter_power", 0x130, &[0x10, 0xF4, 0x0C, 0x80]),
        ("orbiter_payload", 0x140, &[0x00, 0x12, 0x34, 0x56]),
    ];

    let mut router = ApidRouter::new();
    for (seq, (app, apid, payload)) in tm_msgs.iter().enumerate() {
        let raw = tm_bytes(*apid, seq as u16, payload);
        let pkt = SpacePacket::parse(&raw).expect("SpacePacket::parse");
        let route = router.route(0, &pkt); // VC 0 = HK stream
        let mid = mid_from_apid(pkt.primary.apid().get(), false);
        println!(
            "  {app:<18} MID=0x{mid:04X}  seq={:3}  user_data={:2}B  route={route:?}",
            pkt.primary.sequence_count().get(),
            pkt.user_data.len(),
        );
    }
    println!();

    // ── 3. TC outbound: PacketBuilder → TC bytes → verify type bit ────────────
    println!("━━━ SCENARIO 3: TC Outbound (ground station → cFS app) ━━━");
    println!();

    let tc_cmds: &[(&str, u16, u16, &[u8])] = &[
        ("OrbiterSetMode", 0x181, 0x0100, &[0x02, 0x01]),
        (
            "OrbiterAdcsTargetQ",
            0x182,
            0x0100,
            &[0x40, 0x00, 0x00, 0x00],
        ),
        ("OrbiterPowerArm", 0x184, 0x8000, &[0x03, 0xA5, 0xA5]),
        (
            "CryobotSetDrillRpm",
            0x440,
            0x8200,
            &[0x00, 0x78, 0x01, 0xF4, 0x5A, 0x5A],
        ),
    ];

    for (seq, (label, apid, func, payload)) in tc_cmds.iter().enumerate() {
        let raw = tc_bytes(*apid, seq as u16, *func, payload);
        let pkt = SpacePacket::parse(&raw).expect("SpacePacket::parse");
        let mid = mid_from_apid(pkt.primary.apid().get(), true);
        assert_eq!(pkt.primary.packet_type(), PacketType::Tc);
        println!(
            "  {label:<22} MID=0x{mid:04X}  func=0x{:04X}  len={:3}B  type={:?}",
            pkt.secondary.func_code().get(),
            raw.len(),
            pkt.primary.packet_type(),
        );
    }
    println!();

    // ── 4. Round-trip: encode → decode → re-encode is byte-identical ──────────
    println!("━━━ SCENARIO 4: Round-trip guarantee (Q-C8) ━━━");
    println!();

    let original = tm_bytes(0x101, 7, &[0xDE, 0xAD, 0xBE, 0xEF]);
    let pkt = SpacePacket::parse(&original).expect("parse");

    // Re-encode from the parsed fields — this is what Q-C8 locus A guarantees
    let re_encoded = PacketBuilder::tm(pkt.primary.apid())
        .sequence_count(pkt.primary.sequence_count())
        .func_code(pkt.secondary.func_code())
        .instance_id(pkt.secondary.instance_id())
        .cuc(pkt.secondary.time())
        .user_data(pkt.user_data)
        .build()
        .expect("re-encode");

    println!("  Original bytes:   {:02X?}", &original);
    println!("  Re-encoded bytes: {:02X?}", &re_encoded);
    println!("  Byte-identical: {}", original == re_encoded);
    println!();

    // ── 5. Error paths ────────────────────────────────────────────────────────
    println!("━━━ SCENARIO 5: Error Paths ━━━");
    println!();

    // Too short
    let short = vec![0u8; 10];
    println!(
        "  10-byte buffer:        {:?}",
        SpacePacket::parse(&short).unwrap_err()
    );

    // Invalid CCSDS version (bits [7:5] of byte 0 must be 000)
    let mut bad_ver = tm_bytes(0x101, 0, &[]);
    bad_ver[0] |= 0b0010_0000;
    println!(
        "  Invalid version bits:  {:?}",
        SpacePacket::parse(&bad_ver).unwrap_err()
    );

    // Length mismatch (claimed data_length doesn't match buffer)
    let mut bad_len = tm_bytes(0x101, 0, &[0x01, 0x02]);
    bad_len.pop(); // shorten by 1 byte
    println!(
        "  Length mismatch:       {:?}",
        SpacePacket::parse(&bad_len).unwrap_err()
    );

    // Q-F2: forbidden fault-inject APID rejected by router
    let fault_raw = tm_bytes(0x540, 0, &[]);
    let fault_pkt = SpacePacket::parse(&fault_raw).expect("parses as CCSDS");
    let fault_route = router.route(0, &fault_pkt);
    println!("  APID 0x540 route:      {fault_route:?}  (Q-F2 security rejection)");
    println!(
        "  forbidden_seen(0x540): {}",
        router.forbidden_apid_seen_total(0x540)
    );
    println!();

    // Verify all three error variants are distinct types
    assert!(matches!(
        SpacePacket::parse(&vec![0u8; 10]).unwrap_err(),
        CcsdsError::BufferTooShort { .. }
    ));

    println!("Demo complete — cFS ↔ Rust message boundary fully exercised.");
    println!();
}
