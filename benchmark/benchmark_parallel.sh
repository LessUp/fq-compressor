#!/usr/bin/env bash
# =============================================================================
# TBB Parallel Pipeline Performance Benchmark
# =============================================================================
# 测试不同线程数下的压缩/解压性能
#
# 用法:
#   ./benchmark_parallel.sh <input.fastq> <threads...>
#
# 示例:
#   ./benchmark_parallel.sh test.fq 1 2 4 8
# =============================================================================

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的消息
info() { echo -e "${BLUE}[INFO]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }
success() { echo -e "${GREEN}[OK]${NC} $*"; }

# 检查参数
if [ $# -lt 2 ]; then
    echo "Usage: $0 <input.fastq> <threads...>"
    echo ""
    echo "Examples:"
    echo "  $0 test.fq 1 2 4 8       # 测试 1/2/4/8 线程"
    echo "  $0 large.fq 1 4 8 16     # 测试 1/4/8/16 线程"
    exit 1
fi

INPUT_FILE="$1"
shift
THREAD_COUNTS=("$@")

# 检查输入文件
if [ ! -f "$INPUT_FILE" ]; then
    error "Input file not found: $INPUT_FILE"
    exit 1
fi

# 检查 fq-compressor 可执行文件
FQC_BIN="${FQC_BIN:-./build/gcc-release/fq-compressor}"
if [ ! -f "$FQC_BIN" ] && [ ! -x "$FQC_BIN" ]; then
    error "fq-compressor not found at: $FQC_BIN"
    error "Please set FQC_BIN environment variable or build the project first"
    exit 1
fi

# 输出目录
OUTPUT_DIR="./benchmark_results"
mkdir -p "$OUTPUT_DIR"

# 结果文件
RESULTS_CSV="$OUTPUT_DIR/parallel_benchmark_$(date +%Y%m%d_%H%M%S).csv"
RESULTS_MD="$OUTPUT_DIR/parallel_benchmark_$(date +%Y%m%d_%H%M%S).md"

info "==================================================================="
info "TBB Parallel Pipeline Performance Benchmark"
info "==================================================================="
info "Input file:     $INPUT_FILE"
info "Input size:     $(du -h "$INPUT_FILE" | cut -f1)"
info "Output dir:     $OUTPUT_DIR"
info "Threads to test: ${THREAD_COUNTS[*]}"
info "==================================================================="

# 获取输入文件统计
info "Analyzing input file..."
TOTAL_READS=$(grep -c "^@" "$INPUT_FILE" || echo "unknown")
info "Total reads:    $TOTAL_READS"

# CSV header
echo "Threads,Operation,Elapsed(s),Throughput(MB/s),InputSize(MB),OutputSize(MB),Ratio,Speedup" > "$RESULTS_CSV"

# Markdown header
cat > "$RESULTS_MD" <<EOF
# TBB Parallel Pipeline Benchmark Results

**Date**: $(date '+%Y-%m-%d %H:%M:%S')
**Input**: $INPUT_FILE
**Total Reads**: $TOTAL_READS
**Input Size**: $(du -h "$INPUT_FILE" | cut -f1)

## Compression Benchmark

| Threads | Time (s) | Throughput (MB/s) | Output Size (MB) | Ratio | Speedup |
|---------|----------|-------------------|------------------|-------|---------|
EOF

# 基准时间（单线程）
BASELINE_TIME=0

# 遍历每个线程数
for THREADS in "${THREAD_COUNTS[@]}"; do
    info ""
    info "-------------------------------------------------------------------"
    info "Testing with $THREADS thread(s)..."
    info "-------------------------------------------------------------------"

    OUTPUT_FQC="$OUTPUT_DIR/output_t${THREADS}.fqc"

    # =================================================================
    # 压缩测试
    # =================================================================

    info "[Compression] Running with $THREADS threads..."

    # 删除旧输出
    rm -f "$OUTPUT_FQC"

    # 运行压缩并计时
    START=$(date +%s.%N)
    "$FQC_BIN" compress "$INPUT_FILE" -o "$OUTPUT_FQC" \
        --threads "$THREADS" \
        --level 6 \
        --no-progress \
        > "$OUTPUT_DIR/compress_t${THREADS}.log" 2>&1
    END=$(date +%s.%N)

    ELAPSED=$(echo "$END - $START" | bc)
    INPUT_SIZE_MB=$(du -m "$INPUT_FILE" | cut -f1)
    OUTPUT_SIZE_MB=$(du -m "$OUTPUT_FQC" | cut -f1)
    THROUGHPUT=$(echo "scale=2; $INPUT_SIZE_MB / $ELAPSED" | bc)
    RATIO=$(echo "scale=2; $INPUT_SIZE_MB / $OUTPUT_SIZE_MB" | bc)

    # 计算加速比
    if [ "$THREADS" -eq "${THREAD_COUNTS[0]}" ]; then
        BASELINE_TIME="$ELAPSED"
        SPEEDUP="1.00"
    else
        SPEEDUP=$(echo "scale=2; $BASELINE_TIME / $ELAPSED" | bc)
    fi

    success "[Compression] Completed in ${ELAPSED}s (${THROUGHPUT} MB/s, ${SPEEDUP}x speedup)"

    # 记录到 CSV
    echo "$THREADS,Compression,$ELAPSED,$THROUGHPUT,$INPUT_SIZE_MB,$OUTPUT_SIZE_MB,$RATIO,$SPEEDUP" >> "$RESULTS_CSV"

    # 记录到 Markdown
    echo "| $THREADS | $ELAPSED | $THROUGHPUT | $OUTPUT_SIZE_MB | $RATIO | $SPEEDUP |" >> "$RESULTS_MD"

    # =================================================================
    # 解压测试
    # =================================================================

    OUTPUT_FASTQ="$OUTPUT_DIR/output_t${THREADS}.fq"

    info "[Decompression] Running with $THREADS threads..."

    # 删除旧输出
    rm -f "$OUTPUT_FASTQ"

    # 运行解压并计时
    START=$(date +%s.%N)
    "$FQC_BIN" decompress "$OUTPUT_FQC" -o "$OUTPUT_FASTQ" \
        --threads "$THREADS" \
        --no-progress \
        > "$OUTPUT_DIR/decompress_t${THREADS}.log" 2>&1
    END=$(date +%s.%N)

    ELAPSED=$(echo "$END - $START" | bc)
    OUTPUT_SIZE_MB=$(du -m "$OUTPUT_FASTQ" | cut -f1)
    THROUGHPUT=$(echo "scale=2; $OUTPUT_SIZE_MB / $ELAPSED" | bc)

    if [ "$THREADS" -eq "${THREAD_COUNTS[0]}" ]; then
        DECOMP_BASELINE="$ELAPSED"
        SPEEDUP="1.00"
    else
        SPEEDUP=$(echo "scale=2; $DECOMP_BASELINE / $ELAPSED" | bc)
    fi

    success "[Decompression] Completed in ${ELAPSED}s (${THROUGHPUT} MB/s, ${SPEEDUP}x speedup)"

    # 记录到 CSV
    echo "$THREADS,Decompression,$ELAPSED,$THROUGHPUT,$OUTPUT_SIZE_MB,$OUTPUT_SIZE_MB,1.0,$SPEEDUP" >> "$RESULTS_CSV"

    # =================================================================
    # 验证正确性
    # =================================================================

    info "[Verification] Comparing with original..."

    if diff -q "$INPUT_FILE" "$OUTPUT_FASTQ" > /dev/null 2>&1; then
        success "[Verification] Output matches original! ✓"
    else
        warn "[Verification] Output differs from original (this may be expected for lossy modes)"
    fi

    # 清理临时文件（可选）
    # rm -f "$OUTPUT_FQC" "$OUTPUT_FASTQ"
done

# Markdown footer
cat >> "$RESULTS_MD" <<EOF

## Decompression Benchmark

| Threads | Time (s) | Throughput (MB/s) | Speedup |
|---------|----------|-------------------|---------|
EOF

# 添加解压结果到 Markdown
for THREADS in "${THREAD_COUNTS[@]}"; do
    DECOMP_LINE=$(grep "^$THREADS,Decompression," "$RESULTS_CSV")
    ELAPSED=$(echo "$DECOMP_LINE" | cut -d',' -f3)
    THROUGHPUT=$(echo "$DECOMP_LINE" | cut -d',' -f4)
    SPEEDUP=$(echo "$DECOMP_LINE" | cut -d',' -f8)
    echo "| $THREADS | $ELAPSED | $THROUGHPUT | $SPEEDUP |" >> "$RESULTS_MD"
done

# 性能图表（需要 gnuplot）
cat >> "$RESULTS_MD" <<EOF

## Performance Chart

To generate a performance chart, install gnuplot and run:

\`\`\`bash
gnuplot <<EOF
set terminal png size 800,600
set output 'speedup.png'
set title 'TBB Parallel Speedup'
set xlabel 'Threads'
set ylabel 'Speedup (x)'
set grid
set key left top
plot '$RESULTS_CSV' using 1:8 every ::1::$(echo "${THREAD_COUNTS[@]}" | wc -w) with linespoints title 'Compression', \\
     '$RESULTS_CSV' using 1:8 every ::$(echo "${THREAD_COUNTS[@]}" | wc -w + 1) with linespoints title 'Decompression'
EOF
\`\`\`

## System Information

- **CPU**: $(lscpu | grep "Model name" | cut -d':' -f2 | xargs)
- **Cores**: $(nproc)
- **Memory**: $(free -h | grep Mem | awk '{print $2}')
- **OS**: $(uname -sr)

## Notes

- Baseline thread count: ${THREAD_COUNTS[0]}
- Compression level: 6 (default)
- All tests run with \`--no-progress\` flag

EOF

info ""
info "==================================================================="
success "Benchmark complete!"
info "==================================================================="
info "Results saved to:"
info "  - CSV:      $RESULTS_CSV"
info "  - Markdown: $RESULTS_MD"
info "  - Logs:     $OUTPUT_DIR/*.log"
info "==================================================================="
