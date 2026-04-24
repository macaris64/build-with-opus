//! AOS Transfer Frame receiver (`AosFramer`).
//!
//! Implements the first stage of the ingest pipeline:
//! `docs/architecture/06-ground-segment-rust.md` §5.2, §6.5, §9.1, §9.3.
//!
//! # Frame layout (CCSDS 732.0-B-4, Q-C4: 1024 B fixed)
//!
//! ```text
//! [0..6]       Primary Header (6 B)
//! [6..1018]    Data Field     (1012 B, OCF present)  ┐ mutually
//! [6..1022]    Data Field     (1016 B, OCF absent)   ┘ exclusive
//! [1018..1022] OCF / CLCW    (4 B, when OCF_FLAG=1)
//! [1022..1024] FECF           (2 B, CRC-16/IBM-3740 over [0..1022])
//! ```

use bytes::Bytes;
use crc::{Crc, CRC_16_IBM_3740};
use std::{collections::VecDeque, time::Duration};
use tokio::io::{AsyncRead, AsyncReadExt};
use tokio::sync::{mpsc, watch};
use tokio::time::Instant;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/// AOS Transfer Frame size in bytes (Q-C4, CCSDS 732.0-B-4 §4.1).
pub const AOS_FRAME_LEN: usize = 1024;

/// Bytes over which FECF is computed (frame minus trailing 2 FECF bytes).
const FECF_PAYLOAD_LEN: usize = AOS_FRAME_LEN - 2; // 1022

/// AOS Transfer Frame Primary Header length.
const PRIMARY_HEADER_LEN: usize = 6;

/// OCF (CLCW) field length in bytes.
const OCF_LEN: usize = 4;

/// Byte offset of OCF when `OCF_FLAG=1`: `AOS_FRAME_LEN` - FECF(2) - OCF(4).
const OCF_OFFSET: usize = AOS_FRAME_LEN - 2 - OCF_LEN; // 1018

/// End of data field when OCF is present (= `OCF_OFFSET`).
const DATA_WITH_OCF_END: usize = OCF_OFFSET; // 1018

/// End of data field when OCF is absent (= `FECF_PAYLOAD_LEN`).
const DATA_WITHOUT_OCF_END: usize = FECF_PAYLOAD_LEN; // 1022

/// Minimum interval between `AOS-FECF-MISMATCH` event emissions (§9.1: ≤1/s).
const FECF_EVENT_MIN_INTERVAL: Duration = Duration::from_secs(1);

/// Width of the rolling window for Degraded-state FECF error-rate check (§9.3).
const DEGRADED_WINDOW: Duration = Duration::from_secs(30);

/// LOS threshold: no valid frames for this duration → `Los` (§9.3).
const LOS_WINDOW: Duration = Duration::from_secs(10);

/// AOS rate sub-window: ≥1 valid frame per this duration satisfies the rate
/// condition (§9.3).
const AOS_RATE_WINDOW: Duration = Duration::from_secs(2);

/// How long the AOS rate condition must hold continuously before → `Aos` (§9.3).
const AOS_SUSTAIN: Duration = Duration::from_secs(5);

/// FECF error-rate threshold that triggers `Degraded` (§9.3: >10 %).
const DEGRADED_ERROR_THRESHOLD: f64 = 0.10;

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

/// Ground-segment link state (§9.3).
///
/// | Observation | Transition |
/// |---|---|
/// | ≥1 valid frame / 2 s, sustained 5 s | → `Aos` |
/// | 0 valid frames for 10 s | → `Los` |
/// | FECF error rate > 10 % over 30 s | → `Degraded` |
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LinkState {
    /// Acquisition of Signal: valid frames arriving on schedule.
    Aos,
    /// Loss of Signal: no valid frames received for ≥10 s.
    Los,
    /// Link degraded: FECF error rate exceeds 10 % over a 30 s window.
    Degraded,
}

