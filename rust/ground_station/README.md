# ground_station

SAKURA-II ground station binary: AOS TM ingest, CFDP Class 1 reception, TC uplink, and operator-UI backend. Written in Rust stable (Ferrocene-compatible target).

## Quick Start

```bash
# From the repo root
cargo build -p ground_station
cargo run -p ground_station -- 127.0.0.1:10000
```

The first positional argument is the listen address for the AOS/TC socket (default: `127.0.0.1:10000`).

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `RUST_LOG` | `info` | Log level (`trace`, `debug`, `info`, `warn`, `error`) |

## Pipeline Overview

```
AOS socket
    │
    ▼
AosFramer          FECF CRC-16, OCF/CLCW extraction, LinkState tracking
    │
    ▼
VcDemultiplexer    VC0 → SppDecoder; VC1 → event sink; VC2 → CFDP
    │
    ├──▶ SppDecoder → ApidRouter → HK sink / event sink / CFDP sink
    ├──▶ CFDP Class 1 receiver (Class1Receiver)
    ├──▶ MFileAssembler (out-of-order reassembly)
    └──▶ TC uplink: TcBuilder → Cop1Engine (FOP-1) → TcFramer
                                   │
                                   └──▶ Operator-UI (axum REST + WebSocket)
```

## API Surfaces (Phase 29)

| Route | Method | Description |
|-------|--------|-------------|
| `/api/hk` | GET | Per-APID housekeeping snapshots |
| `/api/events` | GET | Rolling event log (filter: severity, APID) |
| `/api/cfdp` | GET | Active + completed CFDP transactions |
| `/api/mfile` | GET | MFile chunk-gap status |
| `/api/link` | GET | AOS link state (AOS/LOS/Degraded) |
| `/api/cop1` | GET | FOP-1 state, window occupancy, retransmit count |
| `/api/time` | GET | TAI offset, drift budget, sync-packet age |
| `/api/tc` | POST | Submit a TC command (validates validity window) |
| `/ws` | WS | JSON snapshot of all 7 surfaces, then closes |

## Examples

Five runnable demos in [`examples/`](examples/) exercise the full production stack
with no mocking — real library code, real wire formats:

| Example | What it shows |
|---------|---------------|
| [`pipeline_demo`](examples/pipeline_demo.rs) | End-to-end AOS → VcDemux → SppDecoder → ApidRouter with live tokio RF link |
| [`uplink_session`](examples/uplink_session.rs) | TC uplink: TcBuilder → FOP-1 (Cop1Engine) → SDLP framing (TcFramer) |
| [`cfdp_session`](examples/cfdp_session.rs) | CFDP Class 1 downlink: happy path, CRC mismatch, out-of-order, timeout |
| [`mfile_reassembly`](examples/mfile_reassembly.rs) | M-File chunked reassembly: in-order, reordered, duplicates, timeout |
| [`cfs_bridge`](examples/cfs_bridge.rs) | cFS ↔ Rust MID decode, ApidRouter routing, round-trip guarantee (Q-C8) |

```bash
cargo run -p ground_station --example pipeline_demo
```

See [`examples/README.md`](examples/README.md) for full descriptions and usage.

## Architecture Reference

- Design: [`docs/architecture/06-ground-segment-rust.md`](../../docs/architecture/06-ground-segment-rust.md)
- Acceptance gate: § 12 (all 13 checkboxes passed at v0.1.0)
- Protocol stack: [`docs/architecture/07-comms-stack.md`](../../docs/architecture/07-comms-stack.md)

## Running Tests

```bash
cargo test -p ground_station                          # unit tests (108)
cargo test -p ccsds_wire --test proptests             # 7 property-based tests
cargo test -p ccsds_wire --test known_answers         # 5 KATs
cargo tarpaulin --workspace --skip-clean              # coverage (≥85 % ccsds_wire, ≥70 % ingest+cfdp)
```
