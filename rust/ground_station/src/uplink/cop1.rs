//! `Cop1Engine` — FOP-1 state machine (CCSDS 232.1-B-2).
//!
//! Consumes CLCW feedback from the [`tokio::sync::watch`] channel published
//! by `ingest::AosFramer` (§6.5) and drives the five-state FOP-1 machine
//! described in `docs/architecture/06-ground-segment-rust.md §6.4`.
//!
//! # State diagram (arch §6.4)
//!
//! ```text
//! INITIAL → INITIALIZING : initialize() sends BC frame (Set V(R))
//! INITIALIZING → ACTIVE  : CLCW accepted (lockout=0, retransmit=0)
//! ACTIVE → RETRANSMIT_WITHOUT_WAIT : CLCW retransmit=1
//! ACTIVE → RETRANSMIT_WITH_WAIT   : T1 expired, retransmit_count < max
//! RETRANSMIT_WITHOUT_WAIT → ACTIVE : CLCW clean (lockout=0, retransmit=0)
//! RETRANSMIT_WITH_WAIT    → ACTIVE : CLCW clean
//! RETRANSMIT_WITHOUT_WAIT → INITIAL : retransmit_count ≥ max (abort)
//! RETRANSMIT_WITH_WAIT    → INITIAL : retransmit_count ≥ max (abort)
//! ```
//!
//! # Q-F3 note
//!
//! `Cop1Engine::vs` (V(S) sequence counter) is a radiation-sensitive
//! uplink-sequence anchor — analogous to the CFDP transaction-id counter
//! listed in docs/mission/requirements/GND-SRD.md GND-REQ-0020. It is
//! reserved for `Vault<u8>` wrapping when `rust/vault/` lands (Phase C+).
//! Until then, the field is marked with this comment as the Q-F3 anchor site.

use std::collections::VecDeque;
use std::time::{Duration, Instant};

use tokio::sync::watch;

use super::{COP1_MAX_RETRANSMIT, COP1_STALE_ABORT_MULT, COP1_STALE_WARN_MULT, COP1_WINDOW_SIZE};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/// Minimum interval between `COP1-CLCW-STALE` event emissions (≤ 1/s).
const CLCW_STALE_EVENT_MIN_INTERVAL: Duration = Duration::from_secs(1);

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

/// FOP-1 operating states (CCSDS 232.1-B-2 §6).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Fop1State {
    /// No TC session active; waiting for `initialize()`.
    Initial,
    /// BC frame sent to set V(R); waiting for CLCW acceptance.
    Initializing,
    /// Normal operation: TC frames accepted within the sliding window.
    Active,
    /// CLCW signalled retransmit; retransmitting immediately.
    RetransmitWithoutWait,
    /// T1 expired; retransmitting after waiting for T1.
    RetransmitWithWait,
}

/// Frame type for TC SDLP framing.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TcFrameType {
    /// Type-A/AD: sequence-controlled (VC 0/1).
    TypeAd,
    /// Type-B/BD: bypass data (VC 7, emergency).
    TypeBd,
    /// Type-B/BC: bypass control (Set V(R) directive).
    TypeBc,
}

/// TC frame ready for [`super::framer::TcFramer`].
#[derive(Debug)]
pub struct TcFrame {
    /// Virtual channel ID.
    pub vc_id: u8,
    /// Frame type (AD, BD, or BC).
    pub frame_type: TcFrameType,
    /// Frame sequence number (V(S) for AD; 0 for BD/BC).
    pub sequence: u8,
    /// SPP payload bytes.
    pub payload: Vec<u8>,
}

/// Errors returned by [`Cop1Engine`] operations.
#[derive(Debug, thiserror::Error)]
pub enum Cop1Error {
    /// `submit()` called when engine is not in [`Fop1State::Active`].
    #[error("submit requires Active state, current state: {0:?}")]
    NotInActiveState(Fop1State),
    /// Sliding window is full (`COP1_WINDOW_SIZE` unacknowledged frames).
    #[error("COP-1 window full ({COP1_WINDOW_SIZE} frames outstanding)")]
    WindowFull,
}

// ---------------------------------------------------------------------------
// Cop1Engine
// ---------------------------------------------------------------------------

