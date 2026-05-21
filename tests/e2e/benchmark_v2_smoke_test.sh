#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEST_DIR="$(mktemp -d)"
trap 'rm -rf "${TEST_DIR}"' EXIT

assert_sorted_equals() {
    local actual_path="$1"
    local expected_path="$2"
    sort "${actual_path}" > "${actual_path}.sorted"
    sort "${expected_path}" > "${expected_path}.sorted"
    diff -u "${expected_path}.sorted" "${actual_path}.sorted"
}

assert_manifest_error() {
    local case_name="$1"
    local loader_name="$2"
    local workloads_content="$3"
    local tools_content="$4"
    local expected_fragment="$5"
    local case_dir="${TEST_DIR}/${case_name}"
    mkdir -p "${case_dir}"
    printf '%s\n' "${workloads_content}" > "${case_dir}/workloads.yaml"
    printf '%s\n' "${tools_content}" > "${case_dir}/tools.yaml"

    MANIFEST_DIR="${case_dir}" \
        LOADER_NAME="${loader_name}" \
        EXPECTED_FRAGMENT="${expected_fragment}" \
        PROJECT_ROOT="${PROJECT_ROOT}" \
        python3 <<'PY'
import os
import sys
from pathlib import Path

sys.path.insert(0, os.environ["PROJECT_ROOT"])

from benchmark_v2 import manifest

manifest._MANIFEST_DIR = Path(os.environ["MANIFEST_DIR"])
loader = getattr(manifest, os.environ["LOADER_NAME"])
expected_fragment = os.environ["EXPECTED_FRAGMENT"]

try:
    loader()
except ValueError as exc:
    message = str(exc)
    if expected_fragment not in message:
        raise AssertionError(
            f"expected {expected_fragment!r} in ValueError message {message!r}"
        ) from exc
else:
    raise AssertionError("expected ValueError")
PY
}

assert_python_validation_error() {
    local expected_fragment="$1"
    local python_snippet="$2"

    EXPECTED_FRAGMENT="${expected_fragment}" \
        PYTHON_SNIPPET="${python_snippet}" \
        PROJECT_ROOT="${PROJECT_ROOT}" \
        python3 <<'PY'
import os
import sys

sys.path.insert(0, os.environ["PROJECT_ROOT"])

expected_fragment = os.environ["EXPECTED_FRAGMENT"]
python_snippet = os.environ["PYTHON_SNIPPET"]

try:
    exec(python_snippet, {"__name__": "__main__"})
except ValueError as exc:
    message = str(exc)
    if expected_fragment not in message:
        raise AssertionError(
            f"expected {expected_fragment!r} in ValueError message {message!r}"
        ) from exc
else:
    raise AssertionError("expected ValueError")
PY
}

python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" --list-workloads > "${TEST_DIR}/workloads.txt"
cat <<'EOF' > "${TEST_DIR}/expected_workloads.txt"
small20k-single
small20k-paired
big100k-single
big100k-paired
EOF
assert_sorted_equals "${TEST_DIR}/workloads.txt" "${TEST_DIR}/expected_workloads.txt"

python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" --list-tools > "${TEST_DIR}/tools.txt"
cat <<'EOF' > "${TEST_DIR}/expected_tools.txt"
fqc
gzip
xz
bzip2
EOF
assert_sorted_equals "${TEST_DIR}/tools.txt" "${TEST_DIR}/expected_tools.txt"

assert_manifest_error \
    top_level_list \
    load_workloads \
    $'- just\n- a\n- list' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml must contain a top-level mapping"

assert_manifest_error \
    missing_workload_id \
    load_workloads \
    $'workloads:\n  - layout: single\n    inputs: ["small/test_1.fq.gz"]\n    read_limit: 20000\n    tier: dev\n    comparable_tools: [fqc]' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml entry #1 missing required key 'workload_id'"

assert_manifest_error \
    invalid_workload_layout \
    load_workloads \
    $'workloads:\n  - workload_id: broken-layout\n    layout: triplet\n    inputs: ["small/test_1.fq.gz"]\n    read_limit: 20000\n    tier: dev\n    comparable_tools: [fqc]' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml workload 'broken-layout' has invalid layout 'triplet'"

assert_manifest_error \
    paired_workload_wrong_inputs \
    load_workloads \
    $'workloads:\n  - workload_id: broken-paired\n    layout: paired\n    inputs: ["small/test_1.fq.gz"]\n    read_limit: 20000\n    tier: dev\n    comparable_tools: [fqc]' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml workload 'broken-paired' with layout 'paired' must define exactly 2 inputs"

