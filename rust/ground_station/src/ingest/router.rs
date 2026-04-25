//! APID-based packet router — ingest pipeline stage 4.
//!
//! Dispatches decoded [`SpacePacket`]s to typed sink routes per
//! `docs/architecture/06-ground-segment-rust.md §5.4`.
//!
//! **Security-critical:** enforces Q-F2 / SYS-REQ-0041 by rejecting fault-inject
//! APIDs `0x540`–`0x543` (and the full sim/ground-internal blocks) before any
//! positive routing is attempted.

use ccsds_wire::SpacePacket;
use std::collections::HashMap;

// ---------------------------------------------------------------------------
// Forbidden APID ranges (Q-F2, SYS-REQ-0041) — checked before positive routing
// ---------------------------------------------------------------------------

const FAULT_INJECT_LO: u16 = 0x540;
const FAULT_INJECT_HI: u16 = 0x543;

const SIM_SENSOR_LO: u16 = 0x500;
const SIM_SENSOR_HI: u16 = 0x53F;

const SIM_DIAG_LO: u16 = 0x544;
const SIM_DIAG_HI: u16 = 0x57F;

const GND_INT_LO: u16 = 0x600;
const GND_INT_HI: u16 = 0x67F;

// ---------------------------------------------------------------------------
// Positive routing APID ranges (docs/interfaces/apid-registry.md + §5.4)
// ---------------------------------------------------------------------------

const ORBITER_TM_LO: u16 = 0x100;
const ORBITER_TM_HI: u16 = 0x17F;

const RELAY_TM_LO: u16 = 0x200;
const RELAY_TM_HI: u16 = 0x27F;

const MCU_TM_LO: u16 = 0x280;
const MCU_TM_HI: u16 = 0x2FF;

const ROVER_TM_LO: u16 = 0x300;
const ROVER_TM_HI: u16 = 0x45F;

const IDLE_FILL_APID: u16 = 0x7FF;

// ---------------------------------------------------------------------------
// VC discriminants for VC-gated sinks (docs/interfaces/apid-registry.md)
// ---------------------------------------------------------------------------

/// VC 1 carries the EVS event-log stream exclusively.
const VC_EVENT_LOG: u8 = 1;

/// VC 2 carries CFDP Class 1 file-transfer PDUs exclusively.
const VC_CFDP: u8 = 2;

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

/// Reason a packet was rejected by [`ApidRouter`].
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum RejectReason {
    /// APID ∈ `0x540`–`0x543`: Gazebo fault-inject sideband, forbidden on RF (Q-F2, SYS-REQ-0041).
    ForbiddenFaultInjectApid,
    /// APID ∈ `0x500`–`0x53F` or `0x544`–`0x57F`: Gazebo sim injection, forbidden on RF.
    ForbiddenSimApid,
    /// APID ∈ `0x600`–`0x67F`: ground-internal, never on any RF link.
    ForbiddenGroundInternal,
    /// APID does not match any known registry entry.
    UnknownBlock,
}

/// Routing decision returned by [`ApidRouter::route`] for each decoded space packet.
///
/// Routing table: `docs/architecture/06-ground-segment-rust.md §5.4`.
/// Fault-injection APID rejection: §8.2 / Q-F2.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Route {
    /// Housekeeping telemetry → HK ring-buffer sink.
    Hk,
    /// Event record → persistent event-log sink (VC 1 EVS stream).
    EventLog,
    /// CFDP protocol data unit → `Class1Receiver::on_pdu` (VC 2 only).
    CfdpPdu,
    /// Rover telemetry → rover-forward archive.
    RoverForward,
    /// Idle fill (APID `0x7FF`) → silently discarded; no counter, no event.
    IdleFill,
    /// Rejected packet — see [`RejectReason`] for the triggering rule.
    Rejected { reason: RejectReason },
}

