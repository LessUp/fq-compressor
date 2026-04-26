#!/usr/bin/env bash

set -euo pipefail

strict=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --strict)
            strict=1
            shift
            ;;
        -h|--help)
            cat <<'EOF'
Usage: ./scripts/dev/preflight.sh [--strict]

Checks:
  - current branch
  - working tree cleanliness
  - active OpenSpec changes

--strict  Exit non-zero on actionable warnings.
EOF
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 2
    fi
}

require_cmd git
require_cmd openspec

repo_root=$(git rev-parse --show-toplevel)
cd "$repo_root"

warnings=()
current_branch=$(git branch --show-current 2>/dev/null || echo "detached")

dirty_status=$(git --no-pager status --short --untracked-files=normal)
if [[ -n "$dirty_status" ]]; then
    warnings+=("working tree has uncommitted changes")
fi

active_changes=$(openspec list --json 2>/dev/null || echo '{"changes":[]}')
active_change_count=$(printf '%s' "$active_changes" | jq 'if type == "object" then (.changes // [] | length) else length end' 2>/dev/null || echo 0)
if [[ "$active_change_count" != "0" && "$active_change_count" != "1" ]]; then
    warnings+=("more than one OpenSpec change is active")
fi

echo "== fq-compressor preflight =="
echo "repo:            $repo_root"
echo "current branch:  $current_branch"
echo "dirty tree:      $([[ -n "$dirty_status" ]] && echo yes || echo no)"
echo "active changes:  $active_change_count"

if [[ -n "$dirty_status" ]]; then
    echo
    echo "Uncommitted changes:"
    printf '%s\n' "$dirty_status"
fi

if ((${#warnings[@]} > 0)); then
    echo
    echo "Warnings:"
    printf '  - %s\n' "${warnings[@]}"
fi

if ((strict)) && ((${#warnings[@]} > 0)); then
    exit 1
fi
