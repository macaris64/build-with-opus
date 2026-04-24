# APID & MID Registry

> Terminology: [../GLOSSARY.md](../GLOSSARY.md). Parent protocol spec: [CCSDS 133.0-B-2 Space Packet Protocol](../standards/references.md). Source of truth for SCID: [`_defs/mission_config.h`](../../_defs/mission_config.h).

This is the **canonical registry** for CCSDS APIDs and cFE Message IDs used in SAKURA-II. Every MID macro defined under `apps/**`, every AOS virtual-channel assignment, and every ground-side decoder must trace back to an entry here.

## Identifiers and Ranges

CCSDS primary-header fields relevant to SAKURA-II:

| Field | Bits | Value | Source |
|---|---|---|---|
| Version | 3 | `0b000` (CCSDS v1) | CCSDS 133.0-B-2 |
| Type | 1 | `0` = TM, `1` = TC | per-packet |
| Secondary Header Flag | 1 | `1` (we always include a time tag) | project convention |
| APID | 11 | see allocation below | this doc |
| Sequence Flags | 2 | `0b11` (standalone) | per-packet |
| Packet Sequence Count | 14 | per-APID rolling counter | FSW runtime |

**Spacecraft ID (SCID)** — 10-bit field in the TM Transfer Frame (CCSDS 132.0-B) and in CCSDS secondary headers.
Authoritative value: `SPACECRAFT_ID = 42U` — see [`_defs/mission_config.h`](../../_defs/mission_config.h).
`MISSION_NAME = "SAKURA_II"` was adopted in Phase 11 (see `_defs/targets.cmake` and `_defs/mission_config.h`). Fleet SCID allocation anchors on `SAKURA_II_SCID_BASE = 42U`; per-instance SCIDs are derived by offset from the anchor and are not allocated via additional entries in this registry. Any change to the anchor MUST be reflected here before the build flips.

## APID Allocation

APIDs are partitioned into fleet-class blocks. A **block is owned by a vehicle class** (orbiter, relay, land rover, UAV, cryobot); instance multiplicity within a class is handled by the `instance_id` field of the CCSDS secondary header, **not** by burning additional APIDs. This preserves APID space as the fleet scales from MVC (1 of each) to the stress-test target (5+ of each).

The 11-bit APID space is 0x000–0x7FF. Reserved ranges per CCSDS convention:

| Range | Purpose |
|---|---|
| `0x000` | CCSDS Idle Packet (reserved, never used) |
| `0x001`–`0x0FF` | Reserved for future inter-agency interop |
| `0x7FF` | Idle / fill (reserved) |

SAKURA-II allocation within the usable window:

| APID range | Block | Vehicle class | Direction | Notes |
|---|---|---|---|---|
| `0x100`–`0x17F` | Orbiter TM | cFS orbiters | downlink | 128 APIDs; subdivided per-app below |
| `0x180`–`0x1FF` | Orbiter TC | cFS orbiters | uplink | 128 APIDs; one per-app command endpoint |
| `0x200`–`0x23F` | Relay TM | FreeRTOS smallsat relay | downlink | 64 APIDs |
| `0x240`–`0x27F` | Relay TC | FreeRTOS smallsat relay | uplink | 64 APIDs |
| `0x280`–`0x2FF` | Subsystem-MCU | FreeRTOS MCUs on cFS bus | bidirectional (via cFS gateway) | Per-MCU-class, see below |
| `0x300`–`0x37F` | Rover-Land TM | Space ROS 2 land rover | downlink (via relay) | 128 APIDs |
| `0x380`–`0x3BF` | Rover-Land TC | Space ROS 2 land rover | uplink (via relay) | 64 APIDs |
| `0x3C0`–`0x3FF` | Rover-UAV TM/TC | Space ROS 2 UAV | bidirectional | 64 APIDs |
| `0x400`–`0x43F` | Rover-Cryobot TM | Subsurface rover | downlink (tether-gated) | 64 APIDs; low-BW, see note |
| `0x440`–`0x45F` | Rover-Cryobot TC | Subsurface rover | uplink (tether-gated) | 32 APIDs |
| `0x500`–`0x57F` | Sim injection | Gazebo → FSW | sideband | 128 APIDs; never flight-path; see [`ICD-sim-fsw.md`](ICD-sim-fsw.md) (planned) |
| `0x600`–`0x67F` | Ground-segment internal | within `rust/ground_station/` | N/A | Not on any RF link; local bus only |

**Cryobot note.** The cryobot's tether link is bandwidth-constrained (see open question in plan; exact budget lands in `ICD-cryobot-tether.md`). Its 96 allocated APIDs are a pool — only a handful will be active simultaneously, and the rest exist so future instruments can join without reallocation.

