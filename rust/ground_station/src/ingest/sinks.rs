//! Typed sink tasks: HK ring buffer and event log.
//!
//! Wires the `ApidRouter` sink channels to `UiState`, completing Link 1
//! (cFS → Ground) and Link 2 (Space ROS → Ground via VC3/RoverForward).
//!
//! # HK sink
//!
//! Merges the `Hk` and `RoverForward` sink channels into one updater.
//! Extracts APID and TAI timestamp from the CCSDS secondary header (Q-C6)
//! and upserts a [`HkSnapshot`] in `UiState::hk` keyed by APID.
//! Ring-buffer depth: [`HK_FRAMES_PER_APID`].
//!
//! # Event sink
//!
//! Consumes the `EventLog` sink channel (VC 1).  Extracts APID and TAI
//! timestamp; treats the payload after the 16-byte CCSDS header as the
//! EVS message text. Ring-buffer depth: [`EVENT_RING_MAX`].

use bytes::Bytes;
use ccsds_wire::SecondaryHeader;
use std::sync::Arc;
use tokio::sync::mpsc;

use crate::ui::{EventEntry, HkFrame, HkSnapshot, UiState};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/// Ring-buffer depth: maximum HK frames retained per APID in [`UiState::hk`].
pub const HK_FRAMES_PER_APID: usize = 20;

/// Ring-buffer cap: maximum event entries retained in [`UiState::events`].
pub const EVENT_RING_MAX: usize = 500;

/// CCSDS primary + secondary header size in bytes (6 + 10 = 16).
const HDR_LEN: usize = 16;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Spawn the HK sink task.
///
/// Merges `hk_rx` (APID 0x100–0x2FF) and `rover_rx` (APID 0x300–0x43F) into
/// a shared updater that writes to `UiState::hk`.  Both channels are consumed
/// concurrently; the task exits when both senders are dropped.
pub fn spawn_hk_sink(
    hk_rx: mpsc::Receiver<Bytes>,
    rover_rx: mpsc::Receiver<Bytes>,
    state: Arc<UiState>,
) -> tokio::task::JoinHandle<()> {
    tokio::spawn(async move {
        let state2 = state.clone();
        // Spawn the rover channel as an inner task so both run concurrently.
        tokio::spawn(async move {
            drain_hk(rover_rx, state2).await;
        });
        drain_hk(hk_rx, state).await;
    })
}

/// Spawn the event-log sink task.
///
/// Consumes `event_rx` (VC 1 `EventLog` packets) and appends [`EventEntry`]
/// items to `UiState::events`, evicting the oldest when the ring buffer is
/// full.  The task exits when the sender is dropped.
pub fn spawn_event_sink(
    event_rx: mpsc::Receiver<Bytes>,
    state: Arc<UiState>,
) -> tokio::task::JoinHandle<()> {
    tokio::spawn(drain_events(event_rx, state))
}

// ---------------------------------------------------------------------------
// Internal drain loops
// ---------------------------------------------------------------------------

async fn drain_hk(mut rx: mpsc::Receiver<Bytes>, state: Arc<UiState>) {
    while let Some(bytes) = rx.recv().await {
        process_hk_packet(&bytes, &state).await;
    }
}

async fn drain_events(mut rx: mpsc::Receiver<Bytes>, state: Arc<UiState>) {
    while let Some(bytes) = rx.recv().await {
        process_event_packet(&bytes, &state).await;
    }
}

// ---------------------------------------------------------------------------
// Packet processors
// ---------------------------------------------------------------------------

async fn process_hk_packet(bytes: &Bytes, state: &Arc<UiState>) {
    let Some(apid) = extract_apid(bytes) else { return };
    let timestamp_utc = extract_utc(bytes, &state.converter);
    let asset = apid_to_asset(apid);

    let frame = HkFrame {
        timestamp_utc,
        // Store the application payload only (strip the 16-byte CCSDS header
        // so the REST consumer sees semantic data, not wire framing).
        data: bytes.get(HDR_LEN..).map(<[u8]>::to_vec).unwrap_or_default(),
    };

    let mut hk = state.hk.write().await;
    if let Some(snapshot) = hk.iter_mut().find(|s| s.apid == apid) {
        snapshot.frames.push(frame);
        // Evict from the front to maintain the ring-buffer depth.
        if snapshot.frames.len() > HK_FRAMES_PER_APID {
            let excess = snapshot.frames.len() - HK_FRAMES_PER_APID;
            snapshot.frames.drain(..excess);
        }
    } else {
        hk.push(HkSnapshot {
            asset,
            apid,
            frames: vec![frame],
        });
    }
}

