# Glossary

Canonical acronyms and terms used across SAKURA-II documentation. When a term appears spelled-out-first in a doc, add it here — do not redefine it locally.

Grouped by discipline. Cross-discipline overloads (e.g. "node", "task") get explicit disambiguation.

## Mission & Systems Engineering

| Term | Expansion / Meaning |
|---|---|
| ConOps | Concept of Operations — how the system is used across mission phases |
| SRD | System Requirements Document |
| SDD | Software Design Document |
| ICD | Interface Control Document — contract at a box boundary |
| V&V | Verification and Validation |
| CM | Configuration Management |
| PDR | Preliminary Design Review |
| CDR | Critical Design Review |
| FMEA | Failure Modes and Effects Analysis |
| A/I/D/T | Verification methods: Analysis / Inspection / Demonstration / Test |
| TBD / TBR | To Be Determined / To Be Resolved — placeholder markers in draft docs |
| MET | Mission Elapsed Time |
| MOI | Mars Orbit Insertion |
| EDL | Entry, Descent, Landing |
| LOS | Loss of Signal (comms blackout, typically Mars occultation) |
| AOS | Acquisition of Signal (comms reacquired) — **disambiguation**: also the CCSDS protocol; context makes it clear |
| SAA | South Atlantic Anomaly (Earth-orbit radiation region; referenced for model pedigree only — Mars equivalents are documented in `architecture/09-failure-and-radiation.md`) |

## Spaceflight Software (cFS / cFE)

| Term | Expansion / Meaning |
|---|---|
| cFS | Core Flight System — NASA's open-source flight software framework |
| cFE | Core Flight Executive — cFS runtime and service APIs |
| OSAL | Operating System Abstraction Layer (cFS) |
| PSP | Platform Support Package (cFS) |
| SB | Software Bus — cFE publish/subscribe message bus |
| SCH | Scheduler application (cFS) |
| TO | Telemetry Output application (cFS) |
| CI | Command Ingest application (cFS) |
| ES | Executive Services (cFE) |
| TBL | Table Services (cFE) |
| EVS | Event Services (cFE) |
| TIME | Time Services (cFE) |
| APID | Application Process Identifier — 11-bit CCSDS routing tag, mirrored onto cFE message IDs |
| MID | Message Identifier — cFE internal message key; allocation mirrored in `interfaces/apid-registry.md` |
| SCID | Spacecraft Identifier — CCSDS secondary header field; see `_defs/mission_config.h` |
| MISRA C | MISRA C:2012 coding guidelines — normative style ruleset for `apps/` (see `.claude/rules/general.md`) |
| FSW | Flight Software |

## FreeRTOS & Embedded

| Term | Expansion / Meaning |
|---|---|
| FreeRTOS | Real-time kernel used for the smallsat relay and subsystem MCUs |
| RTOS | Real-Time Operating System |
| MCU | Microcontroller Unit |
| RWA | Reaction Wheel Assembly |
| EPS | Electrical Power Subsystem |
| SpW | SpaceWire (ECSS-E-ST-50-12C) |
| CAN | Controller Area Network |
| UART | Universal Asynchronous Receiver-Transmitter |
| SEU | Single Event Upset (radiation-induced bit flip) |
| SET | Single Event Transient |
| SEL | Single Event Latch-up |
| TID | Total Ionizing Dose |
| EDAC | Error Detection and Correction (memory protection) |
| SBC | Single Board Computer |

## Robotics (Space ROS 2)

| Term | Expansion / Meaning |
|---|---|
| ROS 2 | Robot Operating System 2 |
| Space ROS | NASA / OSRF-led ROS 2 distribution hardened for flight use |
| LifecycleNode | `rclcpp_lifecycle::LifecycleNode` — managed-lifetime ROS 2 node; the only node base class permitted in `ros2_ws/` per `.claude/rules/ros2-nodes.md` |
| DDS | Data Distribution Service — ROS 2's transport layer |
| QoS | Quality of Service — DDS reliability/durability/deadline policy bundle |
| URDF | Unified Robot Description Format |
| SDF | Simulation Description Format (Gazebo) |
| Node | **Disambiguation**: in ROS 2, a process-level compute unit; in CCSDS, an addressable network endpoint. Context decides; default assumption in architecture docs is the ROS 2 sense unless CCSDS is the subject. |