/// Ingest pipeline stage 4: APID-based packet router.
///
/// Call [`ApidRouter::route`] once per decoded packet. The router is
/// intentionally synchronous — it makes a pure routing decision with no I/O.
/// Downstream sink dispatch (channel sends) is the caller's responsibility.
///
/// Maintains a per-APID `forbidden_apid_seen_total` counter as required by
/// `docs/architecture/06-ground-segment-rust.md §8.2`.
pub struct ApidRouter {
    forbidden_seen: HashMap<u16, u64>,
}

impl ApidRouter {
    /// Construct a new [`ApidRouter`] with zero counters.
    #[must_use]
    pub fn new() -> Self {
        Self {
            forbidden_seen: HashMap::new(),
        }
    }

    /// Route a decoded space packet to its downstream sink.
    ///
    /// Security checks (Q-F2, SYS-REQ-0041) are evaluated **first** — before
    /// any positive routing — so a forbidden APID can never reach a HK store,
    /// event log, or CFDP receiver regardless of virtual channel.
    pub fn route(&mut self, vc_id: u8, pkt: &SpacePacket<'_>) -> Route {
        let apid = pkt.primary.apid().get();

        // 1. Security rejection — must precede all positive routing (§8.2, Q-F2)
        if (FAULT_INJECT_LO..=FAULT_INJECT_HI).contains(&apid) {
            *self.forbidden_seen.entry(apid).or_insert(0) += 1;
            return Route::Rejected {
                reason: RejectReason::ForbiddenFaultInjectApid,
            };
        }
        if (SIM_SENSOR_LO..=SIM_SENSOR_HI).contains(&apid)
            || (SIM_DIAG_LO..=SIM_DIAG_HI).contains(&apid)
        {
            *self.forbidden_seen.entry(apid).or_insert(0) += 1;
            return Route::Rejected {
                reason: RejectReason::ForbiddenSimApid,
            };
        }
        if (GND_INT_LO..=GND_INT_HI).contains(&apid) {
            *self.forbidden_seen.entry(apid).or_insert(0) += 1;
            return Route::Rejected {
                reason: RejectReason::ForbiddenGroundInternal,
            };
        }

        // 2. Idle fill — silent discard (no counter, no event per §5.4)
        if apid == IDLE_FILL_APID {
            return Route::IdleFill;
        }

        // 3. VC-gated sinks (VC is the authoritative discriminant for these streams)
        if vc_id == VC_CFDP {
            return Route::CfdpPdu;
        }
        if vc_id == VC_EVENT_LOG {
            return Route::EventLog;
        }

        // 4. APID-range dispatch (§5.4 routing table)
        match apid {
            ORBITER_TM_LO..=ORBITER_TM_HI | RELAY_TM_LO..=RELAY_TM_HI | MCU_TM_LO..=MCU_TM_HI => {
                Route::Hk
            }
            ROVER_TM_LO..=ROVER_TM_HI => Route::RoverForward,
            _ => Route::Rejected {
                reason: RejectReason::UnknownBlock,
            },
        }
    }

    /// Number of times a forbidden APID has been seen since construction.
    ///
    /// Labeled-metric accessor per §8.2. Returns `0` for any APID that has
    /// never triggered a rejection.
    #[must_use]
    pub fn forbidden_apid_seen_total(&self, apid: u16) -> u64 {
        self.forbidden_seen.get(&apid).copied().unwrap_or(0)
    }
}

impl Default for ApidRouter {
    fn default() -> Self {
        Self::new()
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::cast_possible_truncation
)]
mod tests {
    use super::*;
    use ccsds_wire::{Apid, FuncCode, InstanceId, PacketBuilder};

    fn make_pkt_bytes(apid_raw: u16) -> Vec<u8> {
        PacketBuilder::tm(Apid::new(apid_raw).unwrap())
            .func_code(FuncCode::new(0x0001).unwrap())
            .instance_id(InstanceId::new(1).unwrap())
            .build()
            .unwrap()
    }