async fn process_event_packet(bytes: &Bytes, state: &Arc<UiState>) {
    let Some(apid) = extract_apid(bytes) else { return };
    let timestamp_utc = extract_utc(bytes, &state.converter);

    // cFE EVS payload starts at byte 16. Byte 0 of the payload holds the
    // event type (severity); the null-terminated message follows at byte 1.
    // If the packet is shorter than the header, treat it as an empty message.
    let (severity, message) = if bytes.len() > HDR_LEN {
        let payload = bytes.get(HDR_LEN..).unwrap_or(&[]);
        let severity = *payload.first().unwrap_or(&0);
        // Find null terminator or use entire remaining slice as message text.
        let msg_bytes = payload.get(1..).unwrap_or(&[]);
        let end = msg_bytes.iter().position(|&b| b == 0).unwrap_or(msg_bytes.len());
        let message = String::from_utf8_lossy(msg_bytes.get(..end).unwrap_or(&[]))
            .into_owned();
        (severity, message)
    } else {
        (0u8, String::new())
    };

    let entry = EventEntry { timestamp_utc, apid, severity, message };

    let mut events = state.events.write().await;
    events.push(entry);
    if events.len() > EVENT_RING_MAX {
        let excess = events.len() - EVENT_RING_MAX;
        events.drain(..excess);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Extract the 11-bit APID from the CCSDS primary header (bytes 0–1).
fn extract_apid(bytes: &Bytes) -> Option<u16> {
    let b0 = *bytes.first()?;
    let b1 = *bytes.get(1)?;
    Some(u16::from(b0 & 0x07) << 8 | u16::from(b1))
}

/// Extract a UTC ISO-8601 timestamp from the CCSDS secondary header
/// (bytes 6–15, Q-C6: 7-byte CUC + `func_code` + `instance_id`).
///
/// Falls back to the current UTC wall-clock time when secondary-header
/// decode fails (e.g., stub packets with reserved `func_code=0x0000`).
fn extract_utc(bytes: &Bytes, converter: &crate::ui::time::TaiUtcConverter) -> String {
    if let Some(sec_slice) = bytes.get(6..) {
        if let Ok(hdr) = SecondaryHeader::decode(sec_slice) {
            return converter.tai_to_iso8601(&hdr.time());
        }
    }
    // Fallback: format the current UTC wall-clock time.
    let utc = time::OffsetDateTime::now_utc();
    utc.format(&time::format_description::well_known::Rfc3339)
        .unwrap_or_default()
}

/// Map a CCSDS APID to a human-readable asset name.
fn apid_to_asset(apid: u16) -> String {
    match apid {
        0x100 => "sample_app".into(),
        0x101 => "orbiter_cdh".into(),
        0x110 => "orbiter_adcs".into(),
        0x111 => "orbiter_adcs_wheel".into(),
        0x120 => "orbiter_comm".into(),
        0x128 => "ros2_bridge".into(),
        0x130 => "orbiter_power".into(),
        0x140 => "orbiter_payload".into(),
        0x300 => "rover_land".into(),
        0x3C0 => "rover_uav".into(),
        0x400 => "rover_cryobot".into(),
        _ => format!("apid_0x{apid:03X}"),
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
    clippy::indexing_slicing
)]
mod tests {
    use super::*;
    use ccsds_wire::{Apid, Cuc, FuncCode, InstanceId, PacketBuilder, SequenceCount};
    use crate::ui::UiState;

    /// Build a minimal valid CCSDS TM space packet for testing.
    fn build_test_packet(apid: u16, tai_coarse: u32, payload: &[u8]) -> Bytes {
        let pkt = PacketBuilder::tm(Apid::new(apid).unwrap())
            .sequence_count(SequenceCount::new(1).unwrap())
            .cuc(Cuc { coarse: tai_coarse, fine: 0 })
            .func_code(FuncCode::new(0x0002).unwrap())
            .instance_id(InstanceId::new(1).unwrap())
            .user_data(payload)
            .build()
            .unwrap();
        Bytes::from(pkt)
    }

    /// `spawn_hk_sink` processes an HK packet and upserts `UiState::hk`.
    #[tokio::test]
    async fn test_hk_sink_upserts_snapshot() {
        let (tc_tx, _tc_rx) = tokio::sync::mpsc::channel(1);
        let state = Arc::new(UiState::new(37, tc_tx));

        let (hk_tx, hk_rx) = mpsc::channel::<Bytes>(8);
        let (_, rover_rx) = mpsc::channel::<Bytes>(8);

        spawn_hk_sink(hk_rx, rover_rx, state.clone());

        // Send one orbiter_cdh packet (APID 0x101, TAI coarse = 1_000_000).
        let pkt = build_test_packet(0x101, 1_000_000, &[0xAB, 0xCD]);
        hk_tx.send(pkt).await.unwrap();
        drop(hk_tx);

        // Give the sink task time to process the message.
        tokio::time::sleep(std::time::Duration::from_millis(50)).await;

        let hk = state.hk.read().await;
        assert_eq!(hk.len(), 1, "expected one HkSnapshot");
        let snap = &hk[0];
        assert_eq!(snap.apid, 0x101);
        assert_eq!(snap.asset, "orbiter_cdh");
        assert_eq!(snap.frames.len(), 1);
        assert_eq!(snap.frames[0].data, vec![0xAB, 0xCD]);
        assert!(!snap.frames[0].timestamp_utc.is_empty());
    }

    /// Ring buffer evicts oldest frames when depth exceeds `HK_FRAMES_PER_APID`.
    #[tokio::test]
    async fn test_hk_sink_ring_buffer_evicts_oldest() {
        let (tc_tx, _tc_rx) = tokio::sync::mpsc::channel(1);
        let state = Arc::new(UiState::new(37, tc_tx));

        let (hk_tx, hk_rx) = mpsc::channel::<Bytes>(64);
        let (_, rover_rx) = mpsc::channel::<Bytes>(1);
        spawn_hk_sink(hk_rx, rover_rx, state.clone());

        // Send HK_FRAMES_PER_APID + 5 packets for APID 0x110.
        for seq in 0..(HK_FRAMES_PER_APID + 5) {
            let pkt = build_test_packet(0x110, seq as u32, &[seq as u8]);
            hk_tx.send(pkt).await.unwrap();
        }
        drop(hk_tx);

        tokio::time::sleep(std::time::Duration::from_millis(100)).await;

        let hk = state.hk.read().await;
        let snap = hk.iter().find(|s| s.apid == 0x110).expect("snapshot exists");
        assert_eq!(
            snap.frames.len(),
            HK_FRAMES_PER_APID,
            "ring buffer must not exceed HK_FRAMES_PER_APID"
        );
    }

    /// `spawn_event_sink` appends an `EventEntry` to `UiState::events`.
    #[tokio::test]
    async fn test_event_sink_appends_entry() {
        let (tc_tx, _tc_rx) = tokio::sync::mpsc::channel(1);
        let state = Arc::new(UiState::new(37, tc_tx));

        let (ev_tx, ev_rx) = mpsc::channel::<Bytes>(8);
        spawn_event_sink(ev_rx, state.clone());

        // Severity byte + null-terminated message text.
        let mut payload = vec![3u8]; // severity = 3
        payload.extend_from_slice(b"test event message\0");

        let pkt = build_test_packet(0x102, 2_000_000, &payload);
        ev_tx.send(pkt).await.unwrap();
        drop(ev_tx);

        tokio::time::sleep(std::time::Duration::from_millis(50)).await;

        let events = state.events.read().await;
        assert_eq!(events.len(), 1);
        assert_eq!(events[0].apid, 0x102);
        assert_eq!(events[0].severity, 3);
        assert_eq!(events[0].message, "test event message");
    }

    /// `apid_to_asset` returns the correct asset name for known APIDs.
    #[test]
    fn test_apid_to_asset_known_apiids() {
        assert_eq!(apid_to_asset(0x101), "orbiter_cdh");
        assert_eq!(apid_to_asset(0x110), "orbiter_adcs");
        assert_eq!(apid_to_asset(0x300), "rover_land");
        assert_eq!(apid_to_asset(0x3C0), "rover_uav");
        assert_eq!(apid_to_asset(0x400), "rover_cryobot");
    }

    /// `apid_to_asset` falls back to the APID hex representation for unknowns.
    #[test]
    fn test_apid_to_asset_unknown_apid() {
        assert_eq!(apid_to_asset(0x200), "apid_0x200");
    }

    /// `extract_apid` decodes the 11-bit APID from the first two bytes.
    #[test]
    fn test_extract_apid_parses_correctly() {
        // APID 0x101: primary header bytes [0x09, 0x01, ...]
        let pkt = build_test_packet(0x101, 0, &[]);
        assert_eq!(extract_apid(&pkt), Some(0x101));
    }
}
