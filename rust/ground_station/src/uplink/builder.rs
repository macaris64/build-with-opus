//! `TcBuilder` — catalog validation and SPP serialization for TC uplink.
//!
//! Maps typed operator intent ([`TcIntent`]) to a validated CCSDS Space Packet
//! byte buffer. Every (`APID`, `func_code`, `instance_id`, `user_data`) tuple is a
//! compile-time constant derived from `docs/interfaces/packet-catalog.md §5–6`;
//! no catalog file I/O occurs at runtime.
//!
//! # Q-C8 compliance
//!
//! All user-data fields wider than 1 byte are serialized with `.to_be_bytes()`
//! (big-endian, per Q-C8). The header itself is encoded by
//! `ccsds_wire::PacketBuilder`, which is the sole Q-C8 locus for CCSDS headers.

use ccsds_wire::{Apid, Cuc, FuncCode, InstanceId, PacketBuilder, SequenceCount};

// ---------------------------------------------------------------------------
// Safety limit constants (packet-catalog.md §6.2)
// ---------------------------------------------------------------------------

/// Maximum permissible cryobot drill current limit in the `SetDrillRpm` TC.
///
/// The catalog forbids `max_current_10ma > 1000` (= 10 A) to protect the
/// drill motor from thermal runaway (packet-catalog.md §6.2).
const CRYOBOT_DRILL_MAX_CURRENT_10MA: u16 = 1000;

// ---------------------------------------------------------------------------
// TcIntent
// ---------------------------------------------------------------------------

/// Typed operator intent — one variant per catalogued telecommand.
///
/// Variants cover the Phase-B minimum TC set from packet-catalog.md §5–6.
/// The enum structure prevents arbitrary APID injection; every arm maps to a
/// fixed, catalog-approved `(APID, func_code)` pair.
#[derive(Debug, Clone)]
pub enum TcIntent {
    /// `PKT-TC-0181-0100` — Orbiter CDH: Set Mode.
    OrbiterSetMode { new_mode: u8, reason_code: u8 },
    /// `PKT-TC-0181-0200` — Orbiter CDH: Event Filter Set.
    OrbiterEventFilter {
        filter_app_id: u8,
        min_event_type: u8,
        event_mask: u32,
    },
    /// `PKT-TC-0182-0100` — Orbiter ADCS: Target Quaternion.
    OrbiterAdcsTargetQ { quaternion: [i32; 4], slew_mode: u8 },
    /// `PKT-TC-0184-8000` — Orbiter Power: Arm Load Switch.
    OrbiterPowerArm {
        switch_index: u8,
        confirm_magic: u16,
    },
    /// `PKT-TC-0184-8100` — Orbiter Power: Load Switch (safety-interlocked).
    OrbiterPowerSwitch {
        switch_index: u8,
        new_state: bool,
        confirm_magic: u16,
    },
    /// `PKT-TC-0440-0100` — Cryobot: Set Mode.
    CryobotSetMode { new_mode: u8, confirm_magic: u16 },
    /// `PKT-TC-0440-0300` — Cryobot: Request BW-Collapse HK.
    CryobotBwCollapse { enable: bool, duration_s: u16 },
    /// `PKT-TC-0440-8000` — Cryobot: Arm (shared).
    CryobotArm { confirm_magic: u16 },
    /// `PKT-TC-0440-8200` — Cryobot: Set Drill Target RPM (safety-interlocked).
    CryobotSetDrillRpm {
        target_rpm: i16,
        max_current_10ma: u16,
        confirm_magic: u16,
    },
}

// ---------------------------------------------------------------------------
// BuilderError
// ---------------------------------------------------------------------------

