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
TEST_DIR="${FQC_PERF_TEST_DIR:-$(mktemp -d)}"
FQC_BIN="${FQC_BIN:-$PROJECT_ROOT/build/clang-release/src/fqc}"
RESULTS_DIR="${RESULTS_DIR:-$TEST_DIR/benchmark_results}"
FQC_PERF_SIZES="${FQC_PERF_SIZES:-1 2}"
FQC_PERF_MEMORY_MIB="${FQC_PERF_MEMORY_MIB:-16384}"
FQC_PERF_DATA="${FQC_PERF_DATA:-structured}"
FQC_PERF_KEEP_TEMP="${FQC_PERF_KEEP_TEMP:-0}"
FQC_PERF_ENFORCE_SLA="${FQC_PERF_ENFORCE_SLA:-0}"
FQC_PERF_REPEATS="${FQC_PERF_REPEATS:-3}"
FQC_PERF_MIN_COMPRESS_MIB_S="${FQC_PERF_MIN_COMPRESS_MIB_S:-50}"
FQC_PERF_MIN_DECOMPRESS_MIB_S="${FQC_PERF_MIN_DECOMPRESS_MIB_S:-100}"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

cleanup() {
    if [[ "$FQC_PERF_KEEP_TEMP" != "1" ]]; then
        rm -rf "$TEST_DIR"
    fi
}
trap cleanup EXIT

log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_bench() {
    echo -e "${YELLOW}[BENCH]${NC} $*"
}

