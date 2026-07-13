#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEST_DIR="$(mktemp -d)"
trap 'rm -rf "${TEST_DIR}"' EXIT
BENCHMARK_V2_DATA_ROOT="${TEST_DIR}/fixture-data"

assert_sorted_equals() {
    local actual_path="$1"
    local expected_path="$2"
    sort "${actual_path}" > "${actual_path}.sorted"
    sort "${expected_path}" > "${expected_path}.sorted"
    diff -u "${expected_path}.sorted" "${actual_path}.sorted"
}

assert_command_failure_contains() {
    local expected_fragment="$1"
    shift
    local stderr_path="${TEST_DIR}/command-stderr.txt"

    if "$@" > /dev/null 2> "${stderr_path}"; then
        echo "expected command to fail: $*" >&2
        return 1
    fi

    grep -F -- "${expected_fragment}" "${stderr_path}" > /dev/null
}

assert_command_failure_contains_no_traceback() {
    local expected_fragment="$1"
    shift
    local stderr_path="${TEST_DIR}/command-stderr.txt"

    if "$@" > /dev/null 2> "${stderr_path}"; then
        echo "expected command to fail: $*" >&2
        return 1
    fi

    grep -F -- "${expected_fragment}" "${stderr_path}" > /dev/null
    if grep -F -- "Traceback (most recent call last):" "${stderr_path}" > /dev/null; then
        echo "expected normal CLI error without traceback: $*" >&2
        cat "${stderr_path}" >&2
        return 1
    fi
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

assert_python_tuple_normalized() {
    local python_snippet="$1"

    PYTHON_SNIPPET="${python_snippet}" \
        PROJECT_ROOT="${PROJECT_ROOT}" \
        python3 <<'PY'
import os
import sys

sys.path.insert(0, os.environ["PROJECT_ROOT"])

python_snippet = os.environ["PYTHON_SNIPPET"]
exec(python_snippet, {"__name__": "__main__"})
PY
}

create_gzip_fastq() {
    local output_path="$1"
    local read_count="$2"
    local sequence="$3"
    mkdir -p "$(dirname "${output_path}")"

    OUTPUT_PATH="${output_path}" \
        READ_COUNT="${read_count}" \
        SEQUENCE="${sequence}" \
        python3 <<'PY'
import gzip
import os
from pathlib import Path

output_path = Path(os.environ["OUTPUT_PATH"])
read_count = int(os.environ["READ_COUNT"])
sequence = os.environ["SEQUENCE"]
quality = "I" * len(sequence)

with gzip.open(output_path, "wt", encoding="ascii") as fout:
    for index in range(1, read_count + 1):
        fout.write(f"@read{index}\n{sequence}\n+\n{quality}\n")
PY
}

create_gzip_fastq "${BENCHMARK_V2_DATA_ROOT}/small/test_1.fq.gz" 20000 "ACGT"
create_gzip_fastq "${BENCHMARK_V2_DATA_ROOT}/small/test_2.fq.gz" 20000 "TGCA"

cat <<'EOF' > "${TEST_DIR}/expected-workloads.txt"
big100k-paired
big100k-single
small20k-paired
small20k-single
EOF

bash "${PROJECT_ROOT}/scripts/benchmark_v2.sh" --list-workloads > "${TEST_DIR}/actual-workloads.txt"
assert_sorted_equals "${TEST_DIR}/actual-workloads.txt" "${TEST_DIR}/expected-workloads.txt"

cat <<'EOF' > "${TEST_DIR}/expected-tools.txt"
bzip2
fqc
gzip
spring
xz
EOF

bash "${PROJECT_ROOT}/scripts/benchmark_v2.sh" --list-tools > "${TEST_DIR}/actual-tools.txt"
assert_sorted_equals "${TEST_DIR}/actual-tools.txt" "${TEST_DIR}/expected-tools.txt"

assert_command_failure_contains \
    "cannot combine --list-workloads with subcommands" \
    python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" \
    --list-workloads \
    prepare \
    --workload small20k-single \
    --data-root "${BENCHMARK_V2_DATA_ROOT}" \
    --output-dir "${TEST_DIR}/mixed-list-workloads"

assert_command_failure_contains \
    "cannot combine --list-tools with subcommands" \
    python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" \
    --list-tools \
    prepare \
    --workload small20k-single \
    --data-root "${BENCHMARK_V2_DATA_ROOT}" \
    --output-dir "${TEST_DIR}/mixed-list-tools"

assert_command_failure_contains_no_traceback \
    "error: unknown workload: missing-workload" \
    python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" \
    prepare \
    --workload missing-workload \
    --data-root "${BENCHMARK_V2_DATA_ROOT}" \
    --output-dir "${TEST_DIR}/unknown-workload"

mkdir -p "${TEST_DIR}/missing-input-data/small"
assert_command_failure_contains_no_traceback \
    "error: [Errno 2] No such file or directory" \
    python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" \
    prepare \
    --workload small20k-single \
    --data-root "${TEST_DIR}/missing-input-data" \
    --output-dir "${TEST_DIR}/missing-input-output"

mkdir -p "${TEST_DIR}/bad-gzip-data/small"
cat <<'EOF' > "${TEST_DIR}/bad-gzip-data/small/test_1.fq.gz"
not a gzip file
EOF
assert_command_failure_contains_no_traceback \
    "error: Not a gzipped file" \
    python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" \
    prepare \
    --workload small20k-single \
    --data-root "${TEST_DIR}/bad-gzip-data" \
    --output-dir "${TEST_DIR}/bad-gzip-output"

python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" prepare \
    --workload small20k-paired \
    --data-root "${BENCHMARK_V2_DATA_ROOT}" \
    --output-dir "${TEST_DIR}/prepared"

test -f "${TEST_DIR}/prepared/small20k-paired_R1.fastq"
test -f "${TEST_DIR}/prepared/small20k-paired_R2.fastq"

python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" run \
    --workload small20k-single \
    --data-root "${BENCHMARK_V2_DATA_ROOT}" \
    --tools fqc,gzip \
    --threads 1 4 \
    --runs 1 \
    --json "${TEST_DIR}/single.json" \
    --report "${TEST_DIR}/single.md"

PROJECT_ROOT="${PROJECT_ROOT}" TEST_DIR="${TEST_DIR}" python3 <<'PY'
import json
import os
from pathlib import Path

report = json.loads((Path(os.environ["TEST_DIR"]) / "single.json").read_text(encoding="utf-8"))
suite = report["suite"]
results = suite["results"]
tool_ids = {result["tool_id"] for result in results}
if suite["layout"] != "single":
    raise AssertionError(f"expected single layout, got {suite['layout']!r}")
if tool_ids != {"fqc", "gzip"}:
    raise AssertionError(f"expected fqc and gzip results, got {tool_ids!r}")
if not all(result["success"] for result in results):
    raise AssertionError("expected all single-end results to succeed")
gzip_threads = {result["threads"] for result in results if result["tool_id"] == "gzip"}
if gzip_threads != {1}:
    raise AssertionError(f"gzip should only report effective thread 1, got {gzip_threads!r}")
peer_standing = report["peer_standing"]
for metric in ("compression_ratio", "compression_mib_per_s", "decompression_mib_per_s"):
    if "position_band" not in peer_standing[metric]:
        raise AssertionError(f"missing position_band for {metric}")
PY

grep -q '^## Peer Standing$' "${TEST_DIR}/single.md"
grep -q 'fq-compressor stands in the' "${TEST_DIR}/single.md"

python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" run \
    --workload small20k-paired \
    --data-root "${BENCHMARK_V2_DATA_ROOT}" \
    --tools fqc,gzip \
    --threads 1 \
    --runs 1 \
    --json "${TEST_DIR}/paired.json"

grep -q '"tool_id": "fqc"' "${TEST_DIR}/paired.json"
grep -q '"success": true' "${TEST_DIR}/paired.json"
if grep -q '"tool_id": "gzip"' "${TEST_DIR}/paired.json"; then
    echo "paired run should gate gzip results" >&2
    exit 1
fi

assert_command_failure_contains_no_traceback \
    "error: threads must be positive" \
    python3 "${PROJECT_ROOT}/benchmark_v2/cli.py" \
    run \
    --workload small20k-single \
    --data-root "${BENCHMARK_V2_DATA_ROOT}" \
    --tools fqc \
    --threads 0 \
    --runs 1

PROJECT_ROOT="${PROJECT_ROOT}" python3 <<'PY'
import contextlib
import io
import os
import sys
from pathlib import Path

sys.path.insert(0, os.environ["PROJECT_ROOT"])

from benchmark_v2.models import BenchmarkResult, BenchmarkSuite
import benchmark_v2.cli as cli

cli._get_workload = lambda _workload_id: object()
cli._get_tools = lambda tool_ids: list(tool_ids)
cli.run_matrix = lambda **kwargs: BenchmarkSuite(
    workload_id="small20k-single",
    layout="single",
    results=(
        BenchmarkResult(
            tool_id="fqc",
            layout="single",
            operation="compress",
            threads=1,
            input_bytes=1,
            output_bytes=1,
            elapsed_seconds=0.1,
            success=False,
        ),
    ),
)

sys.argv = [
    "benchmark_v2/cli.py",
    "run",
    "--workload",
    "small20k-single",
    "--data-root",
    str(Path(os.environ["PROJECT_ROOT"])),
    "--tools",
    "fqc",
    "--threads",
    "1",
    "--runs",
    "1",
]

try:
    with contextlib.redirect_stdout(io.StringIO()):
        cli.main()
except SystemExit as exc:
    if exc.code == 0:
        raise AssertionError("expected non-zero exit when suite contains failed results")
else:
    raise AssertionError("expected SystemExit when suite contains failed results")
PY

PROJECT_ROOT="${PROJECT_ROOT}" python3 <<'PY'
import os
import tempfile
from pathlib import Path

import benchmark_v2.tool_adapters as tool_adapters

with tempfile.TemporaryDirectory() as temp_dir:
    fake_root = Path(temp_dir)
    release = fake_root / "build/clang-release/src/fqc"
    debug = fake_root / "build/clang-debug/src/fqc"
    release.parent.mkdir(parents=True, exist_ok=True)
    debug.parent.mkdir(parents=True, exist_ok=True)
    release.write_text("#!/bin/sh\n", encoding="utf-8")
    debug.write_text("#!/bin/sh\n", encoding="utf-8")
    release.chmod(0o755)
    debug.chmod(0o755)
    original_root = tool_adapters._PROJECT_ROOT
    try:
        tool_adapters._PROJECT_ROOT = fake_root
        resolved = tool_adapters._resolve_fqc_binary()
    finally:
        tool_adapters._PROJECT_ROOT = original_root
    if resolved != release:
        raise AssertionError(f"expected release binary preference, got {resolved!r}")
PY

PROJECT_ROOT="${PROJECT_ROOT}" python3 <<'PY'
import os
import sys
from pathlib import Path

sys.path.insert(0, os.environ["PROJECT_ROOT"])

from benchmark_v2.models import ToolSpec
from benchmark_v2.models import BenchmarkResult, BenchmarkSuite
from benchmark_v2.report import build_report, render_report_markdown
from benchmark_v2.tool_adapters import create_adapter

adapter = create_adapter(
    ToolSpec(
        tool_id="gzip",
        category="baseline",
        supports_paired=False,
        compress_template="gzip -c {input} > {output}",
        decompress_template="gzip -dc {input} > {output}",
    )
)
command = adapter.compress_command([Path("input.fastq")], Path("output.gz"), 1)
if command[:2] != ["bash", "-c"]:
    raise AssertionError(f"expected ['bash', '-c', ...], got {command!r}")

report = build_report(
    BenchmarkSuite(
        workload_id="synthetic",
        layout="single",
        results=(
            BenchmarkResult(
                tool_id="fqc",
                layout="single",
                operation="compress",
                threads=4,
                input_bytes=100,
                output_bytes=20,
                elapsed_seconds=1.0,
                success=True,
            ),
            BenchmarkResult(
                tool_id="fqc",
                layout="single",
                operation="decompress",
                threads=4,
                input_bytes=20,
                output_bytes=100,
                elapsed_seconds=1.0,
                success=True,
            ),
            BenchmarkResult(
                tool_id="fqc",
                layout="single",
                operation="compress",
                threads=1,
                input_bytes=100,
                output_bytes=25,
                elapsed_seconds=0.25,
                success=True,
            ),
            BenchmarkResult(
                tool_id="fqc",
                layout="single",
                operation="decompress",
                threads=1,
                input_bytes=25,
                output_bytes=100,
                elapsed_seconds=0.5,
                success=True,
            ),
            BenchmarkResult(
                tool_id="gzip",
                layout="single",
                operation="compress",
                threads=1,
                input_bytes=100,
                output_bytes=30,
                elapsed_seconds=2.0,
                success=True,
            ),
            BenchmarkResult(
                tool_id="gzip",
                layout="single",
                operation="decompress",
                threads=1,
                input_bytes=30,
                output_bytes=100,
                elapsed_seconds=2.0,
                success=True,
            ),
        ),
    )
)
fqc_metrics = next(item for item in report["tool_metrics"] if item["tool_id"] == "fqc")
if round(float(fqc_metrics["compression_ratio"]), 2) != 5.0:
    raise AssertionError(f"expected best ratio 5.0x, got {fqc_metrics['compression_ratio']!r}")
if report["peer_standing"]["compression_ratio"]["position_band"] != "leader":
    raise AssertionError(report["peer_standing"]["compression_ratio"])
if fqc_metrics["compression_ratio_threads"] != 4:
    raise AssertionError(fqc_metrics)
if fqc_metrics["compression_mib_per_s_threads"] != 1:
    raise AssertionError(fqc_metrics)
if fqc_metrics["decompression_mib_per_s_threads"] != 1:
    raise AssertionError(fqc_metrics)
if "(T4)" not in render_report_markdown(report) or "(T1)" not in render_report_markdown(report):
    raise AssertionError(render_report_markdown(report))

standalone_report = build_report(
    BenchmarkSuite(
        workload_id="paired-only",
        layout="paired",
        results=(
            BenchmarkResult(
                tool_id="fqc",
                layout="paired",
                operation="compress",
                threads=1,
                input_bytes=100,
                output_bytes=20,
                elapsed_seconds=1.0,
                success=True,
            ),
            BenchmarkResult(
                tool_id="fqc",
                layout="paired",
                operation="decompress",
                threads=1,
                input_bytes=20,
                output_bytes=100,
                elapsed_seconds=1.0,
                success=True,
            ),
        ),
    )
)
if standalone_report["peer_standing"]["compression_ratio"]["position_band"] != "standalone":
    raise AssertionError(standalone_report["peer_standing"]["compression_ratio"])
if "only measured tool" not in standalone_report["conclusion"]:
    raise AssertionError(standalone_report["conclusion"])

try:
    build_report(
        BenchmarkSuite(
            workload_id="incomplete",
            layout="single",
            results=(
                BenchmarkResult(
                    tool_id="fqc",
                    layout="single",
                    operation="compress",
                    threads=1,
                    input_bytes=100,
                    output_bytes=20,
                    elapsed_seconds=1.0,
                    success=True,
                ),
                BenchmarkResult(
                    tool_id="fqc",
                    layout="single",
                    operation="decompress",
                    threads=1,
                    input_bytes=20,
                    output_bytes=100,
                    elapsed_seconds=1.0,
                    success=True,
                ),
                BenchmarkResult(
                    tool_id="gzip",
                    layout="single",
                    operation="compress",
                    threads=1,
                    input_bytes=100,
                    output_bytes=30,
                    elapsed_seconds=2.0,
                    success=False,
                ),
            ),
        )
    )
except ValueError as exc:
    if "incomplete benchmark suite" not in str(exc):
        raise
else:
    raise AssertionError("expected build_report to reject incomplete comparison suites")
PY

mkdir -p "${TEST_DIR}/short-data/small"
cat <<'EOF' > "${TEST_DIR}/short-data/small/too-short.fastq"
@read1
ACGT
+
!!!!
EOF
gzip -c "${TEST_DIR}/short-data/small/too-short.fastq" > "${TEST_DIR}/short-data/small/too-short.fq.gz"

assert_python_validation_error \
    "contains only 1 complete reads, fewer than requested 2" \
    "$(cat <<PY
from pathlib import Path
from benchmark_v2.models import WorkloadSpec
from benchmark_v2.prepare_inputs import prepare_workload

prepare_workload(
    WorkloadSpec(
        workload_id="too-short",
        layout="single",
        inputs=("small/too-short.fq.gz",),
        read_limit=2,
        tier="dev",
        comparable_tools=("fqc",),
    ),
    data_root=Path("${TEST_DIR}/short-data"),
    output_dir=Path("${TEST_DIR}/short-output"),
)
PY
)"

