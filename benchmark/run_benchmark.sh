#!/usr/bin/env bash
# =============================================================================
# fq-compressor Benchmark Runner
# =============================================================================
# Wrapper script for running benchmarks with common configurations.
#
# Usage:
#   ./run_benchmark.sh <input.fastq> [options]
#
# Examples:
#   ./run_benchmark.sh data.fastq                    # Default benchmark
#   ./run_benchmark.sh data.fastq --all              # All tools
#   ./run_benchmark.sh data.fastq -t 1 4 8           # Custom thread counts
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
REPORTS_DIR="${REPORTS_DIR:-$PROJECT_ROOT/reports}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

usage() {
    cat << EOF
Usage: $(basename "$0") <input.fastq> [options]

Options:
    --all               Test all available tools
    --tools TOOLS       Comma-separated list of tools (default: fqc,gzip,pigz)
    -t, --threads N...  Thread counts to test (default: 1 4)
    -r, --runs N        Number of runs per config (default: 3)
    -o, --output DIR    Output directory for reports (default: reports/)
    --no-report         Skip report generation
    --json              Also generate JSON output
    -h, --help          Show this help

Available tools: fqc, spring, gzip, pigz, zstd, xz, bzip2

Examples:
    $(basename "$0") data.fastq --all -t 1 4 8
    $(basename "$0") data.fastq --tools fqc,spring,gzip -r 5
EOF
}

# Check dependencies
check_deps() {
    if ! command -v python3 &>/dev/null; then
        log_error "Python 3 is required"
        exit 1
    fi
    
    # Check PyYAML
    if ! python3 -c "import yaml" 2>/dev/null; then
        log_warn "PyYAML not installed, installing..."
        pip3 install --user pyyaml || {
            log_error "Failed to install PyYAML"
            exit 1
        }
    fi
}

main() {
    if [[ $# -lt 1 ]] || [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
        usage
        exit 0
    fi
    
    local input_file="$1"
    shift
    
    if [[ ! -f "$input_file" ]]; then
        log_error "Input file not found: $input_file"
        exit 1
    fi
    
    check_deps
    
    # Default options
    local tools="fqc,gzip,pigz"
    local threads="1 4"
    local runs=3
    local output_dir="$REPORTS_DIR"
    local generate_report=true
    local generate_json=false
    local all_tools=false
    
    # Parse options
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --all)
                all_tools=true
                shift
                ;;
            --tools)
                tools="$2"
                shift 2
                ;;
            -t|--threads)
                shift
                threads=""
                while [[ $# -gt 0 ]] && [[ "$1" =~ ^[0-9]+$ ]]; do
                    threads="$threads $1"
                    shift
                done
                ;;
            -r|--runs)
                runs="$2"
                shift 2
                ;;
            -o|--output)
                output_dir="$2"
                shift 2
                ;;
            --no-report)
                generate_report=false
                shift
                ;;
            --json)
                generate_json=true
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
    
    mkdir -p "$output_dir"
    
    # Build command
    local cmd="python3 $SCRIPT_DIR/benchmark.py -i $input_file"
    
    if [[ "$all_tools" == true ]]; then
        cmd="$cmd --all"
    else
        cmd="$cmd --tools $tools"
    fi
    
    cmd="$cmd -t $threads -r $runs"
    
    if [[ "$generate_report" == true ]]; then
        local report_file="$output_dir/benchmark_${TIMESTAMP}.md"
        cmd="$cmd --report $report_file"
    fi
    
    if [[ "$generate_json" == true ]]; then
        local json_file="$output_dir/benchmark_${TIMESTAMP}.json"
        cmd="$cmd --json $json_file"
    fi
    
    log_info "Running benchmark..."
    log_info "Input: $input_file"
    log_info "Command: $cmd"
    echo ""
    
    eval "$cmd"
    
    echo ""
    log_info "Benchmark complete!"
    
    if [[ "$generate_report" == true ]]; then
        log_info "Report: $report_file"
        # Create latest symlink
        ln -sf "$(basename "$report_file")" "$output_dir/benchmark_latest.md"
    fi
}

main "$@"
