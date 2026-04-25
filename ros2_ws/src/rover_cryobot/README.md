# rover_cryobot

Drill control node with HDLC-lite tether framing for the subsurface cryobot (Q-C9).

**APID**: `0x400` (TM block `0x400–0x43F` per `apid-registry.md`)
**Package version**: 0.1.0

## Components

### `DrillCtrlNode`

Lifecycle node that receives drill commands and emits HK telemetry over the tether.

**Lifecycle base**: `rover_common::LifecycleBase` → `rclcpp_lifecycle::LifecycleNode`

| Direction | Topic | Message type | QoS constant |
|---|---|---|---|
| Subscribe | `/drill_cmd` | `std_msgs/UInt8MultiArray` | `rover_common::CMD_QOS` |

A 1 Hz HK timer calls `TetherClient::pack_and_frame` and logs the resulting wire-frame size via
`RCLCPP_DEBUG`. No physical socket is opened in Phase 37; the TCP transport is wired in Phase 40.

### `TetherClient`

Combines `TmBridge` (CCSDS SPP encoding) and `HdlcLite` (tether framing) into a single pipeline:

```
HK payload
    → TmBridge(0x400U)          CCSDS TM Space Packet (16 B header + payload)
    → HdlcLite::encode(mode)    HDLC-lite wire frame with CRC-16 and byte stuffing
    → wire bytes                returned to caller for transport
```

`pack_and_frame` returns an empty vector if the SPP cannot be built (payload > 65 526 B) or the
HDLC frame would exceed the mode payload limit.

### `HdlcLite` (pure-C++ library, no ROS dependencies)

Implements the HDLC-lite framing protocol per `ICD-cryobot-tether.md §3` (Q-C9).

#### Wire frame layout

```
0x7E | mode(1B) | length(2B BE) | byte-stuffed(payload + CRC-16/CCITT-FALSE(2B)) | 0x7E
```

CRC is computed over un-stuffed `mode + length_hi + length_lo + payload` bytes, then appended
before stuffing.  CRC bytes are emitted MSB-first (big-endian, Q-C8).

#### Constants

| Constant | Value | Description |
|---|---|---|
| `FLAG` | `0x7E` | Frame delimiter |
| `ESC` | `0x7D` | Escape prefix |
| `MODE_NOMINAL` | `0x00` | Standard full-bandwidth mode (≤240 B payload) |
| `MODE_COLLAPSE` | `0x01` | Bandwidth-collapse mode (≤80 B payload) |
| `MODE_SYNC` | `0x02` | Clock synchronization frame |
| `MODE_KEEPALIVE` | `0x03` | Keepalive heartbeat |
| `MAX_PAYLOAD_NOMINAL` | `240` | Max payload bytes in NOMINAL mode |
| `MAX_PAYLOAD_COLLAPSE` | `80` | Max payload bytes in COLLAPSE mode |

#### Byte stuffing

Applied to the entire body (mode + length + payload + CRC) after CRC is computed:

| Raw byte | Stuffed sequence |
|---|---|
| `0x7E` | `{0x7D, 0x5E}` |
| `0x7D` | `{0x7D, 0x5D}` |

#### CRC-16/CCITT-FALSE

Polynomial `0x1021`, init `0xFFFF`, no input/output reflection, no final XOR.
**Test vector**: `CRC("123456789") == 0x29B1`

#### API

```cpp
#include "rover_cryobot/hdlc_lite.hpp"

// Encode: throws std::invalid_argument if payload exceeds the mode limit.
std::vector<uint8_t> wire = rover_cryobot::HdlcLite::encode(
    rover_cryobot::HdlcLite::MODE_NOMINAL, payload);

// Decode: returns std::nullopt on CRC error, bad framing, or length mismatch.
auto decoded = rover_cryobot::HdlcLite::decode(wire);
if (decoded.has_value()) { /* use *decoded */ }

// CRC only (useful for offline verification):
uint16_t crc = rover_cryobot::HdlcLite::crc16_ccitt_false(data);
```

## Build and Test

```bash
cd ros2_ws
colcon build --symlink-install --packages-select rover_cryobot
colcon test --packages-select rover_cryobot
colcon test-result --verbose
```

## Test Coverage (`test/test_rover_cryobot.cpp` — 14 cases)

| Test | Description |
|---|---|
| `CrcTestVector` | `CRC({'1'..'9'}) == 0x29B1` (mandatory KAT) |
| `ByteStuffing` | Payload `{0x01,0x7E,0x7D,0x02}` → no bare `0x7E` in body |
| `Roundtrip` | `decode(encode({0x11,0x7E,0x7D,0x5E,0xFF}))` == original payload |
| `RoundtripEmpty` | Empty payload survives encode→decode |
| `RoundtripMaxNominal` | 240 B payload survives encode→decode |
| `SizeGuardNominal` | 241 B payload + NOMINAL → `std::invalid_argument` |
| `SizeGuardCollapse` | 81 B payload + COLLAPSE → `std::invalid_argument` |
| `CorruptCrc` | Flipped CRC byte → `std::nullopt` |
| `TooShortWire` | Truncated frame → `std::nullopt` |
| `DrillCtrlLifecycle` | `configure → activate → deactivate → cleanup` → all `SUCCESS` |
| `DrillCtrlApid` | `TmBridge(0x400U).pack_hk({},0,0)` → bits [10:0] == `0x400` |
| `TetherClientFlags` | `pack_and_frame({},0,0)` → starts and ends with `0x7E` |

## Dependencies

| Package | Role |
|---|---|
| `rover_common` | `LifecycleBase`, `TmBridge`, `CMD_QOS` |
| `rclcpp_lifecycle` | Lifecycle node framework |
| `std_msgs` | `UInt8MultiArray` (drill command) |
