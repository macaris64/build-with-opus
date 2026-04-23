mod telemetry;

use anyhow::Result;
use log::info;
use std::io::{self, Read};

fn main() -> Result<()> {
    env_logger::init();

    info!("Ground station starting — reading telemetry from stdin");

    let mut raw = Vec::new();
    io::stdin().read_to_end(&mut raw)?;

    match telemetry::parse(&raw) {
        Ok(pkt) => {
            info!(
                "Received telemetry: apid={} seq={} len={}",
                pkt.apid, pkt.sequence_count, pkt.data_length
            );
        }
        Err(e) => {
            log::error!("Telemetry parse error: {e}");
        }
    }

    Ok(())
}
