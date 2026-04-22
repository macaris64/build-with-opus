#!/usr/bin/env bash
# Called before Write or Edit. Blocks edits to .env files and private keys.
# Exit code 2 = block the action and show stderr as feedback to Claude.

set -euo pipefail

INPUT=$(cat)
FILE_PATH=$(echo "$INPUT" | jq -r '.tool_input.file_path // empty' 2>/dev/null || true)

if [[ -z "$FILE_PATH" ]]; then
  exit 0
fi

BASENAME=$(basename "$FILE_PATH")

case "$BASENAME" in
  .env|.env.local|.env.production|.env.staging|.env.test)
    echo "Blocked: Direct edits to $BASENAME are not allowed." >&2
    echo "Edit .env.example instead and update your local .env manually." >&2
    exit 2
    ;;
  *.pem|*.key|id_rsa|id_ed25519|id_ecdsa)
    echo "Blocked: Private key files must not be edited by Claude." >&2
    exit 2
    ;;
esac

exit 0
