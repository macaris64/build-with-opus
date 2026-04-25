use anyhow::Result;
use bytes::Bytes;
use ground_station::ingest::{
    self, AosFrame, AOS_TO_DEMUX_CAP, SPP_TO_ROUTER_CAP,
    ApidRouter, Route,
    decoder::SppDecoder,
    demux::VcDemultiplexer,
    framer::{AosFramer, AOS_FRAME_LEN},
};
use ground_station::ui::{self, UiState};
use log::info;
use std::sync::Arc;
use tokio::io::AsyncWriteExt;
use tokio::sync::{mpsc, watch};

/// Ground station entry point.
///
/// Binds a UDP socket on `listen_addr`, feeds received AOS frames through the
/// full ingest pipeline (`AosFramer` → `VcDemux` → `SppDecoder` × 4 → `ApidRouter`),
/// and dispatches decoded packets to typed sink channels.
///
/// # Errors
///
/// Returns an error if the UDP socket cannot be bound or if ctrl-c cannot be
/// registered.
// Spawned tasks cannot propagate `?`; `.expect()` in tasks is the idiomatic
// abort-on-invariant-violation pattern when the invariant is statically guaranteed
// (e.g., `with_default_channels` always provides VCs 0–3).
// `too_many_lines`: main() orchestrates the full pipeline + UI server in one
// place intentionally; splitting across helpers obscures the startup order.
#[allow(clippy::expect_used, clippy::indexing_slicing, clippy::too_many_lines)]
#[tokio::main]
async fn main() -> Result<()> {
    env_logger::init();

    let listen_addr = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:10000".to_owned());

    info!("Ground station starting — listening on UDP {listen_addr}");

    // ── Shared UI state ───────────────────────────────────────────────────────
    let ui_state = Arc::new(UiState::new(37));

    // ── Pipeline channels ─────────────────────────────────────────────────────
    let (frame_tx, frame_rx) = mpsc::channel::<AosFrame>(AOS_TO_DEMUX_CAP);
    let (clcw_tx, _clcw_rx) = watch::channel::<Option<[u8; 4]>>(None);
    let (mut demux, mut vc_rxs) = VcDemultiplexer::with_default_channels();

    // Tagged (vc_id, packet bytes) channel feeds the ApidRouter.
    let (tagged_tx, mut tagged_rx) =
        mpsc::channel::<(u8, Bytes)>(SPP_TO_ROUTER_CAP * 4);

    // Sink channels — receivers kept as _ until Phase E sinks land.
    let (hk_tx, _hk_rx) = mpsc::channel::<Bytes>(ingest::ROUTER_TO_HK_CAP);
    let (event_tx, _event_rx) = mpsc::channel::<Bytes>(ingest::ROUTER_TO_EVENT_CAP);
    let (cfdp_tx, _cfdp_rx) = mpsc::channel::<Bytes>(ingest::ROUTER_TO_CFDP_CAP);
    let (rover_tx, _rover_rx) = mpsc::channel::<Bytes>(ingest::ROUTER_TO_ROVER_CAP);

    // ── Stage 1a: UDP receive → pipe ─────────────────────────────────────────
    // UdpSocket does not implement AsyncRead (datagrams, not a byte stream).
    // Bridge via tokio duplex: write UDP datagrams into the write-half so
    // AosFramer can read them as a continuous AOS-aligned byte stream.
    let (mut pipe_writer, pipe_reader) = tokio::io::duplex(AOS_FRAME_LEN * 64);

    tokio::spawn(async move {
        let socket = tokio::net::UdpSocket::bind(&listen_addr)
            .await
            .expect("failed to bind UDP socket");
        info!("UDP socket bound to {listen_addr}");
        let mut buf = vec![0u8; AOS_FRAME_LEN * 16];
        loop {
            match socket.recv(&mut buf).await {
                Ok(n) => {
                    if pipe_writer.write_all(&buf[..n]).await.is_err() {
                        break; // pipe_reader dropped — pipeline shutting down
                    }
                }
                Err(e) => {
                    log::error!("UDP recv error: {e}");
                    break;
                }
            }
        }
    });

    // ── Stage 1b: AosFramer ───────────────────────────────────────────────────
    tokio::spawn(async move {
        let mut framer = AosFramer::new(frame_tx, clcw_tx);
        if let Err(e) = framer.run(pipe_reader).await {
            log::error!("AosFramer error: {e}");
        }
    });

    // ── Stage 2: VcDemultiplexer ─────────────────────────────────────────────
    tokio::spawn(async move {
        demux.run(frame_rx).await;
    });

    // ── Stage 3: SppDecoder + vc-tag relay (one task pair per VC) ────────────
    for vc_id in [0u8, 1, 2, 3] {
        let vc_rx = vc_rxs
            .remove(&vc_id)
            .expect("VcDemux with_default_channels provides VC 0–3");
        let (spp_tx, mut spp_rx) = mpsc::channel::<Bytes>(SPP_TO_ROUTER_CAP);
        let tagged = tagged_tx.clone();

        tokio::spawn(async move {
            let mut dec = SppDecoder::new(vc_id);
            dec.run(vc_rx, spp_tx).await;
        });

        tokio::spawn(async move {
            while let Some(bytes) = spp_rx.recv().await {
                if tagged.send((vc_id, bytes)).await.is_err() {
                    break;
                }
            }
        });
    }
    // All per-VC relay tasks hold their own clone; drop the original so the
    // tagged_rx closes when all relay tasks finish.
    drop(tagged_tx);

    // ── Stage 4: ApidRouter + sink dispatch ──────────────────────────────────
    let ui_state_router = ui_state.clone();
    tokio::spawn(async move {
        use ccsds_wire::SpacePacket;
        let mut router = ApidRouter::new();
        while let Some((vc_id, bytes)) = tagged_rx.recv().await {
            let pkt = match SpacePacket::parse(&bytes) {
                Ok(p) => p,
                Err(e) => {
                    log::warn!("INGEST-ROUTER-PARSE-FAIL: {e}");
                    continue;
                }
            };
            match router.route(vc_id, &pkt) {
                Route::Hk => {
                    let _ = hk_tx.try_send(bytes);
                }
                Route::EventLog => {
                    let _ = event_tx.try_send(bytes);
                }
                Route::CfdpPdu => {
                    let _ = cfdp_tx.try_send(bytes);
                }
                Route::RoverForward => {
                    let _ = rover_tx.try_send(bytes);
                }
                Route::IdleFill => {}
                Route::Rejected { reason } => {
                    let apid = pkt.primary.apid().get();
                    log::warn!(
                        "INGEST-FORBIDDEN-APID apid=0x{apid:03X} reason={reason:?}"
                    );
                    // APID 0x541 (clock-skew) sets the time_suspect badge.
                    // try_write is non-blocking; skipped if lock is briefly held.
                    // Phase 40 DoD; Q-F2; §638.
                    if apid == 0x0541 {
                        if let Ok(mut auth) = ui_state_router.time_auth.try_write() {
                            auth.time_suspect_seen = true;
                        }
                    }
                }
            }
        }
    });

    // ── UI server (Phase 40+) — serves GET /api/time with time_suspect badge ─
    let ui_bind = std::env::var("UI_BIND").unwrap_or_else(|_| "0.0.0.0:8080".to_owned());
    tokio::spawn(async move {
        let app = ui::router(ui_state);
        let listener = tokio::net::TcpListener::bind(&ui_bind)
            .await
            .expect("UI TCP bind failed");
        info!("UI server listening on {ui_bind}");
        axum::serve(listener, app).await.expect("UI server error");
    });

    // ── Graceful shutdown ─────────────────────────────────────────────────────
    tokio::signal::ctrl_c().await?;
    info!("Ground station shutdown — SIGINT received");
    Ok(())
}

