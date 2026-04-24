use anyhow::Result;
use ground_station::ingest;
use log::info;

/// Ground station entry point.
///
/// Starts the tokio async runtime and wires the ingest pipeline channels.
/// Pipeline tasks (`AosFramer`, `VcDemux`, `SppDecoder`, `ApidRouter`, Sinks) are
/// spawned in Phase 22+. The uplink, CFDP, M-File, and UI workers follow in
/// Phases 23–29.
///
/// # Errors
///
/// Returns an error if the tokio runtime cannot be initialised or if any
/// mandatory startup I/O fails.
#[tokio::main]
async fn main() -> Result<()> {
    env_logger::init();

    // CLI: first positional arg is the ground station listen address.
    let listen_addr = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:10000".to_owned());

    info!("Ground station starting — listening on {listen_addr}");

    // Stub pipeline wiring (Phase 21 scaffold).
    // Channel pairs are declared with architecture-specified capacities (§5.3)
    // but are not yet connected to tasks. Spawn logic lands in Phase 22+.
    let (_aos_tx, _aos_rx) = tokio::sync::mpsc::channel::<Vec<u8>>(ingest::AOS_TO_DEMUX_CAP);
    let (_demux_tx, _demux_rx) = tokio::sync::mpsc::channel::<Vec<u8>>(ingest::DEMUX_TO_SPP_CAP);
    let (_spp_tx, _spp_rx) = tokio::sync::mpsc::channel::<Vec<u8>>(ingest::SPP_TO_ROUTER_CAP);
    let (_hk_tx, _hk_rx) = tokio::sync::mpsc::channel::<Vec<u8>>(ingest::ROUTER_TO_HK_CAP);
    let (_event_tx, _event_rx) = tokio::sync::mpsc::channel::<Vec<u8>>(ingest::ROUTER_TO_EVENT_CAP);
    let (_cfdp_tx, _cfdp_rx) = tokio::sync::mpsc::channel::<Vec<u8>>(ingest::ROUTER_TO_CFDP_CAP);
    let (_rover_tx, _rover_rx) = tokio::sync::mpsc::channel::<Vec<u8>>(ingest::ROUTER_TO_ROVER_CAP);

    // Yield once so the runtime is exercised before pipeline tasks land (Phase 22+).
    tokio::task::yield_now().await;

    Ok(())
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::panic)]
mod migration_tests {
    use ccsds_wire::PrimaryHeader;

    #[test]
    fn test_primary_header_decode_replaces_local_parser() {
        // Conformant CCSDS TM header: ver=000, type=0, sec-hdr=1, APID=0x123,
        // seq-flags=0b11, seq=1, data_length=4.
        // byte[0]=0x09: ver=000, type=0, sec-hdr=1, apid-hi=001 — sec-hdr flag
        // required by SAKURA-II mandate (arch §2.4); old telemetry.rs accepted
        // sec-hdr=0 which was a correctness gap.
        let buf = [0x09u8, 0x23, 0xC0, 0x01, 0x00, 0x04];
        let hdr = PrimaryHeader::decode(&buf).unwrap();
        assert_eq!(hdr.apid().get(), 0x0123);
        assert_eq!(hdr.sequence_count().get(), 1);
        assert_eq!(hdr.data_length().get(), 4);
    }

    #[test]
    fn test_too_short_returns_err() {
        let buf = [0x09u8; 5];
        assert!(PrimaryHeader::decode(&buf).is_err());
    }

    #[test]
    fn test_invalid_version_returns_err() {
        // byte[0]=0x29: ver=001 → InvalidVersion
        let buf = [0x29u8, 0x23, 0xC0, 0x01, 0x00, 0x04];
        assert!(PrimaryHeader::decode(&buf).is_err());
    }
}