## cFE Message ID (MID) Scheme

cFE MIDs are the cFS-internal 16-bit identifiers that name Software Bus messages. SAKURA-II uses the cFE v1 MID-to-APID mapping:

```
MID = 0x1800 | APID        for commands (TC path)
MID = 0x0800 | APID        for telemetry (TM path)
```

This keeps the MID derivable from the APID, so the registry here is the single source of truth — MIDs do not need their own table.

When cFS v7+ and the new MID-encoding scheme land, this relationship will change; that change is deferred to the cFS version-bump ticket and will be reflected here before merge.

## Orbiter TM Sub-Allocation (0x100–0x17F)

Reserved per orbiter application. As `apps/orbiter_*` modules come online, add rows here **before** defining the MID macro in source.

| APID | App | Purpose | Packet catalog entry |
|---|---|---|---|
| `0x100` | `sample_app` | HK telemetry (existing template app) | TBR — see [`packet-catalog.md`](packet-catalog.md) (planned) |
| `0x101`–`0x10F` | `orbiter_cdh` | Command & Data Handling HK, event logs | TBR |
| `0x110`–`0x11F` | `orbiter_adcs` | ADCS state, attitude, wheel telemetry | TBR |
| `0x120`–`0x12F` | `orbiter_comm` | Comm stack status, link budget | TBR |
| `0x130`–`0x13F` | `orbiter_power` | EPS telemetry (via MCU gateway) | TBR |
| `0x140`–`0x15F` | `orbiter_payload` | Science payload TM | TBR |
| `0x160`–`0x17F` | *reserved* | Unallocated; for future orbiter apps | — |

## Orbiter TC Sub-Allocation (0x180–0x1FF)

One TC APID per commandable app. Commands within an app are distinguished by the CCSDS secondary-header command code, not by burning additional APIDs.

| APID | App | Command scope |
|---|---|---|
| `0x180` | `sample_app` | Reference command endpoint |
| `0x181` | `orbiter_cdh` | Mode transitions, EVS filter commands |
| `0x182` | `orbiter_adcs` | Attitude mode, target quaternion |
| `0x183` | `orbiter_comm` | Link configuration, downlink rate |
| `0x184` | `orbiter_power` | Load switches (safety-interlocked) |
| `0x185` | `orbiter_payload` | Payload on/off, science mode |
| `0x186`–`0x1FF` | *reserved* | — |

## Subsystem-MCU Sub-Allocation (0x280–0x2FF)

Each FreeRTOS MCU class gets a 16-APID block. Per-instance multiplicity is handled via the secondary-header `instance_id`. Gateway app in cFS bridges between the SB and the physical bus (SpW/CAN/UART); see `ICD-mcu-cfs.md` (planned).

| APID range | MCU class | Physical bus (simulated) |
|---|---|---|
| `0x280`–`0x28F` | `mcu_payload` | SpaceWire |
| `0x290`–`0x29F` | `mcu_rwa` | CAN |
| `0x2A0`–`0x2AF` | `mcu_eps` | UART |
| `0x2B0`–`0x2FF` | *reserved* | — |

## AOS Virtual Channel (VC) Allocation

AOS Transfer Frames multiplex multiple data streams on a single link. SAKURA-II uses three VCs on the orbiter-to-ground downlink (CCSDS 732.0-B):

| VC ID | Stream | Rate class | APID blocks carried |
|---|---|---|---|
| `0` | Real-time HK | High-rate, best-effort | `0x100`–`0x13F` |
| `1` | Event log | Low-rate, reliable | EVS-only subset of HK |
| `2` | CFDP file transfer | Bulk, Class 1 today | File-transfer PDUs (APID per CFDP entity) |

The orbiter-to-relay cross-link and the relay-to-surface Proximity-1 link use their own VC plans, documented in `ICD-orbiter-relay.md` and `ICD-relay-surface.md` (planned).

## Change Control

- Any new APID or MID allocation requires a PR that (a) updates this file, (b) updates the corresponding packet-catalog entry, and (c) adds the MID macro in source — in that order.
- The APID/MID consistency linter (shipped as `apid-mid-lint` CI job) enforces that every MID macro under `apps/**` has a registry entry and that `SPACECRAFT_ID` here matches `_defs/mission_config.h`.
- CCSDS reserved ranges (`0x000`, `0x001`–`0x0FF`, `0x7FF`) are immutable; allocating into them is a deviation that must be recorded in [`../standards/deviations.md`](../standards/deviations.md).
