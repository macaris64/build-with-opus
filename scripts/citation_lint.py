#!/usr/bin/env python3
"""
Standards citation integrity linter for SAKURA-II.

Rules enforced (per docs/dev/linter-specs/citation-lint.md):

  R1. Every CCSDS ID cited in docs/**/*.md exists in docs/standards/references.md.
  R2. Every NPR ID cited in docs/**/*.md exists in references.md.
  R3. Every ECSS ID cited in docs/**/*.md exists in references.md.
  R4. Every NASA-STD ID cited in docs/**/*.md exists in references.md.
  R5. Every IEEE ID cited in docs/**/*.md exists in references.md.
  R6. Every standard ID in references.md has at least one citation (warning only,
      exit 0).

Exit codes: 0 pass (R6 warnings may appear on stderr), 1 R1-R5 violation,
2 I/O error.

Usage:
    python3 scripts/citation_lint.py [--repo-root PATH]
"""

import argparse
import re
import sys
from pathlib import Path

# Regexes copied verbatim from docs/dev/linter-specs/citation-lint.md §4.
CCSDS_RE = re.compile(r"\bCCSDS\s+(\d{3}\.\d(?:-[A-Z])?(?:-\d)?)\b")
NPR_RE = re.compile(r"\bNPR\s+(\d{4}\.\d[A-Z]?)\b")
ECSS_RE = re.compile(r"\bECSS-[EQMS]-ST-\d{2}(?:-\d{2})?[A-Z](?:\s+Rev\.\d)?\b")
NASASTD_RE = re.compile(r"\bNASA-STD-\d{4}(?:\.\d[A-Z]?)?\b")
IEEE_RE = re.compile(r"\bIEEE\s+\d{3,4}\b")

STANDARDS = [
    ("CCSDS", "R1", CCSDS_RE),
    ("NPR", "R2", NPR_RE),
    ("ECSS", "R3", ECSS_RE),
    ("NASA-STD", "R4", NASASTD_RE),
    ("IEEE", "R5", IEEE_RE),
]

_INLINE_CODE_RE = re.compile(r"`[^`]*`")


def _normalize(text: str) -> str:
    """Collapse internal whitespace."""
    return re.sub(r"\s+", " ", text.strip())


def extract_citations(text: str) -> list:
    """
    Return list of (kind, rule, raw_id, lineno) for each standards ID found
    in text, excluding fenced code blocks and inline code spans.
    """
    results = []
    in_fence = False
    for lineno, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        # Toggle fence state on opening/closing ```
        if stripped.startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        clean = _INLINE_CODE_RE.sub("", line)
        for kind, rule, pattern in STANDARDS:
            for m in pattern.finditer(clean):
                results.append((kind, rule, _normalize(m.group(0)), lineno))
    return results


def extract_ref_ids(ref_path: Path) -> set:
    """Return the set of all standard IDs present anywhere in references.md."""
    if not ref_path.is_file():
        return set()
    text = ref_path.read_text(encoding="utf-8")
    found = set()
    for _kind, _rule, pattern in STANDARDS:
        for m in pattern.finditer(text):
            found.add(_normalize(m.group(0)))
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Repository root (default: parent of scripts/)",
    )
    args = parser.parse_args()
    repo = args.repo_root

    docs_root = repo / "docs"
    ref_path = docs_root / "standards" / "references.md"

    try:
        ref_ids = extract_ref_ids(ref_path)
    except OSError as exc:
        print(f"citation_lint: I/O error reading references.md: {exc}", file=sys.stderr)
        return 2

    errors = []
    all_cited: set = set()

    for md_path in sorted(docs_root.rglob("*.md")):
        if md_path == ref_path:
            continue
        try:
            text = md_path.read_text(encoding="utf-8")
        except OSError as exc:
            print(f"citation_lint: I/O error: {exc}", file=sys.stderr)
            return 2

        for kind, rule, std_id, lineno in extract_citations(text):
            all_cited.add(std_id)
            # Accept an exact match OR a prefix match so that shorthand citations
            # (e.g. "CCSDS 133.0-B" or "ECSS-E-ST-40C") resolve against the full
            # versioned entry in references.md ("CCSDS 133.0-B-2", "ECSS-E-ST-40C Rev.1").
            found = std_id in ref_ids or any(
                ref_id.startswith(std_id) for ref_id in ref_ids
            )
            if not found:
                rel = md_path.relative_to(repo)
                errors.append(
                    f"{rel}:{lineno}: cites {std_id}"
                    f" — add row to docs/standards/references.md ({rule})"
                )

    # R6: warn about orphan rows (warning only, exit 0).
    warnings = []
    for ref_id in sorted(ref_ids):
        if ref_id not in all_cited:
            rel = ref_path.relative_to(repo)
            warnings.append(
                f"{rel}: {ref_id} row has no citation under docs/**"
                " (R6 — remove row or add a citing doc)"
            )

    if warnings:
        print(f"citation_lint: {len(warnings)} warning(s):", file=sys.stderr)
        for w in warnings:
            print(f"  {w}", file=sys.stderr)

    if errors:
        print(f"citation_lint: {len(errors)} error(s):", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1

    n_cited = len(all_cited)
    n_docs = sum(1 for p in docs_root.rglob("*.md") if p != ref_path)
    n_refs = len(ref_ids)
    n_orphans = len(warnings)
    print(
        f"citation_lint: OK — {n_cited} unique citation(s) across"
        f" {n_docs} doc(s), {n_refs} references.md entry(ies),"
        f" {n_orphans} orphan(s)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
