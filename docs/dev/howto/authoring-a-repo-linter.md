# How-To: Author a Repo Linter

> Terminology: [../../GLOSSARY.md](../../GLOSSARY.md). Rules: [`../../../.claude/rules/general.md`](../../../.claude/rules/general.md), [`security.md`](../../../.claude/rules/security.md), [`testing.md`](../../../.claude/rules/testing.md). V&V plan: [`../../mission/verification/V&V-Plan.md §7.2`](../../mission/verification/V&V-Plan.md). Reference exemplar: [`../../../scripts/traceability-lint.py`](../../../scripts/traceability-lint.py). Linter specs using this template: [`../linter-specs/apid-mid-lint.md`](../linter-specs/apid-mid-lint.md), [`../linter-specs/citation-lint.md`](../linter-specs/citation-lint.md).

SAKURA-II enforces several authoring invariants — requirement-ID traceability, APID/MID consistency, citation integrity, and more — via small Python linters under [`scripts/`](../../../scripts/). This guide is the canonical template for writing a new one. It describes file shape, CLI contract, rule-authoring conventions, and CI integration so every linter reads like the same tool with a different rule set.

Implementation precedent: [`scripts/traceability-lint.py`](../../../scripts/traceability-lint.py) — 128 lines, stdlib-only, zero external deps. Every new linter should be indistinguishable in shape.

## 1. When this applies

- You are codifying a doc-level or source-level invariant that would otherwise require reviewer vigilance (e.g. "every standards ID must round-trip to `references.md`").
- The invariant is mechanically checkable from text (markdown, C headers, TOML) without running the build.
- The check is cheap enough to gate every PR (§8 — under 3 s on a clean checkout).

**Do not** author a linter when:

- The invariant requires executing code (use `ctest`, `cargo test`, `colcon test` instead).
- The invariant belongs in a compiler warning (use `-Wall -Wextra -Werror`, `cppcheck`, `cargo clippy`).
- The invariant is stylistic and subjective (put it in [`.claude/rules/`](../../../.claude/rules/) and let code review catch it).

## 2. Precedent

Read [`scripts/traceability-lint.py`](../../../scripts/traceability-lint.py) end-to-end before writing a new linter. It fixes the four conventions this guide formalises:

1. stdlib-only (no `pip install`)
2. `argparse` with `--repo-root` defaulting to the script's grandparent
3. Rule-by-rule scan, with all errors collected before any exit
4. `gcc`-style diagnostics on stderr, one line per violation

Copy its top-of-file docstring shape — it is the linter's user-facing contract.

## 3. File shape

```python
#!/usr/bin/env python3
"""
<One-sentence purpose>.

Rules enforced (per <source-of-truth doc + section/line anchor>):

  1. <Rule 1 verbatim>
  2. <Rule 2 verbatim>
  ...

Exit codes: 0 on success, 1 on any violation, 2 on usage/I/O error.

Usage:
    python3 scripts/<subject>-lint.py [--repo-root PATH]

CI wiring (Phase C open per V&V-Plan §7.2): intended to run on every PR
touching <relevant path globs>.
"""

import argparse
import re
import sys
from pathlib import Path

# Compile regexes and build any static lookup tables at module scope so they
# cost nothing per-file during the walk.

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
    )
    args = parser.parse_args()

    errors: list[str] = []

    # --- 1. Collect inputs.
    # --- 2. Apply rules, append to errors.
    # --- 3. Report.

    if errors:
        print(f"<subject>-lint: {len(errors)} error(s):", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1

    print(f"<subject>-lint: OK — <summary counters>")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

The shape is not a suggestion. If a new linter does not match this outline section-for-section, the code review should push it back.

## 4. Exit codes

| Code | Meaning |
|---|---|
| `0` | All rules satisfied. Stdout carries a one-line "OK — <counters>" summary. |
| `1` | At least one rule violated. Stderr carries the count + one line per violation. |
| `2` | Invalid CLI usage, missing `--repo-root`, or I/O failure reading an expected file. Stderr describes the failure. |

CI treats `1` and `2` identically (red check), but the split lets a developer distinguish "I wrote buggy prose" from "the linter itself is broken."

## 5. Rule authoring

Every rule stated in the linter's top-level docstring must:

- Be **quoted verbatim** from the source-of-truth doc where possible (SRD line, README bullet, style guide rule). If paraphrased, cite the source section so a reviewer can verify faithfulness.
- Map to **exactly one error-append path** in code. One rule → one `errors.append(...)` call site; do not OR multiple checks into a single violation message.
- Emit `gcc`-style diagnostics: `<path>:<line>: <message>` where `<path>` is repo-relative. Tools like VS Code, vim `:cfile`, and GitHub Actions annotations parse this format automatically. Example from [`traceability-lint.py`](../../../scripts/traceability-lint.py):

  ```
  docs/mission/requirements/FSW-SRD.md: cites undefined requirement SYS-REQ-XXXX
  ```

- Collect **all** violations before returning. Never `sys.exit` from inside a rule loop — a contributor wants to see every mistake in one run.

## 6. CI integration

Per [`V&V-Plan.md §7.2`](../../mission/verification/V&V-Plan.md) and the forthcoming [`../ci-workflow.md`](../ci-workflow.md), every linter gets its own matrix job so a red linter annotates the PR with the exact failing lines. The job shape is always:

```yaml
- name: <subject>-lint
  run: python3 scripts/<subject>-lint.py --repo-root .
