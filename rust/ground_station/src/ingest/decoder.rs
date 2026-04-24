//! SPP decoder (docs/architecture/06-ground-segment-rust.md §5.2).
//!
//! Stage 3 of the telemetry ingest pipeline. Parses per-VC `M_PDU` bytes
//! (carried as `AosFrame.data_field`) into validated `SpacePacket` views
//! using `ccsds_wire::SpacePacket::parse`, then forwards the original
//! `Bytes` slice to the `ApidRouter` stage via a bounded mpsc channel.
//!
//! # Error policy (§5.2)
//!
//! A [`ccsds_wire::CcsdsError`] on any frame causes that frame to be
//! discarded. `decode_fail_total` is incremented and a rate-limited
//! `EVENT-SPP-DECODE-FAIL` event (≤ 1/s per error variant) is emitted to
//! prevent log floods during a sustained RF anomaly.
//!
//! # Backpressure (§5.3)
//!
//! When the downstream `ApidRouter` channel is full, [`SppDecoder`]
//! increments `dropped_total` and emits `EVENT-INGEST-BACKPRESSURE`
//! (rate-limited to ≤ 1 Hz).

use std::collections::HashMap;
use std::time::{Duration, Instant};

use bytes::Bytes;
use ccsds_wire::{CcsdsError, SpacePacket};
use tokio::sync::mpsc;

use super::AosFrame;

/// Minimum interval between consecutive `EVENT-SPP-DECODE-FAIL` emissions
/// for the same error variant (§5.2: ≤ 1/s per variant).
const DECODE_FAIL_EVENT_MIN_INTERVAL: Duration = Duration::from_secs(1);

/// Minimum interval between consecutive `EVENT-INGEST-BACKPRESSURE` emissions.
const BP_EVENT_MIN_INTERVAL: Duration = Duration::from_secs(1);

/// Stage 3 of the TM ingest pipeline (docs/architecture/06-ground-segment-rust.md §5.2).
///
/// Consumes [`AosFrame`]s from the [`super::demux::VcDemultiplexer`] channel,
/// validates `data_field` bytes as a CCSDS Space Packet, and forwards valid
/// `Bytes` slices to the `ApidRouter` stage.
///
/// Q-C8: all multi-byte decoding is delegated to `ccsds_wire`, the sole
/// sanctioned BE conversion locus per docs/standards/decisions-log.md.
///
/// Q-F3: transient TM pipeline state; explicitly excluded from `Vault<T>`
/// per docs/architecture/09-failure-and-radiation.md §5.2.
pub struct SppDecoder {
    /// Virtual Channel this decoder is assigned to (for log context).
    vc_id: u8,
    /// Cumulative `M_PDU` buffers that failed `SpacePacket::parse`.
    decode_fail_total: u64,
    /// Cumulative packets dropped because the downstream channel was full.
    dropped_total: u64,
    /// Per-variant rate-limit timestamps for `EVENT-SPP-DECODE-FAIL`.
    last_decode_fail_events: HashMap<&'static str, Instant>,
    /// Rate-limit timestamp for `EVENT-INGEST-BACKPRESSURE`.
    last_bp_event: Option<Instant>,
}

impl SppDecoder {
    /// Create an `SppDecoder` assigned to `vc_id`.
    #[must_use]
    pub fn new(vc_id: u8) -> Self {
        Self {
            vc_id,
            decode_fail_total: 0,
            dropped_total: 0,
            last_decode_fail_events: HashMap::new(),
            last_bp_event: None,
        }
    }

    /// Cumulative `M_PDU` frames that failed `SpacePacket::parse`.
    #[must_use]
    pub fn decode_fail_total(&self) -> u64 {
        self.decode_fail_total
    }

    /// Cumulative packets dropped due to a full downstream channel.
    #[must_use]
    pub fn dropped_total(&self) -> u64 {
        self.dropped_total
    }

    /// Drive the decode loop until `frame_rx` is closed by the upstream stage.
    pub async fn run(
        &mut self,
        mut frame_rx: mpsc::Receiver<AosFrame>,
        router_tx: mpsc::Sender<Bytes>,
    ) {
        while let Some(frame) = frame_rx.recv().await {
            self.process(frame, &router_tx);
        }
    }

    /// Process one frame. Synchronous so the hot path stays lock-free.
    fn process(&mut self, frame: AosFrame, router_tx: &mpsc::Sender<Bytes>) {
        match SpacePacket::parse(&frame.data_field) {
            Ok(_) => {
                if router_tx.try_send(frame.data_field).is_err() {
                    self.dropped_total += 1;
                    self.emit_bp_event();
                }
            }
            Err(e) => {
                self.decode_fail_total += 1;
                self.emit_decode_fail_event(&e);
            }
        }
    }

    fn emit_decode_fail_event(&mut self, e: &CcsdsError) {
        let label = error_label(e);
        let now = Instant::now();
        let should_emit = self
            .last_decode_fail_events
            .get(label)
            .is_none_or(|t| now.duration_since(*t) >= DECODE_FAIL_EVENT_MIN_INTERVAL);
        if should_emit {
            self.last_decode_fail_events.insert(label, now);
            log::warn!(
                "EVENT-SPP-DECODE-FAIL vc_id={} variant={label} err={e}",
                self.vc_id
            );
        }
    }

