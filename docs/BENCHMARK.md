# fq-compressor Benchmark Framework

本文档详细说明了 fq-compressor 的性能评估框架、如何运行基准测试以及如何解释结果。

## 📊 Benchmark框架概览

fq-compressor 包含一个完整的性能评估框架，用于：

1. **编译器性能对比**: 比较 GCC 和 Clang 编译优化的影响
2. **多维度性能分析**: 评估压缩/解压速度、压缩率、内存使用、并行扩展性
3. **可视化报告生成**: 自动生成 PNG 图表、Markdown 和 HTML 格式的综合报告

## 🔧 快速开始

### 构建性能基准

首先，构建 GCC Release 和 Clang Release 两个版本：

```bash
# 构建 GCC Release 版本
./scripts/build.sh gcc-release 4

# 构建 Clang Release 版本
./scripts/build.sh clang-release 4
```

### 运行编译器对比基准测试

使用提供的测试数据运行编译器性能对比：

```bash
python3 benchmark/compiler_benchmark.py \
  --input fq-data/E150035817_L01_1201_1.sub10.fq.gz \
  --gcc-binary build/gcc-release/src/fqc \
  --clang-binary build/clang-release/src/fqc \
  -t 1 4 8 \
  -r 3 \
  --output-dir docs/benchmark \
  --visualize
```

参数说明：
- `--input`: 输入 FASTQ 或 FASTQ.GZ 文件
- `--gcc-binary`: GCC 编译的 fqc 二进制文件路径
- `--clang-binary`: Clang 编译的 fqc 二进制文件路径
- `-t, --threads`: 要测试的线程数（空格分隔）
- `-r, --runs`: 每个配置运行的次数（默认：3）
- `--output-dir`: 输出报告目录（默认：docs/benchmark）
- `--visualize`: 生成可视化图表和报告

## 📈 评估指标

Benchmark 框架评估以下关键指标：

### 1. 压缩性能
- **压缩速度 (MB/s)**: 每秒处理的原始数据量
- **压缩率**: 压缩后大小 / 原始大小（越小越好）
- **比特/碱基**: 每个碱基的比特数（越小越好，理论极限 ~0.4-0.5）
- **内存使用 (MB)**: 压缩过程中的峰值内存使用

### 2. 解压性能
- **解压速度 (MB/s)**: 每秒处理的压缩数据量
- **内存使用 (MB)**: 解压过程中的峰值内存使用

### 3. 并行扩展性
- **加速比**: 相对于单线程的性能改进倍数
- **并行效率**: (最大线程数时的加速比) / (最大线程数)

## 📊 输出文件

运行基准测试会生成以下文件：

```
docs/benchmark/
├── benchmark-results.json           # 原始数据（JSON格式）
├── benchmark-report.md              # Markdown格式报告
├── benchmark-report.html            # HTML格式报告（自包含）
├── compression-speed.png            # 压缩速度对比图表
├── decompression-speed.png          # 解压速度对比图表
├── compression-ratio.png            # 压缩率对比图表
├── memory-usage.png                 # 内存使用对比图表
└── scalability.png                  # 并行扩展性分析图表
```

### 查看报告

- **HTML报告**: 在浏览器中打开 `benchmark-report.html`（推荐，包含所有图表）
- **Markdown报告**: 在任何Markdown查看器中打开 `benchmark-report.md`
- **原始数据**: JSON 文件可用于自定义分析

## 🔬 基准测试工具

### compiler_benchmark.py

专门用于 GCC vs Clang 编译器性能对比的工具。

**特点**：
- 自动解压 `.gz` 输入文件
- 测量压缩/解压时间和内存使用
- 支持自定义线程数
- 生成多格式报告（JSON、Markdown、HTML）

**使用示例**：

```bash
# 基础使用
python3 benchmark/compiler_benchmark.py \
  --input data.fq.gz \
  --gcc-binary build/gcc-release/src/fqc \
  --clang-binary build/clang-release/src/fqc

# 完整使用（含所有选项）
python3 benchmark/compiler_benchmark.py \
  --input data.fq.gz \
  --gcc-binary build/gcc-release/src/fqc \
  --clang-binary build/clang-release/src/fqc \
  --threads 1 2 4 8 \
  --runs 5 \
  --report reports/gcc_vs_clang.md \
  --json reports/benchmark.json \
  --output-dir reports/gcc_vs_clang \
  --visualize
```

### benchmark.py

通用基准框架，支持多个压缩工具的对比。

