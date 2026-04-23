# SAKURA-II Documentation

> Project codename: **SAKURA-II** — multi-layer software-in-the-loop Mars mission simulator, on a design path toward NASA HPSC flight hardware.
>
> Start-of-context: [../CLAUDE.md](../CLAUDE.md) • [../README.md](../README.md) • [GLOSSARY.md](GLOSSARY.md) • [REPO_MAP.md](REPO_MAP.md)

This is the documentation root for SAKURA-II. The tree is organized **by purpose**, not by audience — you pick your entry point below based on what you came here to do.

## Pick your entry point

| If you are a... | Start here |
|---|---|
| First-time reader, curious what SAKURA-II *is* | [mission/conops/ConOps.md](mission/conops/ConOps.md) → [architecture/00-system-of-systems.md](architecture/00-system-of-systems.md) |
| Developer: just want to build it | [dev/quickstart.md](dev/quickstart.md) |
| Developer: build commands / iteration flow | [dev/build-runbook.md](dev/build-runbook.md) |
| Running the sim end-to-end | [dev/simulation-walkthrough.md](dev/simulation-walkthrough.md) |
| Something broke | [dev/troubleshooting.md](dev/troubleshooting.md) |
| Adding a new cFS app / ROS 2 node / plugin / crate | [dev/howto/](dev/howto/) |
| Architect: how does subsystem X fit? | [architecture/00-system-of-systems.md](architecture/00-system-of-systems.md), then the `architecture/0X-*.md` that covers X |
| Someone writing an interface between two boxes | [interfaces/apid-registry.md](interfaces/apid-registry.md), then the relevant `interfaces/ICD-*.md` |
| Forking this repo for a new mission | [dev/fork-bootstrap.md](dev/fork-bootstrap.md) |
| Setting up MCP servers (GitHub / Sentry / Postgres) | [dev/mcp-setup.md](dev/mcp-setup.md) |
| Formal reviewer (mission assurance, SE, QA) | [mission/conops/ConOps.md](mission/conops/ConOps.md) → [mission/requirements/SRD.md](mission/requirements/SRD.md) → [mission/requirements/traceability.md](mission/requirements/traceability.md) → [mission/verification/V&V-Plan.md](mission/verification/V&V-Plan.md) → [mission/verification/compliance-matrix.md](mission/verification/compliance-matrix.md) → [mission/configuration/CM-Plan.md](mission/configuration/CM-Plan.md) |
| Looking up a term or acronym | [GLOSSARY.md](GLOSSARY.md) |
| Hunting down a file in the repo | [REPO_MAP.md](REPO_MAP.md) |
| Wanting to cite a standard | [standards/references.md](standards/references.md) |

## Tree

