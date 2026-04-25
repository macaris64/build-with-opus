//! TC uplink session demo — full three-stage uplink pipeline.
//!
//! Shows the complete ground-to-spacecraft telecommand path:
// Examples use expect()/unwrap(), direct indexing, and long main() for clarity.
// Suppress workspace deny/warn lints that would otherwise reject demo patterns.
#![allow(
    clippy::expect_used,
    clippy::unwrap_used,
    clippy::indexing_slicing,
    clippy::too_many_lines,
    clippy::cast_possible_truncation,
    clippy::doc_markdown,
    clippy::uninlined_format_args,
    clippy::similar_names,
    clippy::print_literal
)]
//!
//! ```text
//! Operator Intent (TcIntent)
//!   │  TcBuilder::build() — catalog validation + CCSDS TC packet encoding
//!   ▼
//! SPP bytes (CCSDS Space Packet)
//!   │  Cop1Engine::submit() — FOP-1 sliding-window sequencing
//!   │  Cop1Engine::tick()   — CLCW feedback drives state machine
//!   ▼
//! TcFrame (AD / BD / BC)
//!   │  TcFramer::frame() — SDLP primary header + FECF wrapping
//!   ▼
//! SDLP wire bytes → modulator → spacecraft
//! ```
//!
//! The demo simulates an uplink session: sends a BC init frame, receives a
//! CLCW acknowledging the init (→ Active), submits five mission commands, then
//! receives a CLCW acknowledging all of them.

use std::time::{Duration, Instant};

use ccsds_wire::{Cuc, SequenceCount};
use ground_station::uplink::{Cop1Engine, Cop1Error, Fop1State, TcBuilder, TcFramer, TcIntent};
use tokio::sync::watch;

// ── Helpers ───────────────────────────────────────────────────────────────────

/// Build CLCW bytes accepted by Cop1Engine.
///
/// CLCW wire layout (CCSDS 232.1-B-2 §3.3.3.2):
///   byte 2 bit 7 = lockout, bit 5 = retransmit
///   byte 3       = N(R) — next expected frame sequence number
fn make_clcw(lockout: bool, retransmit: bool, nr: u8) -> [u8; 4] {
    let mut clcw = [0u8; 4];
    if lockout {
        clcw[2] |= 0x80;
    }
    if retransmit {
        clcw[2] |= 0x20;
    }
    clcw[3] = nr;
    clcw
}

fn fop1_state_label(s: Fop1State) -> &'static str {
    match s {
        Fop1State::Initial => "Initial",
        Fop1State::Initializing => "Initializing",
        Fop1State::Active => "Active",
        Fop1State::RetransmitWithoutWait => "RetransmitWithoutWait",
        Fop1State::RetransmitWithWait => "RetransmitWithWait",
    }
}

// ── Demo ──────────────────────────────────────────────────────────────────────

