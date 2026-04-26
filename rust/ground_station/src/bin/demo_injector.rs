//! Demo telemetry injector.
//!
//! Generates synthetic CCSDS/AOS frames for all known APIDs and sends them
//! over UDP to the ground station so the frontend LIVE mode shows real-looking
//! data without requiring actual cFS or Space ROS processes.
//!
//! Routing rules satisfied (see `ingest/router.rs`):
//!   VC 0  APID 0x100–0x17F → Hk      (orbiter housekeeping)
//!   VC 1  any non-forbidden APID → `EventLog`
//!   VC 3  APID 0x300–0x45F → `RoverForward` → merged into HK store
//!
//! # Usage
//! ```
//! cargo run --bin demo_injector                   # → 127.0.0.1:10000
//! cargo run --bin demo_injector 127.0.0.1:10000   # explicit target
//! ```

// This binary is a development tool, not flight software. The casts and
// potential panics here are acceptable in a demo context.
#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss,
    clippy::cast_precision_loss,
    clippy::cast_lossless,
    clippy::missing_errors_doc,
    clippy::missing_panics_doc,
    clippy::too_many_lines,
    // demo tool: all frame indices are constant-bounded (vec![0u8; AOS_FRAME_LEN])
    // and EVENTS index is guaranteed in-bounds by modulo; panics acceptable here.
    clippy::indexing_slicing
)]

use anyhow::Result;
use ccsds_wire::{Apid, Cuc, FuncCode, InstanceId, PacketBuilder, SequenceCount};
use crc::{Crc, CRC_16_IBM_3740};
use std::{
    net::UdpSocket,
    thread,
    time::{Duration, SystemTime, UNIX_EPOCH},
};

// ── AOS frame constants (mirrored from ingest/framer.rs) ─────────────────────

const AOS_FRAME_LEN: usize = 1024;
/// Bytes over which FECF (CRC-16) is computed.
const FECF_PAYLOAD_LEN: usize = AOS_FRAME_LEN - 2; // 1022
/// AOS primary header length.
const PRIMARY_HEADER_LEN: usize = 6;
/// Maximum packet bytes that fit in the data field (OCF absent).
const DATA_FIELD_MAX: usize = FECF_PAYLOAD_LEN - PRIMARY_HEADER_LEN; // 1016
/// Spacecraft ID used in the AOS primary header.
const SCID: u8 = 42;

// ── Virtual channel assignments ───────────────────────────────────────────────

const VC_HK: u8 = 0;
const VC_EVENT: u8 = 1;
const VC_ROVER: u8 = 3;

// ── Entry point ───────────────────────────────────────────────────────────────

fn main() -> Result<()> {
    let target = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:10000".to_owned());

    let socket = UdpSocket::bind("0.0.0.0:0")?;
    println!("demo_injector → {target}  (Ctrl-C to stop)");

    let mut state = InjectorState::default();

    loop {
        let tai = tai_now();
        inject_tick(&socket, &target, &mut state, tai)?;
        println!(
            "  tick={:>4}  seq={:>5}  tai={}",
            state.tick, state.seq, tai
        );
        state.tick = state.tick.wrapping_add(1);
        thread::sleep(Duration::from_secs(1));
    }
}

// ── Per-tick injection ────────────────────────────────────────────────────────