/// Errors returned by [`TcBuilder::build`].
#[derive(Debug, thiserror::Error)]
pub enum BuilderError {
    /// A parameter violates a catalog-defined safety limit.
    #[error("parameter out of safe range: {0}")]
    ForbiddenParam(&'static str),
    /// `ccsds_wire` encoding failed (should never happen for catalog-valid inputs).
    #[error("serialization failed: {0}")]
    Serialize(#[from] ccsds_wire::CcsdsError),
}

// ---------------------------------------------------------------------------
// TcBuilder
// ---------------------------------------------------------------------------

/// Stateless TC builder: validates and serializes operator intent.
pub struct TcBuilder;

impl TcBuilder {
    /// Build a validated CCSDS Space Packet from operator intent.
    ///
    /// # Errors
    ///
    /// - [`BuilderError::ForbiddenParam`] if a safety-critical parameter
    ///   exceeds its catalog-defined limit (e.g., cryobot drill current).
    /// - [`BuilderError::Serialize`] on `ccsds_wire` encoding failure (only
    ///   reachable via internal logic bugs — catalog constants are in range).
    pub fn build(intent: &TcIntent, seq: SequenceCount, now: Cuc) -> Result<Vec<u8>, BuilderError> {
        let (apid_raw, func_raw, instance_raw, user_data) = Self::decompose(intent)?;

        // All literal values are compile-time constants in their valid ranges;
        // the .map_err wraps the infallible-in-practice error path.
        let apid = Apid::new(apid_raw).map_err(BuilderError::Serialize)?;
        let func_code = FuncCode::new(func_raw).map_err(BuilderError::Serialize)?;
        let instance_id = InstanceId::new(instance_raw).map_err(BuilderError::Serialize)?;

        PacketBuilder::tc(apid)
            .func_code(func_code)
            .instance_id(instance_id)
            .cuc(now)
            .sequence_count(seq)
            .user_data(&user_data)
            .build()
            .map_err(BuilderError::Serialize)
    }

    /// Map [`TcIntent`] to `(apid_raw, func_raw, instance_raw, user_data)`.
    ///
    /// Returns `Err(BuilderError::ForbiddenParam)` for out-of-range safety params.
    fn decompose(intent: &TcIntent) -> Result<(u16, u16, u8, Vec<u8>), BuilderError> {
        match intent {
            // ── Orbiter CDH ─────────────────────────────────────────────────
            TcIntent::OrbiterSetMode {
                new_mode,
                reason_code,
            } => Ok((0x181, 0x0100, 1, vec![*new_mode, *reason_code])),

            TcIntent::OrbiterEventFilter {
                filter_app_id,
                min_event_type,
                event_mask,
            } => {
                let mut ud = vec![*filter_app_id, *min_event_type];
                ud.extend_from_slice(&event_mask.to_be_bytes());
                Ok((0x181, 0x0200, 1, ud))
            }

            // ── Orbiter ADCS ─────────────────────────────────────────────────
            TcIntent::OrbiterAdcsTargetQ {
                quaternion,
                slew_mode,
            } => {
                let mut ud = Vec::with_capacity(18);
                for &q in quaternion {
                    ud.extend_from_slice(&q.to_be_bytes());
                }
                ud.push(*slew_mode);
                ud.push(0x00); // reserved per catalog §5.5
                Ok((0x182, 0x0100, 1, ud))
            }

            // ── Orbiter Power ─────────────────────────────────────────────────
            TcIntent::OrbiterPowerArm {
                switch_index,
                confirm_magic,
            } => {
                // catalog §5.7: switch_index (1 B) + confirm_magic (2 B BE)
                let mut ud = vec![*switch_index];
                ud.extend_from_slice(&confirm_magic.to_be_bytes());
                Ok((0x184, 0x8000, 1, ud))
            }

            TcIntent::OrbiterPowerSwitch {
                switch_index,
                new_state,
                confirm_magic,
            } => {
                // catalog §5.6: switch_index + new_state (bool_u8) + confirm_magic BE
                let mut ud = vec![*switch_index, u8::from(*new_state)];
                ud.extend_from_slice(&confirm_magic.to_be_bytes());
                Ok((0x184, 0x8100, 1, ud))
            }

            // ── Cryobot ──────────────────────────────────────────────────────
            TcIntent::CryobotSetMode {
                new_mode,
                confirm_magic,
            } => {
                // catalog §6.1: new_mode + reserved(0) + confirm_magic BE
                let mut ud = vec![*new_mode, 0x00];
                ud.extend_from_slice(&confirm_magic.to_be_bytes());
                Ok((0x440, 0x0100, 1, ud))
            }

            TcIntent::CryobotBwCollapse { enable, duration_s } => {
                // catalog §6.3: enable(bool_u8) + reserved(0) + duration_s BE
                let mut ud = vec![u8::from(*enable), 0x00];
                ud.extend_from_slice(&duration_s.to_be_bytes());
                Ok((0x440, 0x0300, 1, ud))
            }

            TcIntent::CryobotArm { confirm_magic } => {
                // catalog §6.4: confirm_magic = 0xCBA0 (2 B BE)
                let ud = confirm_magic.to_be_bytes().to_vec();
                Ok((0x440, 0x8000, 1, ud))
            }

            TcIntent::CryobotSetDrillRpm {
                target_rpm,
                max_current_10ma,
                confirm_magic,
            } => {
                // catalog §6.2: max_current_10ma > 1000 (10 A) is forbidden.
                if *max_current_10ma > CRYOBOT_DRILL_MAX_CURRENT_10MA {
                    return Err(BuilderError::ForbiddenParam(
                        "max_current_10ma > 1000 (10 A): exceeds cryobot drill current safety limit",
                    ));
                }
                let mut ud = Vec::with_capacity(8);
                ud.extend_from_slice(&target_rpm.to_be_bytes());
                ud.extend_from_slice(&max_current_10ma.to_be_bytes());
                ud.extend_from_slice(&confirm_magic.to_be_bytes());
                ud.extend_from_slice(&0u16.to_be_bytes()); // reserved per catalog §6.2
                Ok((0x440, 0x8200, 1, ud))
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used,
    clippy::indexing_slicing
)]
mod tests {
    use super::*;
    use ccsds_wire::{Cuc, SequenceCount, SpacePacket};

    fn seq0() -> SequenceCount {
        SequenceCount::new(0).unwrap()
    }

    fn t0() -> Cuc {
        Cuc { coarse: 0, fine: 0 }
    }

    // ── B1 ──────────────────────────────────────────────────────────────────
    // Given: valid OrbiterSetMode intent.
    // When: TcBuilder::build is called.
    // Then: output parses as CCSDS SPP with APID=0x181, func=0x0100,
    //       user_data=[new_mode, reason_code].
    #[test]
    fn b1_orbiter_set_mode_serializes_correctly() {
        let intent = TcIntent::OrbiterSetMode {
            new_mode: 2,
            reason_code: 7,
        };
        let bytes = TcBuilder::build(&intent, seq0(), t0()).unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.primary.apid().get(), 0x181);
        assert_eq!(pkt.secondary.func_code().get(), 0x0100);
        assert_eq!(pkt.user_data, [2u8, 7u8]);
        // TC bit set (byte 0 bit 4)
        assert!(bytes[0] & 0x10 != 0, "TC bit must be set in byte 0");
    }

    // ── B2 ──────────────────────────────────────────────────────────────────
    // Given: valid OrbiterAdcsTargetQ intent.
    // When: built.
    // Then: user-data encodes 4×i32 BE + slew_mode + reserved 0x00.
    #[test]
    fn b2_orbiter_adcs_target_q_encodes_quaternion_be() {
        let quaternion = [1_000_000_i32, -1_000_000, 500_000, -500_000];
        let intent = TcIntent::OrbiterAdcsTargetQ {
            quaternion,
            slew_mode: 1,
        };
        let bytes = TcBuilder::build(&intent, seq0(), t0()).unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.primary.apid().get(), 0x182);
        assert_eq!(pkt.secondary.func_code().get(), 0x0100);
        assert_eq!(pkt.user_data.len(), 18); // 4×4 + 1 + 1 reserved
                                             // Verify first quaternion component big-endian
        let q0 = i32::from_be_bytes(pkt.user_data[0..4].try_into().unwrap());
        assert_eq!(q0, 1_000_000);
        assert_eq!(pkt.user_data[16], 1); // slew_mode
        assert_eq!(pkt.user_data[17], 0); // reserved
    }

    // ── B3 ──────────────────────────────────────────────────────────────────
    // Given: valid OrbiterPowerSwitch intent with confirm_magic=0xC0DE.
    // When: built.
    // Then: user-data has switch_index, new_state, confirm_magic BE = 0xC0DE.
    #[test]
    fn b3_orbiter_power_switch_encodes_confirm_magic() {
        let intent = TcIntent::OrbiterPowerSwitch {
            switch_index: 2,
            new_state: true,
            confirm_magic: 0xC0DE,
        };
        let bytes = TcBuilder::build(&intent, seq0(), t0()).unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.primary.apid().get(), 0x184);
        assert_eq!(pkt.secondary.func_code().get(), 0x8100);
        assert_eq!(pkt.user_data[0], 2); // switch_index
        assert_eq!(pkt.user_data[1], 1); // new_state = true → 1
        let magic = u16::from_be_bytes(pkt.user_data[2..4].try_into().unwrap());
        assert_eq!(magic, 0xC0DE);
    }

    // ── B4 ──────────────────────────────────────────────────────────────────
    // Given: CryobotSetDrillRpm with max_current_10ma = 1001 (> 1000 limit).
    // When: built.
    // Then: BuilderError::ForbiddenParam is returned.
    #[test]
    fn b4_cryobot_drill_rpm_rejects_excess_current() {
        let intent = TcIntent::CryobotSetDrillRpm {
            target_rpm: 100,
            max_current_10ma: 1001, // exceeds 10 A limit
            confirm_magic: 0xCB02,
        };
        let err = TcBuilder::build(&intent, seq0(), t0()).unwrap_err();
        assert!(matches!(err, BuilderError::ForbiddenParam(_)));
    }

    // ── B5 ──────────────────────────────────────────────────────────────────
    // Boundary: max_current_10ma = 1000 is exactly at the limit → accepted.
    #[test]
    fn b5_cryobot_drill_rpm_accepts_max_current_at_limit() {
        let intent = TcIntent::CryobotSetDrillRpm {
            target_rpm: -50, // reverse for jam-recovery (negative is valid)
            max_current_10ma: 1000,
            confirm_magic: 0xCB02,
        };
        let bytes = TcBuilder::build(&intent, seq0(), t0()).unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.primary.apid().get(), 0x440);
        assert_eq!(pkt.secondary.func_code().get(), 0x8200);
        // target_rpm = -50 → big-endian i16
        let rpm = i16::from_be_bytes(pkt.user_data[0..2].try_into().unwrap());
        assert_eq!(rpm, -50);
    }

