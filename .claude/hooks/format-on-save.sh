#!/usr/bin/env bash
# Called after Write or Edit. Formats the changed file with the appropriate
# space-systems formatter: rustfmt for Rust, clang-format for C/C++.
# Claude Code passes a JSON payload on stdin.

set -euo pipefail

INPUT=$(cat)
FILE_PATH=$(echo "$INPUT" | jq -r '.tool_result.file_path // .tool_input.file_path // empty' 2>/dev/null || true)

if [[ -z "$FILE_PATH" ]]; then
  exit 0
fi

case "$FILE_PATH" in
  *.rs)
    if command -v rustfmt &>/dev/null; then
      rustfmt "$FILE_PATH" 2>/dev/null || true
    fi
    ;;
  *.c|*.h|*.cpp|*.hpp)
    if command -v clang-format &>/dev/null; then
      # -style=file reads .clang-format from the project root
      clang-format -style=file -i "$FILE_PATH" 2>/dev/null || true
    fi
    ;;
  *.md|*.json|*.yaml|*.toml)
    # Intentionally unformatted — no formatter dependency for config/docs
    ;;
  *)
    exit 0
    ;;
esac

exit 0
