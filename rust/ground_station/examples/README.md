# ground_station examples

Runnable demos that exercise the full production ground station stack — no mocking,
no test fixtures. Each binary drives the real library code so you can see live data
flowing through every pipeline stage.

## Running an example

```bash
# From the repo root
cargo run -p ground_station --example <name>
```

---

## pipeline_demo

**Full end-to-end telemetry pipeline — spacecraft to ground station.**

Simulates a nine-packet SAKURA-II downlink session through every ingest layer:

```
SPACECRAFT (9 packets: orbiter + rover + fault-inject + idle)
  │  PacketBuilder → CCSDS TM Space Packets (ccsds_wire)
  │  Wrapped in 1024-byte AOS Transfer Frames (SCID=42, FECF CRC-16/IBM-3740)
  ▼
RF LINK  (tokio duplex — stands in for TCP/UDP)
  ▼
GROUND STATION
  Stage 1: AosFramer     FECF validation, link-state machine
  Stage 2: VcDemux       routes frames by Virtual Channel ID
  Stage 3: SppDecoder    SpacePacket::parse() per VC
  Stage 4: ApidRouter    APID-range dispatch + Q-F2 forbidden-APID rejection
```

Packet catalog covered: `orbiter_cdh` (VC0), `orbiter_adcs` (VC0), `orbiter_comm` (VC0),
`orbiter_power` (VC0), `orbiter_payload` (VC0), CDH event (VC1), `rover_land` (VC3),
fault-inject APID 0x540 (Q-F2 rejection), and idle fill (VC63 discard).

```bash
cargo run -p ground_station --example pipeline_demo
```

---

## uplink_session

**TC uplink — three-stage command pipeline (TcBuilder → FOP-1 → SDLP).**

Walks a complete operator-to-spacecraft telecommand session:

```
TcIntent (operator intent)
  │  TcBuilder::build()  catalog validation + CCSDS TC encoding
  ▼
CCSDS TC Space Packet
  │  Cop1Engine::submit() / tick()  FOP-1 sliding-window sequencing
  ▼
TcFrame (AD / BD / BC)
  │  TcFramer::frame()  SDLP primary header + FECF wrapping
  ▼
SDLP wire bytes → modulator → spacecraft
```

Demonstrates: safety rejection (drill current > 10 A), BC init sequence
(`Initial → Initializing → Active`), six AD command frames, CLCW acknowledgement,
and FECF verification of encoded SDLP frames.

```bash
cargo run -p ground_station --example uplink_session
```

---

## cfdp_session

**CFDP Class 1 file downlink — four transfer scenarios.**

Simulates CCSDS File Delivery Protocol (727.0-B-5) unacknowledged downlink:

| Transfer | Scenario |
|----------|----------|
| 1 | Happy path — Metadata → FileData × 2 → EOF → finalize |
| 2 | CRC mismatch (bit error) — `.partial` file retained |
| 3 | Out-of-order PDU delivery — FileData before Metadata |
| 4 | Timeout eviction — incomplete transfer abandoned |

```bash
cargo run -p ground_station --example cfdp_session
```

---

## mfile_reassembly

**M-File chunked reassembly — five delivery scenarios.**

Exercises `MFileAssembler`, the relay-satellite file-transfer protocol (AOS VC2,
34-byte binary header with CRC-32 per chunk):

| Scenario | Description |
|----------|-------------|
| 1 | In-order delivery — 600 B file, 3 chunks |
| 2 | Out-of-order delivery — chunks arrive as [2, 0, 1] |
| 3 | Duplicate chunk (same content) — idempotent, counter incremented |
| 4 | Duplicate chunk (different content) — first-arrival wins, mismatch flagged |
| 5 | Timeout eviction — incomplete transfer abandoned as `.partial` |

```bash
cargo run -p ground_station --example mfile_reassembly
```

---

## cfs_bridge

**cFS ↔ Rust message boundary — cFE MID decoding and APID routing.**

Shows how the ground station ingests raw cFE Software Bus bytes using only
`ccsds_wire` (Q-C8 locus A):

| Scenario | Description |
|----------|-------------|
| 1 | MID table — encode and decode all SAKURA-II orbiter MIDs |
| 2 | TM inbound — raw SB bytes → SpacePacket → ApidRouter routing |
| 3 | TC outbound — PacketBuilder → CCSDS TC bytes → type-bit check |
| 4 | Round-trip — encode → decode → re-encode is byte-identical (Q-C8) |
| 5 | Error paths — truncated buffer, invalid version, length mismatch, Q-F2 reject |

```bash
cargo run -p ground_station --example cfs_bridge
```