fn inject_tick(socket: &UdpSocket, target: &str, s: &mut InjectorState, tai: u32) -> Result<()> {
    // Orbiter housekeeping — VC 0
    send(socket, target, s, 0x101, VC_HK, tai, &cdh_hk(s.tick))?;
    send(socket, target, s, 0x110, VC_HK, tai, &adcs_hk(s.tick))?;
    send(socket, target, s, 0x120, VC_HK, tai, &comm_hk(s.tick))?;
    send(
        socket,
        target,
        s,
        0x128,
        VC_HK,
        tai,
        &ros2_bridge_hk(s.tick),
    )?;
    send(socket, target, s, 0x129, VC_HK, tai, &prx1_hk(s.tick))?;
    send(socket, target, s, 0x130, VC_HK, tai, &power_hk(s.tick))?;
    send(socket, target, s, 0x140, VC_HK, tai, &payload_hk(s.tick))?;
    send(socket, target, s, 0x160, VC_HK, tai, &fleet_hk(s.tick))?;

    // Rover housekeeping — VC 3 (APID 0x300–0x45F → RoverForward)
    // Land rovers × 3 (0x300–0x302)
    send(
        socket,
        target,
        s,
        0x300,
        VC_ROVER,
        tai,
        &rover_land_hk(s.tick),
    )?;
    send(
        socket,
        target,
        s,
        0x301,
        VC_ROVER,
        tai,
        &rover_land_hk_n(s.tick, -8.0, 5.0, 2.0),
    )?;
    send(
        socket,
        target,
        s,
        0x302,
        VC_ROVER,
        tai,
        &rover_land_hk_n(s.tick, 3.0, -6.0, 4.0),
    )?;

    // Aerial drones × 5 (0x3C0–0x3C4)
    send(
        socket,
        target,
        s,
        0x3c0,
        VC_ROVER,
        tai,
        &rover_uav_hk(s.tick),
    )?;
    send(
        socket,
        target,
        s,
        0x3c1,
        VC_ROVER,
        tai,
        &rover_uav_hk_n(s.tick, -6.0, 4.0, 6.0, 1.2, 0.13),
    )?;
    send(
        socket,
        target,
        s,
        0x3c2,
        VC_ROVER,
        tai,
        &rover_uav_hk_n(s.tick, 8.0, -3.0, 4.0, 2.4, 0.21),
    )?;
    send(
        socket,
        target,
        s,
        0x3c3,
        VC_ROVER,
        tai,
        &rover_uav_hk_n(s.tick, -3.0, -7.0, 7.0, 0.7, 0.15),
    )?;
    send(
        socket,
        target,
        s,
        0x3c4,
        VC_ROVER,
        tai,
        &rover_uav_hk_n(s.tick, 5.0, 7.0, 5.0, 3.1, 0.19),
    )?;

    // Cryobot × 1 (0x400)
    send(
        socket,
        target,
        s,
        0x400,
        VC_ROVER,
        tai,
        &rover_cryo_hk(s.tick),
    )?;

    // ── Titan vehicles (0x410–0x433, still within ROVER_TM 0x300–0x45F range) ─

    // Titan land rovers × 3 (0x410–0x412)
    send(
        socket,
        target,
        s,
        0x410,
        VC_ROVER,
        tai,
        &titan_rover_hk(s.tick, 12.0, 8.0, 0.0),
    )?;
    send(
        socket,
        target,
        s,
        0x411,
        VC_ROVER,
        tai,
        &titan_rover_hk(s.tick, -11.0, 6.0, 1.5),
    )?;
    send(
        socket,
        target,
        s,
        0x412,
        VC_ROVER,
        tai,
        &titan_rover_hk(s.tick, 8.0, -10.0, 3.0),
    )?;

    // Titan UAVs × 5 (0x420–0x424)
    send(
        socket,
        target,
        s,
        0x420,
        VC_ROVER,
        tai,
        &titan_uav_hk(s.tick, 0.0, 0.0, 12.0, 0.0, 0.17),
    )?;
    send(
        socket,
        target,
        s,
        0x421,
        VC_ROVER,
        tai,
        &titan_uav_hk(s.tick, -7.0, 5.0, 14.0, 1.2, 0.13),
    )?;
    send(
        socket,
        target,
        s,
        0x422,
        VC_ROVER,
        tai,
        &titan_uav_hk(s.tick, 9.0, -4.0, 11.0, 2.4, 0.21),
    )?;
    send(
        socket,
        target,
        s,
        0x423,
        VC_ROVER,
        tai,
        &titan_uav_hk(s.tick, -3.0, -8.0, 13.0, 0.7, 0.15),
    )?;
    send(
        socket,
        target,
        s,
        0x424,
        VC_ROVER,
        tai,
        &titan_uav_hk(s.tick, 6.0, 6.0, 15.0, 3.1, 0.19),
    )?;

    // Titan cryobots × 4 (0x430–0x433)
    send(
        socket,
        target,
        s,
        0x430,
        VC_ROVER,
        tai,
        &titan_cryo_hk(s.tick, 0.0, 0.0),
    )?;
    send(
        socket,
        target,
        s,
        0x431,
        VC_ROVER,
        tai,
        &titan_cryo_hk(s.tick, 2.0, 1.2),
    )?;
    send(
        socket,
        target,
        s,
        0x432,
        VC_ROVER,
        tai,
        &titan_cryo_hk(s.tick, 5.0, 2.4),
    )?;
    send(
        socket,
        target,
        s,
        0x433,
        VC_ROVER,
        tai,
        &titan_cryo_hk(s.tick, 8.0, 0.9),
    )?;

    // Event log — VC 1 (any non-forbidden APID works; use the source app APID)
    let (ev_apid, ev_sev, ev_msg) = event_template(s.tick);
    send(
        socket,
        target,
        s,
        ev_apid,
        VC_EVENT,
        tai,
        &event_payload(ev_sev, ev_msg),
    )?;

    Ok(())
}

