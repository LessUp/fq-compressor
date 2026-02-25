#!/usr/bin/env bash
# =============================================================================
# fq-compressor - Spring Comparison Script
# =============================================================================
# Compares fqc compression with Spring for validation and benchmarking.
#
# Usage: ./scripts/compare_spring.sh <input.fastq> [options]
#
# Options:
#   -t <threads>    Number of threads (default: auto)
#   -o <dir>        Output directory (default: ./compare_results)
#   -k              Keep intermediate files
#   -v              Verbose output
#
# Requirements:
#   - fqc binary in PATH or ../build/build/Release/bin/
#   - spring binary in PATH or ./ref-projects/Spring/build/spring
#   - diff, md5sum
#
# Requirements validated: 1.1.2
# =============================================================================

set -euo pipefail

# Default settings
THREADS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
OUTPUT_DIR="./compare_results"
KEEP_FILES=false
VERBOSE=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Print usage
usage() {
    echo "Usage: $0 <input.fastq> [options]"
    echo ""
    echo "Options:"
    echo "  -t <threads>    Number of threads (default: auto)"
    echo "  -o <dir>        Output directory (default: ./compare_results)"
    echo "  -k              Keep intermediate files"
    echo "  -v              Verbose output"
    echo "  -h              Show this help"
    exit 1
}

# Log functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

log_verbose() {
    if [[ "$VERBOSE" == "true" ]]; then
        echo -e "[DEBUG] $*"
    fi
}

# Parse arguments
INPUT_FILE=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        -t)
            THREADS="$2"
            shift 2
            ;;
        -o)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -k)
            KEEP_FILES=true
            shift
            ;;
        -v)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        -*)
            log_error "Unknown option: $1"
            usage
            ;;
        *)
            if [[ -z "$INPUT_FILE" ]]; then
                INPUT_FILE="$1"
            else
                log_error "Multiple input files not supported"
                usage
            fi
            shift
            ;;
    esac
done

# Validate input
if [[ -z "$INPUT_FILE" ]]; then
    log_error "Input file required"
    usage
fi

if [[ ! -f "$INPUT_FILE" ]]; then
    log_error "Input file not found: $INPUT_FILE"
    exit 1
fi

# Find binaries
find_binary() {
    local name="$1"
    shift
    local paths=("$@")
    
    # Check PATH first
    if command -v "$name" &>/dev/null; then
        command -v "$name"
        return 0
    fi
    
    # Check provided paths
    for path in "${paths[@]}"; do
        if [[ -x "$path" ]]; then
            echo "$path"
            return 0
        fi
    done
    
    return 1
}

# Find fqc binary
FQC_BIN=$(find_binary "fqc" \
    "./build/clang-release/src/fqc" \
    "./build/gcc-release/src/fqc" \
    "./build/clang-debug/src/fqc" \
    "./build/gcc-debug/src/fqc") || {
    log_error "fqc binary not found. Build the project first."
    exit 1
}
log_info "Using fqc: $FQC_BIN"

# Find spring binary (optional)
SPRING_BIN=$(find_binary "spring" \
    "./ref-projects/Spring/build/spring" \
    "../ref-projects/Spring/build/spring" \
    "/usr/local/bin/spring") || {
    log_warn "Spring binary not found. Spring comparison will be skipped."
    SPRING_BIN=""
}
if [[ -n "$SPRING_BIN" ]]; then
    log_info "Using spring: $SPRING_BIN"
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Get input file info
INPUT_SIZE=$(stat -c%s "$INPUT_FILE" 2>/dev/null || stat -f%z "$INPUT_FILE")
INPUT_NAME=$(basename "$INPUT_FILE")
log_info "Input: $INPUT_FILE ($INPUT_SIZE bytes)"

# Calculate input checksum
log_info "Calculating input checksum..."
INPUT_MD5=$(md5sum "$INPUT_FILE" | cut -d' ' -f1)
log_verbose "Input MD5: $INPUT_MD5"

# =============================================================================
# FQC Compression/Decompression
# =============================================================================

log_info "=== FQC Compression ==="

FQC_COMPRESSED="$OUTPUT_DIR/${INPUT_NAME}.fqc"
FQC_DECOMPRESSED="$OUTPUT_DIR/${INPUT_NAME}.fqc.fastq"

