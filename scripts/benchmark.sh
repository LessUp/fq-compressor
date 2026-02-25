#!/usr/bin/env bash
# =============================================================================
# fq-compressor Benchmark Script
# =============================================================================
# Comprehensive performance testing for GCC vs Clang builds
#
# Usage:
#   ./scripts/benchmark.sh [options]
#
# Options:
#   --quick       Run quick benchmark (single iteration)
#   --full        Run full benchmark (multiple iterations)
#   --gcc-only    Test GCC build only
#   --clang-only  Test Clang build only
#   --help        Show this help message
#
# Output:
#   - JSON results: docs/benchmark/results-TIMESTAMP.json
#   - Markdown report: docs/benchmark/report-TIMESTAMP.md
#   - Visualization: docs/benchmark/charts-TIMESTAMP.html
# =============================================================================

set -euo pipefail

# =============================================================================
# Configuration
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BENCHMARK_DIR="${PROJECT_ROOT}/docs/benchmark"
RESULTS_DIR="${BENCHMARK_DIR}/results"
DATA_DIR="${PROJECT_ROOT}/fq-data"

# Test data files
TEST_FILE_1="${DATA_DIR}/E150035817_L01_1201_1.sub10.fq.gz"
TEST_FILE_2="${DATA_DIR}/E150035817_L01_1201_2.sub10.fq.gz"

# Benchmark parameters
ITERATIONS=3
QUICK_MODE=false
TEST_GCC=true
TEST_CLANG=true

# Output files
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
RESULTS_JSON="${RESULTS_DIR}/results-${TIMESTAMP}.json"
REPORT_MD="${RESULTS_DIR}/report-${TIMESTAMP}.md"
CHARTS_HTML="${RESULTS_DIR}/charts-${TIMESTAMP}.html"

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

    if ! command -v jq &> /dev/null; then
        missing+=("jq")
    fi

    if [ ${#missing[@]} -gt 0 ]; then
        log_error "Missing dependencies: ${missing[*]}"
        log_error "Please install: sudo apt-get install ${missing[*]}"
        exit 1
    fi

    log_success "All dependencies found"
}

check_test_data() {
    log_info "Checking test data..."

    if [ ! -f "$TEST_FILE_1" ]; then
        log_error "Test file not found: $TEST_FILE_1"
        exit 1
    fi

    if [ ! -f "$TEST_FILE_2" ]; then
        log_error "Test file not found: $TEST_FILE_2"
        exit 1
    fi

    log_success "Test data found"
}

setup_directories() {
    log_info "Setting up directories..."

    mkdir -p "$BENCHMARK_DIR"
    mkdir -p "$RESULTS_DIR"
    mkdir -p "${PROJECT_ROOT}/build/benchmark"

    log_success "Directories created"
}

# =============================================================================
# Build Functions
# =============================================================================

build_gcc() {
    log_info "Building with GCC..."

    "${SCRIPT_DIR}/build.sh" gcc-release > /dev/null 2>&1

    if [ ! -f "${PROJECT_ROOT}/build/gcc-release/src/fqc" ]; then
        log_error "GCC build failed"
        exit 1
    fi

    log_success "GCC build complete"
}

build_clang() {
    log_info "Building with Clang..."

    "${SCRIPT_DIR}/build.sh" clang-release > /dev/null 2>&1

    if [ ! -f "${PROJECT_ROOT}/build/clang-release/src/fqc" ]; then
        log_error "Clang build failed"
        exit 1
    fi

    log_success "Clang build complete"
}

# =============================================================================
# Benchmark Functions
# =============================================================================

get_file_size() {
    stat -c%s "$1" 2>/dev/null || stat -f%z "$1" 2>/dev/null
}

run_compression_test() {
    local compiler=$1
    local binary=$2
    local input_file=$3
    local output_file=$4
    local iteration=$5

    log_info "[$compiler] Compression test (iteration $iteration)..."

    # Decompress input if needed
    local input_fastq="${input_file%.gz}"
    if [ ! -f "$input_fastq" ]; then
        gunzip -c "$input_file" > "$input_fastq"
    fi

    # Run compression
    local start_time=$(date +%s.%N)
    "$binary" compress "$input_fastq" -o "$output_file" --threads 1 > /dev/null 2>&1
    local end_time=$(date +%s.%N)

    local elapsed=$(echo "$end_time - $start_time" | bc)
    local input_size=$(get_file_size "$input_fastq")
    local output_size=$(get_file_size "$output_file")
    local ratio=$(echo "scale=4; $input_size / $output_size" | bc)
    local throughput=$(echo "scale=2; $input_size / $elapsed / 1024 / 1024" | bc)

    # Output JSON
    cat <<EOF
{
  "compiler": "$compiler",
  "test": "compression",
  "iteration": $iteration,
  "input_file": "$(basename "$input_file")",
  "input_size": $input_size,
  "output_size": $output_size,
  "compression_ratio": $ratio,
  "elapsed_seconds": $elapsed,
  "throughput_mbps": $throughput
}
EOF
}

run_decompression_test() {
    local compiler=$1
    local binary=$2
    local input_file=$3
    local output_file=$4
    local iteration=$5

    log_info "[$compiler] Decompression test (iteration $iteration)..."

    # Run decompression
    local start_time=$(date +%s.%N)
    "$binary" decompress "$input_file" -o "$output_file" > /dev/null 2>&1
    local end_time=$(date +%s.%N)

    local elapsed=$(echo "$end_time - $start_time" | bc)
    local input_size=$(get_file_size "$input_file")
    local output_size=$(get_file_size "$output_file")
    local throughput=$(echo "scale=2; $output_size / $elapsed / 1024 / 1024" | bc)

    # Output JSON
    cat <<EOF
{
  "compiler": "$compiler",
  "test": "decompression",
  "iteration": $iteration,
  "input_file": "$(basename "$input_file")",
  "input_size": $input_size,
  "output_size": $output_size,
  "elapsed_seconds": $elapsed,
  "throughput_mbps": $throughput
}
EOF
}

run_benchmark_suite() {
    local compiler=$1
    local binary=$2

    log_info "Running benchmark suite for $compiler..."

    local results=()

    for i in $(seq 1 $ITERATIONS); do
        # Test file 1
        local compressed="${PROJECT_ROOT}/build/benchmark/${compiler}_test1_${i}.fqc"
        local decompressed="${PROJECT_ROOT}/build/benchmark/${compiler}_test1_${i}.fq"

        local result=$(run_compression_test "$compiler" "$binary" "$TEST_FILE_1" "$compressed" "$i")
        results+=("$result")

        result=$(run_decompression_test "$compiler" "$binary" "$compressed" "$decompressed" "$i")
        results+=("$result")

        # Cleanup
        rm -f "$compressed" "$decompressed"
    done

    # Output all results as JSON array
    printf '%s\n' "${results[@]}" | jq -s '.'
}

# =============================================================================
# Report Generation
# =============================================================================

generate_json_report() {
    log_info "Generating JSON report..."

    local all_results="[]"

    if [ "$TEST_GCC" = true ]; then
        local gcc_results=$(run_benchmark_suite "gcc" "${PROJECT_ROOT}/build/gcc-release/src/fqc")
        all_results=$(echo "$all_results" | jq ". + $gcc_results")
    fi

    if [ "$TEST_CLANG" = true ]; then
        local clang_results=$(run_benchmark_suite "clang" "${PROJECT_ROOT}/build/clang-release/src/fqc")
        all_results=$(echo "$all_results" | jq ". + $clang_results")
    fi

    # Add metadata
    local report=$(cat <<EOF
{
  "metadata": {
    "timestamp": "$(date -Iseconds)",
    "hostname": "$(hostname)",
    "cpu": "$(lscpu 2>/dev/null | grep 'Model name' | cut -d: -f2 | xargs || echo 'unknown')",
    "cores": $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1),
    "iterations": $ITERATIONS,
    "test_files": [
      "$(basename "$TEST_FILE_1")",
      "$(basename "$TEST_FILE_2")"
    ]
  },
  "results": $all_results
}
EOF
)

    echo "$report" | jq '.' > "$RESULTS_JSON"
    log_success "JSON report saved: $RESULTS_JSON"
}

