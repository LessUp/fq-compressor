# fq-compressor

<p align="center">
  <a href="https://lessup.github.io/fq-compressor/"><img src="https://img.shields.io/badge/文档-GitBook-blue?logo=gitbook" alt="文档"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/许可证-MIT-green.svg" alt="许可证"></a>
  <a href="https://en.cppreference.com/w/cpp/23"><img src="https://img.shields.io/badge/标准-C%2B%2B23-blue.svg" alt="C++23"></a>
  <a href="https://github.com/LessUp/fq-compressor/releases"><img src="https://img.shields.io/badge/平台-Linux%20%7C%20macOS-lightgrey.svg" alt="平台"></a>
  <a href="https://github.com/LessUp/fq-compressor/releases/latest"><img src="https://img.shields.io/github/v/release/LessUp/fq-compressor?include_prereleases" alt="发布"></a>
</p>

<p align="center">
  <b>面向测序时代的高性能 FASTQ 压缩工具</b>
</p>

<p align="center">
  <a href="README.md">English</a> | 简体中文 | <a href="https://github.com/LessUp/fq-compressor-rust">Rust 实现</a>
</p>

---

## 🚀 快速开始

### 安装

```bash
# Linux / macOS (x86_64)
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.1.0-linux-x86_64-musl/fqc /usr/local/bin/
```

所有平台的安装方法请参见 [安装指南](docs/zh/getting-started/installation.md)。

### 基本用法

```bash
# 压缩
fqc compress -i reads.fastq -o reads.fqc

# 验证并解压
fqc verify reads.fqc
fqc decompress -i reads.fqc -o restored.fastq

# 部分提取（随机访问）
fqc decompress -i reads.fqc --range 1000:2000 -o subset.fastq
```

---

## ✨ 特性

| 特性 | 描述 |
|------|------|
| 🧬 **ABC 算法** | 基于组装的压缩，通过 minimizer 分桶和增量编码实现 3-5 倍压缩比 |
| 📊 **SCM 质量分数** | 统计上下文混合，实现质量分数的最优压缩 |
| ⚡ **并行处理** | 基于 Intel oneTBB 的流水线，可扩展到所有可用核心 |
| 🎯 **随机访问** | O(1) 块级访问，无需完整解压即可部分提取 |
| 🔧 **现代 C++23** | 类型安全、高性能、可维护的代码库 |
| 📦 **多格式支持** | 透明读写 .gz、.bz2、.xz 文件 |

---

## 📊 性能

基准测试：2.27M Illumina reads（511 MB 未压缩）

| 指标 | GCC 15.2 | Clang 21 |
|------|----------|----------|
| **压缩速度** | 11.30 MB/s | 11.90 MB/s |
| **解压速度** | 60.10 MB/s | 62.30 MB/s |
| **压缩比** | 3.97x | 3.97x |

*平台：Intel i7-9700（8 核），Linux 6.8*

详细结果请参见 [基准测试](docs/zh/performance/benchmark.md)。

---

## 🔬 核心算法

### 基于组装的压缩（ABC）

将 reads 视为基因组片段而非独立字符串：

1. **Minimizer 分桶** - 按共享 k-mer 签名分组 reads
2. **TSP 重排序** - 对 reads 排序以最大化相邻相似性
3. **共识生成** - 为每个桶构建局部共识序列
4. **增量编码** - 存储相对共识的编辑内容

### 统计上下文混合（SCM）

针对质量分数的高阶上下文建模：

- 上下文：前序 QVs + 当前碱基 + 位置
- 自适应算术编码
- 接近熵极限的压缩

---

## 📚 文档

| 资源 | 描述 |
|------|------|
| [📖 完整文档](https://lessup.github.io/fq-compressor/) | GitBook 站点（双语） |
| [🚀 快速入门](docs/zh/getting-started/quickstart.md) | 5 分钟内开始 |
| [📦 安装指南](docs/zh/getting-started/installation.md) | 所有平台和方法 |
| [⌨️ CLI 参考](docs/zh/getting-started/cli-usage.md) | 完整命令参考 |
| [🏗️ 架构](docs/zh/core-concepts/architecture.md) | 高层设计 |
| [🧬 算法](docs/zh/core-concepts/algorithms.md) | 压缩算法深入解析 |
| [📋 格式规范](docs/zh/core-concepts/fqc-format.md) | FQC 归档格式规范 |

---

## 🛠️ 从源码构建

### 前置条件

- GCC 14+ 或 Clang 18+
- CMake 3.28+
- Conan 2.x

### 构建

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor

conan install . --build=missing -of=build/clang-release \
    -s build_type=Release -s compiler.cppstd=23

cmake --preset clang-release
cmake --build --preset clang-release -j$(nproc)

# 二进制文件位置：build/clang-release/src/fqc
```

详情请参见 [构建指南](docs/zh/development/build-from-source.md)。

---

## 🐳 Docker

```bash
docker build -f docker/Dockerfile -t fq-compressor .
docker run --rm -v $(pwd):/data fq-compressor compress -i /data/reads.fastq -o /data/reads.fqc
```

---

## 🧪 测试

```bash
# 运行所有测试
./scripts/test.sh clang-release

# 运行特定测试
cmake --build --preset clang-release --target memory_budget_test
build/clang-release/tests/memory_budget_test
```

---

## 🗺️ 路线图

- [ ] 长读优化（PacBio/Nanopore）
- [ ] 流式压缩模式
- [ ] 额外的有损质量模式
- [ ] AWS S3/GCS 原生集成
- [ ] Python 绑定

---

## 🤝 贡献

欢迎贡献！请参见我们的 [贡献指南](docs/zh/development/contributing.md)。

---

## 📄 许可证

- **项目代码**：MIT 许可证 - 参见 [LICENSE](LICENSE)
- `vendor/spring-core/`：Spring 原始研究许可证（非 MIT）

---

## 🙏 致谢

- **Spring** ([Chandak et al., 2019](https://doi.org/10.1101/gr.234583.119)) - ABC 算法灵感
- **fqzcomp5** (Bonfield) - 质量压缩参考
- **Intel oneTBB** - 并行计算框架

---

<p align="center">
  <a href="https://github.com/LessUp/fq-compressor/releases">发布</a> •
  <a href="https://lessup.github.io/fq-compressor/">文档</a> •
  <a href="CHANGELOG.zh-CN.md">变更日志</a>
</p>
