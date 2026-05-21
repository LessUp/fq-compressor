#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEST_DIR="$(mktemp -d)"
trap 'rm -rf "${TEST_DIR}"' EXIT

python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" --list-workloads > "${TEST_DIR}/workloads.txt"
grep -q '^small20k-single$' "${TEST_DIR}/workloads.txt"
grep -q '^big100k-paired$' "${TEST_DIR}/workloads.txt"