/// Errors returned by [`AosFramer::run`].
///
/// Per-frame protocol errors (FECF mismatch) are handled internally and never
/// surfaced here; only transport-layer I/O errors are returned.
#[derive(Debug, thiserror::Error)]
pub enum FramerError {
    /// The underlying `AsyncRead` returned a non-EOF I/O error.
    #[error("I/O error reading AOS byte stream: {0}")]
    Io(#[from] std::io::Error),
}

// ---------------------------------------------------------------------------
// FrameWindow (private)
// ---------------------------------------------------------------------------

/// Sliding-window ring of `(timestamp, is_valid)` entries.
///
/// Uses `tokio::time::Instant` so tests can drive `tokio::time::advance()`
/// for deterministic link-state branch coverage.
struct FrameWindow {
    entries: VecDeque<(Instant, bool)>,
}

impl FrameWindow {
    fn new() -> Self {
        Self {
            entries: VecDeque::new(),
        }
    }

    fn push(&mut self, now: Instant, valid: bool) {
        self.entries.push_back((now, valid));
    }

    /// Evict entries older than `max_age` relative to `now`.
    fn prune(&mut self, now: Instant, max_age: Duration) {
        while self
            .entries
            .front()
            .is_some_and(|(t, _)| now.saturating_duration_since(*t) > max_age)
        {
            self.entries.pop_front();
        }
    }

    /// Count valid entries within `window` of `now`.
    fn valid_count_in(&self, now: Instant, window: Duration) -> usize {
        self.entries
            .iter()
            .filter(|(t, valid)| *valid && now.saturating_duration_since(*t) <= window)
            .count()
    }

    /// FECF error rate (errors / total) for entries within `window` of `now`.
    #[allow(clippy::cast_precision_loss)] // frame counts are small; f64 precision is sufficient
    fn error_rate_over(&self, now: Instant, window: Duration) -> f64 {
        let mut total: usize = 0;
        let mut errors: usize = 0;
        for (t, valid) in &self.entries {
            if now.saturating_duration_since(*t) <= window {
                total += 1;
                if !valid {
                    errors += 1;
                }
            }
        }
        if total == 0 {
            return 0.0;
        }
        errors as f64 / total as f64
    }

    /// Age of the most-recent valid entry, or `None` if no valid entries exist.
    fn last_valid_age(&self, now: Instant) -> Option<Duration> {
        self.entries
            .iter()
            .rev()
            .find(|(_, valid)| *valid)
            .map(|(t, _)| now.saturating_duration_since(*t))
    }
}

// ---------------------------------------------------------------------------
// AosFramer
// ---------------------------------------------------------------------------

/// AOS Transfer Frame receiver (§5.2, §9.1, §9.3).
///
/// Reads a continuous [`AsyncRead`] stream in 1024-byte chunks, verifies the
/// FECF (CRC-16/CCITT-FALSE = CRC-16/IBM-3740), extracts the OCF/CLCW, and
/// forwards valid [`super::AosFrame`] values to the downstream mpsc channel.
///
/// # FECF error policy (§9.1)
///
/// FECF mismatch: discard, increment counter, emit a rate-limited
/// `AOS-FECF-MISMATCH` log event (≤1/s). Corrupt frames are never forwarded.
///
/// # CLCW publication (§6.5)
///
/// Valid frames with `OCF_FLAG=1` publish their 4-byte OCF to `clcw_tx`.
/// `Cop1Engine` (Phase 28) subscribes. Watch semantics (latest-wins) are
/// intentional: CLCW is state, not event history.
pub struct AosFramer {
    frame_tx: mpsc::Sender<super::AosFrame>,
    clcw_tx: watch::Sender<Option<[u8; 4]>>,
    fecf_errors_total: u64,
    last_fecf_event: Option<Instant>,
    link_state: LinkState,
    /// Instant when the Aos rate-condition was first continuously satisfied.
    aos_condition_since: Option<Instant>,
    frame_window: FrameWindow,
}

impl AosFramer {
    /// Construct a new [`AosFramer`].
    ///
    /// Initial [`LinkState`] is [`LinkState::Los`]. The framer transitions to
    /// [`LinkState::Aos`] after sustained valid-frame arrival per §9.3.
    #[must_use]
    pub fn new(
        frame_tx: mpsc::Sender<super::AosFrame>,
        clcw_tx: watch::Sender<Option<[u8; 4]>>,
    ) -> Self {
        Self {
            frame_tx,
            clcw_tx,
            fecf_errors_total: 0,
            last_fecf_event: None,
            link_state: LinkState::Los,
            aos_condition_since: None,
            frame_window: FrameWindow::new(),
        }
    }

    /// Current link state.
    #[must_use]
    pub fn link_state(&self) -> LinkState {
        self.link_state
    }

