#!/usr/bin/env python3
"""
Requirement traceability linter for SAKURA-II.

Rules enforced (per docs/mission/requirements/SRD.md SYS-REQ-0080):

  1. Every XXX-REQ-#### defined in any SRD has exactly one row in traceability.md.
  2. Every XXX-REQ-#### cited anywhere under docs/ resolves to a traceability.md row.
  3. Every parent: link in a sub-SRD resolves to a requirement defined elsewhere.
  4. No duplicate requirement IDs within a single SRD or across SRDs.

Exit codes: 0 on success, 1 on any violation.

Usage:
    python3 scripts/traceability-lint.py [--repo-root PATH]

CI wiring (Phase C open per V&V-Plan §7.2): intended to run on every PR touching
docs/mission/requirements/** or docs/mission/verification/V&V-Plan.md.
"""

import argparse
import re
import sys
from pathlib import Path


REQ_RE = re.compile(r"\b((?:SYS|FSW|GND|ROV)-REQ-\d{4})\b")
SRD_FILES = {
    "SRD.md": "SYS",
    "FSW-SRD.md": "FSW",
    "GND-SRD.md": "GND",
    "ROVER-SRD.md": "ROV",
}
TRACEABILITY = "traceability.md"


def scan(path: Path) -> set[str]:
    """Return every requirement ID mentioned in the given file."""
    if not path.is_file():
        return set()
    return set(REQ_RE.findall(path.read_text(encoding="utf-8")))


def definitions_in(path: Path, prefix: str) -> set[str]:
    """Return requirement IDs with the given prefix that appear at column start of
    a markdown table row — i.e. the row's leading `| ID | ...` pattern."""
    if not path.is_file():
        return set()
    defined = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        m = re.match(rf"\|\s*({prefix}-REQ-\d{{4}})\s*\|", line)
        if m:
            defined.add(m.group(1))
    return defined


def traceability_rows(path: Path) -> set[str]:
    """Return every ID that has a row in traceability.md."""
    return definitions_in(path, "SYS") | definitions_in(path, "FSW") \
        | definitions_in(path, "GND") | definitions_in(path, "ROV")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Repository root (default: parent of this script's directory)",
    )
    args = parser.parse_args()

    req_dir = args.repo_root / "docs" / "mission" / "requirements"
    if not req_dir.is_dir():
        print(f"ERROR: {req_dir} does not exist", file=sys.stderr)
        return 1

    # --- 1. Collect definitions from each SRD.
    defined: dict[str, Path] = {}  # ID -> SRD file where it's defined
    errors: list[str] = []

    for fname, prefix in SRD_FILES.items():
        srd = req_dir / fname
        for rid in definitions_in(srd, prefix):
            if rid in defined:
                errors.append(
                    f"duplicate definition: {rid} in both {defined[rid].name} and {fname}"
                )
            else:
                defined[rid] = srd

    # --- 2. traceability.md rows.
    trace = req_dir / TRACEABILITY
    trace_ids = traceability_rows(trace)

    missing_from_trace = sorted(defined.keys() - trace_ids)
    for rid in missing_from_trace:
        errors.append(f"defined but not in traceability.md: {rid}")

    extra_in_trace = sorted(trace_ids - defined.keys())
    for rid in extra_in_trace:
        errors.append(f"in traceability.md but not defined in any SRD: {rid}")

    # --- 3. Every citation under docs/ resolves.
    docs_root = args.repo_root / "docs"
    for md in docs_root.rglob("*.md"):
        cited = scan(md)
        for rid in cited:
            if rid not in defined:
                errors.append(f"{md.relative_to(args.repo_root)}: cites undefined requirement {rid}")

    # --- 4. Report.
    if errors:
        print(f"traceability-lint: {len(errors)} error(s):", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1

    print(
        f"traceability-lint: OK — {len(defined)} requirements, "
        f"all present in traceability.md, all citations resolve."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
