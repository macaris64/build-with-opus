//! Phase 16 integration test — C-struct → wire-bytes → SpacePacket → C-struct round-trip
//! for every MID-class constant defined in `_defs/mids.h`.
//!
//! Dependency path: Phases 13 (mids), 14 (convert).
//! Source of truth: `docs/interfaces/apid-registry.md`, `docs/architecture/06-ground-segment-rust.md §3.2`.
//! Q-C8: no `*_be_bytes`/`*_le_bytes` calls here — all BE encoding stays in loci A and B.

#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::doc_markdown
)]

use ccsds_wire::{Apid, Cuc, FuncCode, InstanceId, PacketBuilder, PacketType, SpacePacket};
use cfs_bindings::{
    convert::{from_c_message, to_c_message},
    mids::*,
};

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------

fn build_tm(apid_raw: u16) -> Vec<u8> {
    PacketBuilder::tm(Apid::new(apid_raw).unwrap())
        .cuc(Cuc {
            coarse: 100,
            fine: 5,
        })
        .func_code(FuncCode::new(0x0001).unwrap())
        .instance_id(InstanceId::new(1).unwrap())
        .user_data(&[0xCA, 0xFE])
        .build()
        .unwrap()
}

fn build_tc(apid_raw: u16) -> Vec<u8> {
    PacketBuilder::tc(Apid::new(apid_raw).unwrap())
        .func_code(FuncCode::new(0x8001).unwrap())
        .instance_id(InstanceId::new(1).unwrap())
        .build()
        .unwrap()
}

/// Round-trip a TM MID constant: bytes → Message → bytes → SpacePacket.
/// Asserts byte equality and correct APID / PacketType.
fn assert_tm_roundtrip(mid_constant: u32) {
    let apid_raw = (mid_constant & 0x07FF) as u16;
    let orig = build_tm(apid_raw);
    let orig_copy = orig.clone();

    let msg = to_c_message(&orig).unwrap();
    let bytes = from_c_message(&msg).unwrap();

    assert_eq!(
        bytes, orig_copy,
        "TM round-trip bytes mismatch for MID 0x{mid_constant:04X}"
    );

    let pkt = SpacePacket::parse(&bytes).unwrap();
    assert_eq!(
        pkt.primary.apid(),
        Apid::new(apid_raw).unwrap(),
        "APID mismatch for MID 0x{mid_constant:04X}"
    );
    assert_eq!(
        pkt.primary.packet_type(),
        PacketType::Tm,
        "expected TM for MID 0x{mid_constant:04X}"
    );
}

/// Round-trip a TC MID constant: bytes → Message → bytes → SpacePacket.
/// Asserts byte equality and correct APID / PacketType.
fn assert_tc_roundtrip(mid_constant: u32) {
    let apid_raw = (mid_constant & 0x07FF) as u16;
    let orig = build_tc(apid_raw);
    let orig_copy = orig.clone();

    let msg = to_c_message(&orig).unwrap();
    let bytes = from_c_message(&msg).unwrap();

    assert_eq!(
        bytes, orig_copy,
        "TC round-trip bytes mismatch for MID 0x{mid_constant:04X}"
    );

    let pkt = SpacePacket::parse(&bytes).unwrap();
    assert_eq!(
        pkt.primary.apid(),
        Apid::new(apid_raw).unwrap(),
        "APID mismatch for MID 0x{mid_constant:04X}"
    );
    assert_eq!(
        pkt.primary.packet_type(),
        PacketType::Tc,
        "expected TC for MID 0x{mid_constant:04X}"
    );
}

// ------------------------------------------------------------------
// TM round-trip tests — one per HK MID macro in _defs/mids.h
// ------------------------------------------------------------------

/// G: SAMPLE_APP_HK_MID = 0x0900 (TM, APID 0x100).
/// W: TM bytes round-trip through to_c_message → from_c_message.
/// T: bytes identical, APID 0x100, PacketType::Tm.
#[test]
fn test_roundtrip_sample_app_hk() {
    assert_tm_roundtrip(SAMPLE_APP_HK_MID);
}

