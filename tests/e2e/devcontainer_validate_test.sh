#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
TEST_ROOT="$(mktemp -d)"

cleanup() {
    rm -rf "${TEST_ROOT}"
}
trap cleanup EXIT

mkdir -p "${TEST_ROOT}/.devcontainer" "${TEST_ROOT}/docker" "${TEST_ROOT}/scripts"
cp -R "${PROJECT_ROOT}/.devcontainer/." "${TEST_ROOT}/.devcontainer/"
cp "${PROJECT_ROOT}/docker/docker-compose.yml" "${TEST_ROOT}/docker/docker-compose.yml"
cp "${PROJECT_ROOT}/scripts/devcontainer-validate.sh" "${TEST_ROOT}/scripts/devcontainer-validate.sh"

python - <<'PY' "${TEST_ROOT}/.devcontainer/devcontainer.json"
import pathlib
import re
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text()
updated = re.sub(
    r'"postCreateCommand":\s*"[^"]+"',
    '"postCreateCommand": "bash /workspace/.devcontainer/scripts/not-the-contract.sh create"',
    text,
    count=1,
)

if updated == text:
    raise SystemExit("Failed to rewrite postCreateCommand in devcontainer.json")

path.write_text(updated)
PY

set +e
output="$("${TEST_ROOT}/scripts/devcontainer-validate.sh" 2>&1)"
status=$?
set -e

if [[ ${status} -eq 0 ]]; then
    echo "Expected devcontainer validation to fail for a lifecycle command mismatch." >&2
    exit 1
fi

grep -Fq "postCreateCommand" <<<"${output}" || {
    echo "Expected failure output to mention the mismatched lifecycle command." >&2
    echo "${output}" >&2
    exit 1
}
