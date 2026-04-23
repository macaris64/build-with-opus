#!/usr/bin/env python3
"""
APID/MID consistency linter for SAKURA-II.

Rules enforced (per docs/dev/linter-specs/apid-mid-lint.md):

  R1. Every _MID macro in apps/**/*.h whose derived APID (value & 0x7FF) is in
      the usable range 0x100-0x7FE has a row in apid-registry.md.
  R2. Every single-APID non-reserved row in apid-registry.md has a matching
      _MID macro in apps/**/*.h. Skipped when no valid-range macros are found
      (Phase 13 _defs/mids.h not yet landed).
  R3. SPACECRAFT_ID in _defs/mission_config.h matches the SCID stated in
      apid-registry.md.
  R4. Every APID in non-reserved apid-registry.md rows falls within 0x100-0x7FE.
  R5. No two _MID macros in apps/**/*.h derive the same APID.

Exit codes: 0 pass, 1 violation, 2 I/O error.

Usage:
    python3 scripts/apid_mid_lint.py [--repo-root PATH]
"""

import argparse
import re
import sys
from pathlib import Path

MID_RE = re.compile(
    r"^\s*#define\s+([A-Z_][A-Z0-9_]*_MID)\s+\(?(0x[0-9A-Fa-f]+|\d+)U?\)?"
)
SCID_HEADER_RE = re.compile(
    r"^\s*#define\s+SPACECRAFT_ID\s+\(?(0x[0-9A-Fa-f]+|\d+)U?\)?"
)
# Matches rows like | `0x110` | ... | or | `0x101`–`0x10F` | ... |
REGISTRY_ROW_RE = re.compile(
    r"^\|\s*`(0x[0-9A-Fa-f]{3})`([–—-]`0x[0-9A-Fa-f]{3}`)?\s*\|(.+)"
)
SCID_REGISTRY_RE = re.compile(r"SPACECRAFT_ID\s*=\s*(\d+)U?")
RESERVED_MARKER = "*reserved*"

VALID_APID_LOW = 0x100
VALID_APID_HIGH = 0x7FE


def _parse_int(raw: str) -> int:
    return int(raw, 16 if raw.startswith("0x") or raw.startswith("0X") else 10)


def parse_mids(apps_root: Path) -> list:
    """Return list of (macro_name, apid, file_path, lineno) from apps/**/*.h."""
    results = []
    if not apps_root.is_dir():
        return results
    for h_file in sorted(apps_root.rglob("*.h")):
        try:
            lines = h_file.read_text(encoding="utf-8").splitlines()
        except OSError:
            continue
        for lineno, line in enumerate(lines, 1):
            m = MID_RE.match(line)
            if m:
                name = m.group(1)
                raw = m.group(2)
                value = _parse_int(raw)
                apid = value & 0x07FF
                results.append((name, apid, h_file, lineno))
    return results


def parse_scid_header(defs_root: Path):
    """Return (scid_value_or_None, file_path, lineno)."""
    path = defs_root / "mission_config.h"
    if not path.is_file():
        return None, path, 0
    for lineno, line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), 1
    ):
        m = SCID_HEADER_RE.match(line)
        if m:
            return _parse_int(m.group(1)), path, lineno
    return None, path, 0


