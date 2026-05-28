#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT}/.devcontainer/scripts/lib/devcontainer_contract.sh"

check_required_files() {
    while IFS= read -r relative; do
        local required="${ROOT}/${relative}"
        if [[ ! -f "${required}" ]]; then
            echo "[ERROR] Missing required devcontainer file: ${required}" >&2
            exit 1
        fi
    done < <(devcontainer_required_files)
}

check_required_scripts() {
    while IFS= read -r relative; do
        local required="${ROOT}/${relative}"
        if [[ ! -f "${required}" ]]; then
            echo "[ERROR] Missing required devcontainer script: ${required}" >&2
            exit 1
        fi
    done < <(devcontainer_required_scripts)
}

check_required_executables() {
    while IFS= read -r relative; do
        local required="${ROOT}/${relative}"
        if [[ ! -x "${required}" ]]; then
            echo "[ERROR] Required devcontainer script is not executable: ${required}" >&2
            exit 1
        fi
    done < <(devcontainer_required_executables)
}

jsonc_string_property() {
    local file="$1"
    local property="$2"

    python - "$file" "$property" <<'PY'
import pathlib
import re
import sys

text = pathlib.Path(sys.argv[1]).read_text()
property_name = re.escape(sys.argv[2])
match = re.search(r'"%s"\s*:\s*"((?:\\.|[^"])*)"' % property_name, text)
if not match:
    raise SystemExit(1)
print(match.group(1))
PY
}

assert_jsonc_property() {
    local file="$1"
    local property="$2"
    local expected="$3"
    local actual

    if ! actual="$(jsonc_string_property "$file" "$property")"; then
        echo "[ERROR] Missing ${property} in ${file}" >&2
        exit 1
    fi

    if [[ "${actual}" != "${expected}" ]]; then
        echo "[ERROR] ${property} mismatch in ${file}" >&2
        echo "        expected: ${expected}" >&2
        echo "          actual: ${actual}" >&2
        exit 1
    fi
}

assert_contains() {
    local file="$1"
    local expected="$2"

    if ! grep -Fq -- "${expected}" "${file}"; then
        echo "[ERROR] Missing devcontainer contract entry in ${file}" >&2
        echo "        expected fragment: ${expected}" >&2
        exit 1
    fi
}

check_semantic_contract() {
    local vscode_config="${ROOT}/.devcontainer/devcontainer.json"
    local clion_config="${ROOT}/.devcontainer/devcontainer.clion.json"
    local compose_file="${ROOT}/docker/docker-compose.yml"

    assert_jsonc_property "${vscode_config}" "workspaceFolder" "${FQC_DEVCONTAINER_WORKSPACE}"
    assert_jsonc_property "${vscode_config}" "initializeCommand" "${FQC_DEVCONTAINER_INIT_COMMAND}"
    assert_jsonc_property "${vscode_config}" "service" "${FQC_DEVCONTAINER_SERVICE}"
    assert_jsonc_property "${vscode_config}" "postCreateCommand" "${FQC_DEVCONTAINER_POST_CREATE}"
    assert_jsonc_property "${vscode_config}" "postStartCommand" "${FQC_DEVCONTAINER_POST_START}"
    assert_jsonc_property "${vscode_config}" "postAttachCommand" "${FQC_DEVCONTAINER_POST_ATTACH}"
    assert_jsonc_property "${vscode_config}" "WORKSPACE" "${FQC_DEVCONTAINER_WORKSPACE}"
    assert_jsonc_property "${vscode_config}" "WSL_DISTRO_NAME" "\${localEnv:WSL_DISTRO_NAME}"
    assert_jsonc_property "${vscode_config}" "MSYSTEM" "\${localEnv:MSYSTEM}"
    assert_contains "${vscode_config}" "\"dockerComposeFile\": ["
    assert_contains "${vscode_config}" "\"../docker/docker-compose.yml\""
    assert_contains "${vscode_config}" "${FQC_DEVCONTAINER_GITCONFIG_MOUNT}"
    assert_contains "${vscode_config}" "${FQC_DEVCONTAINER_CLAUDE_MOUNT}"
    assert_contains "${vscode_config}" "${FQC_DEVCONTAINER_CODEX_MOUNT}"

    assert_jsonc_property "${clion_config}" "workspaceFolder" "${FQC_DEVCONTAINER_WORKSPACE}"
    assert_jsonc_property "${clion_config}" "remoteUser" "${FQC_DEVCONTAINER_REMOTE_USER}"
    assert_jsonc_property "${clion_config}" "postCreateCommand" "${FQC_DEVCONTAINER_CLION_POST_CREATE}"
    assert_jsonc_property "${clion_config}" "postStartCommand" "${FQC_DEVCONTAINER_CLION_POST_START}"
    assert_jsonc_property "${clion_config}" "postAttachCommand" "${FQC_DEVCONTAINER_CLION_POST_ATTACH}"
    assert_jsonc_property "${clion_config}" "WORKSPACE" "${FQC_DEVCONTAINER_WORKSPACE}"
    assert_jsonc_property "${clion_config}" "WSL_DISTRO_NAME" "\${localEnv:WSL_DISTRO_NAME}"
    assert_jsonc_property "${clion_config}" "MSYSTEM" "\${localEnv:MSYSTEM}"
    assert_contains "${clion_config}" "\"${FQC_DEVCONTAINER_CLION_SSH_PORT_MAP}\""
    assert_contains "${clion_config}" "\"${FQC_DEVCONTAINER_CLION_CONAN_VOLUME}\""
    assert_contains "${clion_config}" "\"${FQC_DEVCONTAINER_CLION_CCACHE_VOLUME}\""

    assert_contains "${compose_file}" "- ..:${FQC_DEVCONTAINER_WORKSPACE}:cached"
    assert_contains "${compose_file}" '- "${FQCOMPRESSOR_SSH_BIND:-127.0.0.1}:${FQCOMPRESSOR_SSH_PORT:-2222}:${FQCOMPRESSOR_SSH_PORT:-2222}"'
    assert_contains "${compose_file}" "- FQCOMPRESSOR_SSH_PORT=\${FQCOMPRESSOR_SSH_PORT:-2222}"
    assert_contains "${compose_file}" "- \${FQCOMPRESSOR_HOST_DATA_DIR:-/tmp/fqcompressor-data}:/data:cached"
}

check_required_files
check_required_scripts
check_required_executables
check_semantic_contract

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