    // -----------------------------------------------------------------------
    // RED test: rejects_fault_apids_on_rf
    //
    // Given  a fresh ApidRouter
    // And    a SpacePacket with APID ∈ {0x540, 0x541, 0x542, 0x543}
    // And    any vc_id (rejection is VC-agnostic per §8.2)
    // When   route() is called
    // Then   Route::Rejected { ForbiddenFaultInjectApid } is returned
    // And    forbidden_apid_seen_total(apid) == 1
    // -----------------------------------------------------------------------
    #[test]
    fn rejects_fault_apids_on_rf() {
        for apid_raw in [0x540_u16, 0x541, 0x542, 0x543] {
            let mut router = ApidRouter::new();
            let bytes = make_pkt_bytes(apid_raw);
            let pkt = SpacePacket::parse(&bytes).unwrap();

            // Test on VC 0 — rejection must be VC-agnostic
            assert_eq!(
                router.route(0, &pkt),
                Route::Rejected {
                    reason: RejectReason::ForbiddenFaultInjectApid
                },
                "APID 0x{apid_raw:03X} must be rejected on VC 0",
            );
            assert_eq!(
                router.forbidden_apid_seen_total(apid_raw),
                1,
                "counter must be 1 after one rejection for APID 0x{apid_raw:03X}",
            );

            // Confirm VC-agnostic: also rejected on VC 3
            let mut router2 = ApidRouter::new();
            assert_eq!(
                router2.route(3, &pkt),
                Route::Rejected {
                    reason: RejectReason::ForbiddenFaultInjectApid
                },
                "APID 0x{apid_raw:03X} must be rejected on VC 3 too",
            );
        }
    }

    // -----------------------------------------------------------------------
    // Sim sensor injection block (0x500–0x53F) → ForbiddenSimApid
    // -----------------------------------------------------------------------
    #[test]
    fn rejects_sim_sensor_apids() {
        for apid_raw in [0x500_u16, 0x51F, 0x53F] {
            let mut router = ApidRouter::new();
            let bytes = make_pkt_bytes(apid_raw);
            let pkt = SpacePacket::parse(&bytes).unwrap();
            assert_eq!(
                router.route(0, &pkt),
                Route::Rejected {
                    reason: RejectReason::ForbiddenSimApid
                },
                "APID 0x{apid_raw:03X}",
            );
            assert_eq!(router.forbidden_apid_seen_total(apid_raw), 1);
        }
    }

    // -----------------------------------------------------------------------
    // Sim diagnostic remainder (0x544–0x57F) → ForbiddenSimApid
    // -----------------------------------------------------------------------
    #[test]
    fn rejects_sim_diag_apids() {
        for apid_raw in [0x544_u16, 0x560, 0x57F] {
            let mut router = ApidRouter::new();
            let bytes = make_pkt_bytes(apid_raw);
            let pkt = SpacePacket::parse(&bytes).unwrap();
            assert_eq!(
                router.route(0, &pkt),
                Route::Rejected {
                    reason: RejectReason::ForbiddenSimApid
                },
                "APID 0x{apid_raw:03X}",
            );
            assert_eq!(router.forbidden_apid_seen_total(apid_raw), 1);
        }
    }

    // -----------------------------------------------------------------------
    // Ground-internal block (0x600–0x67F) → ForbiddenGroundInternal
    // -----------------------------------------------------------------------
    #[test]
    fn rejects_ground_internal_apids() {
        for apid_raw in [0x600_u16, 0x640, 0x67F] {
            let mut router = ApidRouter::new();
            let bytes = make_pkt_bytes(apid_raw);
            let pkt = SpacePacket::parse(&bytes).unwrap();
            assert_eq!(
                router.route(0, &pkt),
                Route::Rejected {
                    reason: RejectReason::ForbiddenGroundInternal
                },
                "APID 0x{apid_raw:03X}",
            );
            assert_eq!(router.forbidden_apid_seen_total(apid_raw), 1);
        }
    }