    /// Cumulative FECF error count since construction.
    #[must_use]
    pub fn fecf_errors_total(&self) -> u64 {
        self.fecf_errors_total
    }

    /// Re-evaluate link state without waiting for a new frame.
    ///
    /// Call periodically (e.g., once per second) so that LOS and Degraded
    /// transitions fire even when the byte stream is silent.
    pub fn tick(&mut self) {
        self.update_link_state(Instant::now());
    }

    /// Drive the framer from a byte stream until EOF or I/O error.
    ///
    /// Reads exactly [`AOS_FRAME_LEN`] bytes per iteration. The stream must
    /// be frame-aligned at the AOS boundary (no ASM sync search in Phase 22).
    ///
    /// # Errors
    ///
    /// Returns [`FramerError::Io`] for non-EOF I/O errors. A clean EOF
    /// (`UnexpectedEof` on a fresh frame read) exits cleanly with `Ok(())`.
    pub async fn run<R: AsyncRead + Unpin>(&mut self, mut reader: R) -> Result<(), FramerError> {
        let crc_engine = Crc::<u16>::new(&CRC_16_IBM_3740);
        let mut buf = [0u8; AOS_FRAME_LEN];

        loop {
            match reader.read_exact(&mut buf).await {
                Ok(_) => {}
                Err(e) if e.kind() == std::io::ErrorKind::UnexpectedEof => break,
                Err(e) => return Err(FramerError::Io(e)),
            }

            self.process_frame(&buf, &crc_engine);
        }

        Ok(())
    }

    // Process one raw 1024-byte AOS frame buffer.
    //
    // All byte-offset accesses are statically bounded: `buf` is typed as
    // `&[u8; AOS_FRAME_LEN]` (= 1024) and every constant index is < 1024.
    #[allow(clippy::indexing_slicing)]
    fn process_frame(&mut self, buf: &[u8; AOS_FRAME_LEN], crc_engine: &Crc<u16>) {
        let now = Instant::now();

        // ── 1. FECF verification (§9.1) ──────────────────────────────────────
        // CRC-16/IBM-3740 (= CRC-16/CCITT-FALSE) over bytes [0..1022].
        // FECF stored big-endian at bytes [1022..1024] (Q-C8: from_be_bytes permitted).
        let computed = crc_engine.checksum(&buf[..FECF_PAYLOAD_LEN]);
        let wire_fecf = u16::from_be_bytes([buf[FECF_PAYLOAD_LEN], buf[FECF_PAYLOAD_LEN + 1]]);

        if computed != wire_fecf {
            self.fecf_errors_total += 1;
            self.frame_window.push(now, false);
            self.maybe_emit_fecf_event(now);
            self.update_link_state(now);
            return; // discard — corrupt frames must never propagate (§9.1)
        }

        // ── 2. Primary header field extraction (CCSDS 732.0-B-4 §4.1) ────────
        //
        // Byte layout of the 6-byte AOS Transfer Frame Primary Header:
        //   byte0[7:6] TF Version (= 0b01)      byte0[5:0] SCID[7:2]
        //   byte1[7:6] SCID[1:0]                 byte1[5:0] VCID (6 bits)
        //   bytes[2..5] VC Frame Count (24-bit, big-endian)
        //   byte5[7]   Replay Flag               byte5[6:5] VC FC Cycle
        //   byte5[4]   Reserved                  byte5[3]   OCF_FLAG
        //   byte5[2:0] Reserved / Data Field Status
        let vc_id = buf[1] & 0x3F;
        let ocf_flag = (buf[5] >> 3) & 0x01;

        // ── 3. Extract data field and OCF ─────────────────────────────────────
        let (ocf, data_field) = if ocf_flag == 1 {
            let ocf_bytes = [
                buf[OCF_OFFSET],
                buf[OCF_OFFSET + 1],
                buf[OCF_OFFSET + 2],
                buf[OCF_OFFSET + 3],
            ];
            let data = Bytes::copy_from_slice(&buf[PRIMARY_HEADER_LEN..DATA_WITH_OCF_END]);
            (Some(ocf_bytes), data)
        } else {
            let data = Bytes::copy_from_slice(&buf[PRIMARY_HEADER_LEN..DATA_WITHOUT_OCF_END]);
            (None, data)
        };

        // ── 4. Publish CLCW to watch channel (§6.5) ───────────────────────────
        if let Some(clcw) = ocf {
            // watch::Sender::send never blocks. Ignore stale-receiver error —
            // Cop1Engine (Phase 28) may not be running during early init.
            let _ = self.clcw_tx.send(Some(clcw));
        }

        // ── 5. Update rolling window and link state ───────────────────────────
        self.frame_window.push(now, true);
        self.update_link_state(now);

        // ── 6. Forward frame downstream ───────────────────────────────────────
        // try_send is non-blocking: channel-full → silent discard per §5.3.
        // Phase 23 will add the labeled `aos_ingest_dropped_total` counter.
        let frame = super::AosFrame { vc_id, ocf, data_field };
        if self.frame_tx.try_send(frame).is_err() {
            // Backpressure drop — labeled counter lands in Phase 23.
        }
    }

