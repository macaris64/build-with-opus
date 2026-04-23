"""
Tests for scripts/citation_lint.py

Each test runs the linter against a minimal fixture repo and asserts the
expected exit code and a substring of the diagnostic output.

Fixture layout:
    scripts/tests/fixtures/citation/<fixture-name>/
        docs/standards/references.md
        docs/test.md
"""

import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
LINTER = REPO_ROOT / "scripts" / "citation_lint.py"
FIXTURES = Path(__file__).parent / "fixtures" / "citation"


def run(fixture: str) -> tuple[int, str, str]:
    result = subprocess.run(
        [sys.executable, str(LINTER), "--repo-root", str(FIXTURES / fixture)],
        capture_output=True,
        text=True,
    )
    return result.returncode, result.stdout, result.stderr


def test_clean_exits_zero():
    rc, stdout, _ = run("clean")
    assert rc == 0
    assert "OK" in stdout


def test_r1_ccsds_missing_exits_one():
    rc, _, stderr = run("r1-ccsds-missing")
    assert rc == 1
    assert "R1" in stderr


def test_r2_npr_missing_exits_one():
    rc, _, stderr = run("r2-npr-missing")
    assert rc == 1
    assert "R2" in stderr


def test_r3_ecss_missing_exits_one():
    rc, _, stderr = run("r3-ecss-missing")
    assert rc == 1
    assert "R3" in stderr


def test_r4_nasa_std_missing_exits_one():
    rc, _, stderr = run("r4-nasa-std-missing")
    assert rc == 1
    assert "R4" in stderr


def test_r5_ieee_missing_exits_one():
    rc, _, stderr = run("r5-ieee-missing")
    assert rc == 1
    assert "R5" in stderr
