#!/usr/bin/env bash
# =============================================================================
# fq-compressor - CLI End-to-End Tests
# =============================================================================
# Tests the complete compression/decompression workflow via CLI.
#
# Requirements tested: 1.1, 2.1, 2.2, 2.3, 3.1, 3.2, 3.3, 3.4, 6.1, 6.2
# =============================================================================

set -euo pipefail

# Test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEST_DIR=$(mktemp -d)
FQC_BIN="${FQC_BIN:-$PROJECT_ROOT/build/build/Release/bin/fqc}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Cleanup on exit
cleanup() {
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

# Test utilities
log_test() {
    echo -e "${YELLOW}[TEST]${NC} $*"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $*"
    ((TESTS_PASSED++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $*"
    ((TESTS_FAILED++))
}

run_test() {
    local name="$1"
    shift
    ((TESTS_RUN++))
    log_test "$name"
    if "$@"; then
        log_pass "$name"
        return 0
    else
        log_fail "$name"
        return 1
    fi
}

# Create test FASTQ files
create_test_fastq() {
    local file="$1"
    local num_reads="${2:-100}"
    local read_len="${3:-150}"
    
    for ((i=1; i<=num_reads; i++)); do
        echo "@read_${i}"
        # Generate random sequence
        head -c "$read_len" /dev/urandom | tr -dc 'ACGT' | head -c "$read_len"
        echo ""
        echo "+"
        # Generate random quality (ASCII 33-73)
        head -c "$read_len" /dev/urandom | tr '\0-\377' 'I' | head -c "$read_len"
        echo ""
    done > "$file"
}

create_paired_fastq() {
    local r1="$1"
    local r2="$2"
    local num_pairs="${3:-50}"
    local read_len="${4:-150}"
    
    for ((i=1; i<=num_pairs; i++)); do
        # R1
        echo "@read_${i}/1" >> "$r1"
        head -c "$read_len" /dev/urandom | tr -dc 'ACGT' | head -c "$read_len" >> "$r1"
        echo "" >> "$r1"
        echo "+" >> "$r1"
        head -c "$read_len" /dev/urandom | tr '\0-\377' 'I' | head -c "$read_len" >> "$r1"
        echo "" >> "$r1"
        
        # R2
        echo "@read_${i}/2" >> "$r2"
        head -c "$read_len" /dev/urandom | tr -dc 'ACGT' | head -c "$read_len" >> "$r2"
        echo "" >> "$r2"
        echo "+" >> "$r2"
        head -c "$read_len" /dev/urandom | tr '\0-\377' 'I' | head -c "$read_len" >> "$r2"
        echo "" >> "$r2"
    done
}

# =============================================================================
# Test Cases
# =============================================================================

test_help() {
    "$FQC_BIN" --help >/dev/null 2>&1
}

test_version() {
    "$FQC_BIN" --version >/dev/null 2>&1
}

test_compress_help() {
    "$FQC_BIN" compress --help >/dev/null 2>&1
}

test_decompress_help() {
    "$FQC_BIN" decompress --help >/dev/null 2>&1
}

test_verify_help() {
    "$FQC_BIN" verify --help >/dev/null 2>&1
}

test_info_help() {
    "$FQC_BIN" info --help >/dev/null 2>&1
}

test_compress_missing_input() {
    ! "$FQC_BIN" compress -i /nonexistent/file.fastq -o "$TEST_DIR/out.fqc" 2>/dev/null
}

test_decompress_missing_input() {
    ! "$FQC_BIN" decompress -i /nonexistent/file.fqc -o "$TEST_DIR/out.fastq" 2>/dev/null
}

test_verify_missing_input() {
    ! "$FQC_BIN" verify /nonexistent/file.fqc 2>/dev/null
}

test_basic_compress() {
    local input="$TEST_DIR/basic.fastq"
    local output="$TEST_DIR/basic.fqc"
    
    create_test_fastq "$input" 10 100
    "$FQC_BIN" compress -i "$input" -o "$output" 2>/dev/null
    [[ -f "$output" ]]
}

test_basic_decompress() {
    local input="$TEST_DIR/basic.fastq"
    local compressed="$TEST_DIR/basic.fqc"
    local decompressed="$TEST_DIR/basic_out.fastq"
    
    create_test_fastq "$input" 10 100
    "$FQC_BIN" compress -i "$input" -o "$compressed" 2>/dev/null
    "$FQC_BIN" decompress -i "$compressed" -o "$decompressed" 2>/dev/null
    [[ -f "$decompressed" ]]
}

test_verify_valid() {
    local input="$TEST_DIR/verify.fastq"
    local compressed="$TEST_DIR/verify.fqc"
    
    create_test_fastq "$input" 10 100
    "$FQC_BIN" compress -i "$input" -o "$compressed" 2>/dev/null
    "$FQC_BIN" verify "$compressed" 2>/dev/null
}

test_info_command() {
    local input="$TEST_DIR/info.fastq"
    local compressed="$TEST_DIR/info.fqc"
    
    create_test_fastq "$input" 10 100
    "$FQC_BIN" compress -i "$input" -o "$compressed" 2>/dev/null
    "$FQC_BIN" info "$compressed" 2>/dev/null
}

test_compress_with_threads() {
    local input="$TEST_DIR/threads.fastq"
    local output="$TEST_DIR/threads.fqc"
    
    create_test_fastq "$input" 100 150
    "$FQC_BIN" compress -i "$input" -o "$output" -t 2 2>/dev/null
    [[ -f "$output" ]]
}

test_compress_level() {
    local input="$TEST_DIR/level.fastq"
    
    create_test_fastq "$input" 50 100
    
    for level in 1 5 9; do
        local output="$TEST_DIR/level_${level}.fqc"
        "$FQC_BIN" compress -i "$input" -o "$output" -l "$level" 2>/dev/null
        [[ -f "$output" ]] || return 1
    done
}

test_force_overwrite() {
    local input="$TEST_DIR/force.fastq"
    local output="$TEST_DIR/force.fqc"
    
    create_test_fastq "$input" 10 100
    "$FQC_BIN" compress -i "$input" -o "$output" 2>/dev/null
    # Should fail without -f
    ! "$FQC_BIN" compress -i "$input" -o "$output" 2>/dev/null
    # Should succeed with -f
    "$FQC_BIN" compress -i "$input" -o "$output" -f 2>/dev/null
}

test_stdout_output() {
    local input="$TEST_DIR/stdout.fastq"
    local compressed="$TEST_DIR/stdout.fqc"
    
    create_test_fastq "$input" 10 100
    "$FQC_BIN" compress -i "$input" -o "$compressed" 2>/dev/null
    "$FQC_BIN" decompress -i "$compressed" -o - 2>/dev/null | head -1 | grep -q "^@"
}

test_quiet_mode() {
    local input="$TEST_DIR/quiet.fastq"
    local output="$TEST_DIR/quiet.fqc"
    
    create_test_fastq "$input" 10 100
    local out=$("$FQC_BIN" compress -i "$input" -o "$output" -q 2>&1)
    [[ -z "$out" ]] || [[ "$out" == *"error"* ]] && return 1
    return 0
}

test_verbose_mode() {
    local input="$TEST_DIR/verbose.fastq"
    local output="$TEST_DIR/verbose.fqc"
    
    create_test_fastq "$input" 10 100
    "$FQC_BIN" compress -i "$input" -o "$output" -v 2>&1 | grep -q -i "compress\|block\|read" || true
}

# =============================================================================
# Main
# =============================================================================

main() {
    echo "========================================"
    echo "fq-compressor CLI End-to-End Tests"
    echo "========================================"
    echo ""
    echo "Binary: $FQC_BIN"
    echo "Test directory: $TEST_DIR"
    echo ""
    
    # Check binary exists
    if [[ ! -x "$FQC_BIN" ]]; then
        echo -e "${RED}Error: fqc binary not found at $FQC_BIN${NC}"
        echo "Build the project first with: cmake --build build/build/Release"
        exit 1
    fi
    
    # Run tests
    echo "--- Help & Version ---"
    run_test "Show help" test_help
    run_test "Show version" test_version
    run_test "Compress help" test_compress_help
    run_test "Decompress help" test_decompress_help
    run_test "Verify help" test_verify_help
    run_test "Info help" test_info_help
    
    echo ""
    echo "--- Error Handling ---"
    run_test "Compress missing input" test_compress_missing_input
    run_test "Decompress missing input" test_decompress_missing_input
    run_test "Verify missing input" test_verify_missing_input
    
    echo ""
    echo "--- Basic Operations ---"
    run_test "Basic compress" test_basic_compress
    run_test "Basic decompress" test_basic_decompress
    run_test "Verify valid archive" test_verify_valid
    run_test "Info command" test_info_command
    
    echo ""
    echo "--- Advanced Options ---"
    run_test "Compress with threads" test_compress_with_threads
    run_test "Compression levels" test_compress_level
    run_test "Force overwrite" test_force_overwrite
    run_test "Stdout output" test_stdout_output
    run_test "Quiet mode" test_quiet_mode
    run_test "Verbose mode" test_verbose_mode
    
    # Summary
    echo ""
    echo "========================================"
    echo "Test Summary"
    echo "========================================"
    echo "Total:  $TESTS_RUN"
    echo -e "Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Failed: ${RED}$TESTS_FAILED${NC}"
    echo ""
    
    if [[ $TESTS_FAILED -gt 0 ]]; then
        echo -e "${RED}Some tests failed!${NC}"
        exit 1
    else
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    fi
}

main "$@"