| Area | Purpose | Key docs |
|---|---|---|
| [`mission/`](mission/) | **Program-surrogate / formal** — ConOps, requirements, V&V, CM. What the system promises and how we prove it. | [ConOps](mission/conops/ConOps.md) ✅ • [mission-phases](mission/conops/mission-phases.md) ✅ • [V&V Plan](mission/verification/V&V-Plan.md) ✅ • [Compliance Matrix](mission/verification/compliance-matrix.md) ✅ • SRDs ✅ • [Traceability](mission/requirements/traceability.md) ✅ • [CM Plan](mission/configuration/CM-Plan.md) ✅ • [Release Process](mission/configuration/release-process.md) ✅ • [Phase D Roadmap](mission/phase-D-roadmap.md) (roadmap + implementation plan agreed; see §8) |
| [`architecture/`](architecture/) | **Technical** — how the system is structured. One doc per segment or cross-cutting concern. | [System of Systems](architecture/00-system-of-systems.md) • segment docs `01`–`06` ✅ • cross-cutting `07`, `08`, `09`, `10` ✅ • [`diagrams/l2-containers.puml`](architecture/diagrams/l2-containers.puml) ✅ |
| [`interfaces/`](interfaces/) | **ICDs** — contracts at each box boundary, plus the canonical APID/MID registry. | [APID Registry](interfaces/apid-registry.md) • seven [ICDs](interfaces/) ✅ • [packet-catalog.md](interfaces/packet-catalog.md) ✅ |
| [`dev/`](dev/) | **Developer** — practical bring-up, runbooks, how-tos, spec docs for planned tooling. | [Quickstart](dev/quickstart.md) ✅ • [Build runbook](dev/build-runbook.md) ✅ • [Docker runbook](dev/docker-runbook.md) ✅ • [Simulation walkthrough](dev/simulation-walkthrough.md) ✅ • [Troubleshooting](dev/troubleshooting.md) ✅ • [MCP setup](dev/mcp-setup.md) ✅ • [Fork bootstrap](dev/fork-bootstrap.md) ✅ • [How-tos](dev/howto/) ✅ • [CI workflow spec](dev/ci-workflow.md) (spec) • [Linter specs](dev/linter-specs/) (spec) |
| [`standards/`](standards/) | **Pedigree** — the external bibliography and any knowing deviations. | [references.md](standards/references.md) ✅ • [deviations.md](standards/deviations.md) (stub) • [decisions-log.md](standards/decisions-log.md) ✅ |
| [`GLOSSARY.md`](GLOSSARY.md) | Canonical terminology. | — |
| [`REPO_MAP.md`](REPO_MAP.md) | Directory-to-segment mapping. | — |

## Phase Status

SAKURA-II documentation shipped in three phases. Phase B/C production was scaffolded by session-local planning files under `~/.claude/plans/` (`in-this-project-i-hazy-kahan.md` for the full Phase B plan, `investigate-the-docs-investigate-memoized-wozniak.md` for Batch B3 + B6, `our-latest-development-summary-peppy-starlight.md` for Batch B4 + B5). Those files are Claude Code scratchpads, not version-controlled artefacts — they are intentionally outside the repo. Phase C landed after the approved plan scope was extended.

### Next Steps (as of this writing)

Phase A, B (all batches), and C are **complete**. 114 requirements across 4 SRDs; [`scripts/traceability-lint.py`](../scripts/traceability-lint.py) confirms all present. [Q-F3](standards/decisions-log.md) is resolved.

**Design-spec complete, implementation deferred** (Phase-C-plus tightening):

- [ ] `.github/workflows/` CI — mechanise the PR-gate per [V&V-Plan §7.2](mission/verification/V&V-Plan.md) and [SYS-REQ-0070..0074](mission/requirements/SRD.md). Spec: [`dev/ci-workflow.md`](dev/ci-workflow.md).
- [ ] `scripts/apid_mid_lint.py` — APID/MID consistency across `apps/**` + [`interfaces/apid-registry.md`](interfaces/apid-registry.md) (authoring-time check §4 below). Spec: [`dev/linter-specs/apid-mid-lint.md`](dev/linter-specs/apid-mid-lint.md). Authoring template: [`dev/howto/authoring-a-repo-linter.md`](dev/howto/authoring-a-repo-linter.md).
- [ ] `scripts/citation_lint.py` — every standards ID in docs exists in [`standards/references.md`](standards/references.md) (§5 below). Spec: [`dev/linter-specs/citation-lint.md`](dev/linter-specs/citation-lint.md).

**Truly open (deferred to a future phase):**

- [ ] Phase-B-plus EDAC / SEU stimulus — reserved per [09 §6](architecture/09-failure-and-radiation.md).
- [ ] HPSC cross-build target — deferred per [Q-H8](standards/decisions-log.md).
- [ ] CFDP Class 2 — deferred per [Q-C3](standards/decisions-log.md); seam preserved in `CfdpProvider` trait.


### Phase A — Foundation (current)

Everything else hangs off these seven. All should be green before Phase B starts.