/// G: ORBITER_CDH_HK_MID = 0x0901 (TM, APID 0x101).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x101, Tm.
#[test]
fn test_roundtrip_orbiter_cdh_hk() {
    assert_tm_roundtrip(ORBITER_CDH_HK_MID);
}

/// G: ORBITER_ADCS_HK_MID = 0x0910 (TM, APID 0x110).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x110, Tm.
#[test]
fn test_roundtrip_orbiter_adcs_hk() {
    assert_tm_roundtrip(ORBITER_ADCS_HK_MID);
}

/// G: ORBITER_COMM_HK_MID = 0x0920 (TM, APID 0x120).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x120, Tm.
#[test]
fn test_roundtrip_orbiter_comm_hk() {
    assert_tm_roundtrip(ORBITER_COMM_HK_MID);
}

/// G: ORBITER_POWER_HK_MID = 0x0930 (TM, APID 0x130).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x130, Tm.
#[test]
fn test_roundtrip_orbiter_power_hk() {
    assert_tm_roundtrip(ORBITER_POWER_HK_MID);
}

/// G: ORBITER_PAYLOAD_HK_MID = 0x0940 (TM, APID 0x140).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x140, Tm.
#[test]
fn test_roundtrip_orbiter_payload_hk() {
    assert_tm_roundtrip(ORBITER_PAYLOAD_HK_MID);
}

/// G: RELAY_HK_MID = 0x0A00 (TM, APID 0x200).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x200, Tm.
#[test]
fn test_roundtrip_relay_hk() {
    assert_tm_roundtrip(RELAY_HK_MID);
}

/// G: MCU_PAYLOAD_HK_MID = 0x0A80 (TM, APID 0x280 — SpaceWire gateway).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x280, Tm.
#[test]
fn test_roundtrip_mcu_payload_hk() {
    assert_tm_roundtrip(MCU_PAYLOAD_HK_MID);
}

/// G: MCU_RWA_HK_MID = 0x0A90 (TM, APID 0x290 — CAN gateway).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x290, Tm.
#[test]
fn test_roundtrip_mcu_rwa_hk() {
    assert_tm_roundtrip(MCU_RWA_HK_MID);
}

/// G: MCU_EPS_HK_MID = 0x0AA0 (TM, APID 0x2A0 — UART gateway).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x2A0, Tm.
#[test]
fn test_roundtrip_mcu_eps_hk() {
    assert_tm_roundtrip(MCU_EPS_HK_MID);
}

/// G: ROVER_LAND_HK_MID = 0x0B00 (TM, APID 0x300).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x300, Tm.
#[test]
fn test_roundtrip_rover_land_hk() {
    assert_tm_roundtrip(ROVER_LAND_HK_MID);
}

/// G: ROVER_UAV_HK_MID = 0x0BC0 (TM, APID 0x3C0 — bidirectional block).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x3C0, Tm.
#[test]
fn test_roundtrip_rover_uav_hk() {
    assert_tm_roundtrip(ROVER_UAV_HK_MID);
}

/// G: ROVER_CRYOBOT_HK_MID = 0x0C00 (TM, APID 0x400 — tether-gated).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x400, Tm.
#[test]
fn test_roundtrip_rover_cryobot_hk() {
    assert_tm_roundtrip(ROVER_CRYOBOT_HK_MID);
}

/// G: SIM_INJECT_HK_MID = 0x0D00 (TM, APID 0x500 — sideband, never flight-path).
/// W: TM bytes round-trip.
/// T: byte-identical, APID 0x500, Tm.
#[test]
fn test_roundtrip_sim_inject_hk() {
    assert_tm_roundtrip(SIM_INJECT_HK_MID);
}

// ------------------------------------------------------------------
// TC round-trip tests — one per CMD MID macro in _defs/mids.h
// ------------------------------------------------------------------