// ── Tests ────────────────────────────────────────────────────────────────────

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::cast_possible_truncation,
    clippy::indexing_slicing
)]
mod tests {
    use super::*;
    use ccsds_wire::{Apid, Cuc, FuncCode, InstanceId, PacketBuilder, SequenceCount};
    use crc::{Crc, CRC_16_IBM_3740};

    const SCID: u8 = 42;
    const DATA_FIELD_LEN: usize = AOS_FRAME_LEN - 6 - 2; // 1016 B (no OCF)
    const USER_DATA_LEN: usize = DATA_FIELD_LEN - 16;

    // Build a valid 1024-byte AOS frame carrying one CCSDS space packet.
    fn build_frame(apid: u16, vc_id: u8, seq: u16, func_code: u16) -> Vec<u8> {
        let mut user_data = vec![0u8; USER_DATA_LEN];
        user_data[0] = (seq >> 8) as u8;
        user_data[1] = (seq & 0xFF) as u8;

        let pkt = PacketBuilder::tm(Apid::new(apid).expect("apid in range"))
            .sequence_count(SequenceCount::new(seq).expect("seq in range"))
            .cuc(Cuc { coarse: 1_000_000 + u32::from(seq) * 100, fine: 0 })
            .func_code(FuncCode::new(func_code).expect("func_code nonzero"))
            .instance_id(InstanceId::new(1).expect("instance_id nonzero"))
            .user_data(&user_data)
            .build()
            .expect("packet build must succeed");

        debug_assert_eq!(pkt.len(), DATA_FIELD_LEN, "packet must fill data field");

        let mut frame = vec![0u8; AOS_FRAME_LEN];
        frame[0] = (0b01 << 6) | (SCID >> 2);
        frame[1] = ((SCID & 0x03) << 6) | (vc_id & 0x3F);
        frame[4] = (seq & 0xFF) as u8;
        frame[5] = 0x00; // OCF_FLAG = 0
        frame[6..6 + DATA_FIELD_LEN].copy_from_slice(&pkt);

        let crc_engine = Crc::<u16>::new(&CRC_16_IBM_3740);
        let fecf = crc_engine.checksum(&frame[..AOS_FRAME_LEN - 2]);
        let [hi, lo] = fecf.to_be_bytes();
        frame[AOS_FRAME_LEN - 2] = hi;
        frame[AOS_FRAME_LEN - 1] = lo;

        frame
    }

