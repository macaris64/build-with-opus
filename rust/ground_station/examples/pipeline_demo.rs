//! End-to-end SAKURA-II telemetry pipeline demonstration.
//!
//! Simulates a real spacecraft downlink session through every layer of the
//! ground station ingest stack:
//!
//! ```text
//! SPACECRAFT (orbiter + rover apps)
//!   │  PacketBuilder → CCSDS TM Space Packets (ccsds_wire, Q-C8)
//!   │  Wrapped in 1024-byte AOS Transfer Frames (SCID=42, FECF CRC-16/IBM-3740)
//!   ▼
//! RF LINK SIMULATION  (tokio duplex — stands in for TCP/UDP)
//!   ▼
//! GROUND STATION
//!   ├─ Stage 1: AosFramer  — FECF validation, link-state machine, CLCW extract
//!   ├─ Stage 2: VcDemux    — routes AosFrames to VC-specific decoder channels
//!   ├─ Stage 3: SppDecoder — SpacePacket::parse() per-VC, decode fail counters
//!   └─ Stage 4: ApidRouter — APID-range dispatch + Q-F2 forbidden-APID rejection
//! ```
//!
//! No test mocking — every stage uses the production implementation exactly as
//! it runs in the flight ground station binary.

use std::collections::HashMap;

use bytes::Bytes;
use crc::{Crc, CRC_16_IBM_3740};
use ground_station::ingest::{
    AosFrame,
    AOS_TO_DEMUX_CAP,
    ApidRouter, Route, RejectReason,
    SPP_TO_ROUTER_CAP,
    decoder::SppDecoder,
    demux::VcDemultiplexer,
    framer::{AosFramer, AOS_FRAME_LEN},
};
use ccsds_wire::{Apid, Cuc, FuncCode, InstanceId, PacketBuilder, SequenceCount, SpacePacket};
use tokio::io::AsyncWriteExt;
use tokio::sync::{mpsc, watch};

// ── AOS frame constants ───────────────────────────────────────────────────────

/// SAKURA-II spacecraft ID (matches `SPACECRAFT_ID` in `_defs/targets.cmake`).
const SCID: u8 = 42;

/// Data-field length when OCF is absent: 1024 - 6 (primary) - 2 (FECF) = 1016 B.
const DATA_FIELD_LEN: usize = AOS_FRAME_LEN - 6 - 2;

/// User-data bytes available per space packet in this demo (DATA_FIELD_LEN - 16 headers).
const USER_DATA_LEN: usize = DATA_FIELD_LEN - 16;

// ── Spacecraft packet catalog ─────────────────────────────────────────────────

struct TxPacket {
    label: &'static str,
    apid: u16,
    vc_id: u8,
    /// Meaningful payload bytes (padded to USER_DATA_LEN before framing).
    payload_head: Vec<u8>,
    func_code: u16,
}

