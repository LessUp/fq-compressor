#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
RESULT_JSON="${PROJECT_ROOT}/benchmark_v2/results/err091571-local-supported.json"
RESULT_MD="${PROJECT_ROOT}/benchmark_v2/results/err091571-local-supported.md"
TMP_DIR="$(mktemp -d)"
MISSING_BINARY="${TMP_DIR}/missing-fqc"

cleanup() {
    for artifact in "${RESULT_JSON}" "${RESULT_MD}"; do
        backup="${TMP_DIR}/$(basename "${artifact}").bak"
        if [[ -f "${backup}" ]]; then
            cp "${backup}" "${artifact}"
        else
            rm -f "${artifact}"
        fi
    done

    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

mkdir -p "$(dirname "${RESULT_JSON}")"

for artifact in "${RESULT_JSON}" "${RESULT_MD}"; do
    if [[ -f "${artifact}" ]]; then
        cp "${artifact}" "${TMP_DIR}/$(basename "${artifact}").bak"
    fi
done

printf '{"sentinel":true}\n' > "${RESULT_JSON}"
printf '# sentinel\n' > "${RESULT_MD}"

if FQC_SMOKE_BINARY="${MISSING_BINARY}" bash "${SCRIPT_DIR}/benchmark_smoke_test.sh"; then
    echo "benchmark smoke unexpectedly succeeded with missing binary" >&2
    exit 1
fi

grep -q '"sentinel":true' "${RESULT_JSON}"
grep -q '^# sentinel$' "${RESULT_MD}"