## Physics & Simulation

| Term | Expansion / Meaning |
|---|---|
| Gazebo Harmonic | Physics simulator (Open Robotics) used for all sensor-data generation |
| ModelPlugin | Gazebo plugin class simulating a vehicle/actuator/sensor |
| SITL | Software-In-The-Loop |
| HITL | Hardware-In-The-Loop |
| Cryobot | Subsurface ice-penetrating robot; SAKURA-II's "sea rover" class drills into Martian brine/ice deposits with a tethered optical comm link |
| UAV | Unmanned Aerial Vehicle |
| USV | Unmanned Surface Vessel — **not used in SAKURA-II**; surface-sea operation is not in scope (Mars has no surface seas). See `architecture/04-rovers-spaceros.md` for the cryobot motivation. |

## Communications (CCSDS Stack)

| Term | Expansion / Meaning |
|---|---|
| CCSDS | Consultative Committee for Space Data Systems |
| SPP | Space Packet Protocol (CCSDS 133.0-B) |
| TM | Telemetry (spacecraft-to-ground) |
| TC | Telecommand (ground-to-spacecraft) |
| AOS | Advanced Orbiting Systems Space Data Link Protocol (CCSDS 732.0-B) |
| TM SDLP | Telemetry Space Data Link Protocol (CCSDS 132.0-B) |
| TC SDLP | Telecommand Space Data Link Protocol (CCSDS 232.0-B) |
| Proximity-1 | CCSDS 211.0-B short-range surface / orbiter-relay-to-surface link |
| CFDP | CCSDS File Delivery Protocol (CCSDS 727.0-B). SAKURA-II ships **Class 1 (unacknowledged)** first; Class 2 (acknowledged) is architected-for but deferred. |
| PDU | Protocol Data Unit |
| VC | Virtual Channel (AOS/TM multiplexing slot) |
| SCLK | Spacecraft Clock |
| UTC | Coordinated Universal Time |
| TAI | International Atomic Time |

## Ground Segment

| Term | Expansion / Meaning |
|---|---|
| DSN | Deep Space Network (NASA). SAKURA-II simulates a DSN-class uplink/downlink in `rust/ground_station/`. |
| Light-time | One-way signal propagation delay; Earth–Mars varies from ~4 to ~24 minutes. Modeled in `architecture/08-timing-and-clocks.md`. |
| TM Ingest | Ground-segment pipeline stage that parses incoming TM frames into typed telemetry points |
| Uplink | Ground-to-spacecraft command flow |
| Downlink | Spacecraft-to-ground telemetry flow |

## Standards / Process

| Term | Expansion / Meaning |
|---|---|
| NPR | NASA Procedural Requirements (e.g. NPR 7150.2D for software engineering) |
| NASA-STD | NASA Technical Standard (e.g. NASA-STD-8739.8 for software assurance) |
| ECSS | European Cooperation for Space Standardization |
| DO-178C | Civil-aerospace software assurance standard (referenced for pedigree, not applied in full) |
| SBOM | Software Bill of Materials |
| SAKURA-II | Project codename — multi-layer Mars SITL mission simulator targeting NASA HPSC hardware |
| HPSC | High Performance Spaceflight Computing — NASA's next-gen radiation-hardened flight processor program |

## Requirement-ID Prefixes (normative)

Fixed at project start to avoid renumbering. See `mission/requirements/traceability.md`.

| Prefix | Scope | Source doc |
|---|---|---|
| `SYS-REQ-####` | System level | `mission/requirements/SRD.md` |
| `FSW-REQ-####` | Flight software (cFS orbiters + FreeRTOS smallsats + FreeRTOS MCUs; segment is a column, not a prefix) | `mission/requirements/FSW-SRD.md` |
| `ROV-REQ-####` | Surface assets (land / UAV / cryobot; form factor is a column) | `mission/requirements/ROVER-SRD.md` |
| `GND-REQ-####` | Ground segment | `mission/requirements/GND-SRD.md` |
