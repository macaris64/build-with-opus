# Linter Spec: `citation_lint.py`

> **Status: spec-only — implementation deferred.** This document fixes the rule set and CLI contract; the Python script does not yet exist. Authoring precedent: [`../howto/authoring-a-repo-linter.md`](../howto/authoring-a-repo-linter.md). Reference exemplar: [`../../../scripts/traceability-lint.py`](../../../scripts/traceability-lint.py).

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Parent invariant: [`../../README.md` §Verification/CI item 5](../../README.md) and [`../../standards/references.md` preamble](../../standards/references.md). Related: [`apid-mid-lint.md`](apid-mid-lint.md), [`../ci-workflow.md`](../ci-workflow.md).

## 1. Purpose

Enforce the invariant declared in [`../../README.md` §Verification/CI item 5](../../README.md) and restated in [`../../standards/references.md`](../../standards/references.md) opening paragraph:

> Every standard / specification cited anywhere in `docs/` must appear here, and no doc may cite a standard that is absent from this file.

Today this is enforced by reviewer vigilance and a note in [`V&V-Plan.md §1.3`](../../mission/verification/V&V-Plan.md) closing paragraph. This linter mechanises the check so a doc PR that cites `CCSDS 999.0-B-1` (typo) or introduces a new ID without adding a references.md row fails the PR gate.

## 2. Input scope

| Role | Path glob | What the linter reads |
|---|---|---|
| Citing docs | `docs/**/*.md` **except** `docs/standards/references.md` itself | Every standards ID matched by the regex catalogue in §4 |
| Canonical bibliography | `docs/standards/references.md` | Every row in sections §1–§5, keyed by the first column (`ID`) |

Code under `apps/`, `rust/`, `ros2_ws/`, `simulation/`, `_defs/` is out of scope. Standards are cited in prose, not code; if a comment in code cites a standard, the enclosing doc for that module must carry the citation.

## 3. Rules enforced

| Rule | Statement |
|---|---|
| **R1** | Every CCSDS ID matching the pattern in §4 that appears in a doc under `docs/**/*.md` (excluding `references.md` itself) must have an exact-match row in `references.md` §1. |
| **R2** | Every NPR ID matching the pattern must have a row in `references.md` §2. |
| **R3** | Every ECSS ID matching the pattern must have a row in `references.md` §3. |
| **R4** | Every NASA-STD ID matching the pattern must have a row in `references.md` §2 (under "NASA-STD — NASA Technical Standards"). |
| **R5** | Every IEEE ID matching the pattern must have a row in `references.md` §2 or §3 (no fixed section — IEEE citations are rare; the linter permits either). |
| **R6** | Every row in `references.md` that is currently "locked" (per `references.md §Changelog Policy` — locked once cited) must have at least one citation somewhere under `docs/**/*.md`. Orphan rows that were never-cited are reported as warnings, not errors (exit 0), to permit adding rows in advance of the citing doc. |

Rules R1–R5 fail with `exit 1`; R6 emits to stderr but does not affect the exit code (warning-only).

## 4. Regex catalogue

Authoritative patterns. The linter author should copy these verbatim; changes require a PR that updates both this spec and the script.

```python
# Version suffixes and amendments are optional; the linter strips them before
# comparing to references.md rows that may or may not carry the suffix.
CCSDS_RE    = re.compile(r"\bCCSDS\s+(\d{3}\.\d(?:-[A-Z])?(?:-\d)?)\b")
NPR_RE      = re.compile(r"\bNPR\s+(\d{4}\.\d[A-Z]?)\b")
ECSS_RE     = re.compile(r"\bECSS-[EQMS]-ST-\d{2}(?:-\d{2})?[A-Z](?:\s+Rev\.\d)?\b")
NASASTD_RE  = re.compile(r"\bNASA-STD-\d{4}(?:\.\d[A-Z]?)?\b")
IEEE_RE     = re.compile(r"\bIEEE\s+\d{3,4}\b")
```

**Whitespace handling:** the linter collapses runs of whitespace inside a match (so `CCSDS  133.0-B-2` with two spaces matches the references.md entry). Version suffix (`-B-2`, `-B-4`) is preserved exactly — a citation of `CCSDS 132.0-B-2` and a references row for `CCSDS 132.0-B-3` is a violation (R1), not a match. Version bumps require the atomic update called out in `references.md §Changelog Policy`.