| Doc | Status |
|---|---|
| [README.md](README.md) | ✅ present (you are here) |
| [GLOSSARY.md](GLOSSARY.md) | ✅ present |
| [REPO_MAP.md](REPO_MAP.md) | ✅ present |
| [mission/conops/ConOps.md](mission/conops/ConOps.md) | ✅ present — MVC scope, 1 nominal + 1 off-nominal scenario |
| [architecture/00-system-of-systems.md](architecture/00-system-of-systems.md) | ✅ present — C4 L1 + L2 |
| [interfaces/apid-registry.md](interfaces/apid-registry.md) | ✅ present |
| [dev/quickstart.md](dev/quickstart.md) | ✅ present — walks local build of existing stacks (Docker/sim = [Phase B]) |

Phase A gate: a developer who did not write the quickstart can run it end-to-end on a fresh clone and reach a clean `ctest` + `cargo test` + `colcon test`.

### Phase B — Design Depth (in progress)

Batched production per the approved Phase B plan.

**Batch B1 — Cross-cutting foundations** ✅ complete

| Doc | Status |
|---|---|
| [standards/references.md](standards/references.md) | ✅ present — CCSDS / NPR / ECSS / NASA-STD bibliography |
| [standards/deviations.md](standards/deviations.md) | ✅ present — empty stub with format |
| [architecture/07-comms-stack.md](architecture/07-comms-stack.md) | ✅ present — 1024 B AOS, CFDP C1 w/ CRC-32, SPP-in-local-frame for tether |
| [architecture/08-timing-and-clocks.md](architecture/08-timing-and-clocks.md) | ✅ present — TAI internal, hybrid authority, 50 ms / 4 h LOS drift, 1 ms fleet precision |
| [architecture/10-scaling-and-config.md](architecture/10-scaling-and-config.md) | ✅ present — four config surfaces, compose profile matrix |

**Batch B2 — Interfaces** ✅ complete

| Doc | Status |
|---|---|
| [interfaces/ICD-orbiter-ground.md](interfaces/ICD-orbiter-ground.md) | ✅ present |
| [interfaces/ICD-orbiter-relay.md](interfaces/ICD-orbiter-relay.md) | ✅ present |
| [interfaces/ICD-relay-surface.md](interfaces/ICD-relay-surface.md) | ✅ present |
| [interfaces/ICD-cryobot-tether.md](interfaces/ICD-cryobot-tether.md) | ✅ present — definition site for [Q-C9](standards/decisions-log.md) (HDLC-lite framing) |
| [interfaces/ICD-mcu-cfs.md](interfaces/ICD-mcu-cfs.md) | ✅ present |
| [interfaces/ICD-sim-fsw.md](interfaces/ICD-sim-fsw.md) | ✅ present |
| [interfaces/packet-catalog.md](interfaces/packet-catalog.md) | ✅ present |

**Batch B3 — Segment architectures** ✅ complete

| Doc | Status |
|---|---|
| [architecture/01-orbiter-cfs.md](architecture/01-orbiter-cfs.md) | ✅ present — cFS service set, mission + gateway app inventory, topology |
| [architecture/02-smallsat-relay.md](architecture/02-smallsat-relay.md) | ✅ present — FreeRTOS end-to-end; definition site for [Q-C7](standards/decisions-log.md) (star topology) |
| [architecture/03-subsystem-mcus.md](architecture/03-subsystem-mcus.md) | ✅ present — FreeRTOS MCU firmware; definition site for [Q-H4](standards/decisions-log.md) (bus classes) |
| [architecture/04-rovers-spaceros.md](architecture/04-rovers-spaceros.md) | ✅ present — Space ROS 2 lifecycle-node compositions |
| [architecture/05-simulation-gazebo.md](architecture/05-simulation-gazebo.md) | ✅ present — Gazebo ModelPlugin suite + `fault_injector` |
| [architecture/06-ground-segment-rust.md](architecture/06-ground-segment-rust.md) | ✅ present — Rust crate boundaries; definition site for [Q-C3](standards/decisions-log.md) + [Q-C8](standards/decisions-log.md) |