# Compress with fqc
log_info "Compressing with fqc..."
FQC_COMPRESS_START=$(date +%s.%N)
"$FQC_BIN" compress -i "$INPUT_FILE" -o "$FQC_COMPRESSED" -t "$THREADS" 2>&1 | \
    if [[ "$VERBOSE" == "true" ]]; then cat; else cat >/dev/null; fi
FQC_COMPRESS_END=$(date +%s.%N)
FQC_COMPRESS_TIME=$(echo "$FQC_COMPRESS_END - $FQC_COMPRESS_START" | bc)

FQC_SIZE=$(stat -c%s "$FQC_COMPRESSED" 2>/dev/null || stat -f%z "$FQC_COMPRESSED")
FQC_RATIO=$(echo "scale=4; $FQC_SIZE / $INPUT_SIZE" | bc)

log_info "  Compressed size: $FQC_SIZE bytes (ratio: $FQC_RATIO)"
log_info "  Compression time: ${FQC_COMPRESS_TIME}s"

# Decompress with fqc
log_info "Decompressing with fqc..."
FQC_DECOMPRESS_START=$(date +%s.%N)
"$FQC_BIN" decompress -i "$FQC_COMPRESSED" -o "$FQC_DECOMPRESSED" -t "$THREADS" 2>&1 | \
    if [[ "$VERBOSE" == "true" ]]; then cat; else cat >/dev/null; fi
FQC_DECOMPRESS_END=$(date +%s.%N)
FQC_DECOMPRESS_TIME=$(echo "$FQC_DECOMPRESS_END - $FQC_DECOMPRESS_START" | bc)

log_info "  Decompression time: ${FQC_DECOMPRESS_TIME}s"

# Verify decompression
log_info "Verifying fqc decompression..."
FQC_DECOMPRESSED_MD5=$(md5sum "$FQC_DECOMPRESSED" | cut -d' ' -f1)
if [[ "$INPUT_MD5" == "$FQC_DECOMPRESSED_MD5" ]]; then
    echo -e "  ${GREEN}[PASS]${NC} Decompressed output matches original"
    FQC_VERIFY="PASS"
else
    echo -e "  ${RED}[FAIL]${NC} Decompressed output differs from original"
    echo -e "  Original MD5:     $INPUT_MD5"
    echo -e "  Decompressed MD5: $FQC_DECOMPRESSED_MD5"
    FQC_VERIFY="FAIL"
fi

# =============================================================================
# Spring Compression/Decompression (if available)
# =============================================================================

SPRING_SIZE=""
SPRING_RATIO=""
SPRING_COMPRESS_TIME=""
SPRING_DECOMPRESS_TIME=""
SPRING_VERIFY=""

if [[ -n "$SPRING_BIN" ]]; then
    log_info ""
    log_info "=== Spring Compression ==="
    
    SPRING_COMPRESSED="$OUTPUT_DIR/${INPUT_NAME}.spring"
    SPRING_DECOMPRESSED="$OUTPUT_DIR/${INPUT_NAME}.spring.fastq"
    
    # Compress with spring
    log_info "Compressing with spring..."
    SPRING_COMPRESS_START=$(date +%s.%N)
    "$SPRING_BIN" -c -i "$INPUT_FILE" -o "$SPRING_COMPRESSED" -t "$THREADS" 2>&1 | \
        if [[ "$VERBOSE" == "true" ]]; then cat; else cat >/dev/null; fi
    SPRING_COMPRESS_END=$(date +%s.%N)
    SPRING_COMPRESS_TIME=$(echo "$SPRING_COMPRESS_END - $SPRING_COMPRESS_START" | bc)
    
    SPRING_SIZE=$(stat -c%s "$SPRING_COMPRESSED" 2>/dev/null || stat -f%z "$SPRING_COMPRESSED")
    SPRING_RATIO=$(echo "scale=4; $SPRING_SIZE / $INPUT_SIZE" | bc)
    
    log_info "  Compressed size: $SPRING_SIZE bytes (ratio: $SPRING_RATIO)"
    log_info "  Compression time: ${SPRING_COMPRESS_TIME}s"
    
    # Decompress with spring
    log_info "Decompressing with spring..."
    SPRING_DECOMPRESS_START=$(date +%s.%N)
    "$SPRING_BIN" -d -i "$SPRING_COMPRESSED" -o "$SPRING_DECOMPRESSED" -t "$THREADS" 2>&1 | \
        if [[ "$VERBOSE" == "true" ]]; then cat; else cat >/dev/null; fi
    SPRING_DECOMPRESS_END=$(date +%s.%N)
    SPRING_DECOMPRESS_TIME=$(echo "$SPRING_DECOMPRESS_END - $SPRING_DECOMPRESS_START" | bc)
    
    log_info "  Decompression time: ${SPRING_DECOMPRESS_TIME}s"
    
    # Verify decompression
    log_info "Verifying spring decompression..."
    SPRING_DECOMPRESSED_MD5=$(md5sum "$SPRING_DECOMPRESSED" | cut -d' ' -f1)
    if [[ "$INPUT_MD5" == "$SPRING_DECOMPRESSED_MD5" ]]; then
        echo -e "  ${GREEN}[PASS]${NC} Decompressed output matches original"
        SPRING_VERIFY="PASS"
    else
        echo -e "  ${YELLOW}[NOTE]${NC} Spring decompressed output differs (may reorder reads)"
        SPRING_VERIFY="REORDERED"
    fi