assert_manifest_error \
    paired_workload_duplicate_inputs \
    load_workloads \
    $'workloads:\n  - workload_id: repeated-paired-input\n    layout: paired\n    inputs: ["small/test_1.fq.gz", "small/test_1.fq.gz"]\n    read_limit: 20000\n    tier: dev\n    comparable_tools: [fqc]' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml workload 'repeated-paired-input' must define two distinct inputs"

assert_manifest_error \
    duplicate_workload_id \
    load_workloads \
    $'workloads:\n  - workload_id: repeated\n    layout: single\n    inputs: ["small/test_1.fq.gz"]\n    read_limit: 20000\n    tier: dev\n    comparable_tools: [fqc]\n  - workload_id: repeated\n    layout: single\n    inputs: ["small/test_1.fq.gz"]\n    read_limit: 20000\n    tier: dev\n    comparable_tools: [fqc]' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml has duplicate workload_id 'repeated'"

assert_manifest_error \
    non_positive_read_limit \
    load_workloads \
    $'workloads:\n  - workload_id: zero-limit\n    layout: single\n    inputs: ["small/test_1.fq.gz"]\n    read_limit: 0\n    tier: dev\n    comparable_tools: [fqc]' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml workload 'zero-limit' read_limit must be positive"

assert_manifest_error \
    empty_comparable_tools \
    load_workloads \
    $'workloads:\n  - workload_id: no-tools\n    layout: single\n    inputs: ["small/test_1.fq.gz"]\n    read_limit: 20000\n    tier: dev\n    comparable_tools: []' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml workload 'no-tools' must define at least one comparable_tools entry"

assert_manifest_error \
    duplicate_comparable_tools \
    load_workloads \
    $'workloads:\n  - workload_id: dupe-tools\n    layout: single\n    inputs: ["small/test_1.fq.gz"]\n    read_limit: 20000\n    tier: dev\n    comparable_tools: [fqc, gzip, fqc]' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml workload 'dupe-tools' has duplicate comparable_tools ID 'fqc'"

assert_manifest_error \
    unknown_comparable_tool \
    load_workloads \
    $'workloads:\n  - workload_id: unknown-tool\n    layout: single\n    inputs: ["small/test_1.fq.gz"]\n    read_limit: 20000\n    tier: dev\n    comparable_tools: [fqc, nope]' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml workload 'unknown-tool' references unknown comparable_tools ID 'nope'"

assert_manifest_error \
    paired_workload_unsupported_tool \
    load_workloads \
    $'workloads:\n  - workload_id: paired-gzip\n    layout: paired\n    inputs: ["small/test_1.fq.gz", "small/test_2.fq.gz"]\n    read_limit: 20000\n    tier: dev\n    comparable_tools: [fqc, gzip]' \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/tools.yaml")" \
    "workloads.yaml workload 'paired-gzip' references tool 'gzip' that does not support paired workloads"

assert_manifest_error \
    missing_tool_id \
    load_tools \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/workloads.yaml")" \
    $'tools:\n  - category: baseline\n    supports_paired: false\n    compress_template: gzip -c {input} > {output}\n    decompress_template: gzip -dc {input} > {output}' \
    "tools.yaml entry #1 missing required key 'tool_id'"

assert_manifest_error \
    duplicate_tool_id \
    load_tools \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/workloads.yaml")" \
    $'tools:\n  - tool_id: gzip\n    category: baseline\n    supports_paired: false\n    compress_template: gzip -c {input} > {output}\n    decompress_template: gzip -dc {input} > {output}\n  - tool_id: gzip\n    category: baseline\n    supports_paired: false\n    compress_template: gzip -c {input} > {output}\n    decompress_template: gzip -dc {input} > {output}' \
    "tools.yaml has duplicate tool_id 'gzip'"

assert_manifest_error \
    missing_compress_placeholder \
    load_tools \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/workloads.yaml")" \
    $'tools:\n  - tool_id: gzip\n    category: baseline\n    supports_paired: false\n    compress_template: gzip -c {input}\n    decompress_template: gzip -dc {input} > {output}' \
    "tools.yaml tool 'gzip' compress_template missing required placeholders: output"

assert_manifest_error \
    unknown_decompress_placeholder \
    load_tools \
    "$(cat "${PROJECT_ROOT}/benchmark_v2/manifests/workloads.yaml")" \
    $'tools:\n  - tool_id: gzip\n    category: baseline\n    supports_paired: false\n    compress_template: gzip -c {input} > {output}\n    decompress_template: gzip -dc {input} > {output} {mystery}' \
    "tools.yaml tool 'gzip' decompress_template has unknown placeholders: mystery"