    /// End-to-end pipeline test: 9-packet AOS stream, all packets route correctly.
    ///
    /// Mirrors `pipeline_demo.rs` but in a deterministic `#[tokio::test]` with
    /// explicit routing assertions per the `ApidRouter` table (§5.4).
    #[tokio::test]
    async fn test_pipeline_routes_nine_packet_stream() {
        // ── Build 9 test frames ───────────────────────────────────────────────
        let packets: &[(u16, u8, u16, &str)] = &[
            (0x101, 0, 1, "orbiter_cdh HK → Hk"),
            (0x110, 0, 2, "orbiter_adcs HK → Hk"),
            (0x120, 0, 3, "orbiter_comm HK → Hk"),
            (0x130, 0, 4, "orbiter_power HK → Hk"),
            (0x140, 0, 5, "orbiter_payload HK → Hk"),
            (0x102, 1, 6, "orbiter_cdh EVENT VC1 → EventLog"),
            (0x300, 3, 7, "rover_land HK VC3 → RoverForward"),
            (0x540, 0, 8, "fault inject APID → Rejected"),
            (0x7FF, 63, 9, "idle fill VC63 → discarded by demux"),
        ];

        // Concatenate all frames into a single byte stream for AosFramer.
        let mut stream_bytes: Vec<u8> = Vec::new();
        for &(apid, vc_id, seq, _) in packets {
            let frame = build_frame(apid, vc_id, seq, seq + 1);
            stream_bytes.extend_from_slice(&frame);
        }

        // ── Wire pipeline (same topology as main()) ───────────────────────────
        let (frame_tx, frame_rx) = mpsc::channel::<AosFrame>(AOS_TO_DEMUX_CAP);
        let (clcw_tx, _clcw_rx) = watch::channel::<Option<[u8; 4]>>(None);
        let (mut demux, mut vc_rxs) = VcDemultiplexer::with_default_channels();
        let (tagged_tx, mut tagged_rx) =
            mpsc::channel::<(u8, Bytes)>(SPP_TO_ROUTER_CAP * 4);

        let (hk_tx, mut hk_rx) = mpsc::channel::<Bytes>(ingest::ROUTER_TO_HK_CAP);
        let (event_tx, mut event_rx) = mpsc::channel::<Bytes>(ingest::ROUTER_TO_EVENT_CAP);
        let (cfdp_tx, _cfdp_rx) = mpsc::channel::<Bytes>(ingest::ROUTER_TO_CFDP_CAP);
        let (rover_tx, mut rover_rx) = mpsc::channel::<Bytes>(ingest::ROUTER_TO_ROVER_CAP);

        // AosFramer reads from an in-memory cursor (no UDP needed in tests).
        let cursor = std::io::Cursor::new(stream_bytes);
        let reader = tokio::io::BufReader::new(cursor);

        tokio::spawn(async move {
            let mut framer = AosFramer::new(frame_tx, clcw_tx);
            framer.run(reader).await.expect("framer run");
        });

        tokio::spawn(async move {
            demux.run(frame_rx).await;
        });

        for vc_id in [0u8, 1, 2, 3] {
            let vc_rx = vc_rxs.remove(&vc_id).expect("vc receiver exists");
            let (spp_tx, mut spp_rx) = mpsc::channel::<Bytes>(SPP_TO_ROUTER_CAP);
            let tagged = tagged_tx.clone();
            tokio::spawn(async move {
                let mut dec = SppDecoder::new(vc_id);
                dec.run(vc_rx, spp_tx).await;
            });
            tokio::spawn(async move {
                while let Some(bytes) = spp_rx.recv().await {
                    if tagged.send((vc_id, bytes)).await.is_err() {
                        break;
                    }
                }
            });
        }
        drop(tagged_tx);

        // Collect routing decisions
        tokio::spawn(async move {
            use ccsds_wire::SpacePacket;
            let mut router = ApidRouter::new();
            while let Some((vc_id, bytes)) = tagged_rx.recv().await {
                let pkt = SpacePacket::parse(&bytes).expect("valid packet bytes");
                match router.route(vc_id, &pkt) {
                    Route::Hk => { let _ = hk_tx.try_send(bytes); }
                    Route::EventLog => { let _ = event_tx.try_send(bytes); }
                    Route::CfdpPdu => { let _ = cfdp_tx.try_send(bytes); }
                    Route::RoverForward => { let _ = rover_tx.try_send(bytes); }
                    Route::IdleFill | Route::Rejected { .. } => {}
                }
            }
        });

        // ── Assert sink counts ────────────────────────────────────────────────
        // Give the async pipeline time to drain. tokio::time::sleep isn't
        // deterministic, so we poll with a short timeout instead.
        let deadline = std::time::Instant::now() + std::time::Duration::from_secs(5);
        let (mut hk_count, mut event_count, mut rover_count) = (0usize, 0usize, 0usize);
        while std::time::Instant::now() < deadline {
            while hk_rx.try_recv().is_ok() { hk_count += 1; }
            while event_rx.try_recv().is_ok() { event_count += 1; }
            while rover_rx.try_recv().is_ok() { rover_count += 1; }
            if hk_count >= 5 && event_count >= 1 && rover_count >= 1 {
                break;
            }
            tokio::time::sleep(std::time::Duration::from_millis(10)).await;
        }

        // HK packets: APIDs 0x101, 0x110, 0x120, 0x130, 0x140 (all on VC 0)
        assert_eq!(hk_count, 5, "expected 5 HK packets routed to HK sink");
        // EventLog: APID 0x102 on VC 1
        assert_eq!(event_count, 1, "expected 1 packet routed to EventLog sink");
        // RoverForward: APID 0x300 on VC 3
        assert_eq!(rover_count, 1, "expected 1 packet routed to RoverForward sink");
        // APID 0x540 is rejected (Q-F2) — never reaches a sink
        // APID 0x7FF on VC 63 is silently discarded by VcDemux
    }

    #[test]
    fn test_primary_header_decode_replaces_local_parser() {
        // Conformant CCSDS TM header: ver=000, type=0, sec-hdr=1, APID=0x123,
        // seq-flags=0b11, seq=1, data_length=4.
        let buf = [0x09u8, 0x23, 0xC0, 0x01, 0x00, 0x04];
        let hdr = ccsds_wire::PrimaryHeader::decode(&buf).unwrap();
        assert_eq!(hdr.apid().get(), 0x0123);
        assert_eq!(hdr.sequence_count().get(), 1);
        assert_eq!(hdr.data_length().get(), 4);
    }

    #[test]
    fn test_too_short_returns_err() {
        let buf = [0x09u8; 5];
        assert!(ccsds_wire::PrimaryHeader::decode(&buf).is_err());
    }

    #[test]
    fn test_invalid_version_returns_err() {
        // byte[0]=0x29: ver=001 → InvalidVersion
        let buf = [0x29u8, 0x23, 0xC0, 0x01, 0x00, 0x04];
        assert!(ccsds_wire::PrimaryHeader::decode(&buf).is_err());
    }
}