    fn maybe_emit_fecf_event(&mut self, now: Instant) {
        let should_emit = self
            .last_fecf_event
            .is_none_or(|last| now.saturating_duration_since(last) >= FECF_EVENT_MIN_INTERVAL);
        if should_emit {
            log::warn!(
                "AOS-FECF-MISMATCH: fecf_errors_total={}",
                self.fecf_errors_total
            );
            self.last_fecf_event = Some(now);
        }
    }

    fn update_link_state(&mut self, now: Instant) {
        // Prune to the longest observation horizon to bound memory.
        self.frame_window.prune(now, DEGRADED_WINDOW);

        let prev = self.link_state;
        let new_state = self.compute_link_state(now);

        if new_state != prev {
            log::info!("LINK-STATE-CHANGE: {prev:?} → {new_state:?}");
            self.link_state = new_state;
            if new_state != LinkState::Aos {
                self.aos_condition_since = None;
            }
        }
    }

    fn compute_link_state(&mut self, now: Instant) -> LinkState {
        // 1. Degraded: FECF error rate > 10 % over last 30 s.
        let error_rate = self.frame_window.error_rate_over(now, DEGRADED_WINDOW);
        if error_rate > DEGRADED_ERROR_THRESHOLD {
            self.aos_condition_since = None;
            return LinkState::Degraded;
        }

        // 2. LOS: no valid frame for ≥10 s (or no valid frames ever).
        let starved = self
            .frame_window
            .last_valid_age(now)
            .is_none_or(|age| age >= LOS_WINDOW);
        if starved {
            self.aos_condition_since = None;
            return LinkState::Los;
        }

        // 3. AOS: rate condition met and sustained for ≥5 s.
        if self.frame_window.valid_count_in(now, AOS_RATE_WINDOW) >= 1 {
            let since = self.aos_condition_since.get_or_insert(now);
            if now.saturating_duration_since(*since) >= AOS_SUSTAIN {
                return LinkState::Aos;
            }
        } else {
            self.aos_condition_since = None;
        }

        // No transition fired: remain in current state.
        self.link_state
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::panic, clippy::expect_used)]
mod tests {
    use super::*;
    use tokio::sync::{mpsc, watch};

    // ── Test frame builder ──────────────────────────────────────────────────

    /// Construct a valid 1024-byte AOS frame with correct FECF.
    ///
    /// SCID is fixed at 42 (`SAKURA_II_SCID_BASE`). `ocf` controls
    /// `OCF_FLAG` and writes the 4-byte OCF field when `Some`.
    fn build_frame(vc_id: u8, ocf: Option<[u8; 4]>) -> [u8; AOS_FRAME_LEN] {
        const SCID: u8 = 42; // SAKURA_II_SCID_BASE
        let mut frame = [0u8; AOS_FRAME_LEN];

        // byte0: TF version (0b01) | SCID[7:2]
        frame[0] = (0b01 << 6) | (SCID >> 2);
        // byte1: SCID[1:0] | VCID
        frame[1] = ((SCID & 0x03) << 6) | (vc_id & 0x3F);
        // bytes 2-4: VC Frame Count = 1 (non-zero, arbitrary)
        frame[4] = 1;
        // byte5: OCF_FLAG at bit 3
        frame[5] = if ocf.is_some() { 0x08 } else { 0x00 };

        if let Some(ocf_bytes) = ocf {
            frame[OCF_OFFSET] = ocf_bytes[0];
            frame[OCF_OFFSET + 1] = ocf_bytes[1];
            frame[OCF_OFFSET + 2] = ocf_bytes[2];
            frame[OCF_OFFSET + 3] = ocf_bytes[3];
        }

        // Append FECF: CRC-16/IBM-3740 over bytes [0..1022], big-endian.
        let crc_engine = Crc::<u16>::new(&CRC_16_IBM_3740);
        let fecf = crc_engine.checksum(&frame[..FECF_PAYLOAD_LEN]);
        let [hi, lo] = fecf.to_be_bytes();
        frame[FECF_PAYLOAD_LEN] = hi;
        frame[FECF_PAYLOAD_LEN + 1] = lo;

        frame
    }