median_value() {
    printf '%s\n' "$@" | sort -n | awk '
        { values[NR] = $1 }
        END {
            middle = int((NR + 1) / 2)
            if (NR % 2 == 1) {
                print values[middle]
            } else {
                print (values[middle] + values[middle + 1]) / 2
            }
        }
    '
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
    python3 - "$file" "$num_reads" "$read_len" <<'PY'
from pathlib import Path
import os
import random
import sys

path = Path(sys.argv[1])
num_reads = int(sys.argv[2])
read_len = int(sys.argv[3])
patterns = ("ACGT", "TGCA", "GATC", "CTAG", "AGTC", "TCAG")
mode = os.environ.get("FQC_PERF_DATA", "structured")
rng = random.Random(0xF0C2026)
structured_quality = "I" * read_len

with path.open("w", encoding="ascii") as fh:
    for i in range(1, num_reads + 1):
        if mode == "random":
            seq = "".join(rng.choices("ACGT", k=read_len))
            qual = "".join(rng.choices("?@ABCDEFGHI", k=read_len))
        else:
            pattern = patterns[i % len(patterns)]
            seq = (pattern * ((read_len + len(pattern) - 1) // len(pattern)))[:read_len]
            qual = structured_quality
        fh.write(f"@read_{i} length={read_len}\n{seq}\n+\n{qual}\n")
PY
}

# Run benchmark
run_benchmark() {
    local name="$1"
    local input="$2"
    local profile="$3"
    
    local compressed="$TEST_DIR/${name}.fqc"
    local decompressed="$TEST_DIR/${name}_out.fastq"
    local compress_rss_file="$TEST_DIR/${name}_compress_rss.txt"
    local decompress_rss_file="$TEST_DIR/${name}_decompress_rss.txt"
    
    local input_size=$(stat -c%s "$input" 2>/dev/null || stat -f%z "$input")
    local input_mb=$(echo "scale=2; $input_size / 1048576" | bc)
    
    log_bench "Benchmark: $name (${input_mb} MiB, profile=${profile})"
    
    # Compress. Use the median wall time and maximum RSS across repeats.
    local -a compress_times=()
    local compress_rss_kib=0
    for ((iteration = 1; iteration <= FQC_PERF_REPEATS; ++iteration)); do
        local start_time=$(date +%s.%N)
        /usr/bin/time -f '%M' -o "$compress_rss_file" \
            "$FQC_BIN" -q --memory-limit "$FQC_PERF_MEMORY_MIB" compress \
            -i "$input" -o "$compressed" --profile "$profile" -f 2>/dev/null
        local end_time=$(date +%s.%N)
        compress_times+=("$(echo "$end_time - $start_time" | bc)")
        local current_rss=$(<"$compress_rss_file")
        if ((current_rss > compress_rss_kib)); then
            compress_rss_kib=$current_rss
        fi
    done
    local compress_time=$(median_value "${compress_times[@]}")
    
    local compressed_size=$(stat -c%s "$compressed" 2>/dev/null || stat -f%z "$compressed")
    local ratio=$(echo "scale=4; $input_size / $compressed_size" | bc)
    local compress_speed=$(echo "scale=2; $input_mb / $compress_time" | bc)
    
    # Decompress with the same median/max aggregation.
    local -a decompress_times=()
    local decompress_rss_kib=0
    for ((iteration = 1; iteration <= FQC_PERF_REPEATS; ++iteration)); do
        start_time=$(date +%s.%N)
        /usr/bin/time -f '%M' -o "$decompress_rss_file" \
            "$FQC_BIN" -q --memory-limit "$FQC_PERF_MEMORY_MIB" decompress \
            -i "$compressed" -o "$decompressed" -f 2>/dev/null
        end_time=$(date +%s.%N)
        decompress_times+=("$(echo "$end_time - $start_time" | bc)")
        current_rss=$(<"$decompress_rss_file")
        if ((current_rss > decompress_rss_kib)); then
            decompress_rss_kib=$current_rss
        fi
    done
    local decompress_time=$(median_value "${decompress_times[@]}")
    local decompress_speed=$(echo "scale=2; $input_mb / $decompress_time" | bc)

    if ! cmp -s "$input" "$decompressed"; then
        echo "Error: round-trip mismatch for benchmark '$name'" >&2
        return 1
    fi
    
    # Output results
    printf "  %-20s %8.2f MiB  ratio: %.4f  compress: %6.2f MiB/s (%s KiB)  decompress: %6.2f MiB/s (%s KiB)\n" \
           "$name" "$input_mb" "$ratio" "$compress_speed" "$compress_rss_kib" \
           "$decompress_speed" "$decompress_rss_kib"
    
    # JSON output
    echo "{\"name\":\"$name\",\"profile\":\"$profile\",\"data\":\"$FQC_PERF_DATA\",\"input_mib\":$input_mb,\"ratio\":$ratio,\"compress_speed_mib_s\":$compress_speed,\"decompress_speed_mib_s\":$decompress_speed,\"compress_max_rss_kib\":$compress_rss_kib,\"decompress_max_rss_kib\":$decompress_rss_kib}" >> "$RESULTS_DIR/benchmarks.jsonl"

    if [[ "$FQC_PERF_ENFORCE_SLA" == "1" ]]; then
        if (( $(echo "$compress_speed < $FQC_PERF_MIN_COMPRESS_MIB_S" | bc -l) )); then
            echo "Error: compression SLA failed for '$name': $compress_speed < $FQC_PERF_MIN_COMPRESS_MIB_S MiB/s" >&2
            return 1
        fi
        if (( $(echo "$decompress_speed < $FQC_PERF_MIN_DECOMPRESS_MIB_S" | bc -l) )); then
            echo "Error: decompression SLA failed for '$name': $decompress_speed < $FQC_PERF_MIN_DECOMPRESS_MIB_S MiB/s" >&2
            return 1
        fi
    fi
    
    if [[ "$FQC_PERF_KEEP_TEMP" != "1" ]]; then
        rm -f "$compressed" "$decompressed"
    fi
}

main() {
    mkdir -p "$TEST_DIR"
    if ! [[ "$FQC_PERF_REPEATS" =~ ^[1-9][0-9]*$ ]]; then
        echo "Error: FQC_PERF_REPEATS must be a positive integer" >&2
        exit 1
    fi
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
    for size_mb in $FQC_PERF_SIZES; do
        local input="$TEST_DIR/test_${size_mb}mb.fastq"
        generate_fastq "$input" "$size_mb"
        run_benchmark "illumina_${size_mb}mib" "$input" illumina
        if [[ "$FQC_PERF_KEEP_TEMP" != "1" ]]; then
            rm -f "$input"
        fi
    done

    # Exercise the distinct long-read framing/profile path at the largest size.
    echo ""
    echo "=== Long-read Profile (${FQC_PERF_SIZES##* } MiB) ==="
    local profile_size="${FQC_PERF_SIZES##* }"
    local long_input="$TEST_DIR/ont_${profile_size}mib.fastq"
    generate_fastq "$long_input" "$profile_size" 20000
    run_benchmark "ont_${profile_size}mib" "$long_input" ont
    if [[ "$FQC_PERF_KEEP_TEMP" != "1" ]]; then
        rm -f "$long_input"
    fi
    
    echo ""
    echo "Results saved to: $RESULTS_DIR/benchmarks.jsonl"
    echo ""
    log_info "Performance tests complete"
}

main "$@"