```

No shell wrappers, no sourcing virtualenvs, no `pip install`. If the linter needs a dep the CI cannot provide out of the box, rewrite the linter (§10 anti-pattern).

Trigger scope: the linter runs on every PR unless the V&V plan explicitly narrows its path-filter. Default to "always on" — linters are cheap (§8) and false-negatives-because-the-job-was-skipped is worse than false-positives-because-an-unrelated-file-changed.

## 7. Test harness

Every linter under `scripts/*-lint.py` has a sibling test file at `scripts/tests/test_<subject>_lint.py`. Use stdlib `unittest` (no pytest dependency). Shape:

- One fixture directory per rule, under `scripts/tests/fixtures/<subject>/<rule-id>/`. The directory is a miniature repo (just the files the rule inspects).
- For each fixture, a golden stdout/stderr file. The test runs the linter against the fixture with `--repo-root <fixture>` and diffs against the golden.
- Include one "clean" fixture per linter (all rules pass) so a regression that over-reports is caught.

Regenerating goldens is a single `python3 scripts/tests/regen-goldens.py <subject>` invocation (helper TBD — first linter to need it adds it). Never hand-edit a golden output without a corresponding code change.

## 8. Performance budget

Under **3 seconds** on a clean checkout for a full repo scan. Measure with `time python3 scripts/<subject>-lint.py --repo-root .`.

Why 3 s: CI runs every linter in parallel, but a developer's pre-push loop runs them serially. At 5 linters × 3 s the loop is 15 s — long enough to notice, short enough to actually run. Past that, developers skip the loop.

If a linter overshoots the budget, the optimisation hierarchy is: (1) compile regexes at module scope; (2) prune the file walk to only the paths the rules touch; (3) memoise file reads keyed on path; (4) last resort, read files once and pass the text to every rule.

## 9. Naming

- Script file: `scripts/<subject>-lint.py` — hyphenated, `-lint.py` suffix. Matches the existing [`traceability-lint.py`](../../../scripts/traceability-lint.py).
- Spec doc: `docs/dev/linter-specs/<subject>-lint.md`.
- Test file: `scripts/tests/test_<subject>_lint.py` — underscored because it is a Python module name.
- CI job name in the workflow: `<subject>-lint` — matches the script stem.

The subject is a noun or noun-phrase naming what is being validated: `traceability`, `apid-mid`, `citation`. Not `check-traceability`, not `validate-citations` — the verb is implicit in `-lint`.

## 10. Anti-patterns

- **No shelling out.** `subprocess.run(["grep", ...])` couples the linter to a specific grep version and hides the regex. Use Python's `re` module directly.
- **No external dependencies.** Stdlib + Python 3.11 only. If you find yourself reaching for `markdown-it-py` or `tomli`, stop and write a 10-line regex instead.
- **No rules that depend on build output.** If the linter needs `cargo build` to have run, it is an integration test, not a linter. Put it in `cargo test --features lint` or similar.
- **No cross-rule coupling.** A rule's error message must be actionable on its own. Never emit "see rule 3 above" — quote the relevant context in the message itself.
- **No silent success.** The final `print(... OK — ...)` line is mandatory. A linter that emits nothing on success is indistinguishable from a linter that failed to run.
- **No file mutation.** Linters are read-only. Formatters (`rustfmt`, `clang-format`) are a separate category and live in [`../../../.claude/hooks/format-on-save.sh`](../../../.claude/hooks/format-on-save.sh), not `scripts/`.

## 11. What this guide does NOT cover

- Rule-specific content — each linter spec under [`../linter-specs/`](../linter-specs/) carries its own rule list, regex catalogue, and CI slot.
- Language-specific linters (`cargo clippy`, `cppcheck`, `clang-tidy`) — those are configured in their respective toolchains, not authored here.
- Per-repo style linting — style rules live in [`../../../.claude/rules/`](../../../.claude/rules/) and are enforced by code review.