    /// Feed a single valid frame to the framer via an in-memory cursor.
    async fn feed_frame(framer: &mut AosFramer, frame: &[u8; AOS_FRAME_LEN]) {
        let reader = tokio::io::BufReader::new(std::io::Cursor::new(frame.to_vec()));
        framer.run(reader).await.unwrap();
    }

    /// Feed a frame with a deliberately corrupted FECF (all bits flipped).
    async fn feed_bad_fecf(framer: &mut AosFramer) {
        let mut frame = build_frame(0, None);
        frame[FECF_PAYLOAD_LEN] ^= 0xFF;
        frame[FECF_PAYLOAD_LEN + 1] ^= 0xFF;
        let reader = tokio::io::BufReader::new(std::io::Cursor::new(frame.to_vec()));
        framer.run(reader).await.unwrap();
    }

    // ── Test 1: valid frame with OCF present ────────────────────────────────

    /// Given: a valid 1024-byte AOS frame with `OCF_FLAG=1` and known CLCW.
    /// When:  `AosFramer` processes it via `AsyncRead`.
    /// Then:  one `AosFrame` is forwarded; OCF and CLCW watch are correct.
    #[tokio::test]
    async fn test_valid_frame_ocf_present() {
        let (frame_tx, mut frame_rx) = mpsc::channel(8);
        let (clcw_tx, clcw_rx) = watch::channel(None);
        let mut framer = AosFramer::new(frame_tx, clcw_tx);

        let clcw = [0xDE_u8, 0xAD, 0xBE, 0xEF];
        feed_frame(&mut framer, &build_frame(0, Some(clcw))).await;

        let f = frame_rx.try_recv().unwrap();
        assert_eq!(f.vc_id, 0);
        assert_eq!(f.ocf, Some(clcw));
        assert_eq!(f.data_field.len(), 1012);
        assert_eq!(framer.fecf_errors_total(), 0);
        assert_eq!(*clcw_rx.borrow(), Some(clcw));
    }

    // ── Test 2: bad FECF — frame discarded, event rate-limited ─────────────

    /// Given: frames with corrupted FECF bytes.
    /// When:  `AosFramer` processes them.
    /// Then:  frames are discarded; counter increments; event rate-limited ≤1/s.
    #[tokio::test]
    async fn test_bad_fecf_discarded() {
        tokio::time::pause();

        let (frame_tx, mut frame_rx) = mpsc::channel(8);
        let (clcw_tx, clcw_rx) = watch::channel(None);
        let mut framer = AosFramer::new(frame_tx, clcw_tx);

        // First bad frame: event emitted (last_fecf_event = None → emit branch).
        feed_bad_fecf(&mut framer).await;
        assert_eq!(framer.fecf_errors_total(), 1);
        assert!(frame_rx.try_recv().is_err(), "corrupt frame must not be forwarded");
        assert_eq!(*clcw_rx.borrow(), None, "CLCW must not be updated on bad frame");

        // Second bad frame within 1 s: event suppressed (elapsed < interval branch).
        tokio::time::advance(Duration::from_millis(500)).await;
        feed_bad_fecf(&mut framer).await;
        assert_eq!(framer.fecf_errors_total(), 2);
        assert!(frame_rx.try_recv().is_err());

        // Third bad frame after ≥1 s: event emitted again (elapsed ≥ interval branch).
        tokio::time::advance(Duration::from_millis(600)).await;
        feed_bad_fecf(&mut framer).await;
        assert_eq!(framer.fecf_errors_total(), 3);
        assert!(frame_rx.try_recv().is_err());
    }

