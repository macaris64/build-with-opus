use anyhow::Result;
use ccsds_wire::PrimaryHeader;
use log::info;
use std::io::{self, Read};

fn main() -> Result<()> {
    env_logger::init();

    info!("Ground station starting — reading telemetry from stdin");

    let mut raw = Vec::new();
    io::stdin().read_to_end(&mut raw)?;

    match PrimaryHeader::decode(&raw) {
        Ok(hdr) => {
            info!(
                "Received telemetry: apid={} seq={} len={}",
                hdr.apid().get(),
                hdr.sequence_count().get(),
                hdr.data_length().get(),
            );
        }
        Err(e) => {
            log::error!("Telemetry parse error: {e}");
        }
    }

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
