#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
RESULT_JSON="${PROJECT_ROOT}/benchmark/results/err091571-local-supported.json"
RESULT_MD="${PROJECT_ROOT}/benchmark/results/err091571-local-supported.md"
TRACKED_INPUT="${PROJECT_ROOT}/benchmark/data/ERR091571/ERR091571_1.2000.fastq"
READ1_SUBSET="${PROJECT_ROOT}/benchmark/data/ERR091571/ERR091571_1.subset.fastq.gz"
READ2_SUBSET="${PROJECT_ROOT}/benchmark/data/ERR091571/ERR091571_2.subset.fastq.gz"
TMP_DIR="$(mktemp -d)"
FQC_SMOKE_BINARY="${FQC_SMOKE_BINARY:-${PROJECT_ROOT}/build/clang-debug/src/fqc}"
HAD_TRACKED_INPUT=0
HAD_READ1_SUBSET=0
HAD_READ2_SUBSET=0

cleanup() {
    for artifact in "${RESULT_JSON}" "${RESULT_MD}"; do
        backup="${TMP_DIR}/$(basename "${artifact}").bak"
        if [[ -f "${backup}" ]]; then
            cp "${backup}" "${artifact}"
        else
            rm -f "${artifact}"
        fi
    done

    if [[ "${HAD_TRACKED_INPUT}" -eq 0 ]]; then
        rm -f "${TRACKED_INPUT}"
    fi
    if [[ "${HAD_READ1_SUBSET}" -eq 0 ]]; then
        rm -f "${READ1_SUBSET}"
    fi
    if [[ "${HAD_READ2_SUBSET}" -eq 0 ]]; then
        rm -f "${READ2_SUBSET}"
    fi

    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

if [[ ! -x "${FQC_SMOKE_BINARY}" ]]; then
    echo "Missing fqc binary: ${FQC_SMOKE_BINARY}" >&2
    exit 1
fi

[[ -f "${TRACKED_INPUT}" ]] && HAD_TRACKED_INPUT=1
[[ -f "${READ1_SUBSET}" ]] && HAD_READ1_SUBSET=1
[[ -f "${READ2_SUBSET}" ]] && HAD_READ2_SUBSET=1

for artifact in "${RESULT_JSON}" "${RESULT_MD}"; do
    if [[ -f "${artifact}" ]]; then
        cp "${artifact}" "${TMP_DIR}/$(basename "${artifact}").bak"
    fi
done

export FQC_BENCHMARK_BINARY="${FQC_SMOKE_BINARY}"

"${PROJECT_ROOT}/scripts/benchmark.sh" \
    --dataset err091571-local-supported \
    --quick \
    --tools fqc,gzip,xz,bzip2 \
    --threads 1

python3 - "${RESULT_JSON}" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text(encoding="utf-8"))

measured = set(data.get("measured_tools") or data.get("tools_tested") or [])
if "fqc" not in measured:
    raise SystemExit("benchmark smoke did not measure fqc")

results = [item for item in data.get("results", []) if item.get("tool") == "fqc"]
if not results:
    raise SystemExit("benchmark smoke did not record fqc results")

operations = {item.get("operation") for item in results if item.get("success")}
if "compress" not in operations or "decompress" not in operations:
    raise SystemExit("benchmark smoke did not record successful fqc compress+decompress results")

if any(not item.get("success") for item in results):
    raise SystemExit("benchmark smoke recorded failed fqc result")
PY
