#!/usr/bin/env bash
# =============================================================================
# fq-compressor - Performance Tests
# =============================================================================
# Benchmarks compression/decompression performance with various file sizes.
#
# Requirements tested: 4.1
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEST_DIR=$(mktemp -d)
FQC_BIN="${FQC_BIN:-$PROJECT_ROOT/build/build/Release/bin/fqc}"
RESULTS_DIR="${RESULTS_DIR:-$PROJECT_ROOT/benchmark_results}"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

cleanup() {
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_bench() {
    echo -e "${YELLOW}[BENCH]${NC} $*"
}

# Generate test FASTQ with specific size
generate_fastq() {
    local file="$1"
    local target_mb="$2"
    local read_len="${3:-150}"
    
    local target_bytes=$((target_mb * 1024 * 1024))
    local bytes_per_read=$((read_len * 2 + 20))  # seq + qual + headers
    local num_reads=$((target_bytes / bytes_per_read))
    
    log_info "Generating ${target_mb}MB FASTQ with $num_reads reads..."
    
    # Use /dev/urandom for faster generation
    for ((i=1; i<=num_reads; i++)); do
        echo "@read_${i} length=${read_len}"
        head -c "$read_len" /dev/urandom 2>/dev/null | tr -dc 'ACGT' | head -c "$read_len"
        echo ""
        echo "+"
        printf 'I%.0s' $(seq 1 $read_len)
        echo ""
    done > "$file"
}

# Run benchmark
run_benchmark() {
    local name="$1"
    local input="$2"
    local threads="$3"
    
    local compressed="$TEST_DIR/${name}.fqc"
    local decompressed="$TEST_DIR/${name}_out.fastq"
    
    local input_size=$(stat -c%s "$input" 2>/dev/null || stat -f%z "$input")
    local input_mb=$(echo "scale=2; $input_size / 1048576" | bc)
    
    log_bench "Benchmark: $name (${input_mb}MB, ${threads} threads)"
    
    # Compress
    local start_time=$(date +%s.%N)
    "$FQC_BIN" compress -i "$input" -o "$compressed" -t "$threads" -q 2>/dev/null
    local end_time=$(date +%s.%N)
    local compress_time=$(echo "$end_time - $start_time" | bc)
    
    local compressed_size=$(stat -c%s "$compressed" 2>/dev/null || stat -f%z "$compressed")
    local ratio=$(echo "scale=4; $compressed_size / $input_size" | bc)
    local compress_speed=$(echo "scale=2; $input_mb / $compress_time" | bc)
    
    # Decompress
    start_time=$(date +%s.%N)
    "$FQC_BIN" decompress -i "$compressed" -o "$decompressed" -t "$threads" -q 2>/dev/null
    end_time=$(date +%s.%N)
    local decompress_time=$(echo "$end_time - $start_time" | bc)
    local decompress_speed=$(echo "scale=2; $input_mb / $decompress_time" | bc)
    
    # Output results
    printf "  %-20s %8.2f MB  ratio: %.4f  compress: %6.2f MB/s  decompress: %6.2f MB/s\n" \
           "$name" "$input_mb" "$ratio" "$compress_speed" "$decompress_speed"
    
    # JSON output
    echo "{\"name\":\"$name\",\"input_mb\":$input_mb,\"ratio\":$ratio,\"compress_speed\":$compress_speed,\"decompress_speed\":$decompress_speed,\"threads\":$threads}" >> "$RESULTS_DIR/benchmarks.jsonl"
    
    # Cleanup
    rm -f "$compressed" "$decompressed"
}

main() {
    echo "========================================"
    echo "fq-compressor Performance Tests"
    echo "========================================"
    echo ""
    
    # Check binary
    if [[ ! -x "$FQC_BIN" ]]; then
        echo "Error: fqc binary not found at $FQC_BIN"
        exit 1
    fi
    
    mkdir -p "$RESULTS_DIR"
    echo "# Benchmark results $(date -Iseconds)" > "$RESULTS_DIR/benchmarks.jsonl"
    
    # Test different file sizes
    echo ""
    echo "=== File Size Scaling ==="
    for size_mb in 1 5 10; do
        local input="$TEST_DIR/test_${size_mb}mb.fastq"
        generate_fastq "$input" "$size_mb"
        run_benchmark "size_${size_mb}mb" "$input" 1
        rm -f "$input"
    done
    
    # Test thread scaling
    echo ""
    echo "=== Thread Scaling (10MB) ==="
    local input="$TEST_DIR/thread_test.fastq"
    generate_fastq "$input" 10
    for threads in 1 2 4; do
        run_benchmark "threads_${threads}" "$input" "$threads"
    done
    rm -f "$input"
    
    echo ""
    echo "Results saved to: $RESULTS_DIR/benchmarks.jsonl"
    echo ""
    log_info "Performance tests complete"
}

main "$@"