/// FOP-1 state machine for the TC uplink pipeline.
pub struct Cop1Engine {
    state: Fop1State,
    /// V(S) — next frame sequence number (0–255 rolling).
    ///
    /// Q-F3: radiation-sensitive uplink-sequence anchor; reserved for
    /// `Vault<u8>` wrapping when `rust/vault/` lands (GND-REQ-0020).
    vs: u8,
    /// Sliding window: (sequence, SPP bytes) tuples pending CLCW ACK.
    window: VecDeque<(u8, Vec<u8>)>,
    /// Retransmit count for the current unacknowledged batch.
    retransmit_count: u8,
    /// T1 timer deadline (set when first frame enters the window).
    t1_deadline: Option<Instant>,
    /// T1 duration = 2 × (OWLT + 5 s) from mission.yaml.
    t1_duration: Duration,
    /// Watch receiver for CLCW bytes published by `AosFramer`.
    clcw_rx: watch::Receiver<Option<[u8; 4]>>,
    /// Timestamp of the most-recently ingested CLCW (for stale detection).
    last_clcw_at: Option<Instant>,
    /// Rate-limit gate for `COP1-CLCW-STALE` event emissions.
    last_stale_event_at: Option<Instant>,
}

impl Cop1Engine {
    /// Construct a new engine in [`Fop1State::Initial`].
    ///
    /// `t1` is the timer period = `2 × (OWLT + 5 s)` from `mission.yaml`.
    #[must_use]
    pub fn new(clcw_rx: watch::Receiver<Option<[u8; 4]>>, t1: Duration) -> Self {
        Self {
            state: Fop1State::Initial,
            vs: 0,
            window: VecDeque::new(),
            retransmit_count: 0,
            t1_deadline: None,
            t1_duration: t1,
            clcw_rx,
            last_clcw_at: None,
            last_stale_event_at: None,
        }
    }

    /// Current FOP-1 state.
    #[must_use]
    pub fn state(&self) -> Fop1State {
        self.state
    }

    /// Current retransmit count (for monitoring / UI).
    #[must_use]
    pub fn retransmit_count(&self) -> u8 {
        self.retransmit_count
    }

    /// Transition `Initial → Initializing` by sending a BC (Set V(R)) frame.
    ///
    /// The returned frame MUST be sent to the spacecraft before calling
    /// [`tick`][Self::tick]. The engine then waits for a clean CLCW to
    /// confirm acceptance.
    pub fn initialize(&mut self) -> TcFrame {
        self.state = Fop1State::Initializing;
        self.vs = 0;
        self.window.clear();
        self.retransmit_count = 0;
        self.t1_deadline = None;
        TcFrame {
            vc_id: 0,
            frame_type: TcFrameType::TypeBc,
            sequence: 0,
            payload: vec![0x00], // Set V(R) = 0
        }
    }

    /// Submit a validated SPP for sequence-controlled uplink (AD frame).
    ///
    /// Only valid in [`Fop1State::Active`]. Returns the [`TcFrame`] to send.
    ///
    /// # Errors
    ///
    /// - [`Cop1Error::NotInActiveState`] if not in `Active`.
    /// - [`Cop1Error::WindowFull`] if all 15 window slots are occupied.
    pub fn submit(&mut self, spp: Vec<u8>) -> Result<Vec<TcFrame>, Cop1Error> {
        if self.state != Fop1State::Active {
            return Err(Cop1Error::NotInActiveState(self.state));
        }
        if self.window.len() >= usize::from(COP1_WINDOW_SIZE) {
            return Err(Cop1Error::WindowFull);
        }
        let seq = self.vs;
        self.vs = self.vs.wrapping_add(1);
        self.window.push_back((seq, spp.clone()));
        // Start T1 if not already running.
        if self.t1_deadline.is_none() {
            self.t1_deadline = Some(Instant::now() + self.t1_duration);
        }
        Ok(vec![TcFrame {
            vc_id: 0,
            frame_type: TcFrameType::TypeAd,
            sequence: seq,
            payload: spp,
        }])
    }

