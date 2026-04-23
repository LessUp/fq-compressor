#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: ./scripts/dev/create-worktree.sh <change-name> [base-branch]" >&2
    exit 2
fi

change_name="$1"
base_branch="${2:-}"

repo_root=$(git rev-parse --show-toplevel)
cd "$repo_root"

if [[ -z "$base_branch" ]]; then
    base_branch=$(
        git symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>/dev/null | sed 's@^origin/@@'
    )
    base_branch=${base_branch:-master}
fi

branch_name="opsx/${change_name}"
worktree_path="$(dirname "$repo_root")/fqc-${change_name}"

if git show-ref --verify --quiet "refs/heads/${branch_name}"; then
    echo "Branch already exists: ${branch_name}" >&2
    exit 1
fi

if [[ -e "$worktree_path" ]]; then
    echo "Worktree path already exists: $worktree_path" >&2
    exit 1
fi

git fetch origin "$base_branch" --quiet || true

if git show-ref --verify --quiet "refs/remotes/origin/${base_branch}"; then
    git worktree add -b "$branch_name" "$worktree_path" "origin/${base_branch}"
else
    git worktree add -b "$branch_name" "$worktree_path" "$base_branch"
fi

cat <<EOF
Created worktree:
  path:   $worktree_path
  branch: $branch_name
  base:   $base_branch

Recommended next steps:
  cd $worktree_path
  ./scripts/dev/preflight.sh
  openspec status --change "$change_name"
  # Or skip this helper entirely and keep working in your current checkout if that is faster.
EOF
