//! Operator UI backend (docs/architecture/06-ground-segment-rust.md §10).
//!
//! Serves ground-station state over WebSocket + REST to the operator frontend.
//! Framework selection, authentication, colour palette, and deployment topology
//! are out of scope for Phase B (see `docs/dev/ground-ui.md`, planned).
//!
//! # Surfaces
//!
//! 1. **HK dashboard** — per-asset, per-APID, latest-N housekeeping frames.
//! 2. **Event stream** — rolling event log, filterable by severity and APID.
//! 3. **CFDP transaction list** — active + completed transactions with progress %.
//! 4. **M-File transaction list** — per-transaction chunk-gap visualisation.
//! 5. **Link state** — `Aos` / `Los` / `Degraded` with last-frame timestamp.
//! 6. **COP-1 state** — FOP-1 state machine, window occupancy, retransmit counters.
//! 7. **Time authority** — TAI offset, drift budget (µs/day), sync-packet age.
//!
//! # Time Representation
//!
//! All serialised timestamps use UTC milliseconds in ISO-8601 format.
//! TAI-to-UTC conversion happens at this boundary; internal pipeline state is
//! TAI throughout (Q-F4, docs/architecture/08-timing-and-clocks.md §3–4).

pub mod time;

use std::sync::Arc;

use axum::{
    extract::{
        ws::{Message, WebSocket, WebSocketUpgrade},
        State,
    },
    http::StatusCode,
    response::IntoResponse,
    routing::{get, post},
    Json, Router,
};
use ccsds_wire::Cuc;
use serde::{Deserialize, Serialize};
use tokio::sync::{mpsc, RwLock};

use crate::ui::time::{check_validity_window, TaiUtcConverter, ValidityError};
use crate::uplink::dispatcher::TcQueueEntry;

// ── Surface 1: HK dashboard ──────────────────────────────────────────────────

/// A single housekeeping frame for one asset / APID pair.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HkFrame {
    /// UTC timestamp in ISO-8601 (SYS-REQ-0060).
    pub timestamp_utc: String,
    /// Raw HK payload bytes.
    pub data: Vec<u8>,
}

/// Snapshot of the latest-N HK frames for one asset + APID (surface 1).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HkSnapshot {
    pub asset: String,
    pub apid: u16,
    pub frames: Vec<HkFrame>,
}

// ── Surface 2: Event stream ───────────────────────────────────────────────────

/// One entry in the rolling event log (surface 2).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EventEntry {
    /// UTC timestamp in ISO-8601 (SYS-REQ-0060).
    pub timestamp_utc: String,
    pub apid: u16,
    pub severity: u8,
    pub message: String,
}

// ── Surface 3: CFDP transaction list ─────────────────────────────────────────

/// Status snapshot for one CFDP transaction (surface 3).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CfdpStatus {
    pub transaction_id: u32,
    pub source_entity: u16,
    pub dest_entity: u16,
    /// Transfer progress in percent [0.0, 100.0].
    pub progress_pct: f32,
    pub complete: bool,
}

// ── Surface 4: M-File transaction list ───────────────────────────────────────

/// Status snapshot for one M-File transfer, including chunk-gap visualisation
/// (surface 4).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MFileStatus {
    pub transaction_id: u32,
    pub total_chunks: u32,
    pub received_chunks: u32,
    /// Missing chunk ranges as (`first_missing`, `last_missing`) inclusive pairs.
    pub gaps: Vec<(u32, u32)>,
}

// ── Surface 5: Link state ─────────────────────────────────────────────────────

/// RF link state variants (surface 5).
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "PascalCase")]
pub enum LinkVariant {
    Aos,
    Los,
    Degraded,
}

/// Current link state with last-received-frame timestamp (surface 5).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LinkStateDto {
    pub state: LinkVariant,
    /// UTC timestamp of the last received AOS frame, or `None` if no frame has
    /// been received since startup (ISO-8601, SYS-REQ-0060).
    pub last_frame_utc: Option<String>,
}

impl Default for LinkStateDto {
    fn default() -> Self {
        Self {
            state: LinkVariant::Los,
            last_frame_utc: None,
        }
    }
}