    /// Drive timers and CLCW ingestion; call at ≥ 10 Hz.
    ///
    /// Returns any TC frames that must be (re)transmitted. An empty `Vec`
    /// means no frames need to be sent this tick.
    ///
    /// `now` is injectable for unit tests; use `Instant::now()` in production.
    pub fn tick(&mut self, now: Instant) -> Vec<TcFrame> {
        // 1. Ingest CLCW — only process state-machine transitions when a NEW
        //    value has been published to the watch channel since our last read.
        //    `last_clcw_at` is updated only on genuinely new frames so that
        //    the stale-detection clock is not reset by repeated polls of an
        //    unchanged watch value.
        let has_new = self.clcw_rx.has_changed().unwrap_or(false);
        let clcw_opt = *self.clcw_rx.borrow_and_update();

        if has_new {
            if let Some(clcw) = clcw_opt {
                let lockout = (clcw[2] >> 7) & 1;
                let retransmit = (clcw[2] >> 5) & 1;
                let nr = clcw[3]; // N(R): spacecraft's next expected sequence

                self.last_clcw_at = Some(now);

                // Slide window: remove frames the spacecraft has acknowledged.
                while let Some(&(seq, _)) = self.window.front() {
                    if seq_before(seq, nr) {
                        self.window.pop_front();
                        self.retransmit_count = 0;
                        if self.window.is_empty() {
                            self.t1_deadline = None;
                        }
                    } else {
                        break;
                    }
                }

                // State transitions driven by CLCW flags.
                match self.state {
                    Fop1State::Initializing => {
                        if lockout == 0 && retransmit == 0 {
                            self.state = Fop1State::Active;
                        }
                    }
                    Fop1State::Active => {
                        if lockout == 0 && retransmit == 1 {
                            // Increment first; abort if we have hit the limit.
                            self.retransmit_count += 1;
                            if self.retransmit_count >= COP1_MAX_RETRANSMIT {
                                self.abort_to_initial();
                                return vec![];
                            }
                            self.state = Fop1State::RetransmitWithoutWait;
                            return self.oldest_frame_as_retransmit();
                        }
                    }
                    Fop1State::RetransmitWithoutWait | Fop1State::RetransmitWithWait => {
                        if lockout == 0 && retransmit == 1 {
                            self.retransmit_count += 1;
                            if self.retransmit_count >= COP1_MAX_RETRANSMIT {
                                self.abort_to_initial();
                                return vec![];
                            }
                            return self.oldest_frame_as_retransmit();
                        }
                        if lockout == 0 && retransmit == 0 {
                            // CLCW clean → back to Active.
                            self.state = Fop1State::Active;
                            self.retransmit_count = 0;
                        }
                    }
                    Fop1State::Initial => {}
                }
            }
        }

        // 2. Check T1 timer expiry.
        if let Some(deadline) = self.t1_deadline {
            if now >= deadline {
                match self.state {
                    Fop1State::Active | Fop1State::RetransmitWithWait => {
                        self.retransmit_count += 1;
                        if self.retransmit_count >= COP1_MAX_RETRANSMIT {
                            self.abort_to_initial();
                            return vec![];
                        }
                        self.state = Fop1State::RetransmitWithWait;
                        self.t1_deadline = Some(now + self.t1_duration);
                        return self.oldest_frame_as_retransmit();
                    }
                    _ => {}
                }
            }
        }

        // 3. CLCW stale detection (§6.5).
        if let Some(last_clcw) = self.last_clcw_at {
            let stale = now.saturating_duration_since(last_clcw);
            let warn_threshold = self.t1_duration * COP1_STALE_WARN_MULT;
            let abort_threshold = self.t1_duration * COP1_STALE_ABORT_MULT;

            if stale >= warn_threshold {
                let should_emit = self.last_stale_event_at.is_none_or(|last| {
                    now.saturating_duration_since(last) >= CLCW_STALE_EVENT_MIN_INTERVAL
                });
                if should_emit {
                    log::warn!("COP1-CLCW-STALE: no CLCW for {stale:?}");
                    self.last_stale_event_at = Some(now);
                }
            }

            if stale >= abort_threshold && self.state == Fop1State::Active {
                self.state = Fop1State::RetransmitWithWait;
            }
        }

        vec![]
    }

