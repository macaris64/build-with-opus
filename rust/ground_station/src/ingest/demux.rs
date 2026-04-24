//! Virtual Channel demultiplexer (docs/architecture/06-ground-segment-rust.md §5.2–5.3).
//!
//! Stage 2 of the telemetry ingest pipeline. Fans incoming [`AosFrame`]s out to
//! per-VC bounded `tokio::sync::mpsc` channels consumed by [`SppDecoder`] tasks.
//!
//! # Routing rules
//!
//! | VC | Treatment |
//! |----|-----------|
//! | 0 – HK | Forward to `SppDecoder` channel |
//! | 1 – Events | Forward to `SppDecoder` channel |
//! | 2 – CFDP | Forward to `SppDecoder` channel |
//! | 3 – Rover-forward | Forward to `SppDecoder` channel |
//! | 63 – Idle fill | Silently discard (no event) |
//! | Any other | Discard + `EVENT-AOS-UNKNOWN-VC` |
//!
//! # Backpressure (§5.3)
//!
//! When a downstream channel is full, [`VcDemultiplexer`] increments
//! `dropped_total` and emits `EVENT-INGEST-BACKPRESSURE` (rate-limited to
//! ≤ 1 Hz to prevent event floods during sustained overload).

use std::collections::{HashMap, HashSet};
use std::time::{Duration, Instant};

use tokio::sync::mpsc;

use super::{AosFrame, DEMUX_TO_SPP_CAP};

/// VC ID reserved for AOS idle fill; silently discarded without an event.
const VC_IDLE: u8 = 63;

/// Minimum interval between consecutive `EVENT-INGEST-BACKPRESSURE` emissions.
const BP_EVENT_MIN_INTERVAL: Duration = Duration::from_secs(1);

/// Fans [`AosFrame`]s from the [`super::framer::AosFramer`] out to per-VC
/// bounded `tokio::sync::mpsc` channels (§5.2, §5.3).
pub struct VcDemultiplexer {
    /// Active VCs: VC ID → bounded sender to the downstream `SppDecoder`.
    senders: HashMap<u8, mpsc::Sender<AosFrame>>,
    /// VCs to silently discard (no event, no counter). Defaults to `{63}`.
    idle_vcs: HashSet<u8>,
    /// Cumulative frames dropped due to a full downstream channel.
    dropped_total: u64,
    /// Cumulative frames discarded because the VC ID is unknown.
    unknown_vc_total: u64,
    /// Timestamp of the last `EVENT-INGEST-BACKPRESSURE` emission for rate-limiting.
    last_bp_event: Option<Instant>,
}

impl VcDemultiplexer {
    /// Build a demultiplexer from a caller-supplied sender map.
    ///
    /// VC 63 (idle fill) is pre-registered as a silent-discard VC.
    #[must_use]
    pub fn new(senders: HashMap<u8, mpsc::Sender<AosFrame>>) -> Self {
        let mut idle_vcs = HashSet::new();
        idle_vcs.insert(VC_IDLE);
        Self {
            senders,
            idle_vcs,
            dropped_total: 0,
            unknown_vc_total: 0,
            last_bp_event: None,
        }
    }

    /// Construct the default 4-VC pipeline used in production.
    ///
    /// Creates one bounded `mpsc` channel per active VC (capacity
    /// [`DEMUX_TO_SPP_CAP`] each) and returns both the demultiplexer and the
    /// per-VC receivers keyed by VC ID.
    #[must_use]
    pub fn with_default_channels() -> (Self, HashMap<u8, mpsc::Receiver<AosFrame>>) {
        let mut senders = HashMap::new();
        let mut receivers = HashMap::new();
        for vc_id in [0u8, 1, 2, 3] {
            let (tx, rx) = mpsc::channel(DEMUX_TO_SPP_CAP);
            senders.insert(vc_id, tx);
            receivers.insert(vc_id, rx);
        }
        (Self::new(senders), receivers)
    }

    /// Add a VC to the silent-discard set (no `EVENT-AOS-UNKNOWN-VC` emitted).
    pub fn add_idle_vc(&mut self, vc_id: u8) {
        self.idle_vcs.insert(vc_id);
    }

    /// Cumulative frames dropped due to full downstream channels.
    #[must_use]
    pub fn dropped_total(&self) -> u64 {
        self.dropped_total
    }

    /// Cumulative frames discarded for an unknown VC ID.
    #[must_use]
    pub fn unknown_vc_total(&self) -> u64 {
        self.unknown_vc_total
    }

    /// Drive the demux loop until `frame_rx` is closed by the upstream stage.
    pub async fn run(&mut self, mut frame_rx: mpsc::Receiver<AosFrame>) {
        while let Some(frame) = frame_rx.recv().await {
            self.dispatch(frame);
        }
    }

    /// Route a single frame; called synchronously to keep the hot path lock-free.
    fn dispatch(&mut self, frame: AosFrame) {
        let vc_id = frame.vc_id;

        if self.idle_vcs.contains(&vc_id) {
            return;
        }

        if let Some(tx) = self.senders.get(&vc_id) {
            if tx.try_send(frame).is_err() {
                self.dropped_total += 1;
                self.emit_bp_event(vc_id);
            }
        } else {
            self.unknown_vc_total += 1;
            log::warn!("EVENT-AOS-UNKNOWN-VC vc_id={vc_id}");
        }
    }

    fn emit_bp_event(&mut self, vc_id: u8) {
        let now = Instant::now();
        let should_emit = self
            .last_bp_event
            .is_none_or(|t| now.duration_since(t) >= BP_EVENT_MIN_INTERVAL);
        if should_emit {
            self.last_bp_event = Some(now);
            log::warn!("EVENT-INGEST-BACKPRESSURE stage=demux vc_id={vc_id}");
        }
    }
}