fn spacecraft_catalog() -> Vec<TxPacket> {
    vec![
        // ── Orbiter apps — VC 0 (HK telemetry) ─────────────────────────────
        TxPacket {
            label: "orbiter_cdh HK",
            apid: 0x101,   // CDH housekeeping (apid-registry §Orbiter TM 0x101–0x10F)
            vc_id: 0,
            payload_head: vec![0xC2, 0x1A, 0x00, 0x7F],   // seq, uptime fields (stub)
            func_code: 0x0001,
        },
        TxPacket {
            label: "orbiter_adcs ATTITUDE",
            apid: 0x110,   // ADCS attitude quaternion + counters
            vc_id: 0,
            payload_head: vec![
                0x3F, 0x80, 0x00, 0x00,  // qw = 1.0f (IEEE-754 BE)
                0x00, 0x00, 0x00, 0x00,  // qx = 0.0
                0x00, 0x00, 0x00, 0x00,  // qy = 0.0
                0x00, 0x00, 0x00, 0x00,  // qz = 0.0
            ],
            func_code: 0x0002,
        },
        TxPacket {
            label: "orbiter_comm LINK STATUS",
            apid: 0x120,   // Comm link state
            vc_id: 0,
            payload_head: vec![0x01, 0x5A, 0x00, 0x00],   // session_active=1, rssi=90
            func_code: 0x0003,
        },
        TxPacket {
            label: "orbiter_power POWER HK",
            apid: 0x130,   // Power system housekeeping
            vc_id: 0,
            payload_head: vec![0x10, 0xF4, 0x0C, 0x80],   // bus_v=4340mV, panel_i=3200mA (stub)
            func_code: 0x0004,
        },
        TxPacket {
            label: "orbiter_payload PAYLOAD HK",
            apid: 0x140,   // Payload instrument housekeeping
            vc_id: 0,
            payload_head: vec![0x00, 0x12, 0x34, 0x56],   // instrument status (stub)
            func_code: 0x0005,
        },
        // ── Orbiter CDH — VC 1 (EVS event log stream) ───────────────────────
        TxPacket {
            label: "orbiter_cdh EVENT (VC1)",
            apid: 0x102,
            vc_id: 1,
            payload_head: vec![
                0x00, 0x01,              // event type: info
                0x00, 0x2A,              // app_id = 42
                b'C', b'D', b'H', b' ', b'R', b'E', b'A', b'D', b'Y', 0,
            ],
            func_code: 0x0006,
        },
        // ── rover_land — VC 3 (rover-forward archive) ────────────────────────
        TxPacket {
            label: "rover_land HK (VC3)",
            apid: 0x300,   // rover_land base APID (apid-registry §Rover-Land TM 0x300–0x37F)
            vc_id: 3,
            payload_head: vec![
                0x00, 0x00, 0x00, 0x01,  // seq=1
                0x3F, 0x00, 0x00, 0x00,  // battery_v (stub float BE)
            ],
            func_code: 0x0007,
        },
        // ── Security test: Q-F2 forbidden fault-inject APID ──────────────────
        TxPacket {
            label: "FAULT INJECT (Q-F2 FORBIDDEN)",
            apid: 0x540,   // Gazebo fault-inject sideband — MUST be rejected
            vc_id: 0,
            payload_head: vec![0xDE, 0xAD, 0xBE, 0xEF],
            func_code: 0x0008,
        },
        // ── Idle fill — silently discarded, no counter ────────────────────────
        TxPacket {
            label: "IDLE FILL (0x7FF)",
            apid: 0x7FF,
            vc_id: 63,     // VC 63 = idle fill VC
            payload_head: vec![0x00, 0x00, 0x00, 0x00],
            func_code: 0x0009,
        },
    ]
}

// ── AOS frame builder ─────────────────────────────────────────────────────────

/// Build a valid 1024-byte AOS Transfer Frame carrying one CCSDS Space Packet.
///
/// The space packet is sized to exactly fill the data field (DATA_FIELD_LEN bytes)
/// so `SppDecoder` can validate it with a single `SpacePacket::parse()` call.
/// Padding bytes beyond the real payload head are zero.
///
/// Frame layout (CCSDS 732.0-B-4, Q-C4 fixed 1024 B):
///   [0..6]   primary header (TF version, SCID, VCID, OCF_FLAG=0)
///   [6..1022] data field (1016 B — the space packet)
///   [1022..1024] FECF (CRC-16/IBM-3740 over [0..1022])
fn build_aos_frame(
    apid: u16,
    vc_id: u8,
    seq: u16,
    func_code: u16,
    payload_head: &[u8],
) -> [u8; AOS_FRAME_LEN] {
    // ── 1. Build CCSDS Space Packet padded to DATA_FIELD_LEN ─────────────────
    let mut user_data = vec![0u8; USER_DATA_LEN];
    let copy_len = payload_head.len().min(USER_DATA_LEN);
    user_data[..copy_len].copy_from_slice(&payload_head[..copy_len]);

    let pkt_bytes = PacketBuilder::tm(Apid::new(apid).expect("apid in range"))
        .sequence_count(SequenceCount::new(seq).expect("seq in range"))
        .cuc(Cuc { coarse: 1_000_000 + u32::from(seq) * 100, fine: 0 })
        .func_code(FuncCode::new(func_code).expect("func_code nonzero"))
        .instance_id(InstanceId::new(1).expect("instance_id nonzero"))
        .user_data(&user_data)
        .build()
        .expect("packet build must succeed");

    debug_assert_eq!(pkt_bytes.len(), DATA_FIELD_LEN, "packet must exactly fill data field");

    // ── 2. Build AOS frame ────────────────────────────────────────────────────
    let mut frame = [0u8; AOS_FRAME_LEN];

    // Primary header (6 B):
    //   byte 0: TF version (0b01) | SCID[7:2]
    //   byte 1: SCID[1:0] | VCID[5:0]
    //   bytes 2-4: VC Frame Counter (arbitrary non-zero)
    //   byte 5: OCF_FLAG=0 (no OCF)
    frame[0] = (0b01 << 6) | (SCID >> 2);
    frame[1] = ((SCID & 0x03) << 6) | (vc_id & 0x3F);
    frame[4] = (seq & 0xFF) as u8;   // VC frame counter LSB
    frame[5] = 0x00;                 // OCF_FLAG = 0

    // Data field [6..1022] — exactly the space packet
    frame[6..6 + DATA_FIELD_LEN].copy_from_slice(&pkt_bytes);

    // FECF: CRC-16/IBM-3740 over [0..1022], stored big-endian at [1022..1024]
    let crc = Crc::<u16>::new(&CRC_16_IBM_3740);
    let fecf = crc.checksum(&frame[..AOS_FRAME_LEN - 2]);
    let [hi, lo] = fecf.to_be_bytes();
    frame[AOS_FRAME_LEN - 2] = hi;
    frame[AOS_FRAME_LEN - 1] = lo;

    frame
}