// ── Frame + UDP send ──────────────────────────────────────────────────────────

fn send(
    socket: &UdpSocket,
    target: &str,
    s: &mut InjectorState,
    apid: u16,
    vc_id: u8,
    tai: u32,
    user_data: &[u8],
) -> Result<()> {
    let seq = s.next_seq();
    let pkt = PacketBuilder::tm(Apid::new(apid)?)
        .sequence_count(SequenceCount::new(seq)?)
        .cuc(Cuc {
            coarse: tai,
            fine: 0,
        })
        .func_code(FuncCode::new(0x0002)?)
        .instance_id(InstanceId::new(1)?)
        .user_data(user_data)
        .build()?;

    let frame = build_aos_frame(&pkt, vc_id, s.next_frame_ctr());
    socket.send_to(&frame, target)?;
    Ok(())
}

fn build_aos_frame(pkt: &[u8], vc_id: u8, frame_ctr: u8) -> Vec<u8> {
    let mut frame = vec![0u8; AOS_FRAME_LEN];

    // AOS primary header (CCSDS 732.0-B-4)
    frame[0] = (0b01 << 6) | (SCID >> 2); // version=01, SCID[9:4]
    frame[1] = ((SCID & 0x03) << 6) | (vc_id & 0x3F); // SCID[3:2], VC_ID
    frame[2] = 0; // master channel frame count (MSB)
    frame[3] = frame_ctr; // master channel frame count (LSB)
    frame[4] = frame_ctr; // virtual channel frame count
    frame[5] = 0x00; // signaling field: no OCF

    // Copy packet into data field; remainder is idle fill (zeros)
    let copy_len = pkt.len().min(DATA_FIELD_MAX);
    frame[PRIMARY_HEADER_LEN..PRIMARY_HEADER_LEN + copy_len].copy_from_slice(&pkt[..copy_len]);

    // FECF: CRC-16/IBM-3740 over bytes 0..1022
    let fecf = Crc::<u16>::new(&CRC_16_IBM_3740).checksum(&frame[..FECF_PAYLOAD_LEN]);
    let [hi, lo] = fecf.to_be_bytes();
    frame[AOS_FRAME_LEN - 2] = hi;
    frame[AOS_FRAME_LEN - 1] = lo;

    frame
}

// ── Injector state ────────────────────────────────────────────────────────────

#[derive(Default)]
struct InjectorState {
    tick: u32,
    seq: u16,
    frame_ctr: u8,
}

impl InjectorState {
    /// Returns the next 14-bit sequence count (wraps at 0x3FFF).
    fn next_seq(&mut self) -> u16 {
        let s = self.seq;
        self.seq = self.seq.wrapping_add(1) & 0x3FFF;
        s
    }

    fn next_frame_ctr(&mut self) -> u8 {
        let c = self.frame_ctr;
        self.frame_ctr = self.frame_ctr.wrapping_add(1);
        c
    }
}

// ── TAI timestamp ─────────────────────────────────────────────────────────────

fn tai_now() -> u32 {
    let unix_s = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();
    // TAI = UTC + 37 leap seconds. u32 wraps in year 2106 — acceptable here.
    unix_s.saturating_add(37) as u32
}

// ── Payload helpers ───────────────────────────────────────────────────────────

fn f32le(v: f32) -> [u8; 4] {
    v.to_le_bytes()
}
fn u32le(v: u32) -> [u8; 4] {
    v.to_le_bytes()
}
fn u32be(v: u32) -> [u8; 4] {
    v.to_be_bytes()
}
fn u16le(v: u16) -> [u8; 2] {
    v.to_le_bytes()
}

// ── Orbiter HK payloads ───────────────────────────────────────────────────────

/// APID 0x101 — CDH: mode, `cmd_counter`, `err_counter`, uptime
fn cdh_hk(tick: u32) -> Vec<u8> {
    let mut v = vec![0x01u8, 0x00, (tick & 0xFF) as u8, 0x00, 0x00, 0x00];
    v.extend_from_slice(&u32le(tick));
    v
}

