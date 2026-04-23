#!/usr/bin/env bash
# Extracts every ```mermaid block from docs/**/*.md and validates each one with mmdc.
# Exit 0 if no blocks found (safe for repos without Mermaid diagrams yet).
# Usage: bash scripts/mermaid-parse.sh <docs-root>
set -euo pipefail

DOCS_ROOT="${1:-docs}"

if [[ ! -d "$DOCS_ROOT" ]]; then
    echo "mermaid-parse: error: directory not found: $DOCS_ROOT" >&2
    exit 2
fi

if ! command -v mmdc &>/dev/null; then
    echo "mermaid-parse: error: mmdc not found — install @mermaid-js/mermaid-cli" >&2
    exit 2
fi

TMPDIR_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_ROOT"' EXIT

failures=0
blocks_found=0

while IFS= read -r -d '' md_file; do
    block_idx=0
    in_block=0
    block_content=""

    while IFS= read -r line; do
        if [[ $in_block -eq 0 && "$line" =~ ^[[:space:]]*\`\`\`mermaid ]]; then
            in_block=1
            block_content=""
            continue
        fi
        if [[ $in_block -eq 1 ]]; then
            if [[ "$line" =~ ^[[:space:]]*\`\`\` ]]; then
                # End of block — write to temp file and validate
                block_idx=$(( block_idx + 1 ))
                blocks_found=$(( blocks_found + 1 ))
                tmpfile="$TMPDIR_ROOT/block_${blocks_found}.mmd"
                printf '%s\n' "$block_content" > "$tmpfile"
                if ! mmdc -i "$tmpfile" -o /dev/null 2>/dev/null; then
                    rel="${md_file#./}"
                    echo "mermaid-parse: FAIL: $rel block $block_idx" >&2
                    failures=$(( failures + 1 ))
                fi
                in_block=0
                block_content=""
            else
                if [[ -n "$block_content" ]]; then
                    block_content="${block_content}
${line}"
                else
                    block_content="$line"
                fi
            fi
        fi
    done < "$md_file"
done < <(find "$DOCS_ROOT" -name "*.md" -print0)

if [[ $blocks_found -eq 0 ]]; then
    echo "mermaid-parse: OK — no mermaid blocks found."
    exit 0
fi

if [[ $failures -gt 0 ]]; then
    echo "mermaid-parse: $failures block(s) failed to parse." >&2
    exit 1
fi

echo "mermaid-parse: OK — $blocks_found block(s) parsed successfully."
exit 0
