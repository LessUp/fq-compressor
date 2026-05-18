#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FQC_BIN="${FQC_BIN:-${FQC_SMOKE_BINARY:-${PROJECT_ROOT}/build/clang-debug/src/fqc}}"
TEST_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "${TEST_DIR}"
}
trap cleanup EXIT

if [[ ! -x "${FQC_BIN}" ]]; then
    echo "Missing fqc binary: ${FQC_BIN}" >&2
    exit 1
fi

cat > "${TEST_DIR}/sample.fastq" <<'EOF'
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

"${FQC_BIN}" compress -i "${TEST_DIR}/sample.fastq" -o "${TEST_DIR}/sample.fqc" -q
"${FQC_BIN}" -q --no-progress compress -i - -o "${TEST_DIR}/stdin.fqc" \
    < "${TEST_DIR}/sample.fastq"
"${FQC_BIN}" info "${TEST_DIR}/sample.fqc" > "${TEST_DIR}/info.txt"
"${FQC_BIN}" verify "${TEST_DIR}/sample.fqc" > "${TEST_DIR}/verify.txt"
"${FQC_BIN}" decompress -i "${TEST_DIR}/sample.fqc" -o "${TEST_DIR}/restored.fastq" -q
"${FQC_BIN}" decompress -i "${TEST_DIR}/stdin.fqc" -o "${TEST_DIR}/stdin-restored.fastq" -q

cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/restored.fastq"
cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/stdin-restored.fastq"
grep -q "Total reads" "${TEST_DIR}/info.txt"
grep -q "Status:  OK" "${TEST_DIR}/verify.txt"