/// APID 0x110 — ADCS: quaternion (w,x,y,z) + angular rates
fn adcs_hk(tick: u32) -> Vec<u8> {
    let theta = tick as f32 * 0.02;
    let w = (theta / 2.0).cos();
    let z = (theta / 2.0).sin();
    let mut v = Vec::new();
    v.extend_from_slice(&f32le(w));
    v.extend_from_slice(&f32le(0.0));
    v.extend_from_slice(&f32le(0.0));
    v.extend_from_slice(&f32le(z));
    v.extend_from_slice(&f32le(theta.sin() * 0.01));
    v.extend_from_slice(&f32le(theta.cos() * 0.005));
    v.extend_from_slice(&f32le(0.001));
    v.extend_from_slice(&[0u8; 4]); // padding
    v
}

/// APID 0x120 — COMM: mode, `vc0_rate`, `cmd_counter`
fn comm_hk(tick: u32) -> Vec<u8> {
    vec![
        0x01,
        0x00,
        0x64,
        0x00,
        (tick & 0xFF) as u8,
        0x00,
        0x00,
        0x00,
    ]
}

/// APID 0x128 — `ros2_bridge`: 6 × u32 LE counters
fn ros2_bridge_hk(tick: u32) -> Vec<u8> {
    let mut v = Vec::new();
    v.extend_from_slice(&u32le(tick * 3)); // packets_routed
    v.extend_from_slice(&u32le(2)); // apid_rejects
    v.extend_from_slice(&u32le(tick)); // tc_forwarded
    v.extend_from_slice(&u32le(tick)); // uptime_s
    v.extend_from_slice(&u32le(tick * 3 + 2)); // cmd_counter
    v.extend_from_slice(&u32le(0)); // err_counter
    v
}

/// APID 0x129 — Proximity-1: `session_active`, `signal_strength`, `last_contact` (f64 BE)
fn prx1_hk(tick: u32) -> Vec<u8> {
    let strength = (180.0f32 + (tick as f32 * 0.1).sin() * 60.0) as u8;
    let contact_s = (1_700_000_000.0f64 + tick as f64).to_be_bytes();
    let mut v = vec![1u8, strength];
    v.extend_from_slice(&contact_s);
    v
}

/// APID 0x130 — Power: `bus_voltage_mv`, `battery_pct`, `solar_current_ma` (all u16 LE)
fn power_hk(tick: u32) -> Vec<u8> {
    let voltage_mv = (28_000.0f32 + (tick as f32 * 0.05).sin() * 1_400.0) as u16;
    let battery_pct = (90u32).saturating_sub(tick / 120) as u16;
    let solar_ma: u16 = 2_110;
    let mut v = Vec::new();
    v.extend_from_slice(&u16le(voltage_mv));
    v.extend_from_slice(&u16le(battery_pct));
    v.extend_from_slice(&u16le(solar_ma));
    v.extend_from_slice(&u16le(0)); // padding
    v
}

/// APID 0x140 — Payload: active, `data_rate`, `frame_count`
fn payload_hk(tick: u32) -> Vec<u8> {
    let frame_count = (tick & 0xFFFF) as u16;
    let mut v = vec![0x01u8, 0x00];
    v.extend_from_slice(&u16le(1_000)); // data_rate kbps
    v.extend_from_slice(&u16le(frame_count));
    v.extend_from_slice(&[(tick & 0xFF) as u8, ((tick >> 8) & 0xFF) as u8]);
    v
}

/// APID 0x160 — `fleet_monitor`: `health_mask` + 3 × `age_ms` (u32 BE)
fn fleet_hk(tick: u32) -> Vec<u8> {
    let land_age = (800.0f32 + (tick as f32 * 0.15).sin() * 100.0) as u32;
    let uav_age = (900.0f32 + (tick as f32 * 0.12).sin() * 150.0) as u32;
    let cryo_age = (950.0f32 + (tick as f32 * 0.08).sin() * 200.0) as u32;
    let mut v = vec![0x07u8]; // health_mask: bits 0–2 = land|uav|cryo all healthy
    v.extend_from_slice(&u32be(land_age));
    v.extend_from_slice(&u32be(uav_age));
    v.extend_from_slice(&u32be(cryo_age));
    v
}

// ── Rover HK payloads ─────────────────────────────────────────────────────────