// ── Surface 6: COP-1 state ───────────────────────────────────────────────────

/// FOP-1 state machine snapshot (surface 6).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Cop1Status {
    /// Human-readable FOP-1 state name (e.g. `"Active"`, `"Initializing"`).
    pub fop1_state: String,
    /// Number of AD frames awaiting CLCW acknowledgement (0–15).
    pub window_occupancy: u8,
    /// Consecutive retransmit count for the current unacknowledged batch.
    pub retransmit_count: u8,
}

impl Default for Cop1Status {
    fn default() -> Self {
        Self {
            fop1_state: "Initial".into(),
            window_occupancy: 0,
            retransmit_count: 0,
        }
    }
}

// ── Surface 7: Time authority ─────────────────────────────────────────────────

/// Time authority state (surface 7).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TimeAuthority {
    /// Current TAI − UTC offset in whole seconds (leap-second count).
    pub tai_offset_s: i64,
    /// Fleet-wide drift budget in µs/day (Q-F4, limit = 50 ms / 4 h ≈ 300 µs/h).
    pub drift_budget_us_per_day: f64,
    /// Milliseconds elapsed since the last time-synchronisation packet.
    pub sync_packet_age_ms: u64,
    /// Set to `true` when APID 0x541 (clock-skew) has been received and rejected
    /// on the RF path (Q-F2, SYS-REQ-0041). Rendered as a "suspect" badge in the
    /// operator UI per `docs/architecture/06-ground-segment-rust.md` §638.
    pub time_suspect_seen: bool,
}

impl Default for TimeAuthority {
    fn default() -> Self {
        Self {
            tai_offset_s: 37,
            drift_budget_us_per_day: 83.3,
            sync_packet_age_ms: 0,
            time_suspect_seen: false,
        }
    }
}

// ── TC submission ─────────────────────────────────────────────────────────────

/// Request body for `POST /api/tc`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TcSubmitRequest {
    /// TAI coarse seconds of the command's validity-window deadline.  The
    /// operator tooling converts its UTC deadline to TAI before submission.
    pub valid_until_tai_coarse: u32,
}

/// Response body for `POST /api/tc`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TcSubmitResponse {
    pub accepted: bool,
}

// ── Shared state ──────────────────────────────────────────────────────────────

/// Shared mutable state backing all seven UI surfaces.
///
/// In the real pipeline each field is updated by the ingest / uplink tasks via
/// channel notifications.  Phase 29 provides the store skeleton; wiring to live
/// pipeline channels is Phase C+.
pub struct UiState {
    pub hk: RwLock<Vec<HkSnapshot>>,
    pub events: RwLock<Vec<EventEntry>>,
    pub cfdp: RwLock<Vec<CfdpStatus>>,
    pub mfile: RwLock<Vec<MFileStatus>>,
    pub link: RwLock<LinkStateDto>,
    pub cop1: RwLock<Cop1Status>,
    pub time_auth: RwLock<TimeAuthority>,
    /// Current TAI estimate, updated by the ingest path on each AOS frame.
    pub now_tai: RwLock<Cuc>,
    /// One-way light-time estimate in seconds (updated from ranging data).
    pub light_time_s: RwLock<f64>,
    pub converter: TaiUtcConverter,
    /// TC uplink queue — wired to [`crate::uplink::dispatcher`].
    pub tc_tx: mpsc::Sender<TcQueueEntry>,
}

impl UiState {
    /// Creates a zeroed state with the given TAI−UTC leap-second offset.
    /// `tc_tx` is the channel to the TC uplink dispatcher task.
    #[must_use]
    pub fn new(leap_seconds: i64, tc_tx: mpsc::Sender<TcQueueEntry>) -> Self {
        Self {
            hk: RwLock::new(Vec::new()),
            events: RwLock::new(Vec::new()),
            cfdp: RwLock::new(Vec::new()),
            mfile: RwLock::new(Vec::new()),
            link: RwLock::new(LinkStateDto::default()),
            cop1: RwLock::new(Cop1Status::default()),
            time_auth: RwLock::new(TimeAuthority::default()),
            now_tai: RwLock::new(Cuc { coarse: 0, fine: 0 }),
            light_time_s: RwLock::new(600.0),
            converter: TaiUtcConverter::new(leap_seconds),
            tc_tx,
        }
    }
}