mkdir -p "${TEST_DIR}/non-ascii-data/small"
python3 <<PY
import gzip
from pathlib import Path

output_path = Path("${TEST_DIR}") / "non-ascii-data" / "small" / "non-ascii.fq.gz"
output_path.parent.mkdir(parents=True, exist_ok=True)

with gzip.open(output_path, "wb") as fout:
    fout.write(b"@read1\nACGT\xff\n+\nIIII\n")
PY

assert_python_validation_error \
    "codec can't decode byte" \
    "$(cat <<PY
from pathlib import Path
from benchmark_v2.models import WorkloadSpec
from benchmark_v2.prepare_inputs import prepare_workload

prepare_workload(
    WorkloadSpec(
        workload_id="non-ascii",
        layout="single",
        inputs=("small/non-ascii.fq.gz",),
        read_limit=1,
        tier="dev",
        comparable_tools=("fqc",),
    ),
    data_root=Path("${TEST_DIR}/non-ascii-data"),
    output_dir=Path("${TEST_DIR}/non-ascii-output"),
)
PY
)"

test ! -e "${TEST_DIR}/non-ascii-output/non-ascii_R1.fastq"

mkdir -p "${TEST_DIR}/malformed-structure-data/small"
cat <<'EOF' > "${TEST_DIR}/malformed-structure-data/small/malformed.fastq"
read1
ACGT
+
IIII
EOF
gzip -c "${TEST_DIR}/malformed-structure-data/small/malformed.fastq" > "${TEST_DIR}/malformed-structure-data/small/malformed.fq.gz"