/// APID 0x300 — Land rover: `pos_x`, `pos_y`, heading (f32 LE) + mode + status
fn rover_land_hk(tick: u32) -> Vec<u8> {
    let x = tick as f32 * 0.05;
    let y = (tick as f32 * 0.03).sin() * 10.0;
    let heading = (tick as f32 * 2.0) % 360.0;
    let mut v = Vec::new();
    v.extend_from_slice(&f32le(x));
    v.extend_from_slice(&f32le(y));
    v.extend_from_slice(&f32le(heading));
    v.push(0x14); // mode = NOMINAL (matches hkDecoder check)
    v.push(0x00); // status = OK
    v
}

/// APID 0x3C0 — UAV: altitude, `pos_x`, `pos_y`, `battery_pct` (f32 LE)
fn rover_uav_hk(tick: u32) -> Vec<u8> {
    let alt = 50.0f32 + (tick as f32 * 0.05).sin() * 10.0;
    let x = (tick as f32 * 0.03).cos() * 30.0;
    let y = (tick as f32 * 0.03).sin() * 30.0;
    let batt = (85.0f32 - tick as f32 * 0.01).max(0.0);
    let mut v = Vec::new();
    v.extend_from_slice(&f32le(alt));
    v.extend_from_slice(&f32le(x));
    v.extend_from_slice(&f32le(y));
    v.extend_from_slice(&f32le(batt));
    v
}

/// APID 0x400 — Cryobot: `depth_m`, `drill_rpm` (f32 LE) + `temp_c×10` (i16 LE)
fn rover_cryo_hk(tick: u32) -> Vec<u8> {
    let depth = tick as f32 * 0.002;
    let rpm = 450.0f32 + (tick as f32 * 0.1).sin() * 50.0;
    let temp_raw = (-180.0f32 + (tick as f32 * 0.02).sin() * 5.0) * 10.0;
    let temp_i16 = temp_raw as i16;
    let mut v = Vec::new();
    v.extend_from_slice(&f32le(depth));
    v.extend_from_slice(&f32le(rpm));
    v.extend_from_slice(&temp_i16.to_le_bytes());
    v
}

/// APID 0x301–0x302 — Additional land rovers with distinct base positions and phases.
///
/// Payload layout matches the 0x300 decoder in `hkDecoder.ts`:
/// bytes 0–3: `x_m` (F32LE), 4–7: `y_m` (F32LE), 8–11: `heading_deg` (F32LE), 12–13: `speed_cm_s` (U16LE)
fn rover_land_hk_n(tick: u32, base_x: f32, base_z: f32, phase: f32) -> Vec<u8> {
    let t = tick as f32;
    let x = base_x + (t * 0.05 + phase).sin() * 2.0;
    let z = base_z + (t * 0.04 + phase + 1.0).cos() * 2.0;
    let heading = (t * 2.0 + phase * 57.3) % 360.0;
    let speed_cm_s = (10.0f32 + (t * 0.3 + phase).sin().abs() * 20.0) as u16;
    let mut v = Vec::new();
    v.extend_from_slice(&f32le(x));
    v.extend_from_slice(&f32le(z));
    v.extend_from_slice(&f32le(heading));
    v.extend_from_slice(&u16le(speed_cm_s));
    v
}

/// APID 0x3C1–0x3C4 — Additional UAV drones with distinct base positions and hover phases.
///
/// Payload layout matches the 0x3C0 decoder in `hkDecoder.ts`:
/// bytes 0–3: `altitude_m` (F32LE), 4–7: `x_m` (F32LE), 8–11: `y_m` (F32LE), 12–15: `battery_pct` (F32LE)
fn rover_uav_hk_n(
    tick: u32,
    base_x: f32,
    base_z: f32,
    base_y: f32,
    hover_phase: f32,
    orbit_speed: f32,
) -> Vec<u8> {
    let t = tick as f32;
    let hover_speed = 0.9 + hover_phase * 0.15;
    let alt = base_y + (t * hover_speed + hover_phase).sin() * 0.4;
    let x = base_x + (t * orbit_speed + hover_phase).sin() * 1.5;
    let z = base_z + (t * orbit_speed + hover_phase + 1.5).cos() * 1.5;
    let batt = 80.0f32 + (t * 0.007 + hover_phase).sin() * 10.0;
    let mut v = Vec::new();
    v.extend_from_slice(&f32le(alt));
    v.extend_from_slice(&f32le(x));
    v.extend_from_slice(&f32le(z));
    v.extend_from_slice(&f32le(batt));
    v
}

