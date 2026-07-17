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

expect_exit() {
    local expected="$1"
    shift

    set +e
    "$@" >/dev/null 2>&1
    local actual=$?
    set -e
    if [[ "${actual}" -ne "${expected}" ]]; then
        echo "expected exit ${expected}, got ${actual}: $*" >&2
        exit 1
    fi
}

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
cat "${TEST_DIR}/sample.fastq" | \
    "${FQC_BIN}" -q compress -i - -o - | \
    "${FQC_BIN}" -q decompress -i - -o - \
    > "${TEST_DIR}/pipeline-restored.fastq"

cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/restored.fastq"
cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/stdin-restored.fastq"
cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/gzip-stdin-restored.fastq"
cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/stdout-restored.fastq"
cmp -s "${TEST_DIR}/sample.fastq" "${TEST_DIR}/pipeline-restored.fastq"

# Empty FASTQ produces a valid zero-frame archive and restores to an empty file.
: > "${TEST_DIR}/empty.fastq"
"${FQC_BIN}" -q compress -i "${TEST_DIR}/empty.fastq" -o "${TEST_DIR}/empty.fqc"
"${FQC_BIN}" -q verify "${TEST_DIR}/empty.fqc"
"${FQC_BIN}" -q decompress -i "${TEST_DIR}/empty.fqc" -o "${TEST_DIR}/empty-restored.fastq"
cmp -s "${TEST_DIR}/empty.fastq" "${TEST_DIR}/empty-restored.fastq"

# Concatenated gzip members are accepted when the gzip stream arrives on stdin.
cat > "${TEST_DIR}/member-1.fastq" <<'EOF'
@member_1
ACGT
+
!!!!
EOF
cat > "${TEST_DIR}/member-2.fastq" <<'EOF'
@member_2
Nacg
+
~~~~
EOF
gzip -c "${TEST_DIR}/member-1.fastq" > "${TEST_DIR}/member-1.fastq.gz"
gzip -c "${TEST_DIR}/member-2.fastq" > "${TEST_DIR}/member-2.fastq.gz"
cat "${TEST_DIR}/member-1.fastq" "${TEST_DIR}/member-2.fastq" \
    > "${TEST_DIR}/members.fastq"
cat "${TEST_DIR}/member-1.fastq.gz" "${TEST_DIR}/member-2.fastq.gz" | \
    "${FQC_BIN}" -q compress -i - -o "${TEST_DIR}/members.fqc"
"${FQC_BIN}" -q decompress -i "${TEST_DIR}/members.fqc" \
    -o "${TEST_DIR}/members-restored.fastq"
cmp -s "${TEST_DIR}/members.fastq" "${TEST_DIR}/members-restored.fastq"

# Process exit categories are stable across usage, I/O, format, checksum, and codec failures.
"${FQC_BIN}" --help >/dev/null
expect_exit 1 "${FQC_BIN}"
expect_exit 2 "${FQC_BIN}" -q verify "${TEST_DIR}/missing.fqc"
: > "${TEST_DIR}/not-an-archive.fqc"
expect_exit 3 "${FQC_BIN}" -q verify "${TEST_DIR}/not-an-archive.fqc"
python3 - "${TEST_DIR}/sample.fqc" "${TEST_DIR}/bad-checksum.fqc" \
    "${TEST_DIR}/bad-codec.fqc" <<'PY'
from pathlib import Path
import sys

source = bytearray(Path(sys.argv[1]).read_bytes())
bad_checksum = bytearray(source)
bad_checksum[24] ^= 0x01
Path(sys.argv[2]).write_bytes(bad_checksum)

bad_codec = bytearray(source)
bad_codec[17] = 0x7F
Path(sys.argv[3]).write_bytes(bad_codec)
PY
expect_exit 4 "${FQC_BIN}" -q verify "${TEST_DIR}/bad-checksum.fqc"
expect_exit 5 "${FQC_BIN}" -q verify "${TEST_DIR}/bad-codec.fqc"

# Existing outputs are protected unless --force is explicit.
printf 'preserve this output\n' > "${TEST_DIR}/protected.fqc"
cp "${TEST_DIR}/protected.fqc" "${TEST_DIR}/protected.before"
expect_exit 2 "${FQC_BIN}" -q compress -i "${TEST_DIR}/sample.fastq" \
    -o "${TEST_DIR}/protected.fqc"
cmp -s "${TEST_DIR}/protected.before" "${TEST_DIR}/protected.fqc"
"${FQC_BIN}" -q compress -i "${TEST_DIR}/sample.fastq" \
    -o "${TEST_DIR}/protected.fqc" --force
"${FQC_BIN}" -q verify "${TEST_DIR}/protected.fqc"

# Input/output identity is rejected before the input can be damaged.
cp "${TEST_DIR}/sample.fastq" "${TEST_DIR}/same-path.fastq"
cp "${TEST_DIR}/same-path.fastq" "${TEST_DIR}/same-path.before"
expect_exit 1 "${FQC_BIN}" -q compress -i "${TEST_DIR}/same-path.fastq" \
    -o "${TEST_DIR}/same-path.fastq" --force
cmp -s "${TEST_DIR}/same-path.before" "${TEST_DIR}/same-path.fastq"

# A failed forced decode preserves the original output and removes its staging file.
printf 'original destination\n' > "${TEST_DIR}/preserved.fastq"
cp "${TEST_DIR}/preserved.fastq" "${TEST_DIR}/preserved.before"
expect_exit 4 "${FQC_BIN}" -q decompress -i "${TEST_DIR}/bad-checksum.fqc" \
    -o "${TEST_DIR}/preserved.fastq" --force
cmp -s "${TEST_DIR}/preserved.before" "${TEST_DIR}/preserved.fastq"
if find "${TEST_DIR}" -maxdepth 1 -name 'preserved.fastq.tmp.*' -print -quit | grep -q .; then
    echo "failed decode left a staging file" >&2
    exit 1
fi

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

expect_exit 3 "${FQC_BIN}" -q compress \
    -i "${TEST_DIR}/mate_R1.fastq" \
    -2 "${TEST_DIR}/mate_R2.fastq" \
    -o "${TEST_DIR}/incomplete.fqc"
test ! -e "${TEST_DIR}/incomplete.fqc"
