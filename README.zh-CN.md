# fq-compressor

[![Docs](https://img.shields.io/badge/文档-GitBook-blue?logo=gitbook)](https://lessup.github.io/fq-compressor/)
[![License](https://img.shields.io/badge/许可证-MIT-green.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/标准-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Platform](https://img.shields.io/badge/平台-Linux%20%7C%20macOS-lightgrey.svg)](https://github.com/LessUp/fq-compressor/releases)

[English](README.md) | 简体中文 | [Rust 版本 (fq-compressor-rust)](https://github.com/LessUp/fq-compressor-rust)

> **跨语言实现**：本项目同时提供 **[Rust 实现](https://github.com/LessUp/fq-compressor-rust)**（[中文发布说明](https://github.com/LessUp/fq-compressor-rust/blob/main/RELEASE_zh.md)），共享相同的 `.fqc` 归档格式与 ABC/SCM 压缩算法，以 Rayon + crossbeam 替代 Intel TBB 并引入异步 I/O。

**fq-compressor** 是一个面向 FASTQ 数据的高性能压缩工具，目标是在二代/三代测序场景下，同时兼顾高压缩比、可维护工程架构、并行处理能力与随机访问能力。

## 目录

- [核心特性](#核心特性)
- [快速开始](#快速开始)
- [项目定位](#项目定位)
- [核心算法](#核心算法)
- [工程优化](#工程优化)
- [安装](#安装)
- [使用方法](#使用方法)
- [性能基准测试](#benchmark-概览)
- [文档入口](#文档入口)
- [许可证](#许可证)

## 文档入口

| 资源 | 描述 |
|------|------|
| [GitBook 双语文档](https://lessup.github.io/fq-compressor/) | 官方文档站点 |
| [中文文档总览](docs/README.md) | 中文文档中心 |
| [规格与设计文档](docs/specs/README.md) | 技术规格说明 |
| [研究分析笔记](docs/research/) | 算法分析与参考资料 |
| [最新 Benchmark 报告](docs/benchmark/results/report-latest.md) | 最新性能数据 |

> **说明**：当前 FASTQ 输入流支持 plain text、gzip（`.gz`）、bzip2（`.bz2`）和 xz（`.xz`）。Zstandard 压缩的 FASTQ 输入（`.zst`）暂未支持。

## 项目定位

本项目采用混合压缩策略：

- **Sequence**: 基于 Spring / Mincom 思路的 **Assembly-based Compression (ABC)**
- **Quality**: 借鉴 fqzcomp5 的 **Statistical Context Mixing (SCM)**
- **Identifiers**: **Tokenization + Delta**
- **容器格式**: 自定义 **`.fqc`**，支持分块存储与随机访问

---

## 核心算法

### 1. 序列压缩：Assembly-based Compression (ABC)

不同于传统压缩器将读段视为独立字符串，我们将它们视为底层基因组的片段。

- **Minimizer 分桶**：基于共享 minimizer（签名 k-mer）将读段分组到桶中
- **重排序**：在每个桶内，对读段重新排序以形成近似哈密顿路径（最小化相邻距离）
- **局部共识与增量**：为桶生成局部共识序列，仅存储相对于共识的编辑（替换、插入缺失）
- **参考**：该方法在数学上基于 **Spring** 和 **Mincom** 的工作

### 2. 质量分数：Statistical Context Mixing (SCM)

质量分数噪声较大，不适合"组装"。我们使用高阶算术编码与上下文混合。

- **上下文建模**：压缩器基于先前上下文（如前 2 个 QV + 当前序列碱基）预测下一个质量值
- **算术编码**：预测输入自适应算术编码器，实现接近熵极限的压缩
- **参考**：深受 **fqzcomp5**（James Bonfield）启发

---

## 工程优化

### 1. 并行流水线（Intel TBB）

我们使用 **Intel Threading Building Blocks (oneTBB)** 实现 `read -> filter -> compress -> write` 流水线。

- **基于令牌的流控制**：通过限制"在途"块来控制内存使用
- **负载均衡**：TBB 的工作窃取调度器自动平衡重（重排序）和轻（I/O）任务

### 2. 列式块格式

数据在每个压缩块内拆分为物理上独立的流：

- `[Stream ID]`: 增量编码整数 + 标记化头部
- `[Stream Seq]`: ABC 编码序列增量
- `[Stream Qual]`: SCM 编码质量值

这确保最优算法独立处理每种数据类型。

### 3. 随机访问 O(1)

文件由独立的 **块**（如 10MB 未打包）组成。

- **索引**：轻量级尾部索引将块 ID 映射到文件偏移
- **上下文重置**：压缩模型在块边界重置，实现完全独立的并行解压

---

## 核心特性

- **高压缩比**
  - 面向 FASTQ 数据特征进行专项建模，而非只依赖通用压缩算法。

- **随机访问**
  - 采用 block-based 归档设计，可按范围快速解压。

- **并行流水线**
  - 基于 Intel oneTBB 组织 `read -> compress -> write` 流水线。

- **现代工程化**
  - C++23、Modern CMake、Conan 2.x、GitHub Actions。

- **多模式支持**
  - 支持单端、双端、保序/重排、无损/部分有损质量值处理。

## 快速开始

### 下载预编译二进制

```bash
# 从 GitHub Releases 下载
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.1.0-linux-x86_64-musl/fqc /usr/local/bin/
```

### 基本使用

```bash
# 压缩 FASTQ 文件
fqc compress -i input.fastq -o output.fqc

# 解压
fqc decompress -i output.fqc -o restored.fastq

# 查看归档信息
fqc info output.fqc

# 校验完整性
fqc verify output.fqc

# 部分解压（随机访问）
fqc decompress -i output.fqc --range 1000:2000 -o subset.fastq
```

---

## 安装

### 方式一：预编译二进制（推荐）

从 [GitHub Releases](https://github.com/LessUp/fq-compressor/releases) 下载：

| 平台 | 架构 | 文件 | 说明 |
|------|------|------|------|
| Linux | x86_64 | `fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz` | **静态链接，任意 Linux 可运行** |
| Linux | x86_64 | `fq-compressor-v0.1.0-linux-x86_64-glibc.tar.gz` | 动态链接 (glibc) |
| Linux | aarch64 | `fq-compressor-v0.1.0-linux-aarch64-musl.tar.gz` | **静态链接，任意 Linux 可运行** |
| macOS | x86_64 | `fq-compressor-v0.1.0-macos-x86_64.tar.gz` | Intel Mac |
| macOS | arm64 | `fq-compressor-v0.1.0-macos-arm64.tar.gz` | Apple Silicon |

### 方式二：从源码构建

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor
conan install . --build=missing -of=build/clang-release -s build_type=Release -s compiler.cppstd=20
cmake --preset clang-release
cmake --build --preset clang-release -j$(nproc)
```

### 方式三：Docker

```bash
# 构建镜像
docker build -f docker/Dockerfile -t fq-compressor .

# 运行压缩
docker run --rm -v $(pwd):/data fq-compressor compress -i /data/reads.fastq -o /data/reads.fqc

# 运行解压
docker run --rm -v $(pwd):/data fq-compressor decompress -i /data/reads.fqc -o /data/reads.fastq
```

---

## 使用方法

### 压缩

```bash
# 基本压缩
fqc compress -i input.fastq -o output.fqc

# 指定线程数和内存限制
fqc compress -i input.fastq -o output.fqc --threads 8 --memory-limit 4096

# 双端测序模式（交错格式）
fqc compress -i paired.fastq -o paired.fqc --paired

# 双端测序模式（分离文件）
fqc compress -i reads_1.fastq -i2 reads_2.fastq -o paired.fqc --paired
```

### 解压

```bash
# 完整解压
fqc decompress -i output.fqc -o restored.fastq

# 部分解压（随机访问）
fqc decompress -i output.fqc --range 1000:2000 -o subset.fastq

# 仅提取头部信息
fqc decompress -i output.fqc --header-only -o headers.txt
```

### 信息与验证

```bash
# 查看归档元数据
fqc info output.fqc

# 验证完整性
fqc verify output.fqc
```

---

## Benchmark 概览

当前仓库已包含 benchmark 框架和报告输出能力，重点关注：

- 压缩速度
- 解压速度
- 压缩率 / bits per base
- 峰值内存
- 多线程扩展性

最新公开结果可见：

- [docs/benchmark/results/report-latest.md](docs/benchmark/results/report-latest.md)
- [docs/benchmark/results/charts-latest.html](docs/benchmark/results/charts-latest.html)

## 目录治理建议

- **`docs/`**
  - 作为正式文档主目录，中文为主。

- **`docs/gitbook/`**
  - 作为 GitBook / Honkit 双语站点源码。

- **`tests/`**
  - 正式测试源码目录，应保留。

- **`Testing/`**
  - CTest 运行产物目录，不属于源码资产。

- **`ref-projects/`**
  - 更适合作为本地参考缓存，而非正式仓库结构；建议仅在文档中保留分析结果。

- **`.spec-workflow` 与 `.kiro/specs/fq-compressor`**
  - 已建议向 `docs/specs/` 收敛，后续可删除原目录。

## 文档分层

- `docs/README.md`: 中文文档入口
- `docs/specs/`: 需求、设计、实施计划
- `docs/research/`: 研究分析与参考资料
- `docs/benchmark/`: 基准测试结果
- `docs/archive/reviews/`: 历史评审记录
- `docs/gitbook/`: GitBook 双语站点

## 发布与自动化

本仓库已规划：

- GitHub Actions 持续集成
- 推送语义化 tag 后自动创建 GitHub Release
- 主分支变更后自动构建并发布 GitBook 到 GitHub Pages

## 许可证

本仓库中**项目自研代码**采用 [MIT License](LICENSE)。

`vendor/spring-core/` 下的 vendored Spring 代码继续适用其原始的
"Non-exclusive Research Use License for SPRING Software"，**不包含**在上面的 MIT 许可内。
当前默认构建在 `CMakeLists.txt` 中并未启用该目录，但如果你要重新分发或将其中代码用于商业场景，仍需单独核对该第三方许可条款。