    // -----------------------------------------------------------------------
    // Orbiter HK range (0x100–0x17F) on VC 0 → Hk
    // -----------------------------------------------------------------------
    #[test]
    fn routes_orbiter_hk() {
        let mut router = ApidRouter::new();
        for apid_raw in [0x100_u16, 0x140, 0x17F] {
            let bytes = make_pkt_bytes(apid_raw);
            let pkt = SpacePacket::parse(&bytes).unwrap();
            assert_eq!(router.route(0, &pkt), Route::Hk, "APID 0x{apid_raw:03X}");
        }
    }

    // -----------------------------------------------------------------------
    // Relay TM range (0x200–0x27F) → Hk
    // -----------------------------------------------------------------------
    #[test]
    fn routes_relay_hk() {
        let mut router = ApidRouter::new();
        for apid_raw in [0x200_u16, 0x21F, 0x27F] {
            let bytes = make_pkt_bytes(apid_raw);
            let pkt = SpacePacket::parse(&bytes).unwrap();
            assert_eq!(router.route(0, &pkt), Route::Hk, "APID 0x{apid_raw:03X}");
        }
    }

    // -----------------------------------------------------------------------
    // MCU TM range (0x280–0x2FF) → Hk
    // -----------------------------------------------------------------------
    #[test]
    fn routes_mcu_hk() {
        let mut router = ApidRouter::new();
        for apid_raw in [0x280_u16, 0x290, 0x2FF] {
            let bytes = make_pkt_bytes(apid_raw);
            let pkt = SpacePacket::parse(&bytes).unwrap();
            assert_eq!(router.route(0, &pkt), Route::Hk, "APID 0x{apid_raw:03X}");
        }
    }

    // -----------------------------------------------------------------------
    // Rover TM range (0x300–0x45F) → RoverForward
    // -----------------------------------------------------------------------
    #[test]
    fn routes_rover_forward() {
        let mut router = ApidRouter::new();
        for apid_raw in [0x300_u16, 0x380, 0x45F] {
            let bytes = make_pkt_bytes(apid_raw);
            let pkt = SpacePacket::parse(&bytes).unwrap();
            assert_eq!(
                router.route(3, &pkt),
                Route::RoverForward,
                "APID 0x{apid_raw:03X}",
            );
        }
    }

    // -----------------------------------------------------------------------
    // VC 2 → CfdpPdu (regardless of APID, after security checks pass)
    // -----------------------------------------------------------------------
    #[test]
    fn routes_cfdp_vc2() {
        let mut router = ApidRouter::new();
        // Use an orbiter-range APID arriving on VC 2 — VC gates before APID range
        let bytes = make_pkt_bytes(0x100);
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(router.route(2, &pkt), Route::CfdpPdu);
    }

    // -----------------------------------------------------------------------
    // VC 1 → EventLog (EVS stream; VC gates before APID-range dispatch)
    // -----------------------------------------------------------------------
    #[test]
    fn routes_event_log_vc1() {
        let mut router = ApidRouter::new();
        let bytes = make_pkt_bytes(0x101);
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(router.route(1, &pkt), Route::EventLog);
    }

    // -----------------------------------------------------------------------
    // Idle fill (APID 0x7FF) → IdleFill; no counter incremented
    // -----------------------------------------------------------------------
    #[test]
    fn routes_idle_fill() {
        let mut router = ApidRouter::new();
        let bytes = make_pkt_bytes(0x7FF);
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(router.route(63, &pkt), Route::IdleFill);
        // No counter incremented for idle fill
        assert_eq!(router.forbidden_apid_seen_total(0x7FF), 0);
    }