assert_python_validation_error \
    "malformed FASTQ record #1" \
    "$(cat <<PY
from pathlib import Path
from benchmark_v2.models import WorkloadSpec
from benchmark_v2.prepare_inputs import prepare_workload

prepare_workload(
    WorkloadSpec(
        workload_id="malformed-structure",
        layout="single",
        inputs=("small/malformed.fq.gz",),
        read_limit=1,
        tier="dev",
        comparable_tools=("fqc",),
    ),
    data_root=Path("${TEST_DIR}/malformed-structure-data"),
    output_dir=Path("${TEST_DIR}/malformed-structure-output"),
)
PY
)"

mkdir -p "${TEST_DIR}/mid-record-data/small"
cat <<'EOF' > "${TEST_DIR}/mid-record-data/small/mid-record.fastq"
@read1
TGCA
+
####
@read2
CCCC
+
EOF
gzip -c "${TEST_DIR}/mid-record-data/small/mid-record.fastq" > "${TEST_DIR}/mid-record-data/small/mid-record.fq.gz"

assert_python_validation_error \
    "ends mid-record after 1 complete reads" \
    "$(cat <<PY
from pathlib import Path
from benchmark_v2.models import WorkloadSpec
from benchmark_v2.prepare_inputs import prepare_workload

prepare_workload(
    WorkloadSpec(
        workload_id="mid-record",
        layout="single",
        inputs=("small/mid-record.fq.gz",),
        read_limit=2,
        tier="dev",
        comparable_tools=("fqc",),
    ),
    data_root=Path("${TEST_DIR}/mid-record-data"),
    output_dir=Path("${TEST_DIR}/mid-record-output"),
    )
