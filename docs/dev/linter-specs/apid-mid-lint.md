# Linter Spec: `apid_mid_lint.py`

> **Status: spec-only — implementation deferred.** This document fixes the rule set and CLI contract; the Python script does not yet exist. Authoring precedent: [`../howto/authoring-a-repo-linter.md`](../howto/authoring-a-repo-linter.md). Reference exemplar: [`../../../scripts/traceability-lint.py`](../../../scripts/traceability-lint.py).

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Parent invariant: [`../../README.md` §Verification/CI item 4](../../README.md). Input sources of truth: [`../../interfaces/apid-registry.md`](../../interfaces/apid-registry.md), [`../../interfaces/packet-catalog.md`](../../interfaces/packet-catalog.md), [`../../../_defs/mission_config.h`](../../../_defs/mission_config.h). Related: [`citation-lint.md`](citation-lint.md), [`../ci-workflow.md`](../ci-workflow.md).

## 1. Purpose

Enforce the invariant declared in [`../../README.md` §Verification/CI item 4](../../README.md):

> Every MID macro under `apps/**` has an entry in [`apid-registry.md`](../../interfaces/apid-registry.md); `SPACECRAFT_ID` in [`_defs/mission_config.h`](../../../_defs/mission_config.h) matches the registry SCID.

Today this is enforced by reviewer vigilance. This linter mechanises the check so a PR that adds a MID macro without a registry row, or that drifts `SPACECRAFT_ID` away from the registry's canonical value, fails the PR gate.

## 2. Input scope

| Role | Path glob | What the linter reads |
|---|---|---|
| MID-macro source | `apps/**/*.h` | Every `#define <NAME>_MID <expr>` and every `#define <NAME>_CMD_MID / _TLM_MID` |
| SCID source | `_defs/mission_config.h` | The single `#define SPACECRAFT_ID <value>` line |
| Canonical registry | `docs/interfaces/apid-registry.md` | APID allocation tables + SCID paragraph |
| Packet cross-reference | `docs/interfaces/packet-catalog.md` | PKT-ID rows referenced by registry entries |

Files outside these globs are not read. The linter is silent about them.

## 3. Rules enforced

Every rule has a `gcc`-style diagnostic on violation, quoted verbatim from the source-of-truth doc where practical.

| Rule | Statement | Source anchor |
|---|---|---|
| **R1** | Every `<NAME>_MID` macro defined under `apps/**/*.h` has a row in [`apid-registry.md`](../../interfaces/apid-registry.md) whose derived MID (`0x0800 \| APID` for TM or `0x1800 \| APID` for TC) equals the macro value. | [`apid-registry.md §cFE Message ID (MID) Scheme`](../../interfaces/apid-registry.md) |
| **R2** | Every APID row in [`apid-registry.md`](../../interfaces/apid-registry.md) that is not marked `*reserved*` has a matching MID macro under `apps/**/*.h`. Reserved rows are exempt — the linter treats `*reserved*` as the explicit "no macro required" marker. | [`apid-registry.md §APID Allocation`](../../interfaces/apid-registry.md) |
| **R3** | The integer value of `SPACECRAFT_ID` in [`_defs/mission_config.h`](../../../_defs/mission_config.h) equals the SCID value stated in [`apid-registry.md`](../../interfaces/apid-registry.md) (`42U` at time of spec — the linter reads both, not a hardcoded constant). | [`apid-registry.md §Identifiers and Ranges`](../../interfaces/apid-registry.md) line 21 |
| **R4** | Every APID referenced in [`apid-registry.md`](../../interfaces/apid-registry.md) or derived from an `apps/**` MID macro falls within the usable range `0x100`–`0x7FE`. APIDs `0x000`, `0x001`–`0x0FF`, and `0x7FF` are reserved by CCSDS and must not appear. | [`apid-registry.md §APID Allocation`](../../interfaces/apid-registry.md) table of reserved ranges |
| **R5** | No APID is assigned to two different apps. Two MID macros with the same derived APID, or two registry rows with overlapping APID ranges, is a violation. | [`apid-registry.md §Change Control`](../../interfaces/apid-registry.md) |

All five rules run every invocation; the linter never short-circuits.

## 4. Failure modes

Sample `gcc`-style diagnostics (reviewer reads these on a red PR):

