//! Known-answer tests (KATs) for `ccsds_wire`. Five hand-rolled packets drawn
//! from `docs/interfaces/packet-catalog.md §4`. Each test encodes a fixed byte
//! array from the catalog, parses it with `SpacePacket::parse`, asserts every
//! header field, then rebuilds via `PacketBuilder` and confirms the output
//! matches the wire bytes bit-for-bit. Covers Q-C8 (BE-only wire encoding).

// Test-only conveniences: workspace denies these for production code.
// doc_markdown: field=value pairs in doc comments are intentional annotations.
#![allow(
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used,
    clippy::doc_markdown
)]

use ccsds_wire::{
    Apid, Cuc, FuncCode, InstanceId, PacketBuilder, PacketType, SequenceCount, SpacePacket,
};

// ---------------------------------------------------------------------------
// PKT-TM-0100-0002  Sample App HK  (26 bytes, APID=0x100)
// ---------------------------------------------------------------------------

/// Hand-rolled wire bytes for PKT-TM-0100-0002 (packet-catalog.md §4.2).
///
/// Primary  : version=0, TM, sec_hdr=1, APID=0x100, seq_flags=standalone,
///            seq=0x0042, data_length=19.
/// Secondary: CUC coarse=0x1234_5678, fine=0x9ABC, func_code=0x0002,
///            instance_id=0x01.
/// User data: 10 bytes [0x01..=0x0A] (sample_app HK payload placeholder).
const PKT_TM_0100_0002: &[u8] = &[
    // Primary header (6 B)
    0x09, 0x00, // version=0, TM, sec_hdr=1, APID=0x100
    0xC0, 0x42, // seq_flags=standalone, seq=0x0042
    0x00, 0x13, // data_length=19
    // Secondary header (10 B)
    0x2F, // P_FIELD
    0x12, 0x34, 0x56, 0x78, // coarse=0x1234_5678 (BE)
    0x9A, 0xBC, // fine=0x9ABC (BE)
    0x00, 0x02, // func_code=0x0002 (BE)
    0x01, // instance_id=0x01
    // User data (10 B)
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
];

#[test]
fn kat_pkt_tm_0100_0002() {
    let pkt = SpacePacket::parse(PKT_TM_0100_0002).unwrap();

    assert_eq!(pkt.primary.apid(), Apid::new(0x100).unwrap());
    assert_eq!(pkt.primary.packet_type(), PacketType::Tm);
    assert_eq!(
        pkt.primary.sequence_count(),
        SequenceCount::new(0x0042).unwrap()
    );
    assert_eq!(
        pkt.secondary.time(),
        Cuc {
            coarse: 0x1234_5678,
            fine: 0x9ABC
        }
    );
    assert_eq!(pkt.secondary.func_code(), FuncCode::new(0x0002).unwrap());
    assert_eq!(pkt.secondary.instance_id(), InstanceId::new(0x01).unwrap());
    assert_eq!(
        pkt.user_data,
        &[0x01u8, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A]
    );

    let rebuilt = PacketBuilder::tm(Apid::new(0x100).unwrap())
        .sequence_count(SequenceCount::new(0x0042).unwrap())
        .func_code(FuncCode::new(0x0002).unwrap())
        .instance_id(InstanceId::new(0x01).unwrap())
        .cuc(Cuc {
            coarse: 0x1234_5678,
            fine: 0x9ABC,
        })
        .user_data(&[0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A])
        .build()
        .unwrap();
    assert_eq!(rebuilt.as_slice(), PKT_TM_0100_0002);
}

// ---------------------------------------------------------------------------
// PKT-TM-0101-0002  Orbiter CDH HK  (32 bytes, APID=0x101)
// ---------------------------------------------------------------------------

/// Hand-rolled wire bytes for PKT-TM-0101-0002 (packet-catalog.md §4.2).
///
/// Primary  : TM, APID=0x101, seq=0x0000, data_length=25.
/// Secondary: CUC coarse=0, fine=0, func_code=0x0002, instance_id=0x03.
/// User data: 16 zero bytes (CDH HK payload placeholder).
const PKT_TM_0101_0002: &[u8] = &[
    // Primary header (6 B)
    0x09, 0x01, // TM, sec_hdr=1, APID=0x101
    0xC0, 0x00, // seq_flags=standalone, seq=0x0000
    0x00, 0x19, // data_length=25
    // Secondary header (10 B)
    0x2F, 0x00, 0x00, 0x00, 0x00, // P_FIELD + coarse=0
    0x00, 0x00, // fine=0
    0x00, 0x02, // func_code=0x0002
    0x03, // instance_id=0x03
    // User data (16 B — CDH HK payload placeholder)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
];