    /// Emergency Type-BD frame on VC 7 — bypasses FOP-1 entirely.
    ///
    /// Use for safe-mode recovery only (arch §6.6). Does NOT affect engine
    /// state or V(S). MUST be logged at CRITICAL level by the caller.
    #[must_use]
    pub fn emergency_bd(spp: Vec<u8>) -> TcFrame {
        TcFrame {
            vc_id: 7,
            frame_type: TcFrameType::TypeBd,
            sequence: 0,
            payload: spp,
        }
    }

    // ── Private helpers ──────────────────────────────────────────────────────

    /// Reset to `Initial`, clearing all window state (abort path).
    fn abort_to_initial(&mut self) {
        log::warn!(
            "COP1: abort to Initial after {} retransmits (max={})",
            self.retransmit_count,
            COP1_MAX_RETRANSMIT
        );
        self.state = Fop1State::Initial;
        self.window.clear();
        self.retransmit_count = 0;
        self.t1_deadline = None;
    }

    /// Return the oldest unacknowledged window entry as a retransmit frame.
    fn oldest_frame_as_retransmit(&self) -> Vec<TcFrame> {
        match self.window.front() {
            Some(&(seq, ref payload)) => vec![TcFrame {
                vc_id: 0,
                frame_type: TcFrameType::TypeAd,
                sequence: seq,
                payload: payload.clone(),
            }],
            None => vec![],
        }
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Returns `true` if `seq` is "before" `nr` in the modular sequence space.
///
/// Safe for window sizes ≤ 127 (CCSDS FOP-1 window = 15 ≪ 128).
fn seq_before(seq: u8, nr: u8) -> bool {
    let diff = nr.wrapping_sub(seq);
    diff != 0 && diff <= 128
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
    use tokio::sync::watch;

    // ── Helpers ──────────────────────────────────────────────────────────────

    /// Construct an engine with a very short T1 and a controllable CLCW channel.
    fn make_engine(clcw_tx: &watch::Sender<Option<[u8; 4]>>) -> Cop1Engine {
        let t1 = Duration::from_millis(1); // short so tests can trigger expiry
        Cop1Engine::new(clcw_tx.subscribe(), t1)
    }

    /// Build a CLCW byte array with specified lockout and retransmit flags.
    /// N(R) = `nr` in byte 3.
    fn clcw(lockout: u8, retransmit: u8, nr: u8) -> [u8; 4] {
        // Byte 2: bit 7 = lockout, bit 5 = retransmit
        let b2 = (lockout << 7) | (retransmit << 5);
        [0x00, 0x00, b2, nr]
    }

    /// Instant far enough in the future to guarantee T1 has expired.
    fn far_future() -> Instant {
        Instant::now() + Duration::from_secs(3600)
    }

    // ── C1 ──────────────────────────────────────────────────────────────────
    // Given: Cop1Engine::new.
    // Then: state is Initial.
    #[test]
    fn c1_initial_state_on_construction() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let engine = make_engine(&tx);
        assert_eq!(engine.state(), Fop1State::Initial);
    }

    // ── C2 ──────────────────────────────────────────────────────────────────
    // Given: Initial state.
    // When: initialize() is called.
    // Then: state → Initializing; returned frame is TypeBc on VC 0.
    #[test]
    fn c2_initialize_transitions_to_initializing() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        let bc_frame = engine.initialize();
        assert_eq!(engine.state(), Fop1State::Initializing);
        assert_eq!(bc_frame.frame_type, TcFrameType::TypeBc);
        assert_eq!(bc_frame.vc_id, 0);
    }