```
apid_mid_lint: 3 error(s):
  apps/orbiter_cdh/fsw/src/orbiter_cdh_mid.h:12: ORBITER_CDH_HK_TLM_MID = 0x0901 derives APID 0x101 — no row in docs/interfaces/apid-registry.md (R1)
  docs/interfaces/apid-registry.md: APID 0x110 (orbiter_adcs TM) has no matching _MID macro under apps/** (R2)
  _defs/mission_config.h:10 vs docs/interfaces/apid-registry.md:21: SPACECRAFT_ID mismatch — mission_config.h says 43U, registry says 42U (R3)
```

Diagnostics must be actionable on their own — the reviewer should know which file and line to edit without reading the linter source.

## 5. Sample CLI

```
python3 scripts/apid_mid_lint.py --repo-root .
```

Exit codes per [`../howto/authoring-a-repo-linter.md §4`](../howto/authoring-a-repo-linter.md): `0` pass, `1` violation, `2` I/O error.

On pass, stdout carries a one-line counter summary:

```
apid_mid_lint: OK — 24 MID macros, 19 registry rows, SCID=42 matches.
```

## 6. Regex sketch

Provided as starting points — the linter author may refine these. Registered here so the sibling `scripts/tests/fixtures/apid_mid/` harness can reuse them.

| Purpose | Pattern |
|---|---|
| MID-macro line | `^\s*#define\s+([A-Z_][A-Z0-9_]*)_MID\s+\(?(0x[0-9A-Fa-f]+|[0-9]+)U?\)?` |
| SPACECRAFT_ID line | `^\s*#define\s+SPACECRAFT_ID\s+\(?(0x[0-9A-Fa-f]+|[0-9]+)U?\)?` |
| Registry APID row | `^\|\s*\` + `` ` `` + `(0x[0-9A-Fa-f]{3})(?:-` + `` ` `` + ``0x[0-9A-Fa-f]{3}` )?\s*\|`` |
| Registry SCID paragraph | `SPACECRAFT_ID\s*=\s*([0-9]+)U?` |

## 7. Performance

Under 3 s budget per [`../howto/authoring-a-repo-linter.md §8`](../howto/authoring-a-repo-linter.md). Scope is small: `apps/**/*.h` is ≤ 50 files; `_defs/mission_config.h` is one file; the two registry/catalog docs are < 300 lines each. No memoisation needed.

## 8. CI integration

Per [`../ci-workflow.md §2`](../ci-workflow.md) (slot reserved), the job shape is:

```yaml
- name: apid-mid-lint
  run: python3 scripts/apid_mid_lint.py --repo-root .
```

Trigger: every PR. Path-filter is not applied — a PR that only edits `apid-registry.md` still benefits from the cross-check, and the run cost is negligible.

## 9. Test harness

Fixtures live under `scripts/tests/fixtures/apid_mid/<rule-id>/` per [`../howto/authoring-a-repo-linter.md §7`](../howto/authoring-a-repo-linter.md). One fixture per rule plus a clean fixture:

| Fixture | Trigger |
|---|---|
| `r1-missing-registry-row/` | MID macro with no registry row — expected `exit 1` |
| `r2-orphan-registry-row/` | Non-reserved registry row with no MID — expected `exit 1` |
| `r3-scid-mismatch/` | `_defs/mission_config.h` and `apid-registry.md` disagree — expected `exit 1` |
| `r4-reserved-apid/` | APID `0x001` or `0x7FF` in a macro or registry row — expected `exit 1` |
| `r5-duplicate-apid/` | Two macros derive the same APID — expected `exit 1` |
| `clean/` | Miniature but valid registry + single-app `apps/` tree — expected `exit 0` |

## 10. Binding to requirements

- Supports [`SYS-REQ-0080`](../../mission/requirements/SRD.md) (traceability discipline) by extending the same pattern to APID/MID space.
- Reinforces [`apid-registry.md §Change Control`](../../interfaces/apid-registry.md) — the linter is the mechanisation of that section's second bullet.
- Referenced from [`../../README.md` §Verification/CI item 4](../../README.md) as the planned implementation.

## 11. Out of scope

- TC secondary-header command-code validation — that lives in a future `scripts/command-code-lint.py` or inside integration tests.
- Cross-mission reuse (e.g. when a forked mission reallocates the registry) — the linter reads both files each invocation, so a fork's registry is self-consistent after the fork owners update it once.
- AOS virtual-channel consistency (registry §AOS VC Allocation) — deferred; covered by comms-stack integration tests, not by static checks.