/// G: SAMPLE_APP_CMD_MID = 0x1980 (TC, APID 0x180).
/// W: TC bytes round-trip through to_c_message → from_c_message.
/// T: byte-identical, APID 0x180, PacketType::Tc.
#[test]
fn test_roundtrip_sample_app_cmd() {
    assert_tc_roundtrip(SAMPLE_APP_CMD_MID);
}

/// G: ORBITER_CDH_CMD_MID = 0x1981 (TC, APID 0x181).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x181, Tc.
#[test]
fn test_roundtrip_orbiter_cdh_cmd() {
    assert_tc_roundtrip(ORBITER_CDH_CMD_MID);
}

/// G: ORBITER_ADCS_CMD_MID = 0x1982 (TC, APID 0x182).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x182, Tc.
#[test]
fn test_roundtrip_orbiter_adcs_cmd() {
    assert_tc_roundtrip(ORBITER_ADCS_CMD_MID);
}

/// G: ORBITER_COMM_CMD_MID = 0x1983 (TC, APID 0x183).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x183, Tc.
#[test]
fn test_roundtrip_orbiter_comm_cmd() {
    assert_tc_roundtrip(ORBITER_COMM_CMD_MID);
}

/// G: ORBITER_POWER_CMD_MID = 0x1984 (TC, APID 0x184).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x184, Tc.
#[test]
fn test_roundtrip_orbiter_power_cmd() {
    assert_tc_roundtrip(ORBITER_POWER_CMD_MID);
}

/// G: ORBITER_PAYLOAD_CMD_MID = 0x1985 (TC, APID 0x185).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x185, Tc.
#[test]
fn test_roundtrip_orbiter_payload_cmd() {
    assert_tc_roundtrip(ORBITER_PAYLOAD_CMD_MID);
}

/// G: RELAY_CMD_MID = 0x1A40 (TC, APID 0x240).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x240, Tc.
#[test]
fn test_roundtrip_relay_cmd() {
    assert_tc_roundtrip(RELAY_CMD_MID);
}

/// G: MCU_PAYLOAD_CMD_MID = 0x1A80 (TC, APID 0x280 — SpaceWire gateway).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x280, Tc.
#[test]
fn test_roundtrip_mcu_payload_cmd() {
    assert_tc_roundtrip(MCU_PAYLOAD_CMD_MID);
}

/// G: MCU_RWA_CMD_MID = 0x1A90 (TC, APID 0x290 — CAN gateway).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x290, Tc.
#[test]
fn test_roundtrip_mcu_rwa_cmd() {
    assert_tc_roundtrip(MCU_RWA_CMD_MID);
}

/// G: MCU_EPS_CMD_MID = 0x1AA0 (TC, APID 0x2A0 — UART gateway).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x2A0, Tc.
#[test]
fn test_roundtrip_mcu_eps_cmd() {
    assert_tc_roundtrip(MCU_EPS_CMD_MID);
}

/// G: ROVER_LAND_CMD_MID = 0x1B80 (TC, APID 0x380).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x380, Tc.
#[test]
fn test_roundtrip_rover_land_cmd() {
    assert_tc_roundtrip(ROVER_LAND_CMD_MID);
}

/// G: ROVER_UAV_CMD_MID = 0x1BC0 (TC, APID 0x3C0 — bidirectional block).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x3C0, Tc.
#[test]
fn test_roundtrip_rover_uav_cmd() {
    assert_tc_roundtrip(ROVER_UAV_CMD_MID);
}

/// G: ROVER_CRYOBOT_CMD_MID = 0x1C40 (TC, APID 0x440 — tether-gated).
/// W: TC bytes round-trip.
/// T: byte-identical, APID 0x440, Tc.
#[test]
fn test_roundtrip_rover_cryobot_cmd() {
    assert_tc_roundtrip(ROVER_CRYOBOT_CMD_MID);
}