**Exclusion:** matches inside fenced code blocks and inline code spans are ignored. Citing `CCSDS 133.0-B-2` inside an explanatory paragraph triggers the rule; referencing it inside a `### Example command:` code fence does not. This keeps the linter from flagging standards IDs that appear in sample linter fixtures or regex examples (including this spec's own §4).

## 5. Failure modes

Sample `gcc`-style diagnostics:

```
citation_lint: 2 error(s):
  docs/architecture/07-comms-stack.md:84: cites CCSDS 133.0-B-3 but references.md only lists CCSDS 133.0-B-2 (R1 — version mismatch; update both atomically per references.md Changelog Policy)
  docs/mission/verification/compliance-matrix.md:42: cites ECSS-E-ST-40C Rev.1 — add row to docs/standards/references.md §3 before citing (R3)

citation_lint: 1 warning(s):
  docs/standards/references.md: ECSS-M-ST-10C row has no citation under docs/** (R6 — remove row or add a citing doc)
```

## 6. Sample CLI

```
python3 scripts/citation_lint.py --repo-root .
```

Exit codes per [`../howto/authoring-a-repo-linter.md §4`](../howto/authoring-a-repo-linter.md): `0` pass (with possible R6 warnings on stderr), `1` any R1–R5 violation, `2` I/O error.

On pass, stdout carries a one-line counter summary:

```
citation_lint: OK — 37 citations across 22 docs, 28 references.md rows, 0 orphans.
```

## 7. Performance

Under 3 s budget per [`../howto/authoring-a-repo-linter.md §8`](../howto/authoring-a-repo-linter.md). Scope at time of writing: ~60 markdown files under `docs/`; ~5 regex patterns compiled once at module scope; total characters scanned ≈ 500 KB. A single-pass walk is sufficient.

Fenced-block exclusion is the one non-trivial bit — use a state machine that toggles on ```` ``` ```` lines and skips inline ``` `...` ``` spans.

## 8. CI integration

Per [`../ci-workflow.md §2`](../ci-workflow.md) (slot reserved), the job shape is:

```yaml
- name: citation-lint
  run: python3 scripts/citation_lint.py --repo-root .
```

Trigger: every PR that touches any file under `docs/**`. A PR that only touches `apps/` or `rust/` skips this job via path-filter — citations do not live in code, so the cost is wasted.

## 9. Test harness

Fixtures live under `scripts/tests/fixtures/citation/<rule-id>/` per [`../howto/authoring-a-repo-linter.md §7`](../howto/authoring-a-repo-linter.md):

| Fixture | Trigger |
|---|---|
| `r1-ccsds-missing/` | Doc cites `CCSDS 999.0-B-1`; references.md has no matching row — expected `exit 1` |
| `r2-npr-missing/` | Doc cites `NPR 9999.9A` not in references.md — expected `exit 1` |
| `r3-ecss-missing/` | Doc cites `ECSS-E-ST-99-99A` not in references.md — expected `exit 1` |
| `r4-nasa-std-missing/` | Doc cites `NASA-STD-9999` not in references.md — expected `exit 1` |
| `r5-ieee-missing/` | Doc cites `IEEE 9999` not in references.md — expected `exit 1` |
| `r6-orphan-row/` | references.md has a row no doc cites — expected `exit 0` with stderr warning |
| `fenced-code-excluded/` | `CCSDS 999.0-B-1` appears inside a `` ``` `` fence — expected `exit 0` (rule must ignore it) |
| `clean/` | Small docs tree with one citing doc + matching references.md — expected `exit 0` |

## 10. Binding to requirements and docs

- Mechanises the invariant in [`references.md` preamble](../../standards/references.md) and [`V&V-Plan.md §1.3` closing paragraph](../../mission/verification/V&V-Plan.md).
- Referenced from [`../../README.md` §Verification/CI item 5](../../README.md) as the planned implementation.
- Complements [`apid-mid-lint.md`](apid-mid-lint.md) (same pattern applied to a different invariant) and [`../../../scripts/traceability-lint.py`](../../../scripts/traceability-lint.py) (the same pattern applied to requirement IDs).

## 11. Out of scope

- Inline quoted titles (e.g. "per the TM Synchronization standard") — the linter matches on IDs only, not titles. Title-ID consistency is a code-review concern.
- Version drift inside references.md itself (e.g. pinning `CCSDS 133.0-B-2` while CCSDS publishes B-3) — that is a pedigree concern handled by the `references.md §Changelog Policy` update procedure.
- Non-standards citations (URL-only, book references, internal doc cross-refs) — those are checked by the link-check job in [`../ci-workflow.md §2`](../ci-workflow.md), not here.
- Citation inside non-doc sources (code comments, commit messages) — out of scope; the linter reads `docs/**/*.md` only.