    // -----------------------------------------------------------------------
    // Unknown APID (gap between rover and sim blocks) → UnknownBlock
    // -----------------------------------------------------------------------
    #[test]
    fn rejects_unknown_apid() {
        let mut router = ApidRouter::new();
        // 0x460 is between rover TM hi (0x45F) and sim sensor lo (0x500)
        let bytes = make_pkt_bytes(0x460);
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(
            router.route(0, &pkt),
            Route::Rejected {
                reason: RejectReason::UnknownBlock
            },
        );
        // UnknownBlock does not increment the forbidden counter
        assert_eq!(router.forbidden_apid_seen_total(0x460), 0);
    }

    // -----------------------------------------------------------------------
    // Counter accumulates across multiple calls for the same forbidden APID
    // -----------------------------------------------------------------------
    #[test]
    fn forbidden_counter_accumulates() {
        let mut router = ApidRouter::new();
        let bytes = make_pkt_bytes(0x540);
        let pkt = SpacePacket::parse(&bytes).unwrap();

        for expected in 1_u64..=3 {
            router.route(0, &pkt);
            assert_eq!(
                router.forbidden_apid_seen_total(0x540),
                expected,
                "after {expected} calls",
            );
        }
    }

    // -----------------------------------------------------------------------
    // forbidden_apid_seen_total returns 0 for unseen APID
    // -----------------------------------------------------------------------
    #[test]
    fn counter_zero_for_unseen_apid() {
        let router = ApidRouter::new();
        assert_eq!(router.forbidden_apid_seen_total(0x540), 0);
        assert_eq!(router.forbidden_apid_seen_total(0x100), 0);
    }

    // -----------------------------------------------------------------------
    // Default impl delegates to new()
    // -----------------------------------------------------------------------
    #[test]
    fn default_equals_new() {
        let r: ApidRouter = ApidRouter::default();
        assert_eq!(r.forbidden_apid_seen_total(0x540), 0);
    }

    // -----------------------------------------------------------------------
    // Phase 39 integration: fault_injector ICD-compliant clock-skew SPP
    // rejected by ApidRouter on any RF VC (wires the Phase 25 router into
    // the Phase 39 fault-inject pipeline per DoD §3).
    //
    // Given  an ICD-sim-fsw.md §3.2-compliant PKT-SIM-0541-0001 payload:
    //   asset_class=0 (u8), instance_id=1 (u8), offset_ms=500 (BE i32),
    //   rate_ppm_x1000=0 (BE i32), duration_s=10 (BE u32), crc16 (u16)
    // When   routed on VC 0 (RF uplink path)
    // Then   Route::Rejected { ForbiddenFaultInjectApid } is returned
    // And    forbidden_apid_seen_total(0x541) == 1
    // Q-C8: PacketBuilder is the sole BE encoding locus; no raw to_be_bytes.
    // Q-F3: transient sim state — excluded from Vault<T> per §5.2.
    // -----------------------------------------------------------------------
    #[test]
    fn phase39_fault_inject_clock_skew_spp_rejected_on_rf() {
        let user_data: [u8; 16] = [
            0x00, // asset_class = 0
            0x01, // instance_id = 1
            0x00, 0x00, 0x01, 0xF4, // offset_ms = 500 (BE i32)
            0x00, 0x00, 0x00, 0x00, // rate_ppm_x1000 = 0 (BE i32)
            0x00, 0x00, 0x00, 0x0A, // duration_s = 10 (BE u32)
            0x00, 0x00, // crc16 placeholder (router rejects on APID; ignores payload)
        ];
        let bytes = PacketBuilder::tm(Apid::new(0x0541).unwrap())
            .func_code(FuncCode::new(0x0001).unwrap())
            .instance_id(InstanceId::new(1).unwrap())
            .user_data(&user_data)
            .build()
            .unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();
        let mut router = ApidRouter::new();
        assert_eq!(
            router.route(0, &pkt),
            Route::Rejected {
                reason: RejectReason::ForbiddenFaultInjectApid,
            },
        );
        assert_eq!(router.forbidden_apid_seen_total(0x541), 1);
    }
}