PY
)"

mkdir -p "${TEST_DIR}/escaped-data/small" "${TEST_DIR}/outside"
cat <<'EOF' > "${TEST_DIR}/outside/escaped.fastq"
@read1
TGCA
+
####
EOF
gzip -c "${TEST_DIR}/outside/escaped.fastq" > "${TEST_DIR}/outside/escaped.fq.gz"

assert_python_validation_error \
    "escapes data_root" \
    "$(cat <<PY
from pathlib import Path
from benchmark_v2.models import WorkloadSpec
from benchmark_v2.prepare_inputs import prepare_workload

prepare_workload(
    WorkloadSpec(
        workload_id="escaped-input",
        layout="single",
        inputs=("../outside/escaped.fq.gz",),
        read_limit=1,
        tier="dev",
        comparable_tools=("fqc",),
    ),
    data_root=Path("${TEST_DIR}/escaped-data"),
    output_dir=Path("${TEST_DIR}/escaped-output"),
)
PY
)"

mkdir -p "${TEST_DIR}/paired-data/small"
cat <<'EOF' > "${TEST_DIR}/paired-data/small/mate1.fastq"
@read1
AAAA
+
!!!!
@read2
CCCC
+
####
EOF
cat <<'EOF' > "${TEST_DIR}/paired-data/small/mate2.fastq"
@read1
GGGG
+
!!!!
EOF
gzip -c "${TEST_DIR}/paired-data/small/mate1.fastq" > "${TEST_DIR}/paired-data/small/mate1.fq.gz"
gzip -c "${TEST_DIR}/paired-data/small/mate2.fastq" > "${TEST_DIR}/paired-data/small/mate2.fq.gz"