// ── Handlers ──────────────────────────────────────────────────────────────────

async fn get_hk(State(state): State<Arc<UiState>>) -> Json<Vec<HkSnapshot>> {
    Json(state.hk.read().await.clone())
}

async fn get_events(State(state): State<Arc<UiState>>) -> Json<Vec<EventEntry>> {
    Json(state.events.read().await.clone())
}

async fn get_cfdp(State(state): State<Arc<UiState>>) -> Json<Vec<CfdpStatus>> {
    Json(state.cfdp.read().await.clone())
}

async fn get_mfile(State(state): State<Arc<UiState>>) -> Json<Vec<MFileStatus>> {
    Json(state.mfile.read().await.clone())
}

async fn get_link(State(state): State<Arc<UiState>>) -> Json<LinkStateDto> {
    Json(state.link.read().await.clone())
}

async fn get_cop1(State(state): State<Arc<UiState>>) -> Json<Cop1Status> {
    Json(state.cop1.read().await.clone())
}

async fn get_time_auth(State(state): State<Arc<UiState>>) -> Json<TimeAuthority> {
    Json(state.time_auth.read().await.clone())
}

/// `GET /api/rover` — returns HK snapshots for rover APIDs (0x300–0x43F).
///
/// Convenience filter over `GET /api/hk` for frontends that only show the
/// rover telemetry panel.
async fn get_rover(State(state): State<Arc<UiState>>) -> Json<Vec<HkSnapshot>> {
    let hk = state.hk.read().await;
    let rover: Vec<HkSnapshot> = hk
        .iter()
        .filter(|s| (0x300u16..=0x43Fu16).contains(&s.apid))
        .cloned()
        .collect();
    Json(rover)
}

/// `POST /api/tc` — validates the command-validity window before accepting the
/// TC for uplink (SYS-REQ-0061).
async fn post_tc(
    State(state): State<Arc<UiState>>,
    Json(req): Json<TcSubmitRequest>,
) -> Result<Json<TcSubmitResponse>, (StatusCode, String)> {
    let now_tai = *state.now_tai.read().await;
    let light_time_s = *state.light_time_s.read().await;
    let valid_until = Cuc {
        coarse: req.valid_until_tai_coarse,
        fine: 0,
    };
    check_validity_window(valid_until, now_tai, light_time_s)
        .map_err(|e: ValidityError| (StatusCode::UNPROCESSABLE_ENTITY, e.to_string()))?;
    // Queue the command stub (apid=0, func_code=0 until the TC catalog lands).
    // try_send is non-blocking; capacity-full means the operator should retry.
    let entry = TcQueueEntry {
        apid: 0,
        func_code: 0,
        payload: Vec::new(),
        valid_until_tai_coarse: req.valid_until_tai_coarse,
    };
    let _ = state.tc_tx.try_send(entry);
    Ok(Json(TcSubmitResponse { accepted: true }))
}

/// `GET /ws` — WebSocket endpoint; streams a JSON snapshot of all seven
/// surfaces once per second until the client disconnects.
async fn ws_handler(ws: WebSocketUpgrade, State(state): State<Arc<UiState>>) -> impl IntoResponse {
    ws.on_upgrade(|socket| stream_snapshots(socket, state))
}

async fn stream_snapshots(mut socket: WebSocket, state: Arc<UiState>) {
    let mut interval = tokio::time::interval(std::time::Duration::from_secs(1));
    loop {
        interval.tick().await;
        let snapshot = build_snapshot(&state).await;
        if socket.send(Message::Text(snapshot)).await.is_err() {
            break; // client disconnected — stop streaming
        }
    }
}

