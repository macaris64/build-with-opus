# Surface Rover Sub-System Requirements Document (ROVER-SRD)

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Parent SRD: [`SRD.md`](SRD.md). Traceability: [`traceability.md`](traceability.md). V&V: [`../verification/V&V-Plan.md`](../verification/V&V-Plan.md). Architecture: [`../../architecture/04-rovers-spaceros.md`](../../architecture/04-rovers-spaceros.md). Cryobot tether link: [`../../interfaces/ICD-cryobot-tether.md`](../../interfaces/ICD-cryobot-tether.md). Surface-relay link: [`../../interfaces/ICD-relay-surface.md`](../../interfaces/ICD-relay-surface.md). Coding rules: [`../../../.claude/rules/ros2-nodes.md`](../../../.claude/rules/ros2-nodes.md), [`general.md`](../../../.claude/rules/general.md).

This sub-SRD covers the **three surface rover classes** — `rover_land` (wheeled), `rover_uav` (aerial), `rover_cryobot` (subsurface) — implemented as Space ROS 2 lifecycle-node compositions under [`ros2_ws/src/`](../../../ros2_ws/src/). Per [`SRD.md §1`](SRD.md), every requirement carries a `parent:` or declares "derived."

Per-class, not per-instance: every requirement below applies to the class; Scale-5 multiplicity is parameter-driven per [10 §5](../../architecture/10-scaling-and-config.md).

## 1. Record format

Same as [`FSW-SRD.md §1`](FSW-SRD.md).

## 2. Lifecycle Node Architecture

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| ROV-REQ-0001 | Every rover node shall subclass `rclcpp_lifecycle::LifecycleNode`; plain `rclcpp::Node` is banned. | SYS-REQ-0001 derived | [.claude/rules/ros2-nodes.md](../../../.claude/rules/ros2-nodes.md), [04 §3](../../architecture/04-rovers-spaceros.md) | I | ALL | grep lint |
| ROV-REQ-0002 | Every rover node shall implement all five lifecycle callbacks (`on_configure`, `on_activate`, `on_deactivate`, `on_cleanup`, `on_shutdown`) and shall support the `configure → activate → deactivate → cleanup` round trip. | derived | [.claude/rules/ros2-nodes.md](../../../.claude/rules/ros2-nodes.md), [04 §3](../../architecture/04-rovers-spaceros.md) | T | ALL | `colcon test` |
| ROV-REQ-0003 | Subscription, timer, and service callbacks shall return within 1 ms wall-time; non-trivial work shall be offloaded to a worker thread or a separate callback group. | derived | [.claude/rules/general.md](../../../.claude/rules/general.md) | T, I | ALL | launch_testing |
| ROV-REQ-0004 | QoS profiles shall be declared as file-scope named constants; inline `rclcpp::QoS(...)` at publisher/subscriber call sites is banned. | derived | [.claude/rules/ros2-nodes.md](../../../.claude/rules/ros2-nodes.md) | I | ALL | grep |
| ROV-REQ-0005 | Logging shall use `RCLCPP_INFO(get_logger(), ...)` / `RCLCPP_ERROR(...)`; `std::cout` and `printf` are banned. | SYS-REQ-0070 | [.claude/rules/general.md](../../../.claude/rules/general.md) | I | ALL | grep |

## 3. Per-Class Node Composition

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| ROV-REQ-0010 | Every rover class shall include a `tm_bridge` node whose sole responsibility is marshaling ROS 2 topic data onto CCSDS Space Packets and vice versa. | SYS-REQ-0020 | [04 §1](../../architecture/04-rovers-spaceros.md) | I, T | ALL | integration |
| ROV-REQ-0011 | `rover_land` shall include at minimum: `nav_node`, `teleop_node`, `tm_bridge`; cryobot-host instances shall additionally include `tether_bridge`. | derived | [04 §4.1](../../architecture/04-rovers-spaceros.md) | I | P3–P5 | inspection |
| ROV-REQ-0012 | `rover_uav` shall include at minimum: `flight_ctrl_node`, `state_est_node`, `tm_bridge`. | derived | [04 §4.2](../../architecture/04-rovers-spaceros.md) | I | P3, P4 | inspection |
| ROV-REQ-0013 | `rover_cryobot` shall include at minimum: `drill_ctrl_node`, `tether_client`, `tm_bridge`. | derived | [04 §4.3](../../architecture/04-rovers-spaceros.md) | I | P5 | inspection |

## 4. Communications to Relay and Cryobot

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| ROV-REQ-0020 | Surface rovers shall communicate with the relay via Proximity-1 (CCSDS 211.0-B-6) at 1024 B frame size, Reliable (Sequence-Controlled) mode for TM/TC. | SYS-REQ-0024 | [07 §4](../../architecture/07-comms-stack.md), [ICD-relay-surface.md](../../interfaces/ICD-relay-surface.md) | T | P3–P5 | integration |
| ROV-REQ-0021 | Rover HK shall be emitted at 1 Hz by default; drill/flight/IMU telemetry rates per [packet-catalog §8](../../interfaces/packet-catalog.md). | derived | [packet-catalog §8](../../interfaces/packet-catalog.md) | T | P4 | SCN-NOM-01 |
| ROV-REQ-0022 | The cryobot shall communicate with its host rover via the project-local tether link (HDLC-lite framing with byte stuffing per Q-C9); nominal 240 B max payload, 80 B under BW-collapse. | SYS-REQ-0020 | [Q-C9](../../standards/decisions-log.md), [07 §6](../../architecture/07-comms-stack.md), [ICD-cryobot-tether.md](../../interfaces/ICD-cryobot-tether.md) | T | P5 | integration |
| ROV-REQ-0023 | Cryobot tether-link frames shall carry a CRC-16/CCITT-FALSE trailer; the cryobot shall transition to BW-collapse mode on link-quality threshold crossing per [ICD-cryobot-tether.md §5](../../interfaces/ICD-cryobot-tether.md). | derived | [Q-C9](../../standards/decisions-log.md) | T | P5 | SCN-OFF-01 |