// ---------------------------------------------------------------------------
// Tests (TDD — written before the struct existed to produce the Red step)
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::expect_used, clippy::panic)]
mod tests {
    use bytes::Bytes;
    use tokio::sync::mpsc;

    use super::*;
    use crate::ingest::AosFrame;

    fn make_frame(vc_id: u8) -> AosFrame {
        AosFrame {
            vc_id,
            ocf: None,
            data_field: Bytes::from_static(b"test-payload"),
        }
    }

    // Given: VcDemultiplexer with VC 0 registered
    // When:  AosFrame(vc_id=0) dispatched through run()
    // Then:  Frame appears on VC 0 receiver with vc_id unchanged
    #[tokio::test]
    async fn given_known_vc_when_frame_received_then_forwarded() {
        let (tx, mut rx) = mpsc::channel(8);
        let mut senders = HashMap::new();
        senders.insert(0u8, tx);
        let mut demux = VcDemultiplexer::new(senders);

        let (frame_tx, frame_rx) = mpsc::channel(8);
        frame_tx.send(make_frame(0)).await.unwrap();
        drop(frame_tx);

        demux.run(frame_rx).await;

        let received = rx.try_recv().expect("expected a frame on VC 0");
        assert_eq!(received.vc_id, 0);
        assert_eq!(demux.dropped_total(), 0);
        assert_eq!(demux.unknown_vc_total(), 0);
    }

    // Given: VcDemultiplexer with only VC 0 registered
    // When:  AosFrame(vc_id=5) dispatched (unknown VC)
    // Then:  No frame on VC 0 receiver; unknown_vc_total == 1
    #[tokio::test]
    async fn given_unknown_vc_when_frame_received_then_rejected_with_event() {
        let (tx, mut rx) = mpsc::channel(8);
        let mut senders = HashMap::new();
        senders.insert(0u8, tx);
        let mut demux = VcDemultiplexer::new(senders);

        let (frame_tx, frame_rx) = mpsc::channel(8);
        frame_tx.send(make_frame(5)).await.unwrap();
        drop(frame_tx);

        demux.run(frame_rx).await;

        assert!(
            rx.try_recv().is_err(),
            "VC 0 should not receive a VC 5 frame"
        );
        assert_eq!(demux.unknown_vc_total(), 1);
        assert_eq!(demux.dropped_total(), 0);
    }

    // Given: VcDemultiplexer with default channels (VC 0/1/2/3 active, VC 63 idle)
    // When:  AosFrame(vc_id=63) dispatched
    // Then:  No frame on any active receiver; dropped_total == 0; no unknown_vc event
    #[tokio::test]
    async fn given_idle_vc_when_frame_received_then_silently_discarded() {
        let (mut demux, mut receivers) = VcDemultiplexer::with_default_channels();

        let (frame_tx, frame_rx) = mpsc::channel(8);
        frame_tx.send(make_frame(63)).await.unwrap();
        drop(frame_tx);

        demux.run(frame_rx).await;

        assert_eq!(demux.dropped_total(), 0);
        assert_eq!(demux.unknown_vc_total(), 0);
        for vc_id in [0u8, 1, 2, 3] {
            let rx = receivers.get_mut(&vc_id).unwrap();
            assert!(rx.try_recv().is_err(), "VC {vc_id} should be empty");
        }
    }

    // Given: VcDemultiplexer with VC 0 channel at capacity 1, no consumer
    // When:  Two frames dispatched (first fills; second overflows)
    // Then:  dropped_total == 1
    #[tokio::test]
    async fn given_full_channel_when_frame_sent_then_dropped_and_backpressure_event() {
        let (tx, _rx) = mpsc::channel::<AosFrame>(1); // intentionally not consumed
        let mut senders = HashMap::new();
        senders.insert(0u8, tx);
        let mut demux = VcDemultiplexer::new(senders);

        let (frame_tx, frame_rx) = mpsc::channel(8);
        frame_tx.send(make_frame(0)).await.unwrap(); // fills the capacity-1 channel
        frame_tx.send(make_frame(0)).await.unwrap(); // overflows → dropped
        drop(frame_tx);

        demux.run(frame_rx).await;

        assert_eq!(demux.dropped_total(), 1);
    }

    // Given: VcDemultiplexer with default channels
    // When:  One frame per active VC (0, 1, 2, 3) dispatched
    // Then:  Each receiver gets exactly one frame with the correct vc_id
    #[tokio::test]
    async fn given_default_channels_when_all_vcs_receive_then_each_forwarded() {
        let (mut demux, mut receivers) = VcDemultiplexer::with_default_channels();

        let (frame_tx, frame_rx) = mpsc::channel(16);
        for vc_id in [0u8, 1, 2, 3] {
            frame_tx.send(make_frame(vc_id)).await.unwrap();
        }
        drop(frame_tx);

        demux.run(frame_rx).await;

        for vc_id in [0u8, 1, 2, 3] {
            let rx = receivers.get_mut(&vc_id).unwrap();
            let frame = rx
                .try_recv()
                .unwrap_or_else(|_| panic!("VC {vc_id} missing frame"));
            assert_eq!(frame.vc_id, vc_id);
            assert!(
                rx.try_recv().is_err(),
                "VC {vc_id} should have exactly one frame"
            );
        }
        assert_eq!(demux.dropped_total(), 0);
        assert_eq!(demux.unknown_vc_total(), 0);
    }
}