// ── Titan HK payloads (0x410–0x433, RoverForward range) ──────────────────────

/// APID 0x410–0x412 — Titan land rover: `pos_x`, `pos_z`, heading (f32 LE) + battery (u16 LE)
fn titan_rover_hk(tick: u32, base_x: f32, base_z: f32, phase: f32) -> Vec<u8> {
    let t = tick as f32;
    let x = base_x + (t * 0.04 + phase).sin() * 1.5;
    let z = base_z + (t * 0.03 + phase + 1.0).cos() * 1.5;
    let heading = (t * 1.8 + phase * 57.3) % 360.0;
    let battery = (75.0f32 + (t * 0.008 + phase).sin() * 10.0) as u16;
    let mut v = Vec::new();
    v.extend_from_slice(&f32le(x));
    v.extend_from_slice(&f32le(z));
    v.extend_from_slice(&f32le(heading));
    v.extend_from_slice(&u16le(battery));
    v
}

/// APID 0x420–0x424 — Titan UAV: altitude, `pos_x`, `pos_z`, `battery_pct` (f32 LE)
fn titan_uav_hk(
    tick: u32,
    base_x: f32,
    base_z: f32,
    base_y: f32,
    hover_phase: f32,
    orbit_speed: f32,
) -> Vec<u8> {
    let t = tick as f32;
    let hover_speed = 1.0 + hover_phase * 0.12;
    let alt = base_y + (t * hover_speed + hover_phase).sin() * 0.4;
    let x = base_x + (t * orbit_speed + hover_phase).sin() * 1.2;
    let z = base_z + (t * orbit_speed + hover_phase + 1.5).cos() * 1.2;
    let batt = 75.0f32 + (t * 0.006 + hover_phase).sin() * 12.0;
    let mut v = Vec::new();
    v.extend_from_slice(&f32le(alt));
    v.extend_from_slice(&f32le(x));
    v.extend_from_slice(&f32le(z));
    v.extend_from_slice(&f32le(batt));
    v
}

/// APID 0x430–0x433 — Titan cryobot: `depth_m`, `drill_rpm` (f32 LE) + `temp_c×10` (i16 LE)
fn titan_cryo_hk(tick: u32, base_depth: f32, rpm_phase: f32) -> Vec<u8> {
    let t = tick as f32;
    let depth = base_depth + t * 0.003;
    let rpm = 400.0f32 + (t * 0.09 + rpm_phase).sin() * 80.0;
    // Titan surface temp ≈ −179 °C; stored as integer × 10
    let temp_raw = (-1790.0f32 + (t * 0.015 + rpm_phase).sin() * 30.0) as i16;
    let mut v = Vec::new();
    v.extend_from_slice(&f32le(depth));
    v.extend_from_slice(&f32le(rpm));
    v.extend_from_slice(&temp_raw.to_le_bytes());
    v
}

// ── Event log payloads ────────────────────────────────────────────────────────

const EVENTS: &[(u16, u8, &str)] = &[
    (
        0x101,
        1,
        "CDH: Scheduler cycle nominal. Mode=NOMINAL uptime OK",
    ),
    (
        0x110,
        1,
        "ADCS: Attitude estimate converged. Max deviation 0.003 deg",
    ),
    (0x120, 2, "COMM: VC0 rate set 100 kbps. Frame sync OK"),
    (0x128, 1, "ROS2_BRIDGE: Packets routed milestone reached"),
    (
        0x130,
        1,
        "POWER: Battery SOC nominal. Solar panel charging active",
    ),
    (
        0x140,
        1,
        "PAYLOAD: Science frame captured. Buffer utilisation 42%",
    ),
    (0x300, 1, "ROVER_LAND: Waypoint reached. Next target 30s"),
    (0x3c0, 2, "UAV: Altitude hold engaged at 52m AGL"),
    (0x400, 1, "CRYOBOT: Drill nominal. Depth milestone reached"),
    (
        0x110,
        3,
        "ADCS: WARN angular rate above threshold 0.015 rad/s",
    ),
];

fn event_template(tick: u32) -> (u16, u8, &'static str) {
    let idx = (tick as usize) % EVENTS.len();
    EVENTS[idx]
}

/// EVS payload: severity byte + null-terminated UTF-8 message.
fn event_payload(severity: u8, message: &str) -> Vec<u8> {
    let mut v = vec![severity];
    v.extend_from_slice(message.as_bytes());
    v.push(0u8); // null terminator
    v
}
