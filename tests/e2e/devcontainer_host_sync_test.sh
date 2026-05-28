#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# shellcheck source=/dev/null
source "${PROJECT_ROOT}/.devcontainer/scripts/lib/host_sync.sh"

TEST_ROOT="$(mktemp -d)"
HOST_HOME="${TEST_ROOT}/host-home"
CONTAINER_HOME="${TEST_ROOT}/container-home"
STAGING_ROOT="${TEST_ROOT}/staging"

cleanup() {
    rm -rf "${TEST_ROOT}"
}
trap cleanup EXIT

mkdir -p "${HOST_HOME}/.claude" "${HOST_HOME}/.codex" "${HOST_HOME}/.ssh"
mkdir -p "${CONTAINER_HOME}" "${STAGING_ROOT}"

printf '[user]\n\tname = Host User\n' > "${HOST_HOME}/.gitconfig"
printf '{"apiKey":"abc"}\n' > "${HOST_HOME}/.claude/config.json"
printf '{"theme":"dark"}\n' > "${HOST_HOME}/.claude/settings.kimi.json"
printf '{"session":"token"}\n' > "${HOST_HOME}/.claude.json"
printf 'token = "xyz"\n' > "${HOST_HOME}/.codex/config.toml"
printf '{"auth":"ok"}\n' > "${HOST_HOME}/.codex/auth.json"

host_sync_stage_home "${HOST_HOME}" "${STAGING_ROOT}" "${TEST_ROOT}/repo"
host_sync_apply_container_home "${CONTAINER_HOME}" "${STAGING_ROOT}"

cmp -s "${HOST_HOME}/.gitconfig" "${STAGING_ROOT}/.fqcompressor-host-gitconfig"
cmp -s "${HOST_HOME}/.claude/config.json" "${STAGING_ROOT}/.fqcompressor-host-claude/config.json"
cmp -s "${HOST_HOME}/.claude/settings.kimi.json" "${STAGING_ROOT}/.fqcompressor-host-claude/settings.kimi.json"
cmp -s "${HOST_HOME}/.claude.json" "${STAGING_ROOT}/.fqcompressor-host-claude/.claude.json"
cmp -s "${HOST_HOME}/.codex/auth.json" "${STAGING_ROOT}/.fqcompressor-host-codex/auth.json"
cmp -s "${HOST_HOME}/.codex/config.toml" "${STAGING_ROOT}/.fqcompressor-host-codex/config.toml"

cmp -s "${HOST_HOME}/.gitconfig" "${CONTAINER_HOME}/.gitconfig"
cmp -s "${HOST_HOME}/.claude/config.json" "${CONTAINER_HOME}/.claude/config.json"
cmp -s "${HOST_HOME}/.claude/settings.kimi.json" "${CONTAINER_HOME}/.claude/settings.kimi.json"
cmp -s "${HOST_HOME}/.claude.json" "${CONTAINER_HOME}/.claude.json"
cmp -s "${HOST_HOME}/.codex/auth.json" "${CONTAINER_HOME}/.codex/auth.json"
cmp -s "${HOST_HOME}/.codex/config.toml" "${CONTAINER_HOME}/.codex/config.toml"

test -f "${STAGING_ROOT}/.fqcompressor-host-claude/CLAUDE.md"