#[test]
fn kat_pkt_tm_0101_0002() {
    let pkt = SpacePacket::parse(PKT_TM_0101_0002).unwrap();

    assert_eq!(pkt.primary.apid(), Apid::new(0x101).unwrap());
    assert_eq!(pkt.primary.packet_type(), PacketType::Tm);
    assert_eq!(
        pkt.primary.sequence_count(),
        SequenceCount::new(0x0000).unwrap()
    );
    assert_eq!(pkt.secondary.time(), Cuc { coarse: 0, fine: 0 });
    assert_eq!(pkt.secondary.func_code(), FuncCode::new(0x0002).unwrap());
    assert_eq!(pkt.secondary.instance_id(), InstanceId::new(0x03).unwrap());
    assert_eq!(pkt.user_data, &[0u8; 16]);

    let rebuilt = PacketBuilder::tm(Apid::new(0x101).unwrap())
        .sequence_count(SequenceCount::new(0x0000).unwrap())
        .func_code(FuncCode::new(0x0002).unwrap())
        .instance_id(InstanceId::new(0x03).unwrap())
        .cuc(Cuc { coarse: 0, fine: 0 })
        .user_data(&[0u8; 16])
        .build()
        .unwrap();
    assert_eq!(rebuilt.as_slice(), PKT_TM_0101_0002);
}

// ---------------------------------------------------------------------------
// PKT-TM-0110-0002  Orbiter ADCS State  (60 bytes, APID=0x110)
// ---------------------------------------------------------------------------

/// Hand-rolled wire bytes for PKT-TM-0110-0002 (packet-catalog.md §4.2).
///
/// Primary  : TM, APID=0x110, seq=0x0007, data_length=53.
/// Secondary: CUC coarse=0xDEADBEEF, fine=0xCAFE, func_code=0x0002,
///            instance_id=0x02.
/// User data: 44 bytes [0x00..=0x2B] (ADCS state payload placeholder).
const PKT_TM_0110_0002: &[u8] = &[
    // Primary header (6 B)
    0x09, 0x10, // TM, sec_hdr=1, APID=0x110
    0xC0, 0x07, // seq_flags=standalone, seq=0x0007
    0x00, 0x35, // data_length=53
    // Secondary header (10 B)
    0x2F, 0xDE, 0xAD, 0xBE, 0xEF, // P_FIELD + coarse=0xDEADBEEF
    0xCA, 0xFE, // fine=0xCAFE
    0x00, 0x02, // func_code=0x0002
    0x02, // instance_id=0x02
    // User data (44 B — ADCS state payload placeholder: sequential 0x00..0x2B)
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
];

#[test]
fn kat_pkt_tm_0110_0002() {
    let pkt = SpacePacket::parse(PKT_TM_0110_0002).unwrap();

    assert_eq!(pkt.primary.apid(), Apid::new(0x110).unwrap());
    assert_eq!(pkt.primary.packet_type(), PacketType::Tm);
    assert_eq!(
        pkt.primary.sequence_count(),
        SequenceCount::new(0x0007).unwrap()
    );
    assert_eq!(
        pkt.secondary.time(),
        Cuc {
            coarse: 0xDEAD_BEEF,
            fine: 0xCAFE
        }
    );
    assert_eq!(pkt.secondary.func_code(), FuncCode::new(0x0002).unwrap());
    assert_eq!(pkt.secondary.instance_id(), InstanceId::new(0x02).unwrap());
    let expected_user: Vec<u8> = (0u8..44u8).collect();
    assert_eq!(pkt.user_data, expected_user.as_slice());

    let rebuilt = PacketBuilder::tm(Apid::new(0x110).unwrap())
        .sequence_count(SequenceCount::new(0x0007).unwrap())
        .func_code(FuncCode::new(0x0002).unwrap())
        .instance_id(InstanceId::new(0x02).unwrap())
        .cuc(Cuc {
            coarse: 0xDEAD_BEEF,
            fine: 0xCAFE,
        })
        .user_data(&expected_user)
        .build()
        .unwrap();
    assert_eq!(rebuilt.as_slice(), PKT_TM_0110_0002);
}

// ---------------------------------------------------------------------------
// PKT-TM-0400-0004  Cryobot HK BW-Collapse  (24 bytes, APID=0x400)
// ---------------------------------------------------------------------------

/// Hand-rolled wire bytes for PKT-TM-0400-0004 (packet-catalog.md §4.2).
///
/// NOTE: The catalog defines this packet ID as the cryobot BW-collapse variant
/// using a reduced 4-byte secondary header (§1.4). `ccsds_wire` v1.0 only
/// implements the standard 10-byte secondary header; the reduced variant is
/// deferred to the cryobot tether crate. This KAT exercises the standard
/// layout with the correct APID (0x400) and func_code (0x0004).
///
/// Primary  : TM, APID=0x400, seq=0x0000, data_length=17.
/// Secondary: CUC coarse=100 (0x64), fine=0, func_code=0x0004,
///            instance_id=0x01.
/// User data: 8 bytes (cryobot HK BW-collapse payload:
///            depth=150 m, drill_rpm=500, temp byte, mode, fault, reserved).
const PKT_TM_0400_0004: &[u8] = &[
    // Primary header (6 B)
    0x0C, 0x00, // TM, sec_hdr=1, APID=0x400
    0xC0, 0x00, // seq_flags=standalone, seq=0x0000
    0x00, 0x11, // data_length=17
    // Secondary header (10 B)
    0x2F, 0x00, 0x00, 0x00, 0x64, // P_FIELD + coarse=100
    0x00, 0x00, // fine=0
    0x00, 0x04, // func_code=0x0004
    0x01, // instance_id=0x01
    // User data (8 B — BW-collapse cryobot HK)
    0x00, 0x96, // depth_m = 150 (u16 BE)
    0x01, 0xF4, // drill_rpm = 500 (i16 BE)
    0x80, // borehole_temp byte
    0x01, // mode = 0x01
    0x00, // fault_mask = 0x00
    0x00, // reserved
];