**Batch B4 — Failure + V&V** ✅ complete

| Doc | Status |
|---|---|
| [architecture/09-failure-and-radiation.md](architecture/09-failure-and-radiation.md) | ✅ present — functional-fault injection + reserved-hooks for SEU/EDAC; definition site for [Q-F3](standards/decisions-log.md) |
| [mission/verification/V&V-Plan.md](mission/verification/V&V-Plan.md) | ✅ present — V&V classes, scenario-driven tests, fault-injection test matrix, coverage gates |

**Batch B5 — Developer experience** ✅ complete

| Doc | Status |
|---|---|
| [dev/build-runbook.md](dev/build-runbook.md) | ✅ present |
| [dev/docker-runbook.md](dev/docker-runbook.md) | ✅ present — design-spec only; Phase-B compose infra tracked as open |
| [dev/simulation-walkthrough.md](dev/simulation-walkthrough.md) | ✅ present — SCN-NOM-01 end-to-end + SCN-OFF-01 appendix |
| [dev/troubleshooting.md](dev/troubleshooting.md) | ✅ present |
| [dev/howto/adding-a-cfs-app.md](dev/howto/adding-a-cfs-app.md) | ✅ present |
| [dev/howto/launching-a-ros2-node.md](dev/howto/launching-a-ros2-node.md) | ✅ present |
| [dev/howto/writing-a-gazebo-plugin.md](dev/howto/writing-a-gazebo-plugin.md) | ✅ present |
| [dev/howto/adding-a-rust-crate.md](dev/howto/adding-a-rust-crate.md) | ✅ present |
| [dev/howto/writing-and-running-tests.md](dev/howto/writing-and-running-tests.md) | ✅ present |
| [dev/howto/enabling-a-safety-gate.md](dev/howto/enabling-a-safety-gate.md) | ✅ present |
| [mission/conops/mission-phases.md](mission/conops/mission-phases.md) | ✅ present — per-phase detail, state machines, V&V hooks |

**Batch B6 — Boilerplate usability** ✅ complete

Added outside the original Phase B plan — these close the "fork-to-bootstrap" promise in [../CLAUDE.md](../CLAUDE.md).

| Doc | Status |
|---|---|
| [../apps/README.md](../apps/README.md) | ✅ present — cFS / FreeRTOS FSW signpost |
| [../ros2_ws/README.md](../ros2_ws/README.md) | ✅ present — ROS 2 workspace signpost |
| [../simulation/README.md](../simulation/README.md) | ✅ present — Gazebo plugin signpost |
| [../rust/README.md](../rust/README.md) | ✅ present — Rust workspace signpost |
| [dev/mcp-setup.md](dev/mcp-setup.md) | ✅ present — GitHub / Sentry / Postgres MCP operational guide |
| [dev/fork-bootstrap.md](dev/fork-bootstrap.md) | ✅ present — runbook for starting a new mission from this boilerplate |

### Phase C — Formal / Program-Surrogate ✅ complete

PDR-surrogate posture achieved — 114 requirements defined across four SRDs, traceability linter enforces coverage, compliance matrix binds to external standards, CM + release policy authored.

| Doc | Status |
|---|---|
| [mission/requirements/SRD.md](mission/requirements/SRD.md) | ✅ present — 34 SYS-REQ-#### entries |
| [mission/requirements/FSW-SRD.md](mission/requirements/FSW-SRD.md) | ✅ present — 31 FSW-REQ-#### entries |
| [mission/requirements/GND-SRD.md](mission/requirements/GND-SRD.md) | ✅ present — 24 GND-REQ-#### entries |
| [mission/requirements/ROVER-SRD.md](mission/requirements/ROVER-SRD.md) | ✅ present — 25 ROV-REQ-#### entries |
| [mission/requirements/traceability.md](mission/requirements/traceability.md) | ✅ present — single index, 114 rows |
| [scripts/traceability-lint.py](../scripts/traceability-lint.py) | ✅ present — enforces SYS-REQ-0080 |
| [mission/verification/compliance-matrix.md](mission/verification/compliance-matrix.md) | ✅ present — standards → requirements → V&V evidence |
| [mission/configuration/CM-Plan.md](mission/configuration/CM-Plan.md) | ✅ present — baselines, versioning, change control |
| [mission/configuration/release-process.md](mission/configuration/release-process.md) | ✅ present — tag-cut runbook |
| [standards/deviations.md](standards/deviations.md) | ✅ populated — D-001 (R-S simulated), D-002 (MISRA 8.11 for `.critical_mem`) |