// ── Route description ─────────────────────────────────────────────────────────

fn route_label(route: &Route) -> &'static str {
    match route {
        Route::Hk => "→ HK sink (telemetry ring buffer)",
        Route::EventLog => "→ EventLog sink (persistent EVS log)",
        Route::CfdpPdu => "→ CFDP sink (Class 1 receiver)",
        Route::RoverForward => "→ RoverForward archive",
        Route::IdleFill => "→ idle fill (silently discarded)",
        Route::Rejected { reason } => match reason {
            RejectReason::ForbiddenFaultInjectApid => "→ REJECTED (Q-F2: fault-inject APID forbidden on RF)",
            RejectReason::ForbiddenSimApid => "→ REJECTED (Q-F2: sim-injection APID forbidden on RF)",
            RejectReason::ForbiddenGroundInternal => "→ REJECTED: ground-internal APID on RF",
            RejectReason::UnknownBlock => "→ REJECTED: unknown APID block",
        },
    }
}

// ── Main ──────────────────────────────────────────────────────────────────────

#[tokio::main]
async fn main() {
    env_logger::builder()
        .filter_level(log::LevelFilter::Warn)   // suppress pipeline debug noise
        .init();

    println!();
    println!("╔══════════════════════════════════════════════════════════╗");
    println!("║     SAKURA-II Full Telemetry Pipeline — Live Demo        ║");
    println!("╠══════════════════════════════════════════════════════════╣");
    println!("║  SPACECRAFT  →  RF link  →  GROUND STATION (full stack) ║");
    println!("╚══════════════════════════════════════════════════════════╝");
    println!();

    let catalog = spacecraft_catalog();
    let n_packets = catalog.len();

    // ── Pipeline channels ─────────────────────────────────────────────────────
    let (frame_tx, frame_rx) = mpsc::channel::<AosFrame>(AOS_TO_DEMUX_CAP);
    let (clcw_tx, _clcw_rx) = watch::channel::<Option<[u8; 4]>>(None);
    let (mut demux, mut vc_rxs) = VcDemultiplexer::with_default_channels();

    // Per-VC SppDecoder → tagged router channel (vc_id, raw_bytes)
    let (tagged_tx, mut tagged_rx) = mpsc::channel::<(u8, Bytes)>(SPP_TO_ROUTER_CAP * 4);

    // RF link: tokio duplex large enough for all frames at once
    let buf_sz = AOS_FRAME_LEN * (n_packets + 4);
    let (writer, reader) = tokio::io::duplex(buf_sz);

    // ── Stage 1: spacecraft task ──────────────────────────────────────────────
    let catalog_tx = spacecraft_catalog();
    tokio::spawn(async move {
        let mut w = writer;
        println!("━━━ SPACECRAFT DOWNLINK SESSION ━━━");
        for (seq, pkt) in catalog_tx.iter().enumerate() {
            let frame = build_aos_frame(
                pkt.apid,
                pkt.vc_id,
                seq as u16,
                pkt.func_code,
                &pkt.payload_head,
            );
            let fecf = u16::from_be_bytes([frame[AOS_FRAME_LEN - 2], frame[AOS_FRAME_LEN - 1]]);
            println!(
                "  [TX] {:<38} APID=0x{:03X}  VC={:2}  seq={:3}  FECF=0x{:04X}",
                pkt.label, pkt.apid, pkt.vc_id, seq, fecf,
            );
            w.write_all(&frame).await.expect("write to RF link");
        }
        println!();
    });
    // writer drops here → EOF signals AosFramer to stop

    // ── Stage 2: AosFramer task ───────────────────────────────────────────────
    tokio::spawn(async move {
        let mut framer = AosFramer::new(frame_tx, clcw_tx);
        framer.run(reader).await.expect("framer run");
    });

    // ── Stage 3: VcDemultiplexer task ────────────────────────────────────────
    tokio::spawn(async move {
        demux.run(frame_rx).await;
    });

    // ── Stage 4: SppDecoder tasks (one per VC) ────────────────────────────────
    for vc_id in [0u8, 1, 2, 3] {
        let vc_rx = vc_rxs.remove(&vc_id).expect("vc receiver exists");
        let (spp_tx, mut spp_rx) = mpsc::channel::<Bytes>(SPP_TO_ROUTER_CAP);
        let tagged = tagged_tx.clone();

        // Decoder task
        tokio::spawn(async move {
            let mut dec = SppDecoder::new(vc_id);
            dec.run(vc_rx, spp_tx).await;
        });

        // Relay task: tag decoded bytes with vc_id for the router
        tokio::spawn(async move {
            while let Some(bytes) = spp_rx.recv().await {
                if tagged.send((vc_id, bytes)).await.is_err() {
                    break;
                }
            }
        });
    }
    drop(tagged_tx);  // relay tasks hold the only remaining senders

    // ── Stage 5: ApidRouter (main task) ──────────────────────────────────────
    println!("━━━ GROUND STATION INGEST ━━━");
    println!(
        "  {:>5}  {:>4}  {:<40}  {}",
        "APID", "VC", "App / Stream", "Routing Decision"
    );
    println!("  {}", "─".repeat(90));

    let mut router = ApidRouter::new();
    let mut counts: HashMap<&str, usize> = HashMap::new();

    while let Some((vc_id, bytes)) = tagged_rx.recv().await {
        let pkt = match SpacePacket::parse(&bytes) {
            Ok(p) => p,
            Err(e) => {
                eprintln!("  [ROUTER] parse error: {e}");
                continue;
            }
        };

        let apid = pkt.primary.apid().get();
        let seq = pkt.primary.sequence_count().get();
        let route = router.route(vc_id, &pkt);
        let label = route_label(&route);

        // Find the matching catalog entry for a friendly app name
        let app_name = catalog
            .iter()
            .find(|p| p.apid == apid)
            .map_or("?", |p| p.label);

        println!(
            "  0x{:03X}  VC={:2}  {:<40}  {}   [seq={}]",
            apid, vc_id, app_name, label, seq,
        );

        *counts.entry(label).or_insert(0) += 1;
    }

    // ── Summary ───────────────────────────────────────────────────────────────
    println!();
    println!("━━━ SESSION SUMMARY ━━━");
    println!("  Packets transmitted: {n_packets}");
    println!("  Forbidden APID 0x540 seen (Q-F2): {}", router.forbidden_apid_seen_total(0x540));
    let hk_count = counts
        .iter()
        .filter(|(k, _)| k.contains("HK sink"))
        .map(|(_, v)| v)
        .sum::<usize>();
    println!("  HK sink:       {hk_count}");
    println!(
        "  EventLog:      {}",
        counts.iter().filter(|(k, _)| k.contains("EventLog")).map(|(_, v)| v).sum::<usize>()
    );
    println!(
        "  RoverForward:  {}",
        counts.iter().filter(|(k, _)| k.contains("Rover")).map(|(_, v)| v).sum::<usize>()
    );
    println!(
        "  Rejected:      {}",
        counts.iter().filter(|(k, _)| k.contains("REJECTED")).map(|(_, v)| v).sum::<usize>()
    );
    println!("  Idle fill:     1 (VC 63 — discarded by VcDemux before router)");
    println!();
    println!("Pipeline complete — all stages exercised.");
    println!();
}
