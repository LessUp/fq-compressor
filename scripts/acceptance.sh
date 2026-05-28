#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"${ROOT}/scripts/lint.sh" format-check
"${ROOT}/scripts/test.sh" clang-debug
bash "${ROOT}/tests/e2e/cli_smoke_test.sh"
bash "${ROOT}/tests/e2e/benchmark_v2_smoke_test.sh"
bash "${ROOT}/tests/e2e/devcontainer_validate_test.sh"
bash "${ROOT}/tests/e2e/devcontainer_host_sync_test.sh"
bash "${ROOT}/tests/e2e/devcontainer_sshd_lib_test.sh"
bash "${ROOT}/tests/e2e/devcontainer_adapter_contract_test.sh"
(
    cd "${ROOT}/docs"
    npm ci --quiet
    npm run build
)
bash "${ROOT}/scripts/devcontainer-validate.sh"
