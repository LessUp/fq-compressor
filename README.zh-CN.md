# fq-compressor

<p align="center">
  <b>高性能 FASTQ 压缩工具，支持 O(1) 随机访问</b>
</p>

<p align="center">
  <a href="https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml">
    <img src="https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml/badge.svg" alt="CI 状态">
  </a>
  <a href="https://github.com/LessUp/fq-compressor/releases/latest">
    <img src="https://img.shields.io/github/v/release/LessUp/fq-compressor?include_prereleases&label=发布版本" alt="最新发布">
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/许可证-MIT-green.svg" alt="许可证">
  </a>
  <a href="https://en.cppreference.com/w/cpp/23">
    <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  </a>
</p>

<p align="center">
  <a href="README.md">English</a> •
  <a href="README.zh-CN.md">简体中文</a> •
  <a href="https://github.com/LessUp/fq-compressor-rust">Rust 实现</a>
</p>

---

## 🎯 概述

**fq-compressor** 是一款用 C++23 编写的 FASTQ 压缩工具。它结合**基于组装的压缩（ABC）**与**统计上下文混合（SCM）**，在接近熵极限的压缩比下仍保留对压缩归档的 **O(1) 随机访问**能力。

主要特性：
- 无需完整解压即可随机访问 reads
- Intel oneTBB 并行流水线
- 直接读取 `.gz` FASTQ 输入，无需预先解压
- 追踪 benchmark，支持与同类工具对比（见[性能](#-性能)）

---

## 📦 安装

### 从源码构建（推荐）

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor

# 一步完成 Conan 安装 + CMake 构建
./scripts/build.sh gcc-release

# 二进制位置：build/gcc-release/src/fqc
```

**其他 preset：** `clang-debug`（开发）、`clang-release`、`gcc-debug`、`clang-asan`、`clang-tsan`。完整列表见 [AGENTS.md](AGENTS.md)。

**前置条件：** GCC 14+ 或 Clang 18+，CMake 3.28+，Conan 2.x

### 预编译二进制

Linux（glibc/musl，x86_64/aarch64）与 macOS（x86_64/arm64）的预编译二进制可在 [releases 页面](https://github.com/LessUp/fq-compressor/releases/latest) 下载。

> **注意：** v0.2.0 发布时未附带二进制资产。请使用更新的 release 或从源码构建。

---

## 🚀 用法

### 压缩与解压

```bash
# 将 FASTQ 压缩为 FQC 格式
fqc compress -i reads.fastq -o reads.fqc

# 验证归档完整性
fqc verify reads.fqc

# 完整解压
fqc decompress -i reads.fqc -o restored.fastq
```

### 高级功能

```bash
# 随机访问 — 提取第 1000-2000 条 reads
fqc decompress -i reads.fqc --range 1000:2000 -o subset.fastq

# 多线程压缩（8 线程）
fqc compress -i reads.fastq -o reads.fqc -t 8 -v

# 双端数据
fqc compress -i reads_1.fastq -2 reads_2.fastq \
  -o paired.fqc --paired

# 查看归档信息
fqc info reads.fqc
```

---

## 📊 性能

压缩比与吞吐量来自追踪 benchmark workload（结果位于 `benchmark_v2/results/`）。
fqc 使用 4 线程；对比工具单线程运行，采用各自默认参数。

### 压缩比

| Workload | fqc | gzip | xz | bzip2 |
|----------|-----|------|----|-------|
| small20k-single | 4.04× | 3.25× | 4.05× | 3.98× |
| big100k-single | 4.86× | 3.89× | 5.16× | 4.88× |
| small20k-paired | 3.94× | — | — | — |
| big100k-paired | 4.68× | — | — | — |

fqc 处于压缩比竞争力梯队，优于 gzip，与 xz、bzip2 同级（单端数据）。

### 吞吐量（big100k-single，4 线程）

| 工具 | 压缩 (MiB/s) | 解压 (MiB/s) |
|------|----------------|-----------------|
| fqc | 0.10 | 2.12 |
| gzip | 4.24 | 79.07 |
| xz | 0.24 | 27.56 |
| bzip2 | 7.93 | 4.27 |

吞吐量是已知短板：fqc 以速度换压缩比。当前流水线尚未针对吞吐量优化，
压缩耗时主要消耗在 ABC 重排序与共识生成阶段。

### 复现

```bash
# 追踪 benchmark（数据集驱动，自动准备数据，写入 benchmark_v2/results/）
./scripts/benchmark.sh --dataset err091571-local-supported --build --prepare --quick

# 通过 v2 CLI 进行 workload 级本地对比
./scripts/benchmark_v2.sh prepare \
  --workload big100k-single --data-root benchmark_v2/data \
  --output-dir benchmark_v2/data
./scripts/benchmark_v2.sh run \
  --workload big100k-single --data-root benchmark_v2/data \
  --tools fqc,gzip,xz,bzip2 --threads 4 --runs 1 \
  --json benchmark_v2/results/big100k-single.json \
  --report benchmark_v2/results/big100k-single.md
```

架构与文件格式细节请参阅 [ARCHITECTURE.md](ARCHITECTURE.md)。

---

## 📚 文档

| 文档 | 内容 |
|------|------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | 系统设计、流水线、格式与随机访问 |
| [AGENTS.md](AGENTS.md) | 构建命令、代码规范、开发流程 |
| [发布版本](https://github.com/LessUp/fq-compressor/releases) | 预编译二进制下载 |

---

## 🛠️ 开发

`fq-compressor` 当前处于 **closeout mode**。开发流程刻意保持精简：

```bash
./scripts/build.sh clang-debug
./scripts/lint.sh format-check
./scripts/test.sh clang-debug
```

### 发布检查

在打 release 标签前运行 acceptance 脚本：

```bash
./scripts/acceptance.sh
```

重新生成追踪 benchmark：

```bash
./scripts/benchmark.sh \
  --dataset err091571-local-supported \
  --build \
  --tools fqc,gzip,xz,bzip2 \
  --quick
```

workload 级本地对比请使用 `./scripts/benchmark_v2.sh run`（见[性能 → 复现](#-性能)）。

完整项目规则与架构说明请参阅 [AGENTS.md](AGENTS.md)。

---

## 🤝 贡献

欢迎贡献，尤其是：

- 文档清理
- 带回归测试的缺陷修复
- 工作流与工具链精简

仓库工作流与编码规范请参阅 [AGENTS.md](AGENTS.md)。

---

## 📄 许可证

项目代码采用 MIT 许可证 — 参见 [LICENSE](LICENSE)。

---

## 🙏 致谢

- **Spring** ([Chandak 等，2019](https://doi.org/10.1101/gr.234583.119)) — ABC 算法灵感
- **fqzcomp5** (Bonfield) — 质量压缩参考
- **Intel oneTBB** — 并行计算框架
- **贡献者** — 所有帮助改进本项目的人

---

<p align="center">
  <a href="https://github.com/LessUp/fq-compressor/releases">发布版本</a> •
  <a href="ARCHITECTURE.md">架构</a> •
  <a href="CHANGELOG.md">变更日志</a> •
  <a href="https://github.com/LessUp/fq-compressor/discussions">讨论</a>
</p>
