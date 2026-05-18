#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

required_files=(
    "${ROOT}/.devcontainer/devcontainer.json"
    "${ROOT}/docker/docker-compose.yml"
)

required_scripts=(
    "${ROOT}/.devcontainer/scripts/host-prepare.sh"
    "${ROOT}/.devcontainer/scripts/container-setup.sh"
)

required_executables=(
    "${ROOT}/.devcontainer/scripts/setup-sshd.sh"
    "${ROOT}/.devcontainer/scripts/start-sshd.sh"
)

for required in "${required_files[@]}"; do
    if [[ ! -f "${required}" ]]; then
        echo "[ERROR] Missing required devcontainer file: ${required}" >&2
        exit 1
    fi
done

for required in "${required_scripts[@]}"; do
    if [[ ! -f "${required}" ]]; then
        echo "[ERROR] Missing required devcontainer script: ${required}" >&2
        exit 1
    fi
done

for required in "${required_executables[@]}"; do
    if [[ ! -x "${required}" ]]; then
        echo "[ERROR] Required devcontainer script is not executable: ${required}" >&2
        exit 1
    fi
done

for optional_mount in /tmp/host-gitconfig /tmp/host-claude /tmp/host-codex; do
    if [[ ! -e "${optional_mount}" ]]; then
        echo "[INFO] Optional host mount not present: ${optional_mount}" >&2
    fi
done

if ! command -v docker >/dev/null 2>&1; then
    echo "[WARN] Docker not found; skipping devcontainer CLI validation." >&2
    exit 0
fi

if ! command -v npx >/dev/null 2>&1; then
    echo "[WARN] npx not found; skipping devcontainer CLI validation." >&2
    exit 0
fi

if ! npx -y @devcontainers/cli read-configuration --workspace-folder "${ROOT}" >/dev/null 2>&1; then
    echo "[ERROR] Devcontainer CLI could not read configuration." >&2
    exit 1
fi