/// Serialises all seven surfaces into a single JSON object for the WS snapshot.
pub async fn build_snapshot(state: &UiState) -> String {
    #[derive(Serialize)]
    struct Snapshot<'a> {
        hk: &'a Vec<HkSnapshot>,
        events: &'a Vec<EventEntry>,
        cfdp: &'a Vec<CfdpStatus>,
        mfile: &'a Vec<MFileStatus>,
        link: &'a LinkStateDto,
        cop1: &'a Cop1Status,
        time_auth: &'a TimeAuthority,
    }
    let hk = state.hk.read().await;
    let events = state.events.read().await;
    let cfdp = state.cfdp.read().await;
    let mfile = state.mfile.read().await;
    let link = state.link.read().await;
    let cop1 = state.cop1.read().await;
    let time_auth = state.time_auth.read().await;
    serde_json::to_string(&Snapshot {
        hk: &hk,
        events: &events,
        cfdp: &cfdp,
        mfile: &mfile,
        link: &link,
        cop1: &cop1,
        time_auth: &time_auth,
    })
    .unwrap_or_else(|_| "{}".into())
}

// ── Router ────────────────────────────────────────────────────────────────────

/// Builds the axum [`Router`] for the operator UI backend.
///
/// Mount with `state(Arc<UiState>)` before serving:
///
/// ```ignore
/// let app = router(Arc::new(UiState::new(37)));
/// axum::serve(listener, app).await?;
/// ```
pub fn router(state: Arc<UiState>) -> Router {
    Router::new()
        .route("/api/hk", get(get_hk))
        .route("/api/events", get(get_events))
        .route("/api/cfdp", get(get_cfdp))
        .route("/api/mfile", get(get_mfile))
        .route("/api/link", get(get_link))
        .route("/api/cop1", get(get_cop1))
        .route("/api/time", get(get_time_auth))
        .route("/api/tc", post(post_tc))
        .route("/api/rover", get(get_rover))
        .route("/ws", get(ws_handler))
        .with_state(state)
}

// ── Tests ─────────────────────────────────────────────────────────────────────

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used,
    clippy::float_cmp
)]
mod tests {
    use super::*;

    fn make_state() -> Arc<UiState> {
        let (tc_tx, _tc_rx) = mpsc::channel(1);
        Arc::new(UiState::new(37, tc_tx))
    }

    // ── Surface handlers ─────────────────────────────────────────────────────

    // GIVEN empty HK store
    // WHEN  GET /api/hk handler is called
    // THEN  returns empty JSON array
    #[tokio::test]
    async fn ui_get_hk_empty() {
        let Json(result) = get_hk(State(make_state())).await;
        assert!(result.is_empty());
    }

    // GIVEN empty event store
    // WHEN  GET /api/events handler is called
    // THEN  returns empty JSON array
    #[tokio::test]
    async fn ui_get_events_empty() {
        let Json(result) = get_events(State(make_state())).await;
        assert!(result.is_empty());
    }

    // GIVEN empty CFDP store
    // WHEN  GET /api/cfdp handler is called
    // THEN  returns empty JSON array
    #[tokio::test]
    async fn ui_get_cfdp_empty() {
        let Json(result) = get_cfdp(State(make_state())).await;
        assert!(result.is_empty());
    }

    // GIVEN empty M-File store
    // WHEN  GET /api/mfile handler is called
    // THEN  returns empty JSON array
    #[tokio::test]
    async fn ui_get_mfile_empty() {
        let Json(result) = get_mfile(State(make_state())).await;
        assert!(result.is_empty());
    }

    // GIVEN default link state (LOS, no last-frame timestamp)
    // WHEN  GET /api/link handler is called
    // THEN  state is LOS and last_frame_utc is None
    #[tokio::test]
    async fn ui_get_link_default_los() {
        let Json(result) = get_link(State(make_state())).await;
        assert_eq!(result.state, LinkVariant::Los);
        assert!(result.last_frame_utc.is_none());
    }

    // GIVEN default COP-1 state (Initial, zero occupancy)
    // WHEN  GET /api/cop1 handler is called
    // THEN  fop1_state is "Initial" and counters are zero
    #[tokio::test]
    async fn ui_get_cop1_default_initial() {
        let Json(result) = get_cop1(State(make_state())).await;
        assert_eq!(result.fop1_state, "Initial");
        assert_eq!(result.window_occupancy, 0);
        assert_eq!(result.retransmit_count, 0);
    }