generate_markdown_report() {
    log_info "Generating Markdown report..."

    python3 "${SCRIPT_DIR}/generate_benchmark_report.py" "$RESULTS_JSON" "$REPORT_MD"

    log_success "Markdown report saved: $REPORT_MD"
}

generate_html_charts() {
    log_info "Generating HTML charts..."

    python3 "${SCRIPT_DIR}/generate_benchmark_charts.py" "$RESULTS_JSON" "$CHARTS_HTML"

    log_success "HTML charts saved: $CHARTS_HTML"
}

update_readme() {
    log_info "Updating README.md..."

    local readme="${PROJECT_ROOT}/README.md"
    local latest_report=$(basename "$REPORT_MD")
    local latest_charts=$(basename "$CHARTS_HTML")

    # Check if benchmark section exists
    if ! grep -q "## Benchmark Results" "$readme"; then
        cat >> "$readme" <<EOF

## Benchmark Results

Latest benchmark: [${latest_report}](docs/benchmark/results/${latest_report})
Visualization: [${latest_charts}](docs/benchmark/results/${latest_charts})

See [docs/benchmark/](docs/benchmark/) for historical results.
EOF
    else
        # Update existing section
        sed -i "/Latest benchmark:/c\Latest benchmark: [${latest_report}](docs/benchmark/results/${latest_report})" "$readme"
        sed -i "/Visualization:/c\Visualization: [${latest_charts}](docs/benchmark/results/${latest_charts})" "$readme"
    fi

    log_success "README.md updated"
}

# =============================================================================
# Main
# =============================================================================

main() {
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --quick)
                QUICK_MODE=true
                ITERATIONS=1
                shift
                ;;
            --full)
                ITERATIONS=5
                shift
                ;;
            --gcc-only)
                TEST_CLANG=false
                shift
                ;;
            --clang-only)
                TEST_GCC=false
                shift
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
    log_info "Iterations: $ITERATIONS"
    log_info "Test GCC: $TEST_GCC"
    log_info "Test Clang: $TEST_CLANG"
    echo

    # Setup
    check_dependencies
    check_test_data
    setup_directories

    # Build
    if [ "$TEST_GCC" = true ]; then
        build_gcc
    fi

    if [ "$TEST_CLANG" = true ]; then
        build_clang
    fi

    # Run benchmarks
    generate_json_report

    # Generate reports
    generate_markdown_report
    generate_html_charts

    # Update documentation
    update_readme

    echo
    log_success "=== Benchmark Complete ==="
    log_success "Results: $RESULTS_JSON"
    log_success "Report: $REPORT_MD"
    log_success "Charts: $CHARTS_HTML"
}

main "$@"
