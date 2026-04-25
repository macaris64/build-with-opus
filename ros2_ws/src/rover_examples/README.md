# rover_examples

Demonstrative library showing how to use `rover_cryobot::HdlcLite`, `rover_common::TmBridge`,
and `rover_cryobot::TetherClient`. Every public function has 100% branch coverage enforced by
the gtest suite in `test/test_examples.cpp`.

## What Is This For?

New contributors can read these examples before touching `rover_cryobot` or `rover_common` directly.
The examples deliberately surface both the success path and the error path of each function so that
both are tested and documented.

## Library Functions

### `hdlc_example` (`include/rover_examples/hdlc_example.hpp`)

| Function | Success return | Error return | Trigger for error |
|---|---|---|---|
| `build_nominal_frame(payload)` | HDLC frame bytes | `{}` | payload > 240 B |
| `build_collapse_frame(payload)` | HDLC frame bytes | `{}` | payload > 80 B |
| `decode_frame(wire)` | Original payload | `{}` | CRC/framing error |
| `compute_crc(data)` | CRC-16 value | — | none |

### `tm_bridge_example` (`include/rover_examples/tm_bridge_example.hpp`)

| Function | Success return | Error return | Trigger for error |
|---|---|---|---|
| `pack_single_hk(apid, payload, ...)` | 16-B CCSDS packet | `{}` | payload > 65 526 B |
| `pack_hk_sequence(apid, payloads, ...)` | Vector of packets | fewer entries | individual entry oversize |

### `tether_pipeline_example` (`include/rover_examples/tether_pipeline_example.hpp`)

| Function | Success return | Error return | Trigger for error |
|---|---|---|---|
| `hk_to_wire(apid, hk, ...)` | HDLC NOMINAL wire frame | `{}` | SPP + 16 B header > 240 B |
| `hk_to_wire_collapse(apid, hk, ...)` | HDLC COLLAPSE wire frame | `{}` | SPP + 16 B header > 80 B |

## Demo Executable

`rover_examples_demo` runs through all examples and logs results via `RCLCPP_INFO`. It produces
no side effects and exits cleanly.

```bash
ros2 run rover_examples rover_examples_demo
```

## Build and Test

```bash
cd ros2_ws
colcon build --symlink-install --packages-select rover_examples
colcon test --packages-select rover_examples
colcon test-result --verbose
```

## Test Suite (`test/test_examples.cpp` — 16 tests)

| Test group | Tests | What is covered |
|---|---|---|
| `HdlcExample` | 7 | `compute_crc` KAT; both frame builders × {success, oversized}; `decode_frame` × {success, corrupt} |
| `TmBridgeExample` | 5 | `pack_single_hk` × {success, oversized}; `pack_hk_sequence` × {0 entries, 3 entries, oversized entry} |
| `TetherPipelineExample` | 4 | `hk_to_wire` × {success, failure}; `hk_to_wire_collapse` × {success, failure} |

All 16 tests must pass `colcon test` with `-Wall -Wextra -Werror`.