    // ── B6 ──────────────────────────────────────────────────────────────────
    // OrbiterEventFilter: event_mask serialized big-endian.
    #[test]
    fn b6_orbiter_event_filter_encodes_mask_be() {
        let intent = TcIntent::OrbiterEventFilter {
            filter_app_id: 0,
            min_event_type: 2, // INFO
            event_mask: 0xDEAD_BEEF,
        };
        let bytes = TcBuilder::build(&intent, seq0(), t0()).unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.primary.apid().get(), 0x181);
        assert_eq!(pkt.secondary.func_code().get(), 0x0200);
        let mask = u32::from_be_bytes(pkt.user_data[2..6].try_into().unwrap());
        assert_eq!(mask, 0xDEAD_BEEF);
    }

    // ── B7 ──────────────────────────────────────────────────────────────────
    // CryobotArm: confirm_magic stored BE.
    #[test]
    fn b7_cryobot_arm_encodes_confirm_magic() {
        let intent = TcIntent::CryobotArm {
            confirm_magic: 0xCBA0,
        };
        let bytes = TcBuilder::build(&intent, seq0(), t0()).unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.primary.apid().get(), 0x440);
        assert_eq!(pkt.secondary.func_code().get(), 0x8000);
        let magic = u16::from_be_bytes(pkt.user_data[0..2].try_into().unwrap());
        assert_eq!(magic, 0xCBA0);
    }
}