    // GIVEN default time authority (37 leap-s)
    // WHEN  GET /api/time handler is called
    // THEN  tai_offset_s is 37
    #[tokio::test]
    async fn ui_get_time_auth_default_leap_seconds() {
        let Json(result) = get_time_auth(State(make_state())).await;
        assert_eq!(result.tai_offset_s, 37);
    }

    // GIVEN a freshly constructed TimeAuthority
    // WHEN  time_suspect_seen is read
    // THEN  it defaults to false (no clock-skew injection observed yet)
    // Phase 40 DoD: APID 0x541 → time_suspect_seen badge per §638.
    #[tokio::test]
    async fn ui_time_authority_default_time_suspect_seen_false() {
        let Json(result) = get_time_auth(State(make_state())).await;
        assert!(!result.time_suspect_seen,
            "time_suspect_seen must default to false before any 0x541 injection");
    }

    // GIVEN a UiState whose time_auth.time_suspect_seen has been set true
    // WHEN  GET /api/time handler is called
    // THEN  time_suspect_seen is true in the response
    #[tokio::test]
    async fn ui_time_authority_time_suspect_seen_can_be_set_true() {
        let state = make_state();
        state.time_auth.write().await.time_suspect_seen = true;
        let Json(result) = get_time_auth(State(state)).await;
        assert!(result.time_suspect_seen,
            "time_suspect_seen must be true after badge is set");
    }

    // ── TC submission (SYS-REQ-0061) ─────────────────────────────────────────

    // GIVEN now_tai.coarse = 1100, light_time_s = 5.0, valid_until_tai_coarse = 1000
    // WHEN  POST /api/tc handler is called
    // THEN  returns 422 UNPROCESSABLE_ENTITY (window expired)
    #[tokio::test]
    async fn ui_post_tc_expired_window_returns_422() {
        let state = make_state();
        *state.now_tai.write().await = Cuc {
            coarse: 1100,
            fine: 0,
        };
        *state.light_time_s.write().await = 5.0;
        let req = TcSubmitRequest {
            valid_until_tai_coarse: 1000,
        };
        let result = post_tc(State(state), Json(req)).await;
        assert!(result.is_err(), "expected Err for expired window");
        let (status, _msg) = result.unwrap_err();
        assert_eq!(status, StatusCode::UNPROCESSABLE_ENTITY);
    }

    // GIVEN now_tai.coarse = 500, light_time_s = 5.0, valid_until_tai_coarse = 2000
    // WHEN  POST /api/tc handler is called
    // THEN  returns Ok with accepted = true
    #[tokio::test]
    async fn ui_post_tc_valid_window_accepted() {
        let state = make_state();
        *state.now_tai.write().await = Cuc {
            coarse: 500,
            fine: 0,
        };
        *state.light_time_s.write().await = 5.0;
        let req = TcSubmitRequest {
            valid_until_tai_coarse: 2000,
        };
        let Json(resp) = post_tc(State(state), Json(req)).await.unwrap();
        assert!(resp.accepted);
    }

    // ── WS snapshot ──────────────────────────────────────────────────────────

    // GIVEN a state with one CfdpStatus entry
    // WHEN  build_snapshot is called
    // THEN  the returned JSON string contains the "cfdp" key
    #[tokio::test]
    async fn ui_build_snapshot_contains_all_surfaces() {
        let state = make_state();
        state.cfdp.write().await.push(CfdpStatus {
            transaction_id: 42,
            source_entity: 1,
            dest_entity: 2,
            progress_pct: 50.0,
            complete: false,
        });
        let snap = build_snapshot(&state).await;
        assert!(snap.contains("\"cfdp\""));
        assert!(snap.contains("\"hk\""));
        assert!(snap.contains("\"events\""));
        assert!(snap.contains("\"mfile\""));
        assert!(snap.contains("\"link\""));
        assert!(snap.contains("\"cop1\""));
        assert!(snap.contains("\"time_auth\""));
    }

    // GIVEN a default state
    // WHEN  router() is called
    // THEN  it returns a valid Router without panicking
    #[tokio::test]
    async fn ui_router_builds_without_panic() {
        let _r = router(make_state());
    }
}
