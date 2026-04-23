"""
Tests for scripts/apid_mid_lint.py

Each test runs the linter against a minimal fixture repo and asserts the
expected exit code and a substring of the stderr diagnostic.

Fixture layout:
    scripts/tests/fixtures/apid_mid/<fixture-name>/
        _defs/mission_config.h
        docs/interfaces/apid-registry.md
        apps/<app>/fsw/src/<app>.h
"""

import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
LINTER = REPO_ROOT / "scripts" / "apid_mid_lint.py"
FIXTURES = Path(__file__).parent / "fixtures" / "apid_mid"


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


def test_r1_missing_registry_row_exits_one():
    rc, _, stderr = run("r1-missing-registry-row")
    assert rc == 1
    assert "R1" in stderr


def test_r2_orphan_registry_row_exits_one():
    rc, _, stderr = run("r2-orphan-registry-row")
    assert rc == 1
    assert "R2" in stderr


def test_r3_scid_mismatch_exits_one():
    rc, _, stderr = run("r3-scid-mismatch")
    assert rc == 1
    assert "R3" in stderr


def test_r4_reserved_apid_exits_one():
    rc, _, stderr = run("r4-reserved-apid")
    assert rc == 1
    assert "R4" in stderr


def test_r5_duplicate_apid_exits_one():
    rc, _, stderr = run("r5-duplicate-apid")
    assert rc == 1
    assert "R5" in stderr


def test_r6_inline_mid_in_apps_exits_one():
    rc, _, stderr = run("r6-inline-mid-in-apps")
    assert rc == 1
    assert "R6" in stderr