    // ── C3 ──────────────────────────────────────────────────────────────────
    // Given: Initializing state.
    // When: CLCW arrives with lockout=0, retransmit=0.
    // Then: state → Active.
    #[test]
    fn c3_clean_clcw_in_initializing_transitions_to_active() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        engine.initialize();
        tx.send(Some(clcw(0, 0, 0))).unwrap();
        engine.tick(Instant::now());
        assert_eq!(engine.state(), Fop1State::Active);
    }

    // ── C4 ──────────────────────────────────────────────────────────────────
    // Given: Active state.
    // When: TC submitted within window.
    // Then: emits one TypeAd frame; V(S) increments.
    #[test]
    fn c4_submit_in_active_emits_ad_frame() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        engine.initialize();
        tx.send(Some(clcw(0, 0, 0))).unwrap();
        engine.tick(Instant::now()); // → Active

        let frames = engine.submit(vec![0xAA, 0xBB]).unwrap();
        assert_eq!(frames.len(), 1);
        assert_eq!(frames[0].frame_type, TcFrameType::TypeAd);
        assert_eq!(frames[0].sequence, 0); // first V(S) = 0
        assert_eq!(frames[0].payload, [0xAA, 0xBB]);
        // V(S) should now be 1
        let frames2 = engine.submit(vec![0xCC]).unwrap();
        assert_eq!(frames2[0].sequence, 1);
    }

    // ── C5 ──────────────────────────────────────────────────────────────────
    // Given: Active state with one frame in window.
    // When: CLCW lockout=0, retransmit=1.
    // Then: state → RetransmitWithoutWait; retransmit frame returned.
    #[test]
    fn c5_clcw_retransmit_triggers_retransmit_without_wait() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        engine.initialize();
        tx.send(Some(clcw(0, 0, 0))).unwrap();
        engine.tick(Instant::now()); // → Active

        engine.submit(vec![0x01]).unwrap();
        tx.send(Some(clcw(0, 1, 0))).unwrap(); // retransmit=1
        let frames = engine.tick(Instant::now());
        assert_eq!(engine.state(), Fop1State::RetransmitWithoutWait);
        assert_eq!(frames.len(), 1);
        assert_eq!(frames[0].frame_type, TcFrameType::TypeAd);
    }

    // ── C6 ──────────────────────────────────────────────────────────────────
    // Given: RetransmitWithoutWait state.
    // When: CLCW clean (lockout=0, retransmit=0).
    // Then: state → Active.
    #[test]
    fn c6_clean_clcw_in_retransmit_without_wait_returns_to_active() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        engine.initialize();
        tx.send(Some(clcw(0, 0, 0))).unwrap();
        engine.tick(Instant::now()); // → Active
        engine.submit(vec![0x01]).unwrap();
        tx.send(Some(clcw(0, 1, 0))).unwrap();
        engine.tick(Instant::now()); // → RetransmitWithoutWait

        tx.send(Some(clcw(0, 0, 1))).unwrap(); // clean, N(R)=1 acks seq 0
        engine.tick(Instant::now());
        assert_eq!(engine.state(), Fop1State::Active);
    }

    // ── C7 ──────────────────────────────────────────────────────────────────
    // Given: Active state with a frame in the window.
    // When: T1 expires (retransmit_count < 3).
    // Then: state → RetransmitWithWait; retransmit frame returned.
    #[test]
    fn c7_t1_expiry_in_active_triggers_retransmit_with_wait() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        engine.initialize();
        tx.send(Some(clcw(0, 0, 0))).unwrap();
        engine.tick(Instant::now()); // → Active
        engine.submit(vec![0x01]).unwrap();

        // Pass a far-future 'now' so T1 (1 ms) is definitely expired.
        let frames = engine.tick(far_future());
        assert_eq!(engine.state(), Fop1State::RetransmitWithWait);
        assert_eq!(frames.len(), 1);
        assert_eq!(engine.retransmit_count(), 1);
    }

    // ── C8 ──────────────────────────────────────────────────────────────────
    // Given: RetransmitWithWait state.
    // When: CLCW clean.
    // Then: state → Active.
    #[test]
    fn c8_clean_clcw_in_retransmit_with_wait_returns_to_active() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        engine.initialize();
        tx.send(Some(clcw(0, 0, 0))).unwrap();
        engine.tick(Instant::now()); // → Active
        engine.submit(vec![0x01]).unwrap();
        engine.tick(far_future()); // → RetransmitWithWait (T1 expired)

        tx.send(Some(clcw(0, 0, 1))).unwrap(); // clean CLCW, N(R)=1
        engine.tick(Instant::now());
        assert_eq!(engine.state(), Fop1State::Active);
    }

    // ── C9 ──────────────────────────────────────────────────────────────────
    // State-exhaustion (RetransmitWithoutWait): retransmit_count reaches max.
    // Given: Active with one frame; CLCW retransmit=1 sent 3 times.
    // When: tick() is called on the third retransmit signal.
    // Then: state → Initial (abort).
    #[test]
    fn c9_retransmit_without_wait_aborts_after_max_retransmits() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        engine.initialize();
        tx.send(Some(clcw(0, 0, 0))).unwrap();
        engine.tick(Instant::now()); // → Active
        engine.submit(vec![0x01]).unwrap();

        // Three consecutive CLCW retransmit=1 signals exhaust the limit.
        for _ in 0..COP1_MAX_RETRANSMIT {
            tx.send(Some(clcw(0, 1, 0))).unwrap();
            engine.tick(Instant::now());
        }
        assert_eq!(engine.state(), Fop1State::Initial);
    }

    // ── C10 ─────────────────────────────────────────────────────────────────
    // State-exhaustion (RetransmitWithWait): T1 expires 3 times.
    // Given: Active with one frame; T1 expires 3 times without CLCW progress.
    // When: tick() is called for the third T1 expiry.
    // Then: state → Initial (abort).
    #[test]
    fn c10_retransmit_with_wait_aborts_after_max_retransmits() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        engine.initialize();
        tx.send(Some(clcw(0, 0, 0))).unwrap();
        engine.tick(Instant::now()); // → Active
        engine.submit(vec![0x01]).unwrap();

        // Use strictly-increasing timestamps so each new T1 deadline (set to
        // now + 1 ms after each expiry) is reliably exceeded by the next tick.
        let base = Instant::now() + Duration::from_secs(100);
        for i in 0..u64::from(COP1_MAX_RETRANSMIT) {
            engine.tick(base + Duration::from_secs(i * 100 + 100));
        }
        assert_eq!(engine.state(), Fop1State::Initial);
    }

    // ── C11 ─────────────────────────────────────────────────────────────────
    // Given: any state.
    // When: emergency_bd(spp) called.
    // Then: returns TcFrame { frame_type: TypeBd, vc_id: 7 }.
    #[test]
    fn c11_emergency_bd_returns_bd_frame_on_vc7() {
        let frame = Cop1Engine::emergency_bd(vec![0xDE, 0xAD]);
        assert_eq!(frame.frame_type, TcFrameType::TypeBd);
        assert_eq!(frame.vc_id, 7);
        assert_eq!(frame.payload, [0xDE, 0xAD]);
        assert_eq!(frame.sequence, 0);
    }

    // ── C12 ─────────────────────────────────────────────────────────────────
    // Given: CLCW received once; then 3×T1 passes with no further CLCW.
    // When: tick() is called at stale threshold.
    // Then: COP1-CLCW-STALE is emitted (tested via stale_event state).
    #[test]
    fn c12_stale_clcw_detected_after_3x_t1() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        engine.initialize();
        tx.send(Some(clcw(0, 0, 0))).unwrap();
        let t0 = Instant::now();
        engine.tick(t0); // ingests CLCW, sets last_clcw_at = t0

        // Advance past 3×T1 (T1 = 1 ms, so 10 ms is >> 3 ms).
        let stale_now = t0 + Duration::from_millis(10);
        engine.tick(stale_now);
        // Verify stale event was recorded (last_stale_event_at is Some).
        assert!(
            engine.last_stale_event_at.is_some(),
            "stale event should have been emitted"
        );
    }

    // ── C13 ─────────────────────────────────────────────────────────────────
    // submit() in non-Active state returns NotInActiveState error.
    #[test]
    fn c13_submit_in_initial_returns_error() {
        let (tx, _rx) = watch::channel(None::<[u8; 4]>);
        let mut engine = make_engine(&tx);
        let err = engine.submit(vec![0x01]).unwrap_err();
        assert!(matches!(
            err,
            Cop1Error::NotInActiveState(Fop1State::Initial)
        ));
    }

    // ── C14 ─────────────────────────────────────────────────────────────────
    // seq_before wraps correctly at the 255/0 boundary.
    #[test]
    fn c14_seq_before_handles_wrap_around() {
        assert!(seq_before(254, 1)); // 254 → ... → 255 → 0 → 1
        assert!(!seq_before(1, 254)); // opposite direction
        assert!(!seq_before(5, 5)); // equal → not before
    }
}