## 5. Time Handling

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| ROV-REQ-0030 | Rover nodes shall set `use_sim_time: true` under SITL; time shall be sourced from the simulation `/clock` topic. | SYS-REQ-0030 | [08 §5.4](../../architecture/08-timing-and-clocks.md) | I, T | ALL | launch_testing |
| ROV-REQ-0031 | Non-ROS time shall enter ROS time only through a dedicated `time_bridge` LifecycleNode; `std::chrono::system_clock` shall not appear in rover node code. | derived | [08 §5.4](../../architecture/08-timing-and-clocks.md) | I | ALL | grep |
| ROV-REQ-0032 | Rover `time_bridge` shall consume CCSDS time tags from the relay link and republish them as ROS `rclcpp::Clock` with `RCL_ROS_TIME` semantics. | SYS-REQ-0031 | [08 §5.4](../../architecture/08-timing-and-clocks.md) | T | ALL | integration |

## 6. Safe-Mode

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| ROV-REQ-0040 | Rover safe-mode entry shall transition all lifecycle nodes to `inactive`; mobility and actuator commands received in the safing window shall be rejected with a safing-period error code. | SYS-REQ-0050 | [ConOps §7](../conops/ConOps.md), [mission-phases §3.6](../conops/mission-phases.md) | T | P6 | integration |
| ROV-REQ-0041 | Cryobot drill controller shall transition to `HOLD` on tether DEGRADED latch; `HOLD` shall be fate-shared with an independent watchdog, not solely with a voluntary HK subscription. | SYS-REQ-0050 | [ConOps §5](../conops/ConOps.md) | T | P5 | [V&V §3.2 TC-SCN-OFF-01-D](../verification/V&V-Plan.md) |
| ROV-REQ-0042 | No cryobot actuator command shall be lost-then-executed-late across a safing transition; commands in flight when the fault began shall be rejected, not silently queued. | SYS-REQ-0052 | [ConOps §5](../conops/ConOps.md) | T | P5 | [V&V §3.2](../verification/V&V-Plan.md) |

## 7. Sensor Model

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| ROV-REQ-0050 | Rover sensor values shall be consumed as normal ROS 2 topic data published by Gazebo ModelPlugins; the sim-fsw sideband shall not be consumed by nav/control nodes, only by `tm_bridge`. | SYS-REQ-0041 | [04 §1](../../architecture/04-rovers-spaceros.md) | I | ALL | inspection |
| ROV-REQ-0051 | Sensor-noise injection (APID `0x543`) shall be applied at the Gazebo plugin's sensor-primitive output before the ROS 2 topic publish; nav nodes shall not branch on sim-vs-flight at read time. | SYS-REQ-0041 | [ICD-sim-fsw §3.4](../../interfaces/ICD-sim-fsw.md), [05 §3](../../architecture/05-simulation-gazebo.md) | T | ALL | integration |

## 8. Package Naming

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| ROV-REQ-0060 | ROS 2 packages shall follow `rover_<class>` naming for per-class packages (e.g. `rover_land`, `rover_uav`, `rover_cryobot`); `rover_bringup` holds composite launch. | SYS-REQ-0002 | [04 §2](../../architecture/04-rovers-spaceros.md), [REPO_MAP.md](../../REPO_MAP.md) | I | ALL | inspection |
| ROV-REQ-0061 | Per-instance multiplication shall be achieved via launch-file parameters and ROS namespaces; new rover instances shall not require new ROS packages. | SYS-REQ-0002 | [10 §5](../../architecture/10-scaling-and-config.md) | I, T | ALL | launch_testing |

## 9. Test Coverage

| ID | Statement | Parent | Rationale | Verification | Phase | V&V |
|---|---|---|---|---|---|---|
| ROV-REQ-0070 | Every ROS 2 package under `ros2_ws/src/` shall include a `test/` directory with at least one unit test exercising its lifecycle round trip plus one failure-path test. | SYS-REQ-0074 | [.claude/rules/testing.md](../../../.claude/rules/testing.md) | I, T | ALL | `colcon test` |
| ROV-REQ-0071 | Multi-node integration tests shall use `launch_testing` + pytest. | derived | [V&V-Plan §2.2](../verification/V&V-Plan.md) | I, T | ALL | `colcon test` |

## 10. Open / Deferred

- Relay selection policy when Scale-5 has multiple orbiters above a surface asset ([ConOps §8](../conops/ConOps.md)).
- Cryobot thermal model (OCXO vs TCXO) — tracked in [08 §4](../../architecture/08-timing-and-clocks.md).
- Hardware-in-the-loop (HIL) rover variant — out of scope per [05 §11](../../architecture/05-simulation-gazebo.md).
- Cryobot `SCN-OFF-01-RESUME` TC (Phase C scenario expansion).

## 11. What this sub-SRD is NOT

- Not a coding rulebook. Rules live in [`.claude/rules/`](../../../.claude/rules/).
- Not a segment architecture doc — [`../../architecture/04-rovers-spaceros.md`](../../architecture/04-rovers-spaceros.md) owns rationale.
- Not an ICD — boundary contracts live in [`../../interfaces/`](../../interfaces/).
- Not a rover-operations doc — operator procedures per phase are in [`../conops/mission-phases.md`](../conops/mission-phases.md).