Open (Phase-C-plus, not gating; design specs authored — see Next Steps above):
- `.github/workflows/` CI mechanising the PR-gate per [V&V-Plan §7.2](mission/verification/V&V-Plan.md). Spec: [`dev/ci-workflow.md`](dev/ci-workflow.md).
- APID/MID consistency linter + citation-integrity linter. Specs: [`dev/linter-specs/apid-mid-lint.md`](dev/linter-specs/apid-mid-lint.md), [`dev/linter-specs/citation-lint.md`](dev/linter-specs/citation-lint.md).

### Phase D — Safety & Assurance (scoped; implementation plan agreed)

Next formal-tier step beyond PDR-surrogate: hazard analysis, safety case (GSN notation), concrete per-scenario test matrix, and three additional ICDs (ground-ops console, sim scenario authoring, release pipeline). Pillar content is not authored yet — the roadmap below signposts scope, dependencies on Phase C, entry triggers, and (now) the agreed two-stage implementation plan.

**Agreed scope (per [phase-D-roadmap.md §8](mission/phase-D-roadmap.md)):**

- **Stage 1 — Phase C-plus prerequisites.** Clears the three [§6 entry triggers](mission/phase-D-roadmap.md) by implementing `.github/workflows/ci.yml` per [ci-workflow spec](dev/ci-workflow.md), landing `scripts/apid_mid_lint.py` + `scripts/citation_lint.py` per the [linter specs](dev/linter-specs/), and wiring at least one end-to-end integration test against `simulation/scenarios/scn-nom-01.yaml`. Also clears the "Design-spec complete, implementation deferred" block in Next Steps above.
- **Stage 2 — Four pillars at exhaustive / production depth.** Hazard analysis with full UCA sweep across all controllers and scenarios (≥ 80 rows); GSN safety case with ≥ 34 sub-goals (one per SYS-REQ) and ≥ 100 solution nodes; test matrix with ≥ 150 concrete rows and 100% requirement coverage; three new ICDs (ground-ops console, sim scenario authoring, release pipeline). Bundles cross-cutting updates to this README, the roadmap status banner, `GLOSSARY.md`, `standards/references.md`, `verification/compliance-matrix.md`, `requirements/traceability.md`, and `REPO_MAP.md`.

| Doc | Status |
|---|---|
| [mission/phase-D-roadmap.md](mission/phase-D-roadmap.md) | ✅ present — roadmap + implementation plan (§8) authored; pillar deliverables still pending Stage 1 completion |
| `mission/safety/hazard-analysis.md` | 🔜 planned — Stage 2.A; STPA, exhaustive per [roadmap §2.1](mission/phase-D-roadmap.md) |
| `mission/safety/safety-case.md` + GSN diagrams | 🔜 planned — Stage 2.B; exhaustive per [roadmap §2.2](mission/phase-D-roadmap.md) |
| `mission/verification/test-matrix.md` | 🔜 planned — Stage 2.C; ≥150 rows, 100% requirement coverage per [roadmap §2.3](mission/phase-D-roadmap.md) |
| `interfaces/ICD-{ground-ops-console,sim-scenario-authoring,release-pipeline}.md` | 🔜 planned — Stage 2.D; three new ICDs per [roadmap §2.4](mission/phase-D-roadmap.md) |

## Authoritative Decisions (captured from planning)

