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
    body::Body,
    extract::{
        ws::{Message, WebSocket, WebSocketUpgrade},
        Request, State,
    },
    http::StatusCode,
    response::IntoResponse,
    routing::{get, post},
    Json, Router,
};
use tower::util::ServiceExt as _;
use tower_http::cors::CorsLayer;
use tower_http::services::ServeDir;
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
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct TcSubmitRequest {
    /// TAI coarse seconds of the command's validity-window deadline.  The
    /// operator tooling converts its UTC deadline to TAI before submission.
    pub valid_until_tai_coarse: u32,
    /// CCSDS APID of the target app.  `None` leaves the stub value (Phase C+).
    #[serde(default)]
    pub apid: Option<u16>,
    /// Function code identifying the specific command.  `None` leaves the stub.
    #[serde(default)]
    pub func_code: Option<u16>,
}

/// Response body for `POST /api/tc`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TcSubmitResponse {
    pub accepted: bool,
}

// ── Link health DTOs (Phases 5–7) ────────────────────────────────────────────

/// Parsed view of the `ros2_bridge` HK packet (APID 0x128, cFS ↔ Space ROS link).
///
/// Payload layout: 6 × uint32 in host byte order (LE on x86), starting at
/// `data[0]` after the 16-byte CCSDS header is stripped by the ingest sink.
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct CfsRosLinkDto {
    pub packets_routed: u32,
    pub apid_rejects:   u32,
    pub tc_forwarded:   u32,
    pub uptime_s:       u32,
    pub cmd_counter:    u32,
    pub err_counter:    u32,
    /// UTC ISO-8601 timestamp of the most recent HK frame, or `None` if no
    /// frame has been received since startup.
    pub last_hk_utc: Option<String>,
}

/// Parsed view of the Proximity-1 link-state packet (APID 0x129, ROS 2 ↔ Ground).
///
/// Payload layout (10 B, big-endian):
///   byte 0:    `session_active` (0 = false, 1 = true)
///   byte 1:    `signal_strength` (0–255)
///   bytes 2-9: `last_contact_s` (f64 BE, UNIX seconds)
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct RosGroundLinkDto {
    pub session_active:  bool,
    pub signal_strength: u8,
    pub last_contact_s:  f64,
    /// UTC ISO-8601 timestamp of the most recent HK frame.
    pub last_hk_utc: Option<String>,
}

/// Parsed view of the `fleet_monitor` heartbeat packet (APID 0x160, DDS intra-fleet).
///
/// Payload layout (13 B, big-endian):
///   byte 0:     `health_mask` (bit 0=land, bit 1=uav, bit 2=cryo)
///   bytes 1-4:  `land_age_ms`  (uint32 BE)
///   bytes 5-8:  `uav_age_ms`   (uint32 BE)
///   bytes 9-12: `cryo_age_ms`  (uint32 BE)
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct FleetDdsLinkDto {
    pub health_mask: u8,
    pub land_age_ms: u32,
    pub uav_age_ms:  u32,
    pub cryo_age_ms: u32,
    /// UTC ISO-8601 timestamp of the most recent HK frame.
    pub last_hk_utc: Option<String>,
}

/// One entry in the per-link freshness summary embedded in the WS snapshot
/// (Phase 8).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LinkHealthSnapshot {
    /// Human-readable link identifier (e.g. `"cfs-ros"`, `"fleet-dds"`).
    pub link:        String,
    pub apid:        u16,
    /// UTC ISO-8601 timestamp of the latest received frame, or `None` if no
    /// frame has been received since startup.
    pub last_hk_utc: Option<String>,
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

// ── Link health helpers ───────────────────────────────────────────────────────

/// Return the most recent [`HkFrame`] for `apid`, or `None` if unseen.
fn latest_hk_frame(hk: &[HkSnapshot], apid: u16) -> Option<&HkFrame> {
    hk.iter().find(|s| s.apid == apid).and_then(|s| s.frames.last())
}

/// Read a little-endian `u32` from `data[offset..offset+4]`, returning 0 on
/// under-run (cFS HK payloads are host-byte-order / LE on x86).
fn read_le_u32(data: &[u8], offset: usize) -> u32 {
    data.get(offset..offset.saturating_add(4))
        .and_then(|s| <[u8; 4]>::try_from(s).ok())
        .map_or(0, u32::from_le_bytes)
}

