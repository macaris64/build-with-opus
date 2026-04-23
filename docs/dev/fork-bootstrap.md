# Fork & Bootstrap — Start a New Mission from This Boilerplate

> Entry point: [../README.md](../README.md). Quickstart: [quickstart.md](quickstart.md). MCP setup: [mcp-setup.md](mcp-setup.md).

This repo is a Claude Code boilerplate shaped for mission-critical space software. Fork it to start a new cFS + ROS 2 + Gazebo + Rust project. This doc is the runbook.

It assumes you already have the toolchains working — if not, do [quickstart.md](quickstart.md) against this fork first.

## When NOT to fork

- You want to contribute to SAKURA-II itself → don't fork; branch from this repo.
- You only need the Claude Code harness (skills/agents/rules), not the space-systems scaffolding → just copy `.claude/` into your existing repo.

## The checklist

Work top to bottom. Most steps are one grep + one commit.

### 1. Fork on GitHub and clone

```bash
gh repo fork <this-repo> --clone --remote
# or:
# git clone git@github.com:<you>/<new-mission>.git
cd <new-mission>
```

Rename the repo if you haven't — this doc uses `<new-mission>` as the placeholder.

### 2. Rename the mission codename

SAKURA-II appears in prose across the docs. Global rename to your codename:

```bash
git grep -l 'SAKURA-II\|SAKURA_II' | xargs sed -i 's/SAKURA-II/<CODENAME>/g; s/SAKURA_II/<CODENAME_SCREAMING>/g'
```

Review the diff carefully — some SAKURA-II references are inside quoted historical decisions (e.g. "the SAKURA-II team decided X"). Keep those as-is or update with context; they are narrative breadcrumbs, not configuration.

Specific files that carry the codename:

- [`../../README.md`](../../README.md) — top-level
- [`../../CLAUDE.md`](../../CLAUDE.md) — project instructions
- [`../../docs/README.md`](../README.md) — doc nav hub
- [`../../docs/REPO_MAP.md`](../REPO_MAP.md) — repo map
- [`../../docs/mission/conops/ConOps.md`](../mission/conops/ConOps.md) — mission scenarios
- [`../../_defs/targets.cmake`](../../_defs/targets.cmake) — `MISSION_NAME`

### 3. Reset mission config

Edit [`../../_defs/mission_config.h`](../../_defs/mission_config.h):

- Pick a new `SPACECRAFT_ID` (11-bit value `0x001`–`0x7FE`; `0x7FF` is the CCSDS idle APID and reserved).
- Keep `SAMPLE_MISSION_MAX_PIPES` and `SAMPLE_MISSION_TASK_STACK` unless you have a specific reason to change them.

Edit [`../../_defs/targets.cmake`](../../_defs/targets.cmake):

- `MISSION_NAME` → your codename.
- `SPACECRAFT_ID` → match `mission_config.h`.
- `MISSION_APPS` → delete entries for apps you will not use.

### 4. Re-seed the APID registry

Open [`../interfaces/apid-registry.md`](../interfaces/apid-registry.md) and rewrite the SCID / mission-name header so your SCID matches `_defs/mission_config.h`. The APID block allocations (orbiter `0x100`–`0x17F`, relay `0x200`–`0x23F`, etc.) can stay as-is if your mission architecture matches — a derivative mission with different segment count should revise the blocks.

### 5. Customize `.mcp.json`

Edit [`../../.mcp.json`](../../.mcp.json) for your org:

- `github` MCP server runs against the signed-in user's access — no per-repo tweak needed.
- `sentry` MCP — if your team uses a separate Sentry org, the URL is the same; OAuth will scope to your membership.
- `postgres` MCP — update the default DSN if you have a team telemetry DB, or remove the server entirely if you will not use it. See [mcp-setup.md](mcp-setup.md).

### 6. Review `.claude/rules/`

| File | Keep as-is? |
|---|---|
| [`../../.claude/rules/general.md`](../../.claude/rules/general.md) | **Keep** — universal C / C++ / Rust rules |
| [`../../.claude/rules/security.md`](../../.claude/rules/security.md) | **Keep** — OWASP / MISRA / cargo-audit rules apply to any project |
| [`../../.claude/rules/cfs-apps.md`](../../.claude/rules/cfs-apps.md) | Keep if you use cFS; delete otherwise |
| [`../../.claude/rules/ros2-nodes.md`](../../.claude/rules/ros2-nodes.md) | Keep if you use ROS 2; delete otherwise |
| [`../../.claude/rules/testing.md`](../../.claude/rules/testing.md) | **Keep** — CMocka / colcon test / Rust test rules |