These are the shaping decisions; each is load-bearing for multiple docs.

| Area | Decision |
|---|---|
| Fleet scale | Start minimal (1 orbiter + 1 relay + 1 land + 1 UAV + 1 cryobot); architect for 5× via config only |
| Surface-"sea" asset | **Subsurface cryobot** (tethered optical link through a surface rover) |
| FreeRTOS | Used for both smallsat-class primary FSW *and* subsystem MCU controllers on cFS buses |
| Formal-tier posture | Evolve toward PDR-surrogate; IDs and traceability kept clean from day 1 |
| Fault fidelity | Functional injection now; bit-flip / EDAC layer architected-for, deferred |
| Ground CFDP | Class 1 (unacknowledged) now; Class 2 deferred, crate boundary preserved |
| Time authority | Hybrid — ground primary UTC; orbiter autonomous SCLK with drift model during LOS |
| Docs tooling | Pure Markdown + inline Mermaid; PlantUML-C4 only for the L2 C4 diagram; no Sphinx/MkDocs yet |
| Requirement IDs | `SYS-REQ-####`, `FSW-REQ-####`, `ROV-REQ-####`, `GND-REQ-####`; segment/form-factor is a column, not a prefix |

## Contributing Conventions

- **Coding-style content goes in [../.claude/rules/](../.claude/rules/), not here.** Docs cite rules; they do not restate them.
- **Acronyms spelled out first go in [GLOSSARY.md](GLOSSARY.md).** Do not redefine a term locally.
- **Numeric constants (APIDs, MIDs, stack sizes) reference [`../_defs/`](../_defs/) or [interfaces/apid-registry.md](interfaces/apid-registry.md).** Never hardcode into prose — always link.
- **Requirement IDs are immutable once assigned.** Derived requirements use a `parent:` field, never ID encoding.
- **Per-class, not per-instance.** Scaling the fleet should not create new documents. If you feel the need to write "Orbiter-02's SDD," stop and write parameter tables instead.
- **Diagrams.** Mermaid inline is default; PlantUML-C4 for the handful of C4 diagrams only. Sources under `architecture/diagrams/`; renders committed next to sources.
- **TBD / TBR markers** are acceptable in drafts; they must be tracked (not silently forgotten) and resolved before a Phase-gate close.

## Reviewer Checklist (per tier)

| Tier | Reviewer role | Checklist |
|---|---|---|
| `mission/` | Mission-assurance surrogate | Every requirement has a verification method. Every `TBD`/`TBR` has an owner. Cited standards appear in `standards/references.md`. |
| `architecture/`, `interfaces/` | Architect | Every boundary in L2 has an ICD. Every MID in source has a registry entry. No per-instance docs. |
| `dev/` | Any contributor | Fresh clone can execute the runbook in the time stated. Broken link = build break. |

## Verification / CI

Progressive rollout; full set by end of Phase C:

1. **Link check** across `docs/**` — every intra-repo link must resolve.
2. **Mermaid / PlantUML parse** — every code-fenced `mermaid` block and every `.puml` source must parse.
3. **Requirement-ID linter** — every `XXX-REQ-####` in any SRD appears exactly once in [traceability.md](mission/requirements/traceability.md). Enforced by [`scripts/traceability-lint.py`](../scripts/traceability-lint.py) ✅.
4. **APID / MID consistency** — every MID macro under `apps/**` has an entry in [apid-registry.md](interfaces/apid-registry.md); `SPACECRAFT_ID` in [`../_defs/mission_config.h`](../_defs/mission_config.h) matches the registry SCID.
5. **Citation integrity** — every CCSDS / NPR / ECSS ID cited anywhere exists in [`standards/references.md`](standards/references.md).

Authoring-time checks (no CI required):

- Every doc header links to `GLOSSARY.md`.
- Every segment doc links to its source-of-truth `_defs/` file or source directory.
- Every `TBD`/`TBR` has a tracked follow-up.
