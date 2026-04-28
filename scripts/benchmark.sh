#!/usr/bin/env bash
# =============================================================================
# fq-compressor Benchmark Script
# =============================================================================
# Dataset-driven benchmark entrypoint for tracked local evidence
#
# Usage:
#   ./scripts/benchmark.sh [options]
#
# Options:
#   --dataset ID  Benchmark dataset id from benchmark/datasets.yaml
#   --input PATH  Override benchmark input path
#   --tools CSV   Override tool set
#   --all         Request all configured tools
#   --quick       Run a single benchmark iteration
#   --full        Run three benchmark iterations
#   --prepare     Prepare the dataset subset if needed
#   --prepare-only Prepare the dataset subset and exit
#   --build       Build clang-release before benchmarking
#   --help        Show this help message
#
# Output:
#   - JSON results: benchmark/results/<dataset>.json
#   - Markdown report: benchmark/results/<dataset>.md
# =============================================================================

set -euo pipefail

# =============================================================================
# Configuration
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
RESULTS_DIR="${PROJECT_ROOT}/benchmark/results"
DATASETS_FILE="${PROJECT_ROOT}/benchmark/datasets.yaml"
DEFAULT_DATASET="err091571-local-supported"
DEFAULT_THREADS=(1 4)

DATASET_ID="$DEFAULT_DATASET"
INPUT_OVERRIDE=""
TOOLS_OVERRIDE=""
REQUEST_ALL=false
RUNS=3
PREPARE_DATASET=false
PREPARE_ONLY=false
BUILD_BINARY=false
THREADS=("${DEFAULT_THREADS[@]}")

# =============================================================================
# Helper Functions
# =============================================================================

log_info() {
    echo "[INFO] $*" >&2
}

log_error() {
    echo "[ERROR] $*" >&2
}

log_success() {
    echo "[SUCCESS] $*" >&2
}

show_help() {
    sed -n '/^# Usage:/,/^# =/p' "$0" | sed 's/^# //; s/^#//'
}

