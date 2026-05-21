#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEST_DIR="${PROJECT_ROOT}/.benchmark_v2_smoke_test.$$"
mkdir -p "${TEST_DIR}"
trap 'rm -rf "${TEST_DIR}"' EXIT

python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" --list-workloads > "${TEST_DIR}/workloads.txt"
grep -q '^small20k-single$' "${TEST_DIR}/workloads.txt"
grep -q '^small20k-paired$' "${TEST_DIR}/workloads.txt"
grep -q '^big100k-single$' "${TEST_DIR}/workloads.txt"
grep -q '^big100k-paired$' "${TEST_DIR}/workloads.txt"

python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" --list-tools > "${TEST_DIR}/tools.txt"
grep -q '^fqc$' "${TEST_DIR}/tools.txt"
grep -q '^gzip$' "${TEST_DIR}/tools.txt"
grep -q '^xz$' "${TEST_DIR}/tools.txt"
grep -q '^bzip2$' "${TEST_DIR}/tools.txt"