Deleting a rule file is safer than editing a rule you don't need — the rules are path-scoped, so the file simply becomes dead weight if you never touch those paths.

### 7. Trim `.claude/agents/` and `.claude/skills/`

The four agents and five skills shipped with this boilerplate are generic enough for any space project. Keep them unless you have a reason to remove:

- Agents: `@architect`, `@code-reviewer`, `@debugger`, `@researcher`.
- Skills: `/security-review`, `/code-review`, `/fix-issue`, `/create-pr`, `/update-deps`.

If your team has bespoke skills (e.g. a release runbook), add them under `.claude/skills/<name>/SKILL.md`.

### 8. Update `CLAUDE.md`

Edit [`../../CLAUDE.md`](../../CLAUDE.md) to reflect:

- New mission name and purpose (the intro paragraph).
- Build & Test Commands (remove any stack you deleted — e.g. drop the `colcon` block if you don't use ROS 2).
- Stack section — only the technologies you're keeping.
- Architecture — the directory list should match what's actually in your fork.
- Coding Standards — drop sections for languages you're not using.

Keep the MCP Servers and Claude Code Features tables accurate to your fork.

### 9. Archive the SAKURA-II decisions log

You are inheriting a design-decisions registry that isn't yours. Two options:

**Option A (recommended):** move [`../standards/decisions-log.md`](../standards/decisions-log.md) to `../standards/decisions-log-archive-sakura.md` and open a fresh empty registry at the same path. Cite the archive when you make decisions that deliberately follow SAKURA-II precedent.

**Option B:** keep the SAKURA-II decisions verbatim as a pre-populated starter set. Only sensible if your mission intentionally inherits the same protocol / timing / fault posture.

Same treatment for [`../standards/deviations.md`](../standards/deviations.md) (currently an empty stub — nothing to archive).

### 10. Phase gate: re-run `quickstart.md` end-to-end

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
ctest --test-dir build --output-on-failure
cargo build && cargo test && cargo clippy -- -D warnings
( cd ros2_ws && colcon build --symlink-install && colcon test )
```

If all four exit 0, the bootstrap is clean. Commit the mass-rename + config changes as a single commit with a message like `chore(bootstrap): fork from build-with-opus; rename to <CODENAME>`.

### 11. Reset docs progress tracker

Edit [`../README.md`](../README.md) Phase Status section:

- Phase A: mark docs as `⏳ planned` that you will be rewriting for your mission.
- Phase B: same — your segment architectures and ICDs will differ; don't carry SAKURA-II's completion checkmarks.
- Phase C: keep the structure as a template.

Don't delete the phase model itself — it's useful structure even if your mission content is different.

### 12. Optional: wire GitHub Actions CI

This repo does **not** ship `.github/workflows/`. A sensible first CI is:

- `ctest --test-dir build --output-on-failure`
- `cargo test --workspace`
- `cargo clippy --workspace -- -D warnings`
- `cargo audit`
- `colcon test`

Add as `.github/workflows/ci.yml` after your first "green on local" commit.

## What you get from this bootstrap

- Multi-stack build (C/cFS + ROS 2 + Gazebo + Rust) wired through a single `CMakeLists.txt` + `Cargo.toml`.
- A doc tree that enforces per-class, not per-instance documentation ([docs/README.md](../README.md) conventions).
- A Claude Code harness with format-on-save, secret protection, and the `notify-done` hook.
- A reviewer checklist and PR/review workflow ([`../../CLAUDE.md`](../../CLAUDE.md) PR & Review Process).
- A decisions-log format that scales from Phase A to a PDR-surrogate tier.

## What this doc is NOT

- Not a replacement for a real CM plan — [`mission/configuration/CM-Plan.md`](../mission/configuration/CM-Plan.md) (Phase C) is that.
- Not a license-compliance checklist. Check the LICENSE file of this repo and of any code you import.
- Not a substitute for your own `quickstart.md`. Once you've bootstrapped, rewrite [`quickstart.md`](quickstart.md) with your stack's specifics.
