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

python - <<'PY' "${TEST_ROOT}/.devcontainer/devcontainer.clion.json"
import pathlib
import re
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text()
updated = re.sub(
    r'"127\.0\.0\.1:2222:2222"',
    '"127.0.0.1:2200:2200"',
    text,
    count=1,
)

if updated == text:
    raise SystemExit("Failed to rewrite CLion SSH port mapping")

path.write_text(updated)
PY

set +e
output="$("${TEST_ROOT}/scripts/devcontainer-validate.sh" 2>&1)"
status=$?
set -e

if [[ ${status} -eq 0 ]]; then
    echo "Expected devcontainer validation to fail for a CLion adapter drift." >&2
    exit 1
fi

grep -Fq "devcontainer.clion.json" <<<"${output}" || {
    echo "Expected failure output to mention the CLion adapter." >&2
    echo "${output}" >&2
    exit 1
}