assert_python_validation_error \
    "contains only 1 complete reads, fewer than requested 2" \
    "$(cat <<PY
from pathlib import Path
from benchmark_v2.models import WorkloadSpec
from benchmark_v2.prepare_inputs import prepare_workload

prepare_workload(
    WorkloadSpec(
        workload_id="paired-cleanup",
        layout="paired",
        inputs=("small/mate1.fq.gz", "small/mate2.fq.gz"),
        read_limit=2,
        tier="dev",
        comparable_tools=("fqc",),
    ),
    data_root=Path("${TEST_DIR}/paired-data"),
    output_dir=Path("${TEST_DIR}/paired-output"),
)
PY
)"

test ! -e "${TEST_DIR}/paired-output/paired-cleanup_R1.fastq"
test ! -e "${TEST_DIR}/paired-output/paired-cleanup_R2.fastq"

assert_python_validation_error \
    "escapes output_dir" \
    "$(cat <<PY
from pathlib import Path
from benchmark_v2.models import WorkloadSpec
from benchmark_v2.prepare_inputs import prepare_workload

prepare_workload(
    WorkloadSpec(
        workload_id="../escaped/pwned",
        layout="single",
        inputs=("small/test_1.fq.gz",),
        read_limit=1,
        tier="dev",
        comparable_tools=("fqc",),
    ),
    data_root=Path("${BENCHMARK_V2_DATA_ROOT}"),
    output_dir=Path("${TEST_DIR}/safe-output"),
)
PY
)"

