//! Ground station — library root for all pipeline submodules.
//!
//! Submodule layout (docs/architecture/06-ground-segment-rust.md §5–10):
//!
//! ```text
//! ingest  ─ AosFramer → VcDemux → SppDecoder → ApidRouter → Sinks
//! uplink  ─ TcBuilder → Cop1Engine → TcFramer → UplinkSink
//! cfdp    ─ CfdpProvider / CfdpReceiver traits + Class1Receiver (Phase 25+)
//! mfile   ─ MFileAssembler (out-of-order, BitSet completion, RAM-bounded)
//! ui      ─ WebSocket + REST backend (7 telemetry surfaces, Phase 29+)
//! ```

pub mod cfdp;
pub mod ingest;
pub mod mfile;
pub mod ui;
pub mod uplink;