**支持的工具**：
- `fqc`: fq-compressor（我们的工具）
- `spring`: Spring 压缩器
- `gzip`: 标准 gzip
- `pigz`: 并行 gzip
- `zstd`: Zstandard 压缩
- `xz`: XZ 压缩
- `bzip2`: bzip2 压缩

**使用示例**：

```bash
# 对比 fqc 和 gzip
python3 benchmark/benchmark.py \
  --input data.fq \
  --tools fqc,gzip \
  --threads 1 4 8 \
  --report reports/fqc_vs_gzip.md

# 测试所有可用工具
python3 benchmark/benchmark.py \
  --input data.fq \
  --all \
  --report reports/all_tools.md
```

### visualize_benchmark.py

独立的可视化工具，用于生成图表和报告。

```bash
# 生成所有格式（PNG、Markdown、HTML）
python3 benchmark/visualize_benchmark.py \
  --json benchmark-results.json \
  --output-dir docs/benchmark

# 仅生成 PNG 图表
python3 benchmark/visualize_benchmark.py \
  --json benchmark-results.json \
  --output-dir docs/benchmark \
  --format png
```

## 📋 测试数据

项目包含以下测试数据集用于基准测试：

```
fq-data/
├── E150035817_L01_1201_1.sub10.fq.gz   # 157 MB (压缩后)
└── E150035817_L01_1201_2.sub10.fq.gz   # 165 MB (压缩后)
```

这些是真实的 FASTQ 测试数据，用于性能评估。

## 🎯 性能目标

fq-compressor 的设计目标：

| 指标 | 目标 | 备注 |
|------|------|------|
| 压缩比 | 0.4-0.6 bits/base | 接近理论上限 |
| 压缩速度 | >10 MB/s | 单线程 Release 构建 |
| 并行效率 | >70% | 8线程相对单线程 |
| 内存使用 | <2GB | 对于 100MB+ 文件 |

## 📊 示例报告解读

### 压缩速度图表

- X轴: 线程数（1, 4, 8）
- Y轴: 速度（MB/s）
- 如果 Clang > GCC，说明 Clang 编译优化更好

### 并行扩展性图表

- X轴: 线程数
- Y轴: 加速比（相对于单线程）
- 虚线: 理想的线性加速
- 实线: 实际加速比

如果实线接近虚线，说明并行扩展性好。

### 内存使用图表

- 左侧: 压缩内存使用
- 右侧: 解压内存使用
- 关键指标：看峰值是否随线程数线性增长

## 🔍 故障排除

### "Binary not found" 错误

确保已经构建了对应的二进制文件：

```bash
./scripts/build.sh gcc-release 4
./scripts/build.sh clang-release 4
```

### "Timeout" 错误

对于大文件，增加超时时间：

```bash
# 修改 compiler_benchmark.py 中的 timeout 参数
# 或使用较小的测试文件
```

### matplotlib 字体警告

这是正常的中文字体问题，不影响图表生成。如果需要中文标签，请安装中文字体：

```bash
# Ubuntu/Debian
sudo apt install fonts-noto-cjk

# 然后重新运行 visualization
```

## 🚀 持续集成

Benchmark 测试可以集成到 CI/CD 流水线中：

```yaml
# GitHub Actions 示例
- name: Run Compiler Benchmark
  run: |
    python3 benchmark/compiler_benchmark.py \
      --input fq-data/E150035817_L01_1201_1.sub10.fq.gz \
      --gcc-binary build/gcc-release/src/fqc \
      --clang-binary build/clang-release/src/fqc \
      --output-dir docs/benchmark \
      --visualize
```

## 📚 深入了解

### JSON 数据格式

`benchmark-results.json` 包含以下结构：

```json
{
  "timestamp": "ISO8601时间戳",
  "input_file": "输入文件路径",
  "input_size": 字节数,
  "threads_tested": [线程数数组],
  "results": [
    {
      "compiler": "GCC|Clang",
      "operation": "compress|decompress",
      "input_size": 字节数,
      "output_size": 字节数,
      "elapsed_seconds": 秒数,
      "threads": 线程数,
      "success": 布尔值,
      "peak_memory_mb": 内存MB
    }
  ]
}
```

## 🤝 贡献

欢迎改进 benchmark 框架！可能的改进方向：

- [ ] 支持多个输入文件的批量测试
- [ ] 生成性能趋势图（多个运行的对比）
- [ ] 支持远程基准服务器对接
- [ ] 自动性能回归检测

---

**最后更新**: 2026-01-27
**维护者**: fq-compressor 团队
