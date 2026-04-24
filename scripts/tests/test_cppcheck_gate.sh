#!/usr/bin/env bash
# Phase 18 Red/Green test: gate script must exist and exit 0 on a clean tree.
set -euo pipefail
bash "$(dirname "$0")/../cppcheck-gate.sh"