/// Read a big-endian `u32` from `data[offset..offset+4]`, returning 0 on
/// under-run (ROS 2 `TmBridge` payloads are always big-endian).
fn read_be_u32(data: &[u8], offset: usize) -> u32 {
    data.get(offset..offset.saturating_add(4))
        .and_then(|s| <[u8; 4]>::try_from(s).ok())
        .map_or(0, u32::from_be_bytes)
}

/// Read a big-endian `f64` from `data[offset..offset+8]`, returning 0.0 on
/// under-run.
fn read_be_f64(data: &[u8], offset: usize) -> f64 {
    data.get(offset..offset.saturating_add(8))
        .and_then(|s| <[u8; 8]>::try_from(s).ok())
        .map_or(0.0, f64::from_be_bytes)
}

/// Build the per-link freshness summary for the three comm links.
fn build_links_health(hk: &[HkSnapshot]) -> Vec<LinkHealthSnapshot> {
    const LINKS: [(u16, &str); 3] = [
        (0x128, "cfs-ros"),
        (0x129, "ros-ground"),
        (0x160, "fleet-dds"),
    ];
    LINKS.iter().map(|&(apid, name)| LinkHealthSnapshot {
        link:        name.to_owned(),
        apid,
        last_hk_utc: latest_hk_frame(hk, apid).map(|f| f.timestamp_utc.clone()),
    }).collect()
}

/// `GET /api/link/cfs-ros` — returns cFS ↔ Space ROS link health derived from
/// the `ros2_bridge` HK packet (APID 0x128).
async fn get_cfs_ros_link(State(state): State<Arc<UiState>>) -> Json<CfsRosLinkDto> {
    let hk = state.hk.read().await;
    Json(if let Some(frame) = latest_hk_frame(&hk, 0x128) {
        let d = &frame.data;
        CfsRosLinkDto {
            packets_routed: read_le_u32(d, 0),
            apid_rejects:   read_le_u32(d, 4),
            tc_forwarded:   read_le_u32(d, 8),
            uptime_s:       read_le_u32(d, 12),
            cmd_counter:    read_le_u32(d, 16),
            err_counter:    read_le_u32(d, 20),
            last_hk_utc:    Some(frame.timestamp_utc.clone()),
        }
    } else {
        CfsRosLinkDto::default()
    })
}

/// `GET /api/link/ros-ground` — returns Space ROS ↔ Ground link health derived
/// from the Proximity-1 link-state packet (APID 0x129).
async fn get_ros_ground_link(State(state): State<Arc<UiState>>) -> Json<RosGroundLinkDto> {
    let hk = state.hk.read().await;
    Json(if let Some(frame) = latest_hk_frame(&hk, 0x129) {
        let d = &frame.data;
        RosGroundLinkDto {
            session_active:  d.first().copied().unwrap_or(0) != 0,
            signal_strength: d.get(1).copied().unwrap_or(0),
            last_contact_s:  read_be_f64(d, 2),
            last_hk_utc:     Some(frame.timestamp_utc.clone()),
        }
    } else {
        RosGroundLinkDto::default()
    })
}

