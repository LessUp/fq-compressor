## fq-compressor v0.1.0 Release Notes

### 🎉 First Stable Release

We are excited to announce the first stable release of **fq-compressor**, a high-performance FASTQ compression tool designed for the sequencing era.

---

## 🌟 Highlights

- **ABC Algorithm** - Assembly-based Compression achieves 3-5x compression ratio
- **SCM Quality** - Statistical Context Mixing for optimal quality score compression  
- **Parallel Processing** - Intel oneTBB pipeline scales to all available cores
- **Random Access** - O(1) block-level access for partial extraction
- **C++23 Modern** - Type-safe, high-performance codebase
- **Bilingual Documentation** - Full Chinese and English documentation

---

## 📊 Performance

Benchmarked on 2.27M Illumina reads (511 MB uncompressed):

| Metric | GCC 15.2 | Clang 21 |
|--------|----------|----------|
| Compression Speed | 11.30 MB/s | 11.90 MB/s |
| Decompression Speed | 60.10 MB/s | 62.30 MB/s |
| Compression Ratio | 3.97x | 3.97x |

---

## 📦 Artifacts

### Pre-built Binaries

| Platform | Architecture | File | SHA-256 |
|----------|--------------|------|---------|
| Linux | x86_64 | `fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz` | TBD |
| Linux | x86_64 | `fq-compressor-v0.1.0-linux-x86_64-glibc.tar.gz` | TBD |
| Linux | aarch64 | `fq-compressor-v0.1.0-linux-aarch64-musl.tar.gz` | TBD |
| macOS | x86_64 | `fq-compressor-v0.1.0-macos-x86_64.tar.gz` | TBD |
| macOS | arm64 | `fq-compressor-v0.1.0-macos-arm64.tar.gz` | TBD |

### Quick Start

```bash
# Linux x86_64
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.1.0-linux-x86_64-musl/fqc /usr/local/bin/

# Compress
fqc compress -i reads.fastq -o reads.fqc

# Decompress
fqc decompress -i reads.fqc -o restored.fastq
```

---

## 🆕 Features

### Core Compression
- Minimizer-based read bucketing (k=21, w=11)
- TSP-based read reordering
- Local consensus generation and delta encoding
- High-order context modeling for quality scores
- Adaptive arithmetic coding
- ID tokenization and delta encoding

### FQC Format
- Block-based storage (~10MB blocks)
- Columnar streams (ID/Seq/Qual)
- O(1) random access
- Integrity verification (CRC32C)
- Reorder map for original order restoration

### CLI
- `compress` - FASTQ to FQC with multiple modes
- `decompress` - Full and partial decompression
- `info` - Archive metadata (JSON/text)
- `verify` - Integrity verification
- `split-pe` - Paired-end splitting

### Platform Support
- Linux x86_64 (glibc 2.31+, musl)
- Linux ARM64
- macOS Intel
- macOS Apple Silicon

---

## 📚 Documentation