    fn emit_bp_event(&mut self) {
        let now = Instant::now();
        let should_emit = self
            .last_bp_event
            .is_none_or(|t| now.duration_since(t) >= BP_EVENT_MIN_INTERVAL);
        if should_emit {
            self.last_bp_event = Some(now);
            log::warn!(
                "EVENT-INGEST-BACKPRESSURE stage=decoder vc_id={}",
                self.vc_id
            );
        }
    }
}

/// Map a [`CcsdsError`] variant to a stable `&'static str` label for
/// per-variant rate limiting and event annotation.
fn error_label(e: &CcsdsError) -> &'static str {
    match e {
        CcsdsError::BufferTooShort { .. } => "buffer_too_short",
        CcsdsError::InvalidVersion(_) => "invalid_version",
        CcsdsError::InvalidPField(_) => "invalid_pfield",
        CcsdsError::ApidOutOfRange(_) => "apid_out_of_range",
        CcsdsError::SequenceCountOutOfRange(_) => "seq_count_out_of_range",
        CcsdsError::InstanceIdReserved => "instance_id_reserved",
        CcsdsError::LengthMismatch { .. } => "length_mismatch",
        CcsdsError::SequenceFlagsNotStandalone(_) => "seq_flags_not_standalone",
        CcsdsError::FuncCodeReserved => "func_code_reserved",
    }
}

// ---------------------------------------------------------------------------
// Tests (TDD — written before the struct existed to produce the Red step)
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::expect_used, clippy::panic)]
mod tests {
    use bytes::Bytes;
    use ccsds_wire::{Apid, Cuc, FuncCode, InstanceId, PacketBuilder};
    use tokio::sync::mpsc;

    use super::*;
    use crate::ingest::AosFrame;

    /// Build a valid CCSDS Space Packet wire encoding as `Bytes`.
    fn make_valid_spp() -> Bytes {
        let raw = PacketBuilder::tm(Apid::new(0x100).unwrap())
            .func_code(FuncCode::new(0x0001).unwrap())
            .instance_id(InstanceId::new(1).unwrap())
            .cuc(Cuc { coarse: 0, fine: 0 })
            .user_data(&[0xAB, 0xCD])
            .build()
            .unwrap();
        Bytes::from(raw)
    }

    fn make_frame(data: Bytes) -> AosFrame {
        AosFrame {
            vc_id: 0,
            ocf: None,
            data_field: data,
        }
    }

    // Given: an upstream channel containing one valid CCSDS Space Packet
    // When:  decoder.run() completes (upstream sender dropped)
    // Then:  one Bytes slice on the output channel; both counters zero
    #[tokio::test]
    async fn given_valid_spp_when_run_then_forwarded() {
        let mut decoder = SppDecoder::new(0);
        let (frame_tx, frame_rx) = mpsc::channel(8);
        let (router_tx, mut router_rx) = mpsc::channel(8);

        frame_tx.send(make_frame(make_valid_spp())).await.unwrap();
        drop(frame_tx);

        decoder.run(frame_rx, router_tx).await;

        let pkt = router_rx
            .try_recv()
            .expect("expected one packet on router channel");
        assert!(!pkt.is_empty());
        assert!(router_rx.try_recv().is_err(), "only one packet expected");
        assert_eq!(decoder.decode_fail_total(), 0);
        assert_eq!(decoder.dropped_total(), 0);
    }

    // Given: a data_field shorter than SpacePacket::HEADER_LEN (16 B)
    // When:  run() completes
    // Then:  nothing forwarded; decode_fail_total == 1; dropped_total == 0
    #[tokio::test]
    async fn given_buffer_too_short_when_run_then_counted_and_discarded() {
        let mut decoder = SppDecoder::new(0);
        let (frame_tx, frame_rx) = mpsc::channel(8);
        let (router_tx, mut router_rx) = mpsc::channel(8);

        frame_tx
            .send(make_frame(Bytes::from_static(b"too_short")))
            .await
            .unwrap();
        drop(frame_tx);

        decoder.run(frame_rx, router_tx).await;

        assert!(router_rx.try_recv().is_err(), "no packet should be forwarded");
        assert_eq!(decoder.decode_fail_total(), 1);
        assert_eq!(decoder.dropped_total(), 0);
    }