/// `GET /api/link/fleet-dds` — returns Space ROS ↔ Space ROS DDS heartbeat
/// health derived from the `fleet_monitor` packet (APID 0x160).
async fn get_fleet_dds_link(State(state): State<Arc<UiState>>) -> Json<FleetDdsLinkDto> {
    let hk = state.hk.read().await;
    Json(if let Some(frame) = latest_hk_frame(&hk, 0x160) {
        let d = &frame.data;
        FleetDdsLinkDto {
            health_mask: d.first().copied().unwrap_or(0),
            land_age_ms: read_be_u32(d, 1),
            uav_age_ms:  read_be_u32(d, 5),
            cryo_age_ms: read_be_u32(d, 9),
            last_hk_utc: Some(frame.timestamp_utc.clone()),
        }
    } else {
        FleetDdsLinkDto::default()
    })
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
    // Pass apid/func_code from the request when provided; fall back to 0 until
    // the TC catalog and COP-1 encoder land in Phase C+.
    let entry = TcQueueEntry {
        apid:                   req.apid.unwrap_or(0),
        func_code:              req.func_code.unwrap_or(0),
        payload:                Vec::new(),
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

/// Fallback handler that serves the Vite production build from `static/`.
/// Handles SPA routing: directories append `index.html`.  I/O errors
/// produce a 500 rather than propagating (the operator UI is the only caller).
async fn static_handler(req: Request<Body>) -> impl IntoResponse {
    match ServeDir::new("static")
        .append_index_html_on_directories(true)
        .oneshot(req)
        .await
    {
        Ok(res) => res.into_response(),
        Err(e) => {
            log::warn!("static file serve error: {e}");
            StatusCode::INTERNAL_SERVER_ERROR.into_response()
        }
    }
}

/// Serialises all seven surfaces plus the four-link health summary into a
/// single JSON object for the WS snapshot (Phase 8).
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
        /// Per-link freshness for cfs-ros (0x128), ros-ground (0x129),
        /// fleet-dds (0x160) — derived from the HK ring buffer.
        links_health: Vec<LinkHealthSnapshot>,
    }
    let hk = state.hk.read().await;
    let events = state.events.read().await;
    let cfdp = state.cfdp.read().await;
    let mfile = state.mfile.read().await;
    let link = state.link.read().await;
    let cop1 = state.cop1.read().await;
    let time_auth = state.time_auth.read().await;
    let links_health = build_links_health(&hk);
    serde_json::to_string(&Snapshot {
        hk: &hk,
        events: &events,
        cfdp: &cfdp,
        mfile: &mfile,
        link: &link,
        cop1: &cop1,
        time_auth: &time_auth,
        links_health,
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
        .route("/api/link/cfs-ros",    get(get_cfs_ros_link))
        .route("/api/link/ros-ground", get(get_ros_ground_link))
        .route("/api/link/fleet-dds",  get(get_fleet_dds_link))
        .route("/ws", get(ws_handler))
        .with_state(state)
        .fallback(static_handler)
        .layer(CorsLayer::permissive())
}

// ── Tests ─────────────────────────────────────────────────────────────────────

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used,
    clippy::float_cmp,
    clippy::indexing_slicing
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
            ..Default::default()
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
            ..Default::default()
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

    // ── Phase 5: cFS ↔ Space ROS link (APID 0x128) ──────────────────────────

    // GIVEN no HK snapshot for APID 0x128
    // WHEN  GET /api/link/cfs-ros handler is called
    // THEN  returns default DTO with all-zero counters and no timestamp
    #[tokio::test]
    async fn ui_get_cfs_ros_link_empty_returns_default() {
        let Json(result) = get_cfs_ros_link(State(make_state())).await;
        assert_eq!(result.packets_routed, 0);
        assert_eq!(result.err_counter, 0);
        assert!(result.last_hk_utc.is_none());
    }

    // GIVEN an HK snapshot for APID 0x128 with PacketsRouted=42 (LE bytes)
    // WHEN  GET /api/link/cfs-ros handler is called
    // THEN  returns packets_routed=42 and a non-None timestamp
    #[tokio::test]
    async fn ui_get_cfs_ros_link_parses_le_payload() {
        let state = make_state();
        let mut data = vec![0u8; 24];
        data[0..4].copy_from_slice(&42u32.to_le_bytes());
        state.hk.write().await.push(HkSnapshot {
            asset:  "ros2_bridge".into(),
            apid:   0x128,
            frames: vec![HkFrame { timestamp_utc: "2026-01-01T00:00:00Z".into(), data }],
        });
        let Json(result) = get_cfs_ros_link(State(state)).await;
        assert_eq!(result.packets_routed, 42);
        assert!(result.last_hk_utc.is_some());
    }

    // ── Phase 6: Space ROS ↔ Ground link (APID 0x129) ───────────────────────

    // GIVEN no HK snapshot for APID 0x129
    // WHEN  GET /api/link/ros-ground handler is called
    // THEN  returns default (session_active=false, signal_strength=0)
    #[tokio::test]
    async fn ui_get_ros_ground_link_empty_returns_default() {
        let Json(result) = get_ros_ground_link(State(make_state())).await;
        assert!(!result.session_active);
        assert_eq!(result.signal_strength, 0);
        assert!(result.last_hk_utc.is_none());
    }

    // GIVEN an HK snapshot for APID 0x129 with session_active=1, signal_strength=85
    // WHEN  GET /api/link/ros-ground handler is called
    // THEN  returns session_active=true and signal_strength=85
    #[tokio::test]
    async fn ui_get_ros_ground_link_parses_be_payload() {
        let state = make_state();
        let mut data = vec![0u8; 10];
        data[0] = 1;   // session_active
        data[1] = 85;  // signal_strength (0–255)
        // data[2..10] = 0 → last_contact_s = 0.0
        state.hk.write().await.push(HkSnapshot {
            asset:  "prx1_link_state".into(),
            apid:   0x129,
            frames: vec![HkFrame { timestamp_utc: "2026-01-01T00:00:00Z".into(), data }],
        });
        let Json(result) = get_ros_ground_link(State(state)).await;
        assert!(result.session_active);
        assert_eq!(result.signal_strength, 85);
    }

    // ── Phase 7: Space ROS ↔ Space ROS fleet DDS (APID 0x160) ───────────────

    // GIVEN no HK snapshot for APID 0x160
    // WHEN  GET /api/link/fleet-dds handler is called
    // THEN  returns default (health_mask=0, all ages=0)
    #[tokio::test]
    async fn ui_get_fleet_dds_link_empty_returns_default() {
        let Json(result) = get_fleet_dds_link(State(make_state())).await;
        assert_eq!(result.health_mask, 0);
        assert_eq!(result.land_age_ms, 0);
        assert!(result.last_hk_utc.is_none());
    }

    // GIVEN an HK snapshot for APID 0x160 with health_mask=0x07 and land_age=1000 ms
    // WHEN  GET /api/link/fleet-dds handler is called
    // THEN  returns health_mask=7 and land_age_ms=1000
    #[tokio::test]
    async fn ui_get_fleet_dds_link_parses_be_payload() {
        let state = make_state();
        let mut data = vec![0u8; 13];
        data[0] = 0x07;
        data[1..5].copy_from_slice(&1000u32.to_be_bytes());
        state.hk.write().await.push(HkSnapshot {
            asset:  "fleet_monitor".into(),
            apid:   0x160,
            frames: vec![HkFrame { timestamp_utc: "2026-01-01T00:00:00Z".into(), data }],
        });
        let Json(result) = get_fleet_dds_link(State(state)).await;
        assert_eq!(result.health_mask, 0x07);
        assert_eq!(result.land_age_ms, 1000);
    }

    // ── Phase 8: WS snapshot includes links_health ───────────────────────────

    // GIVEN a default (empty) state
    // WHEN  build_snapshot is called
    // THEN  the JSON contains a "links_health" key
    #[tokio::test]
    async fn ui_build_snapshot_contains_links_health_key() {
        let snap = build_snapshot(&make_state()).await;
        assert!(snap.contains("\"links_health\""),
            "snapshot must contain links_health surface");
    }

    // GIVEN a state with an APID 0x160 HK snapshot
    // WHEN  build_snapshot is called
    // THEN  the JSON contains the "fleet-dds" link name
    #[tokio::test]
    async fn ui_build_snapshot_links_health_includes_fleet_dds() {
        let state = make_state();
        state.hk.write().await.push(HkSnapshot {
            asset:  "fleet_monitor".into(),
            apid:   0x160,
            frames: vec![HkFrame {
                timestamp_utc: "2026-01-01T00:00:00Z".into(),
                data:          vec![0u8; 13],
            }],
        });
        let snap = build_snapshot(&state).await;
        assert!(snap.contains("\"fleet-dds\""),
            "links_health must include fleet-dds entry when APID 0x160 is present");
    }

    // ── Helper unit tests ────────────────────────────────────────────────────

    // GIVEN a byte slice with a known LE u32 at offset 0
    // WHEN  read_le_u32 is called
    // THEN  returns the correct value
    #[test]
    fn ui_read_le_u32_correct() {
        let data = 0xDEAD_BEEFu32.to_le_bytes();
        assert_eq!(read_le_u32(&data, 0), 0xDEAD_BEEF);
    }

    // GIVEN a byte slice with a known BE u32 at offset 1
    // WHEN  read_be_u32 is called
    // THEN  returns the correct value
    #[test]
    fn ui_read_be_u32_with_offset() {
        let mut data = vec![0u8; 5];
        data[1..5].copy_from_slice(&0x0102_0304u32.to_be_bytes());
        assert_eq!(read_be_u32(&data, 1), 0x0102_0304);
    }

    // GIVEN a slice shorter than 4 bytes
    // WHEN  read_le_u32 is called
    // THEN  returns 0 without panicking
    #[test]
    fn ui_read_le_u32_underrun_returns_zero() {
        assert_eq!(read_le_u32(&[0xFF, 0xFF], 0), 0);
    }
}