- [Full Documentation](https://lessup.github.io/fq-compressor/) (GitBook)
- [Installation Guide](docs/en/getting-started/installation.md)
- [Quick Start](docs/en/getting-started/quickstart.md)
- [CLI Reference](docs/en/getting-started/cli-usage.md)
- [Architecture](docs/en/core-concepts/architecture.md)
- [Algorithms](docs/en/core-concepts/algorithms.md)

---

## 🙏 Acknowledgments

- **Spring** (Chandak et al., 2019) - ABC algorithm inspiration
- **fqzcomp5** (Bonfield) - Quality compression reference
- **Intel oneTBB** - Parallel computing framework

---

## 📄 License

- Project code: MIT License
- `vendor/spring-core/`: Spring's original research license

---

## 🐛 Known Issues

- Zstandard-compressed FASTQ input (`.zst`) not yet supported
- Long-read optimization (PacBio/Nanopore) upcoming

---

## 🔗 Links

- [Repository](https://github.com/LessUp/fq-compressor)
- [Documentation](https://lessup.github.io/fq-compressor/)
- [Issues](https://github.com/LessUp/fq-compressor/issues)
- [Discussions](https://github.com/LessUp/fq-compressor/discussions)

---

**Full Changelog**: [CHANGELOG.md](CHANGELOG.md) | [中文变更日志](CHANGELOG.zh-CN.md)

---

## fq-compressor v0.1.0 发布说明

### 🎉 首个稳定版本

我们很高兴宣布 **fq-compressor** 的首个稳定版本，这是一款面向测序时代的高性能 FASTQ 压缩工具。

---

## 🌟 亮点

- **ABC 算法** - 基于组装的压缩，实现 3-5 倍压缩比
- **SCM 质量分数** - 统计上下文混合，最优质量分数压缩
- **并行处理** - 基于 Intel oneTBB 的流水线，可扩展到所有可用核心
- **随机访问** - O(1) 块级访问，支持部分提取
- **现代 C++23** - 类型安全、高性能的代码库
- **双语文档** - 完整的中文和英文文档

---

## 📊 性能

在 2.27M Illumina reads（511 MB 未压缩）上测试：

| 指标 | GCC 15.2 | Clang 21 |
|--------|----------|----------|
| 压缩速度 | 11.30 MB/s | 11.90 MB/s |
| 解压速度 | 60.10 MB/s | 62.30 MB/s |
| 压缩比 | 3.97x | 3.97x |

---

## 📦 二进制文件

### 预构建二进制文件

| 平台 | 架构 | 文件 | SHA-256 |
|----------|--------------|------|---------|
| Linux | x86_64 | `fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz` | 待定 |
| Linux | x86_64 | `fq-compressor-v0.1.0-linux-x86_64-glibc.tar.gz` | 待定 |
| Linux | aarch64 | `fq-compressor-v0.1.0-linux-aarch64-musl.tar.gz` | 待定 |
| macOS | x86_64 | `fq-compressor-v0.1.0-macos-x86_64.tar.gz` | 待定 |
| macOS | arm64 | `fq-compressor-v0.1.0-macos-arm64.tar.gz` | 待定 |

### 快速开始

```bash
# Linux x86_64
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.1.0-linux-x86_64-musl/fqc /usr/local/bin/

# 压缩
fqc compress -i reads.fastq -o reads.fqc

# 解压
fqc decompress -i reads.fqc -o restored.fastq
```

---

## 🆕 特性

### 核心压缩
- 基于 Minimizer 的 reads 分桶（k=21, w=11）
- 基于 TSP 的 reads 重排序
- 局部共识生成和增量编码
- 质量分数的高阶上下文建模
- 自适应算术编码
- ID 标记化和增量编码

### FQC 格式
- 基于块的存储（~10MB 块）
- 列式流（ID/序列/质量）
- O(1) 随机访问
- 完整性验证（CRC32C）
- 用于原始顺序恢复的重排序映射

### 命令行
- `compress` - FASTQ 到 FQC，支持多种模式
- `decompress` - 完整和部分解压
- `info` - 归档元数据（JSON/文本）
- `verify` - 完整性验证
- `split-pe` - 双端测序分离

### 平台支持
- Linux x86_64（glibc 2.31+，musl）
- Linux ARM64
- macOS Intel
- macOS Apple Silicon

---

## 📚 文档

- [完整文档](https://lessup.github.io/fq-compressor/)（GitBook）
- [安装指南](docs/zh/getting-started/installation.md)
- [快速入门](docs/zh/getting-started/quickstart.md)
- [CLI 参考](docs/zh/getting-started/cli-usage.md)
- [架构](docs/zh/core-concepts/architecture.md)
- [算法](docs/zh/core-concepts/algorithms.md)

---

## 🙏 致谢

- **Spring** (Chandak et al., 2019) - ABC 算法灵感
- **fqzcomp5** (Bonfield) - 质量压缩参考
- **Intel oneTBB** - 并行计算框架

---

## 📄 许可证

- 项目代码：MIT 许可证
- `vendor/spring-core/`：Spring 原始研究许可证

---

## 🐛 已知问题

- 暂不支持 Zstandard 压缩的 FASTQ 输入（`.zst`）
- 长读优化（PacBio/Nanopore）即将到来

---

## 🔗 链接

- [仓库](https://github.com/LessUp/fq-compressor)
- [文档](https://lessup.github.io/fq-compressor/)
- [Issues](https://github.com/LessUp/fq-compressor/issues)
- [Discussions](https://github.com/LessUp/fq-compressor/discussions)

---

**完整变更日志**：[CHANGELOG.md](CHANGELOG.md) | [中文变更日志](CHANGELOG.zh-CN.md)