check_dependencies() {
    log_info "Checking dependencies..."

    local missing=()

    if ! command -v python3 &> /dev/null; then
        missing+=("python3")
    fi

    if [ ${#missing[@]} -gt 0 ]; then
        log_error "Missing dependencies: ${missing[*]}"
        exit 1
    fi

    if ! python3 -c "import yaml" >/dev/null 2>&1; then
        log_error "PyYAML is required for benchmark tooling"
        exit 1
    fi

    log_success "All dependencies found"
}

setup_directories() {
    log_info "Setting up directories..."
    mkdir -p "$RESULTS_DIR"

    log_success "Directories created"
}

build_clang() {
    log_info "Building with Clang..."

    "${SCRIPT_DIR}/build.sh" clang-release

    if [ ! -f "${PROJECT_ROOT}/build/clang-release/src/fqc" ]; then
        log_error "Clang build failed"
        exit 1
    fi

    log_success "Clang build complete"
}

prepare_dataset() {
    local dataset_id="$1"

    python3 - "$DATASETS_FILE" "$dataset_id" "$PROJECT_ROOT" <<'PY'
import gzip
import subprocess
import sys
from pathlib import Path

import yaml

datasets_path = Path(sys.argv[1])
dataset_id = sys.argv[2]
project_root = Path(sys.argv[3])

with open(datasets_path, "r", encoding="utf-8") as handle:
    config = yaml.safe_load(handle) or {}

dataset = (config.get("datasets") or {}).get(dataset_id)
if not dataset:
    raise SystemExit(f"unknown dataset: {dataset_id}")

root = project_root / "benchmark/data/ERR091571"
root.mkdir(parents=True, exist_ok=True)

urls = dataset.get("source_urls") or []
if len(urls) < 3:
    raise SystemExit("dataset manifest must provide ENA landing page plus read URLs")

record_limit = 20000
for url in urls[1:3]:
    name = Path(url).name
    if not name.endswith(".fastq.gz"):
        raise SystemExit(f"unexpected FASTQ URL: {url}")
    dst = root / name.replace(".fastq.gz", ".subset.fastq.gz")
    if dst.exists():
        continue

    proc = subprocess.Popen(
        ["curl", "-LfsS", "--retry", "3", url],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.stdout is None:
        raise SystemExit(f"failed to open stream for {url}")

    with gzip.GzipFile(fileobj=proc.stdout) as fin, gzip.open(dst, "wt", encoding="utf-8") as fout:
        for _ in range(record_limit * 4):
            line = fin.readline()
            if not line:
                break
            fout.write(line.decode("utf-8"))

    if proc.stdout is not None:
        proc.stdout.close()
    proc.terminate()
    _, stderr = proc.communicate(timeout=10)
    if proc.returncode not in (0, -15):
        raise SystemExit(f"curl failed for {url}: {stderr.decode('utf-8', errors='ignore').strip()}")

read1_subset_gz = root / "ERR091571_1.subset.fastq.gz"
tracked_fastq = root / "ERR091571_1.2000.fastq"
if not tracked_fastq.exists():
    with gzip.open(read1_subset_gz, "rt", encoding="utf-8") as fin, open(
        tracked_fastq, "w", encoding="utf-8"
    ) as fout:
        for _ in range(2000 * 4):
            line = fin.readline()
            if not line:
                break
            fout.write(line)

print(tracked_fastq)
PY
}

# =============================================================================
# Main
# =============================================================================

main() {
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --dataset)
                DATASET_ID="$2"
                shift 2
                ;;
            --input)
                INPUT_OVERRIDE="$2"
                shift 2
                ;;
            --tools)
                TOOLS_OVERRIDE="$2"
                shift 2
                ;;
            --all)
                REQUEST_ALL=true
                shift
                ;;
            --quick)
                RUNS=1
                shift
                ;;
            --full)
                RUNS=3
                shift
                ;;
            --prepare)
                PREPARE_DATASET=true
                shift
                ;;
            --prepare-only)
                PREPARE_DATASET=true
                PREPARE_ONLY=true
                shift
                ;;
            --build)
                BUILD_BINARY=true
                shift
                ;;
            -t|--threads)
                shift
                THREADS=()
                while [[ $# -gt 0 ]] && [[ "$1" =~ ^[0-9]+$ ]]; do
                    THREADS+=("$1")
                    shift
                done
                ;;
            -r|--runs)
                RUNS="$2"
                shift 2
                ;;
            --help)
                show_help
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done

    log_info "=== fq-compressor Benchmark ==="
    log_info "Dataset: $DATASET_ID"
    log_info "Runs: $RUNS"
    log_info "Threads: ${THREADS[*]}"
    echo

    check_dependencies
    setup_directories

    if [ "$PREPARE_DATASET" = true ]; then
        log_info "Preparing dataset subset..."
        prepare_dataset "$DATASET_ID" >/dev/null
        log_success "Dataset prepared"
    fi

    if [ "$PREPARE_ONLY" = true ]; then
        exit 0
    fi

    if [ "$BUILD_BINARY" = true ]; then
        build_clang
    fi

    local json_out="${RESULTS_DIR}/${DATASET_ID}.json"
    local report_out="${RESULTS_DIR}/${DATASET_ID}.md"
    local cmd=(python3 "${PROJECT_ROOT}/benchmark/benchmark.py" --dataset "$DATASET_ID" --runs "$RUNS" --json "$json_out" --report "$report_out" --command-path "scripts/benchmark.sh")

    if [ -n "$INPUT_OVERRIDE" ]; then
        cmd=(python3 "${PROJECT_ROOT}/benchmark/benchmark.py" --input "$INPUT_OVERRIDE" --runs "$RUNS" --json "$json_out" --report "$report_out" --command-path "scripts/benchmark.sh")
    fi

    if [ "$REQUEST_ALL" = true ]; then
        cmd+=(--all)
    elif [ -n "$TOOLS_OVERRIDE" ]; then
        cmd+=(--tools "$TOOLS_OVERRIDE")
    fi

    if [ ${#THREADS[@]} -gt 0 ]; then
        cmd+=(--threads "${THREADS[@]}")
    fi

    cmd+=(--invocation "./scripts/benchmark.sh --dataset ${DATASET_ID} --runs ${RUNS} --threads ${THREADS[*]}")

    log_info "Running benchmark runner..."
    "${cmd[@]}"

    echo
    log_success "=== Benchmark Complete ==="
    log_success "Results: $json_out"
    log_success "Report: $report_out"
}

main "$@"