    // Given: a buffer with valid headers but declared total length != buf.len()
    // When:  run() completes
    // Then:  nothing forwarded; decode_fail_total == 1
    #[tokio::test]
    async fn given_length_mismatch_when_run_then_counted_and_discarded() {
        let mut decoder = SppDecoder::new(0);
        let (frame_tx, frame_rx) = mpsc::channel(8);
        let (router_tx, mut router_rx) = mpsc::channel(8);

        // Build an 18-byte packet then append 4 extra bytes → LengthMismatch.
        let mut raw = PacketBuilder::tm(Apid::new(0x100).unwrap())
            .func_code(FuncCode::new(0x0001).unwrap())
            .instance_id(InstanceId::new(1).unwrap())
            .user_data(&[0xAA, 0xBB])
            .build()
            .unwrap();
        raw.extend_from_slice(&[0xFF, 0xFF, 0xFF, 0xFF]);

        frame_tx.send(make_frame(Bytes::from(raw))).await.unwrap();
        drop(frame_tx);

        decoder.run(frame_rx, router_tx).await;

        assert!(router_rx.try_recv().is_err(), "no packet should be forwarded");
        assert_eq!(decoder.decode_fail_total(), 1);
        assert_eq!(decoder.dropped_total(), 0);
    }

    // Given: a full downstream channel (capacity=1, no consumer) and two valid packets
    // When:  run() completes
    // Then:  dropped_total == 1; decode_fail_total == 0
    #[tokio::test]
    async fn given_full_router_channel_when_valid_packets_sent_then_one_dropped() {
        let mut decoder = SppDecoder::new(0);
        let (frame_tx, frame_rx) = mpsc::channel(8);
        let (router_tx, _router_rx) = mpsc::channel::<Bytes>(1); // capacity=1, never consumed

        // First fills the channel; second overflows.
        frame_tx.send(make_frame(make_valid_spp())).await.unwrap();
        frame_tx.send(make_frame(make_valid_spp())).await.unwrap();
        drop(frame_tx);

        decoder.run(frame_rx, router_tx).await;

        assert_eq!(decoder.dropped_total(), 1);
        assert_eq!(decoder.decode_fail_total(), 0);
    }

    // Given: frames with two different decode error variants
    // When:  run() completes
    // Then:  decode_fail_total == 2; each variant counted independently
    #[tokio::test]
    async fn given_two_different_error_variants_when_run_then_both_counted() {
        let mut decoder = SppDecoder::new(0);
        let (frame_tx, frame_rx) = mpsc::channel(8);
        let (router_tx, mut router_rx) = mpsc::channel(8);

        // Variant 1: buffer_too_short (< 16 bytes)
        frame_tx
            .send(make_frame(Bytes::from_static(b"short")))
            .await
            .unwrap();

        // Variant 2: length_mismatch — valid headers, extra trailing bytes
        let mut raw = PacketBuilder::tm(Apid::new(0x100).unwrap())
            .func_code(FuncCode::new(0x0001).unwrap())
            .instance_id(InstanceId::new(1).unwrap())
            .user_data(&[0xAA, 0xBB])
            .build()
            .unwrap();
        raw.extend_from_slice(&[0xFF, 0xFF]);
        frame_tx.send(make_frame(Bytes::from(raw))).await.unwrap();

        drop(frame_tx);

        decoder.run(frame_rx, router_tx).await;

        assert_eq!(decoder.decode_fail_total(), 2);
        assert_eq!(decoder.dropped_total(), 0);
        assert!(router_rx.try_recv().is_err());
    }

    // Given: two identical decode errors submitted back-to-back (within 1 s)
    // When:  run() completes
    // Then:  decode_fail_total == 2 (counter tracks every failure regardless
    //        of rate-limiting on the log event)
    #[tokio::test]
    async fn given_two_identical_errors_within_1s_when_run_then_both_counted() {
        let mut decoder = SppDecoder::new(0);
        let (frame_tx, frame_rx) = mpsc::channel(8);
        let (router_tx, _router_rx) = mpsc::channel::<Bytes>(8);

        frame_tx
            .send(make_frame(Bytes::from_static(b"x")))
            .await
            .unwrap();
        frame_tx
            .send(make_frame(Bytes::from_static(b"y")))
            .await
            .unwrap();
        drop(frame_tx);

        decoder.run(frame_rx, router_tx).await;

        assert_eq!(decoder.decode_fail_total(), 2);
    }

    // Verify error_label exhaustively covers all 9 CcsdsError variants and
    // returns a non-empty label for each. The non-exhaustive match in
    // error_label causes a rustc error if a new variant is added without
    // updating the label map — this test documents that invariant.
    #[test]
    fn given_all_ccsds_error_variants_when_error_label_then_non_empty() {
        assert!(!error_label(&CcsdsError::BufferTooShort { need: 16, got: 1 }).is_empty());
        assert!(!error_label(&CcsdsError::InvalidVersion(1)).is_empty());
        assert!(!error_label(&CcsdsError::InvalidPField(0)).is_empty());
        assert!(!error_label(&CcsdsError::ApidOutOfRange(0x800)).is_empty());
        assert!(!error_label(&CcsdsError::SequenceCountOutOfRange(0x4000)).is_empty());
        assert!(!error_label(&CcsdsError::InstanceIdReserved).is_empty());
        assert!(!error_label(&CcsdsError::LengthMismatch {
            declared: 10,
            actual: 5
        })
        .is_empty());
        assert!(!error_label(&CcsdsError::SequenceFlagsNotStandalone(0)).is_empty());
        assert!(!error_label(&CcsdsError::FuncCodeReserved).is_empty());
    }
}