test ! -e "${TEST_DIR}/escaped/pwned_R1.fastq"

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

assert_python_tuple_normalized $'from benchmark_v2.models import BenchmarkResult, BenchmarkSuite, WorkloadSpec\n\nworkload = WorkloadSpec(\n    workload_id="small20k-single",\n    layout="single",\n    inputs=["small/test_1.fq.gz"],\n    read_limit=1,\n    tier="dev",\n    comparable_tools=["fqc"],\n)\nassert isinstance(workload.inputs, tuple)\nassert isinstance(workload.comparable_tools, tuple)\n\nsuite = BenchmarkSuite(\n    workload_id="small20k-single",\n    layout="single",\n    results=[\n        BenchmarkResult(\n            tool_id="fqc",\n            layout="single",\n            operation="compress",\n            threads=1,\n            input_bytes=1,\n            output_bytes=1,\n            elapsed_seconds=0.1,\n            success=True,\n        ),\n    ],\n)\nassert isinstance(suite.results, tuple)\n'

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
    "tier must be a non-empty string" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-single",\n    layout="single",\n    inputs=("small/test_1.fq.gz",),\n    read_limit=1,\n    tier="",\n    comparable_tools=("fqc",),\n)'

assert_python_validation_error \
    "inputs member #2 must be a non-empty string" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-paired",\n    layout="paired",\n    inputs=("small/test_1.fq.gz", None),\n    read_limit=1,\n    tier="dev",\n    comparable_tools=("fqc",),\n)'