assert_python_validation_error \
    "workload_id must be a non-empty string" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="",\n    layout="single",\n    inputs=("small/test_1.fq.gz",),\n    read_limit=1,\n    tier="dev",\n    comparable_tools=("fqc",),\n)'

assert_python_validation_error \
    "layout must be one of" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-single",\n    layout="triplet",\n    inputs=("small/test_1.fq.gz",),\n    read_limit=1,\n    tier="dev",\n    comparable_tools=("fqc",),\n)'

assert_python_validation_error \
    "read_limit must be positive" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-single",\n    layout="single",\n    inputs=("small/test_1.fq.gz",),\n    read_limit=0,\n    tier="dev",\n    comparable_tools=("fqc",),\n)'

assert_python_validation_error \
    "single workloads must define exactly 1 input" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-single",\n    layout="single",\n    inputs=("small/test_1.fq.gz", "small/test_2.fq.gz"),\n    read_limit=1,\n    tier="dev",\n    comparable_tools=("fqc",),\n)'

assert_python_validation_error \
    "paired workloads must define exactly 2 distinct inputs" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-paired",\n    layout="paired",\n    inputs=("small/test_1.fq.gz", "small/test_1.fq.gz"),\n    read_limit=1,\n    tier="dev",\n    comparable_tools=("fqc",),\n)'

assert_python_validation_error \
    "comparable_tools must contain at least one tool ID" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-single",\n    layout="single",\n    inputs=("small/test_1.fq.gz",),\n    read_limit=1,\n    tier="dev",\n    comparable_tools=(),\n)'

assert_python_validation_error \
    "tool_id must be a non-empty string" \
    $'from benchmark_v2.models import ToolSpec\n\nToolSpec(\n    tool_id="",\n    category="baseline",\n    supports_paired=False,\n    compress_template="gzip -c {input} > {output}",\n    decompress_template="gzip -dc {input} > {output}",\n)'

assert_python_validation_error \
    "compress_template must be a non-empty string" \
    $'from benchmark_v2.models import ToolSpec\n\nToolSpec(\n    tool_id="gzip",\n    category="baseline",\n    supports_paired=False,\n    compress_template="",\n    decompress_template="gzip -dc {input} > {output}",\n)'

assert_python_validation_error \
    "operation must be one of" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="invalid",\n    threads=1,\n    input_bytes=1,\n    output_bytes=1,\n    elapsed_seconds=0.1,\n    success=True,\n)'

assert_python_validation_error \
    "threads must be positive" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=0,\n    input_bytes=1,\n    output_bytes=1,\n    elapsed_seconds=0.1,\n    success=True,\n)'

assert_python_validation_error \
    "tool_id must be a non-empty string" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="",\n    layout="single",\n    operation="compress",\n    threads=1,\n    input_bytes=1,\n    output_bytes=1,\n    elapsed_seconds=0.1,\n    success=True,\n)'

assert_python_validation_error \
    "input_bytes must be non-negative" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=1,\n    input_bytes=-1,\n    output_bytes=1,\n    elapsed_seconds=0.1,\n    success=True,\n)'

assert_python_validation_error \
    "output_bytes must be non-negative" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=1,\n    input_bytes=1,\n    output_bytes=-1,\n    elapsed_seconds=0.1,\n    success=True,\n)'

assert_python_validation_error \
    "elapsed_seconds must be positive for successful benchmark results" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=1,\n    input_bytes=1,\n    output_bytes=1,\n    elapsed_seconds=0.0,\n    success=True,\n)'

assert_python_validation_error \
    "elapsed_seconds must be non-negative" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=1,\n    input_bytes=1,\n    output_bytes=1,\n    elapsed_seconds=-0.1,\n    success=False,\n)'

assert_python_validation_error \
    "suite layout" \
    $'from benchmark_v2.models import BenchmarkResult, BenchmarkSuite\n\nBenchmarkSuite(\n    workload_id="small20k-single",\n    layout="single",\n    results=(\n        BenchmarkResult(\n            tool_id="fqc",\n            layout="paired",\n            operation="compress",\n            threads=1,\n            input_bytes=1,\n            output_bytes=1,\n            elapsed_seconds=0.1,\n            success=True,\n        ),\n    ),\n)'

assert_python_validation_error \
    "workload_id must be a non-empty string" \
    $'from benchmark_v2.models import BenchmarkSuite\n\nBenchmarkSuite(\n    workload_id="",\n    layout="single",\n    results=(),\n)'