#[test]
fn kat_pkt_tm_0400_0004() {
    let pkt = SpacePacket::parse(PKT_TM_0400_0004).unwrap();

    assert_eq!(pkt.primary.apid(), Apid::new(0x400).unwrap());
    assert_eq!(pkt.primary.packet_type(), PacketType::Tm);
    assert_eq!(
        pkt.primary.sequence_count(),
        SequenceCount::new(0x0000).unwrap()
    );
    assert_eq!(
        pkt.secondary.time(),
        Cuc {
            coarse: 100,
            fine: 0
        }
    );
    assert_eq!(pkt.secondary.func_code(), FuncCode::new(0x0004).unwrap());
    assert_eq!(pkt.secondary.instance_id(), InstanceId::new(0x01).unwrap());
    assert_eq!(
        pkt.user_data,
        &[0x00u8, 0x96, 0x01, 0xF4, 0x80, 0x01, 0x00, 0x00]
    );

    let rebuilt = PacketBuilder::tm(Apid::new(0x400).unwrap())
        .sequence_count(SequenceCount::new(0x0000).unwrap())
        .func_code(FuncCode::new(0x0004).unwrap())
        .instance_id(InstanceId::new(0x01).unwrap())
        .cuc(Cuc {
            coarse: 100,
            fine: 0,
        })
        .user_data(&[0x00, 0x96, 0x01, 0xF4, 0x80, 0x01, 0x00, 0x00])
        .build()
        .unwrap();
    assert_eq!(rebuilt.as_slice(), PKT_TM_0400_0004);
}

// ---------------------------------------------------------------------------
// PKT-TC-0184-8100  Orbiter Power Load Switch  (20 bytes, APID=0x184)
// ---------------------------------------------------------------------------

/// Hand-rolled wire bytes for PKT-TC-0184-8100 (packet-catalog.md §5).
///
/// Primary  : TC, APID=0x184, seq=0x0000, data_length=13.
/// Secondary: CUC coarse=0x000007E0, fine=0x4000, func_code=0x8100
///            (high bit = arm-then-fire required), instance_id=0x04.
/// User data: 4 bytes (switch_index=3, new_state=ON, confirm_magic=0xC0DE).
const PKT_TC_0184_8100: &[u8] = &[
    // Primary header (6 B)
    0x19, 0x84, // TC, sec_hdr=1, APID=0x184
    0xC0, 0x00, // seq_flags=standalone, seq=0x0000
    0x00, 0x0D, // data_length=13
    // Secondary header (10 B)
    0x2F, 0x00, 0x00, 0x07, 0xE0, // P_FIELD + coarse=0x000007E0
    0x40, 0x00, // fine=0x4000
    0x81, 0x00, // func_code=0x8100
    0x04, // instance_id=0x04
    // User data (4 B — power load-switch command)
    0x03, // switch_index = 3
    0x01, // new_state = ON
    0xC0, 0xDE, // confirm_magic = 0xC0DE (BE)
];

#[test]
fn kat_pkt_tc_0184_8100() {
    let pkt = SpacePacket::parse(PKT_TC_0184_8100).unwrap();

    assert_eq!(pkt.primary.apid(), Apid::new(0x184).unwrap());
    assert_eq!(pkt.primary.packet_type(), PacketType::Tc);
    assert_eq!(
        pkt.primary.sequence_count(),
        SequenceCount::new(0x0000).unwrap()
    );
    assert_eq!(
        pkt.secondary.time(),
        Cuc {
            coarse: 0x0000_07E0,
            fine: 0x4000
        }
    );
    assert_eq!(pkt.secondary.func_code(), FuncCode::new(0x8100).unwrap());
    assert_eq!(pkt.secondary.instance_id(), InstanceId::new(0x04).unwrap());
    assert_eq!(pkt.user_data, &[0x03u8, 0x01, 0xC0, 0xDE]);

    let rebuilt = PacketBuilder::tc(Apid::new(0x184).unwrap())
        .sequence_count(SequenceCount::new(0x0000).unwrap())
        .func_code(FuncCode::new(0x8100).unwrap())
        .instance_id(InstanceId::new(0x04).unwrap())
        .cuc(Cuc {
            coarse: 0x0000_07E0,
            fine: 0x4000,
        })
        .user_data(&[0x03, 0x01, 0xC0, 0xDE])
        .build()
        .unwrap();
    assert_eq!(rebuilt.as_slice(), PKT_TC_0184_8100);
}
