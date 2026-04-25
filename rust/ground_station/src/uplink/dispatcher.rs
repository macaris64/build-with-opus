//! Telecommand uplink dispatcher (Phase stub).
//!
//! Receives validated [`TcQueueEntry`] items from the `POST /api/tc` handler
//! and logs them.  Phase C+ will route each entry through the COP-1 engine and
//! TC framer to the radio uplink socket.

use tokio::sync::mpsc;

/// A validated TC intent ready for uplink.
///
/// Populated by the `POST /api/tc` REST handler after the command-validity
/// window check passes (SYS-REQ-0061).
#[derive(Debug, Clone)]
pub struct TcQueueEntry {
    /// Target APID (11-bit, 0x000–0x7FE; 0x000 is the stub placeholder).
    pub apid: u16,
    /// Function code (0x0000 is the stub placeholder; Phase C+ requires nonzero).
    pub func_code: u16,
    /// Command payload bytes (may be empty for NOOP-class commands).
    pub payload: Vec<u8>,
    /// TAI coarse deadline supplied by the operator (SYS-REQ-0061).
    pub valid_until_tai_coarse: u32,
}

/// Spawn the uplink dispatcher task.
///
/// Receives [`TcQueueEntry`] items from `tc_rx` and logs each one.
/// Phase C+: routes to `Cop1Engine` → `TcFramer` → UDP uplink socket.
#[must_use]
pub fn spawn_tc_dispatcher(tc_rx: mpsc::Receiver<TcQueueEntry>) -> tokio::task::JoinHandle<()> {
    tokio::spawn(run_dispatcher(tc_rx))
}

async fn run_dispatcher(mut rx: mpsc::Receiver<TcQueueEntry>) {
    let mut total: u64 = 0;
    while let Some(entry) = rx.recv().await {
        total += 1;
        log::info!(
            "TC queued: apid=0x{:03X} cc=0x{:04X} valid_until={} total={}",
            entry.apid,
            entry.func_code,
            entry.valid_until_tai_coarse,
            total,
        );
    }
    log::debug!("TC dispatcher shut down after {total} commands");
}
