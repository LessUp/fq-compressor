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
@read_1 instrument comment
ACGTRYSW
+
FFFFFFFF
@read_2
TGCANacg
+
HHHHHHHH
@read_3
GGGGAAAA
+
IIIIIIII
EOF

"${FQC_BIN}" -q compress -i "${TEST_DIR}/sample.fastq" -o "${TEST_DIR}/sample.fqc"
"${FQC_BIN}" -q compress -i - -o "${TEST_DIR}/stdin.fqc" \
    < "${TEST_DIR}/sample.fastq"
gzip -c "${TEST_DIR}/sample.fastq" | \
    "${FQC_BIN}" -q compress -i - -o "${TEST_DIR}/gzip-stdin.fqc"
"${FQC_BIN}" -q verify "${TEST_DIR}/sample.fqc"
"${FQC_BIN}" -q decompress -i "${TEST_DIR}/sample.fqc" -o "${TEST_DIR}/restored.fastq"
"${FQC_BIN}" -q decompress -i "${TEST_DIR}/stdin.fqc" -o "${TEST_DIR}/stdin-restored.fastq"
"${FQC_BIN}" -q decompress -i "${TEST_DIR}/gzip-stdin.fqc" \
    -o "${TEST_DIR}/gzip-stdin-restored.fastq"
"${FQC_BIN}" -q decompress -i "${TEST_DIR}/sample.fqc" -o - \
    > "${TEST_DIR}/stdout-restored.fastq"

cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/restored.fastq"
cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/stdin-restored.fastq"
cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/gzip-stdin-restored.fastq"
cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/stdout-restored.fastq"

cat > "${TEST_DIR}/mate_R1.fastq" <<'EOF'
@pair_1/1
ACGT
+
IIII
@pair_2/1
TGCA
+
IIII
EOF

cat > "${TEST_DIR}/mate_R2.fastq" <<'EOF'
@pair_1/2
TGCA
+
IIII
EOF

if "${FQC_BIN}" -q compress \
    -i "${TEST_DIR}/mate_R1.fastq" \
    -2 "${TEST_DIR}/mate_R2.fastq" \
    -o "${TEST_DIR}/incomplete.fqc"; then
    echo "expected mismatched paired input to fail" >&2
    exit 1
fi
test ! -e "${TEST_DIR}/incomplete.fqc"