assert_python_validation_error \
    "comparable_tools member #2 must be a non-empty string" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-single",\n    layout="single",\n    inputs=("small/test_1.fq.gz",),\n    read_limit=1,\n    tier="dev",\n    comparable_tools=("fqc", None),\n)'

assert_python_validation_error \
    "comparable_tools must not contain duplicates" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-single",\n    layout="single",\n    inputs=("small/test_1.fq.gz",),\n    read_limit=1,\n    tier="dev",\n    comparable_tools=("fqc", "gzip", "fqc"),\n)'

assert_python_validation_error \
    "inputs member #2 must be a non-empty string" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-paired",\n    layout="paired",\n    inputs=("small/test_1.fq.gz", ""),\n    read_limit=1,\n    tier="dev",\n    comparable_tools=("fqc",),\n)'

assert_python_validation_error \
    "comparable_tools member #2 must be a non-empty string" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-single",\n    layout="single",\n    inputs=("small/test_1.fq.gz",),\n    read_limit=1,\n    tier="dev",\n    comparable_tools=("fqc", ""),\n)'

assert_python_validation_error \
    "read_limit must be positive" \
    $'from benchmark_v2.models import WorkloadSpec\n\nWorkloadSpec(\n    workload_id="small20k-single",\n    layout="single",\n    inputs=("small/test_1.fq.gz",),\n    read_limit=True,\n    tier="dev",\n    comparable_tools=("fqc",),\n)'

assert_python_validation_error \
    "tool_id must be a non-empty string" \
    $'from benchmark_v2.models import ToolSpec\n\nToolSpec(\n    tool_id="",\n    category="baseline",\n    supports_paired=False,\n    compress_template="gzip -c {input} > {output}",\n    decompress_template="gzip -dc {input} > {output}",\n)'

assert_python_validation_error \
    "supports_paired must be a boolean" \
    $'from benchmark_v2.models import ToolSpec\n\nToolSpec(\n    tool_id="gzip",\n    category="baseline",\n    supports_paired=1,\n    compress_template="gzip -c {input} > {output}",\n    decompress_template="gzip -dc {input} > {output}",\n)'

assert_python_validation_error \
    "category must be a non-empty string" \
    $'from benchmark_v2.models import ToolSpec\n\nToolSpec(\n    tool_id="gzip",\n    category="",\n    supports_paired=False,\n    compress_template="gzip -c {input} > {output}",\n    decompress_template="gzip -dc {input} > {output}",\n)'

assert_python_validation_error \
    "compress_template must be a non-empty string" \
    $'from benchmark_v2.models import ToolSpec\n\nToolSpec(\n    tool_id="gzip",\n    category="baseline",\n    supports_paired=False,\n    compress_template="",\n    decompress_template="gzip -dc {input} > {output}",\n)'

