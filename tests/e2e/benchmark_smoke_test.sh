#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
RESULT_JSON="${PROJECT_ROOT}/benchmark/results/err091571-local-supported.json"
RESULT_MD="${PROJECT_ROOT}/benchmark/results/err091571-local-supported.md"
TMP_DIR="$(mktemp -d)"
FQC_SMOKE_BINARY="${FQC_SMOKE_BINARY:-${PROJECT_ROOT}/build/clang-debug/src/fqc}"

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

for artifact in "${RESULT_JSON}" "${RESULT_MD}"; do
    if [[ -f "${artifact}" ]]; then
        cp "${artifact}" "${TMP_DIR}/$(basename "${artifact}").bak"
    fi
done

trap cleanup EXIT

if [[ ! -x "${FQC_SMOKE_BINARY}" ]]; then
    echo "Missing fqc binary: ${FQC_SMOKE_BINARY}" >&2
    exit 1
fi

cat > "${TMP_DIR}/smoke.fastq" <<'EOF'
@read_1
ACGTACGT
+
FFFFFFFF
@read_2
TGCATGCA
+
HHHHHHHH
@read_3
GGGGAAAA
+
IIIIIIII
EOF

export FQC_BENCHMARK_BINARY="${FQC_SMOKE_BINARY}"

"${PROJECT_ROOT}/scripts/benchmark.sh" \
    --input "${TMP_DIR}/smoke.fastq" \
    --quick \
    --tools fqc,gzip,xz,bzip2 \
    --threads 1

python3 - "${RESULT_JSON}" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text(encoding="utf-8"))

if data.get("kind") != "benchmark_report_v2":
    raise SystemExit("benchmark smoke did not produce canonical benchmark_v2 report output")

suite = data.get("suite")
if not isinstance(suite, dict):
    raise SystemExit("benchmark smoke did not emit report suite metadata")

benchmark_command = data.get("benchmark_command")
if not isinstance(benchmark_command, dict) or benchmark_command.get("command_path") != "scripts/benchmark.sh":
    raise SystemExit("benchmark smoke did not track wrapper command metadata")

tool_metrics = data.get("tool_metrics")
if not isinstance(tool_metrics, list) or not tool_metrics:
    raise SystemExit("benchmark smoke did not emit aggregated tool metrics")

measured = set(data.get("measured_tools") or [])
if "fqc" not in measured:
    raise SystemExit("benchmark smoke did not measure fqc")

results = [item for item in suite.get("results", []) if item.get("tool_id") == "fqc"]
if not results:
    raise SystemExit("benchmark smoke did not record fqc results")

operations = {item.get("operation") for item in results if item.get("success")}
if "compress" not in operations or "decompress" not in operations:
    raise SystemExit("benchmark smoke did not record successful fqc compress+decompress results")

if any(not item.get("success") for item in results):
    raise SystemExit("benchmark smoke recorded failed fqc result")
PY