def parse_registry(registry_path: Path):
    """
    Return (scid_or_None, rows) where each row is
    (apid, is_reserved, is_range, lineno).
    """
    if not registry_path.is_file():
        return None, []
    scid = None
    rows = []
    for lineno, line in enumerate(
        registry_path.read_text(encoding="utf-8").splitlines(), 1
    ):
        m_scid = SCID_REGISTRY_RE.search(line)
        if m_scid:
            scid = int(m_scid.group(1))
        m_row = REGISTRY_ROW_RE.match(line)
        if m_row:
            apid = int(m_row.group(1), 16)
            is_range = m_row.group(2) is not None
            rest = m_row.group(3)
            # Check case-insensitively: catches both *reserved* allocation rows
            # and the CCSDS reserved-ranges table (which uses plain "reserved").
            is_reserved = "reserved" in rest.lower()
            rows.append((apid, is_reserved, is_range, lineno))
    return scid, rows


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

    try:
        mids = parse_mids(repo / "apps")
        scid_val, scid_file, scid_line = parse_scid_header(repo / "_defs")
        registry_path = repo / "docs" / "interfaces" / "apid-registry.md"
        registry_scid, registry_rows = parse_registry(registry_path)
    except OSError as exc:
        print(f"apid_mid_lint: I/O error: {exc}", file=sys.stderr)
        return 2

    errors = []
    warnings = []

    valid_mids = [
        (n, a, f, l)
        for n, a, f, l in mids
        if VALID_APID_LOW <= a <= VALID_APID_HIGH
    ]
    template_mids = [
        (n, a, f, l)
        for n, a, f, l in mids
        if not (VALID_APID_LOW <= a <= VALID_APID_HIGH)
    ]

    registry_apids = {apid for apid, _, _, _ in registry_rows}
    valid_macro_apids = {a for _, a, _, _ in valid_mids}

    # R1: every valid-range MID macro must have a registry row.
    for name, apid, path, lineno in valid_mids:
        if apid not in registry_apids:
            rel = path.relative_to(repo)
            errors.append(
                f"{rel}:{lineno}: {name} derives APID 0x{apid:03X}"
                f" — no row in docs/interfaces/apid-registry.md (R1)"
            )

    for name, apid, path, lineno in template_mids:
        rel = path.relative_to(repo)
        warnings.append(
            f"{rel}:{lineno}: {name} derives APID 0x{apid:03X}"
            " (outside usable range — template placeholder, skipped)"
        )

    # R2: single-APID non-reserved registry rows must have a matching macro.
    if valid_macro_apids:
        for apid, is_reserved, is_range, lineno in registry_rows:
            if is_reserved or is_range:
                continue
            if VALID_APID_LOW <= apid <= VALID_APID_HIGH and apid not in valid_macro_apids:
                rel = registry_path.relative_to(repo)
                errors.append(
                    f"{rel}:{lineno}: APID 0x{apid:03X} has no matching"
                    " _MID macro under apps/** (R2)"
                )
    else:
        print(
            "apid_mid_lint: note — R2 skipped: no valid-range MID macros found"
            " (Phase 13 _defs/mids.h not yet landed)"
        )

    # R3: SPACECRAFT_ID must match registry SCID.
    if scid_val is None:
        errors.append(
            f"{scid_file.relative_to(repo)}: SPACECRAFT_ID definition not found (R3)"
        )
    elif registry_scid is None:
        errors.append(
            f"{registry_path.relative_to(repo)}: SCID not found in registry (R3)"
        )
    elif scid_val != registry_scid:
        rel_h = scid_file.relative_to(repo)
        rel_r = registry_path.relative_to(repo)
        errors.append(
            f"{rel_h}:{scid_line} vs {rel_r}: SPACECRAFT_ID mismatch"
            f" — {rel_h} says {scid_val}U, registry says {registry_scid}U (R3)"
        )

    # R4: non-reserved registry APIDss must be in the usable range.
    for apid, is_reserved, _, lineno in registry_rows:
        if is_reserved:
            continue
        if not (VALID_APID_LOW <= apid <= VALID_APID_HIGH):
            rel = registry_path.relative_to(repo)
            errors.append(
                f"{rel}:{lineno}: APID 0x{apid:03X} is outside"
                " usable range 0x100-0x7FE (R4)"
            )

    # R5: no two valid-range macros may derive the same APID.
    seen: dict = {}
    for name, apid, path, lineno in valid_mids:
        if apid in seen:
            prev_name, prev_path, prev_line = seen[apid]
            rel = path.relative_to(repo)
            prev_rel = prev_path.relative_to(repo)
            errors.append(
                f"{rel}:{lineno}: {name} and {prev_rel}:{prev_line}:{prev_name}"
                f" both derive APID 0x{apid:03X} (R5)"
            )
        else:
            seen[apid] = (name, path, lineno)

    if warnings:
        for w in warnings:
            print(f"apid_mid_lint: warning: {w}", file=sys.stderr)

    if errors:
        print(f"apid_mid_lint: {len(errors)} error(s):", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1

    n_valid = len(valid_mids)
    n_rows = len(registry_rows)
    scid_str = str(registry_scid) if registry_scid is not None else "?"
    print(
        f"apid_mid_lint: OK — {n_valid} valid-range MID macro(s),"
        f" {n_rows} registry row(s), SCID={scid_str} matches."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