fi

# =============================================================================
# Summary Report
# =============================================================================

log_info ""
log_info "=== Comparison Summary ==="
echo ""
printf "%-20s %15s %15s\n" "Metric" "FQC" "Spring"
printf "%-20s %15s %15s\n" "--------------------" "---------------" "---------------"
printf "%-20s %15d %15s\n" "Compressed Size" "$FQC_SIZE" "${SPRING_SIZE:-N/A}"
printf "%-20s %15s %15s\n" "Compression Ratio" "$FQC_RATIO" "${SPRING_RATIO:-N/A}"
printf "%-20s %15ss %15s\n" "Compress Time" "$FQC_COMPRESS_TIME" "${SPRING_COMPRESS_TIME:-N/A}s"
printf "%-20s %15ss %15s\n" "Decompress Time" "$FQC_DECOMPRESS_TIME" "${SPRING_DECOMPRESS_TIME:-N/A}s"
printf "%-20s %15s %15s\n" "Verification" "$FQC_VERIFY" "${SPRING_VERIFY:-N/A}"
echo ""

# Write JSON report
REPORT_FILE="$OUTPUT_DIR/comparison_report.json"
cat > "$REPORT_FILE" <<EOF
{
  "input": {
    "file": "$INPUT_FILE",
    "size": $INPUT_SIZE,
    "md5": "$INPUT_MD5"
  },
  "fqc": {
    "compressed_size": $FQC_SIZE,
    "ratio": $FQC_RATIO,
    "compress_time": $FQC_COMPRESS_TIME,
    "decompress_time": $FQC_DECOMPRESS_TIME,
    "verification": "$FQC_VERIFY"
  },
  "spring": {
    "compressed_size": ${SPRING_SIZE:-null},
    "ratio": ${SPRING_RATIO:-null},
    "compress_time": ${SPRING_COMPRESS_TIME:-null},
    "decompress_time": ${SPRING_DECOMPRESS_TIME:-null},
    "verification": "${SPRING_VERIFY:-null}"
  },
  "threads": $THREADS,
  "timestamp": "$(date -Iseconds)"
}
EOF
log_info "Report written to: $REPORT_FILE"

# Cleanup
if [[ "$KEEP_FILES" != "true" ]]; then
    log_verbose "Cleaning up intermediate files..."
    rm -f "$FQC_COMPRESSED" "$FQC_DECOMPRESSED"
    if [[ -n "$SPRING_BIN" ]]; then
        rm -f "$SPRING_COMPRESSED" "$SPRING_DECOMPRESSED"
    fi
fi

# Exit with appropriate code
if [[ "$FQC_VERIFY" == "PASS" ]]; then
    echo -e "${GREEN}[INFO]${NC} Comparison complete - FQC verification passed"
    exit 0
else
    echo -e "${RED}[ERROR]${NC} Comparison complete - FQC verification failed"
    exit 1
fi