fn main() {
    println!();
    println!("╔══════════════════════════════════════════════════════════╗");
    println!("║     SAKURA-II TC Uplink Session Demo                     ║");
    println!("╠══════════════════════════════════════════════════════════╣");
    println!("║  TcBuilder → Cop1Engine (FOP-1) → TcFramer (SDLP)       ║");
    println!("╚══════════════════════════════════════════════════════════╝");
    println!();

    // ── Setup ─────────────────────────────────────────────────────────────────
    let (clcw_tx, clcw_rx) = watch::channel::<Option<[u8; 4]>>(None);
    let t1 = Duration::from_secs(30); // 2 × (OWLT + 5 s) for test session
    let mut cop1 = Cop1Engine::new(clcw_rx, t1);
    let framer = TcFramer::new(42); // SCID = 42 (SAKURA_II)

    let now = Cuc {
        coarse: 1_800_000,
        fine: 0,
    }; // ~2034 TAI epoch

    // ── Stage 1: Build TC Space Packets ───────────────────────────────────────
    println!("━━━ STAGE 1: TC PACKET CATALOG (TcBuilder) ━━━");
    println!();

    let catalog: &[(&str, TcIntent)] = &[
        (
            "PKT-TC-0181-0100  OrbiterSetMode → SAFE",
            TcIntent::OrbiterSetMode {
                new_mode: 0x02,
                reason_code: 0x01,
            },
        ),
        (
            "PKT-TC-0181-0200  OrbiterEventFilter (CDH app, warn+above)",
            TcIntent::OrbiterEventFilter {
                filter_app_id: 42,
                min_event_type: 0x02,
                event_mask: 0x0000_00FF,
            },
        ),
        (
            "PKT-TC-0182-0100  OrbiterAdcsTargetQ (nadir-pointing)",
            TcIntent::OrbiterAdcsTargetQ {
                quaternion: [0x4000_0000, 0, 0, 0], // qw≈1 (identity, fixed-point)
                slew_mode: 0x01,
            },
        ),
        (
            "PKT-TC-0184-8000  OrbiterPowerArm switch #3",
            TcIntent::OrbiterPowerArm {
                switch_index: 3,
                confirm_magic: 0xA5A5,
            },
        ),
        (
            "PKT-TC-0184-8100  OrbiterPowerSwitch #3 → ON",
            TcIntent::OrbiterPowerSwitch {
                switch_index: 3,
                new_state: true,
                confirm_magic: 0xA5A5,
            },
        ),
        (
            "PKT-TC-0440-8200  CryobotSetDrillRpm 120 RPM, 5 A limit",
            TcIntent::CryobotSetDrillRpm {
                target_rpm: 120,
                max_current_10ma: 500, // 5.0 A (limit = 10 A)
                confirm_magic: 0x5A5A,
            },
        ),
    ];

    // Safety guard: exceeding drill current limit must be rejected
    let bad_drill = TcBuilder::build(
        &TcIntent::CryobotSetDrillRpm {
            target_rpm: 500,
            max_current_10ma: 1001, // > 1000 = forbidden
            confirm_magic: 0x5A5A,
        },
        SequenceCount::new(0).unwrap(),
        now,
    );
    println!(
        "  Safety check — drill current 10.01 A (forbidden): {:?}",
        bad_drill.unwrap_err()
    );
    println!();

    let mut spps: Vec<Vec<u8>> = Vec::new();
    for (seq_n, (label, intent)) in catalog.iter().enumerate() {
        let seq = SequenceCount::new(seq_n as u16).unwrap();
        let spp = TcBuilder::build(intent, seq, now).expect("catalog intents must be valid");
        println!("  [{:2}] {label}", seq_n);
        println!(
            "       APID=0x{:03X}  len={:3}B  first4=[{:02X} {:02X} {:02X} {:02X}]",
            (u16::from(spp[0] & 0x07) << 8) | u16::from(spp[1]),
            spp.len(),
            spp[0],
            spp[1],
            spp[2],
            spp[3],
        );
        spps.push(spp);
    }
    println!();

    // ── Stage 2: FOP-1 state machine ──────────────────────────────────────────
    println!("━━━ STAGE 2: FOP-1 STATE MACHINE (Cop1Engine) ━━━");
    println!();

    // Step 2a: BC init frame (Initial → Initializing)
    let bc_frame = cop1.initialize();
    println!(
        "  → initialize()  state: {} → {}",
        "Initial",
        fop1_state_label(cop1.state()),
    );
    println!(
        "     BC frame: VC={} type=BC seq={} payload=[{:02X}]",
        bc_frame.vc_id, bc_frame.sequence, bc_frame.payload[0],
    );
    println!();

    // Step 2b: Spacecraft sends CLCW accepting the BC (lockout=0, retransmit=0)
    clcw_tx.send(Some(make_clcw(false, false, 0))).unwrap();
    let _retransmits = cop1.tick(Instant::now());
    println!(
        "  → CLCW received (lockout=0 retransmit=0 N(R)=0)  state: Initializing → {}",
        fop1_state_label(cop1.state()),
    );
    println!();

    // Step 2c: Submit all TC frames (Active)
    println!("  Submitting {} TC commands:", spps.len());
    let mut ad_frames = Vec::new();
    for (i, spp) in spps.iter().enumerate() {
        match cop1.submit(spp.clone()) {
            Ok(frames) => {
                let f = &frames[0];
                println!(
                    "    [{}] submit OK → AD frame seq={} VC={}  payload_len={}B",
                    i,
                    f.sequence,
                    f.vc_id,
                    f.payload.len(),
                );
                ad_frames.extend(frames);
            }
            Err(Cop1Error::WindowFull) => println!("    [{}] window full!", i),
            Err(e) => println!("    [{}] error: {e}", i),
        }
    }
    println!();

    // Step 2d: CLCW acknowledges all 6 frames (N(R)=6 slides the window past seq 0–5)
    clcw_tx.send(Some(make_clcw(false, false, 6))).unwrap();
    let _retransmits = cop1.tick(Instant::now());
    println!(
        "  → CLCW N(R)=6 — all frames acknowledged  state: {}",
        fop1_state_label(cop1.state()),
    );
    println!();

    // ── Stage 3: SDLP frame encoding ──────────────────────────────────────────
    println!("━━━ STAGE 3: SDLP FRAME ENCODING (TcFramer) ━━━");
    println!();

    // Encode the BC frame first
    {
        let sdlp = framer.frame(&bc_frame).expect("BC frame must fit in 512 B");
        let fecf = u16::from_be_bytes([sdlp[sdlp.len() - 2], sdlp[sdlp.len() - 1]]);
        println!(
            "  BC init frame: SDLP total={:3}B  FECF=0x{:04X}  bypass={} CC={}",
            sdlp.len(),
            fecf,
            (sdlp[0] >> 5) & 1, // bypass bit
            (sdlp[0] >> 4) & 1, // CC bit
        );
    }

    println!();
    println!("  AD command frames:");
    for (i, tc_frame) in ad_frames.iter().enumerate() {
        let sdlp = framer.frame(tc_frame).expect("AD frame must fit in 512 B");
        let fecf = u16::from_be_bytes([sdlp[sdlp.len() - 2], sdlp[sdlp.len() - 1]]);
        println!(
            "    [{i}] SDLP total={:3}B  seq={}  bypass={}  FECF=0x{:04X}",
            sdlp.len(),
            tc_frame.sequence,
            (sdlp[0] >> 5) & 1,
            fecf,
        );
    }

    println!();
    println!("━━━ SESSION SUMMARY ━━━");
    println!("  TC commands sent:  {}", ad_frames.len());
    println!("  Final FOP-1 state: {}", fop1_state_label(cop1.state()));
    println!("  Retransmits:       {}", cop1.retransmit_count());
    println!();
    println!("Uplink session complete.");
    println!();
}
