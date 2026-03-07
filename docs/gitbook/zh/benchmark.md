# 性能基准

fq-compressor 包含完整的基准测试框架，用于评估编译器优化和多维性能分析。

## 最新结果

| 编译器 | 压缩速度 | 解压速度 | 压缩比 | Bits/Base |
|--------|---------|---------|--------|-----------|
| GCC 13 | 11.30 MB/s | 60.10 MB/s | 3.97x | ~2.01 |
| Clang 16 | 11.90 MB/s | 62.30 MB/s | 3.97x | ~2.01 |

*测试环境：Intel Core i7-9700 @ 3.00GHz，8 核，2.27M 条 Illumina reads（511 MB 未压缩）*

## 关键指标

| 指标 | 说明 |
|------|------|
| **压缩速度 (MB/s)** | 压缩过程中的吞吐量 |
| **解压速度 (MB/s)** | 解压过程中的吞吐量 |
| **压缩比** | 原始大小 / 压缩大小 |
| **Bits per Base** | 每个 DNA 碱基的压缩位数 |
| **峰值内存 (MB)** | 操作期间的最大内存使用 |
| **扩展性** | 多线程带来的性能提升 |
| **并行效率** | 实际加速比 / 理想加速比 |

## 运行基准测试

### 编译器对比

```bash
# 构建 GCC 和 Clang Release 版本
./scripts/build.sh gcc-release 4
./scripts/build.sh clang-release 4

# 运行编译器性能对比并生成可视化
python3 benchmark/compiler_benchmark.py \
  --input data/reads.fastq.gz \
  --gcc-binary build/gcc-release/src/fqc \
  --clang-binary build/clang-release/src/fqc \
  -t 1 4 8 \
  -r 3 \
  --output-dir docs/benchmark \
  --visualize
```

### 标准基准测试

```bash
# 运行基准测试套件
python3 benchmark/benchmark.py \
  --input data/reads.fastq.gz \
  --binary build/clang-release/src/fqc \
  -t 4 \
  -r 5

# 生成可视化
python3 benchmark/visualize_benchmark.py \
  --input docs/benchmark/results/benchmark-results.json
```

## 生成的报告

基准测试框架自动生成以下输出：

| 输出文件 | 说明 |
|---------|------|
| `benchmark-report.html` | 包含嵌入式可视化的综合 HTML 报告 |
| `benchmark-report.md` | Markdown 格式报告 |
| `benchmark-results.json` | JSON 格式原始数据 |
| `compression-speed.png` | 压缩吞吐量对比 |
| `decompression-speed.png` | 解压吞吐量对比 |
| `compression-ratio.png` | 压缩效率对比 |
| `memory-usage.png` | 峰值内存分析 |
| `scalability.png` | 并行扩展性指标 |

## 扩展性

| 线程数 | 压缩速度 | 解压速度 | 效率 |
|--------|---------|---------|------|
| 1 | ~6 MB/s | ~30 MB/s | 100% |
| 4 | ~12 MB/s | ~60 MB/s | ~50% |
| 8 | ~18 MB/s | ~90 MB/s | ~38% |

*效率 = 实际加速比 / 理想加速比。由于 I/O 瓶颈和内存带宽限制，亚线性扩展是预期的。*