    // ── Test 3: valid frame with OCF absent ─────────────────────────────────

    /// Given: a valid 1024-byte AOS frame with `OCF_FLAG=0`.
    /// When:  `AosFramer` processes it.
    /// Then:  `AosFrame` forwarded with `ocf=None`, `data_field` 1016 B; watch unchanged.
    #[tokio::test]
    async fn test_valid_frame_no_ocf() {
        let (frame_tx, mut frame_rx) = mpsc::channel(8);
        let (clcw_tx, clcw_rx) = watch::channel(None);
        let mut framer = AosFramer::new(frame_tx, clcw_tx);

        feed_frame(&mut framer, &build_frame(1, None)).await;

        let f = frame_rx.try_recv().unwrap();
        assert_eq!(f.vc_id, 1);
        assert_eq!(f.ocf, None);
        assert_eq!(f.data_field.len(), 1016);
        assert_eq!(framer.fecf_errors_total(), 0);
        assert_eq!(*clcw_rx.borrow(), None, "watch must not be updated when OCF absent");
    }

    // ── Test 4: link-state transitions (Aos / Degraded / Los) ───────────────

    /// Given: idle frames (VCID=63) and controlled time advancement.
    /// When:  frames and time progress per §9.3 transition rules.
    /// Then:  framer transitions through Los → Aos → Degraded → Los.
    #[tokio::test]
    async fn test_link_state_transitions() {
        tokio::time::pause();

        let (frame_tx, _frame_rx) = mpsc::channel(64);
        let (clcw_tx, _clcw_rx) = watch::channel(None);
        let mut framer = AosFramer::new(frame_tx, clcw_tx);

        assert_eq!(framer.link_state(), LinkState::Los, "initial state must be Los");

        // ── Phase 1: Los → Aos ──────────────────────────────────────────────
        // t=0 s: feed frame; aos_condition_since = Some(t=0).
        feed_frame(&mut framer, &build_frame(63, None)).await;

        // t=2 s: rate condition still met; sustain not yet reached (2 s < 5 s).
        tokio::time::advance(Duration::from_secs(2)).await;
        feed_frame(&mut framer, &build_frame(63, None)).await;
        assert_eq!(framer.link_state(), LinkState::Los);

        // t=6 s: duration since condition = 6 s ≥ 5 s → Aos.
        tokio::time::advance(Duration::from_secs(4)).await;
        feed_frame(&mut framer, &build_frame(63, None)).await;
        assert_eq!(framer.link_state(), LinkState::Aos, "must reach Aos after 5 s sustain");

        // ── Phase 2: Aos → Degraded ─────────────────────────────────────────
        // 1 bad frame: 1 error / (3 valid + 1 error) = 25 % > 10 % → Degraded.
        feed_bad_fecf(&mut framer).await;
        assert_eq!(
            framer.link_state(),
            LinkState::Degraded,
            "must transition to Degraded when FECF error rate > 10 %"
        );

        // ── Phase 3: Degraded → Los ─────────────────────────────────────────
        // Advance 31 s: all window entries older than 30 s are pruned.
        // tick() evaluates: empty window → no valid frames ever → Los.
        tokio::time::advance(Duration::from_secs(31)).await;
        framer.tick();
        assert_eq!(
            framer.link_state(),
            LinkState::Los,
            "must transition to Los after window empties"
        );
    }

    // ── Latency assertion ────────────────────────────────────────────────────

    /// Assert CRC-16/IBM-3740 over 1024 B completes well under 5 ms per frame
    /// (§5.5 latency budget for `AosFramer`).
    #[test]
    fn test_fecf_latency_under_5ms() {
        use std::time::Instant as StdInstant;

        const ITERATIONS: u32 = 1_000;
        let buf = [0u8; AOS_FRAME_LEN];
        let crc_engine = Crc::<u16>::new(&CRC_16_IBM_3740);

        let start = StdInstant::now();
        for _ in 0..ITERATIONS {
            let _ = crc_engine.checksum(&buf[..FECF_PAYLOAD_LEN]);
        }
        let per_frame = start.elapsed() / ITERATIONS;

        assert!(
            per_frame < Duration::from_millis(5),
            "FECF latency {per_frame:?} per frame exceeds the 5 ms budget (§5.5)"
        );
    }
}