assert_python_validation_error \
    "compress_template requires required placeholders" \
    $'from benchmark_v2.models import ToolSpec\n\nToolSpec(\n    tool_id="gzip",\n    category="baseline",\n    supports_paired=False,\n    compress_template="gzip -c {input}",\n    decompress_template="gzip -dc {output} > {decompressed}",\n)'

assert_python_validation_error \
    "decompress_template requires required placeholders" \
    $'from benchmark_v2.models import ToolSpec\n\nToolSpec(\n    tool_id="gzip",\n    category="baseline",\n    supports_paired=False,\n    compress_template="gzip -c {input} > {output}",\n    decompress_template="gzip -dc {output}",\n)'

assert_python_validation_error \
    "unknown placeholders" \
    $'from benchmark_v2.models import ToolSpec\n\nToolSpec(\n    tool_id="gzip",\n    category="baseline",\n    supports_paired=False,\n    compress_template="gzip -c {input} > {output} {mystery}",\n    decompress_template="gzip -dc {input} > {output}",\n)'

assert_python_validation_error \
    "operation must be one of" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="invalid",\n    threads=1,\n    input_bytes=1,\n    output_bytes=1,\n    elapsed_seconds=0.1,\n    success=True,\n)'

assert_python_validation_error \
    "threads must be positive" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=0,\n    input_bytes=1,\n    output_bytes=1,\n    elapsed_seconds=0.1,\n    success=True,\n)'

assert_python_validation_error \
    "threads must be positive" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=True,\n    input_bytes=False,\n    output_bytes=1,\n    elapsed_seconds=0.1,\n    success="yes",\n)'

assert_python_validation_error \
    "input_bytes must be non-negative" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=1,\n    input_bytes=False,\n    output_bytes=1,\n    elapsed_seconds=0.1,\n    success=True,\n)'

assert_python_validation_error \
    "success must be a boolean" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=1,\n    input_bytes=1,\n    output_bytes=1,\n    elapsed_seconds=0.1,\n    success="yes",\n)'

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
    $'from benchmark_v2.models import BenchmarkResult\nimport math\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=1,\n    input_bytes=1,\n    output_bytes=1,\n    elapsed_seconds=math.nan,\n    success=False,\n)'

assert_python_validation_error \
    "elapsed_seconds must be non-negative" \
    $'from benchmark_v2.models import BenchmarkResult\n\nBenchmarkResult(\n    tool_id="fqc",\n    layout="single",\n    operation="compress",\n    threads=1,\n    input_bytes=1,\n    output_bytes=1,\n    elapsed_seconds=-0.1,\n    success=False,\n)'

assert_python_validation_error \
    "suite layout" \
    $'from benchmark_v2.models import BenchmarkResult, BenchmarkSuite\n\nBenchmarkSuite(\n    workload_id="small20k-single",\n    layout="single",\n    results=(\n        BenchmarkResult(\n            tool_id="fqc",\n            layout="paired",\n            operation="compress",\n            threads=1,\n            input_bytes=1,\n            output_bytes=1,\n            elapsed_seconds=0.1,\n            success=True,\n        ),\n    ),\n)'

assert_python_validation_error \
    "workload_id must be a non-empty string" \
    $'from benchmark_v2.models import BenchmarkSuite\n\nBenchmarkSuite(\n    workload_id="",\n    layout="single",\n    results=(),\n)'

assert_python_validation_error \
    "results must contain at least one BenchmarkResult" \
    $'from benchmark_v2.models import BenchmarkSuite\n\nBenchmarkSuite(\n    workload_id="small20k-single",\n    layout="single",\n    results=(),\n)'

assert_python_validation_error \
    "results item #1 must be a BenchmarkResult" \
    $'from benchmark_v2.models import BenchmarkSuite\n\nBenchmarkSuite(\n    workload_id="small20k-single",\n    layout="single",\n    results=(None,),\n)'
