# fq-compressor

**fq-compressor** 是一个面向测序时代的高性能下一代 FASTQ 压缩工具。它结合了最先进的**基于组装的压缩（Assembly-based Compression, ABC）**策略与工业级工程实践，旨在实现极高的压缩比、快速的并行处理以及原生随机访问能力。

## 核心特性

| 特性 | 说明 |
|------|------|
| **极致压缩** | 基于组装的重排序与共识生成，逼近理论压缩极限 |
| **混合质量压缩** | 统计上下文混合 (SCM) 处理质量值，兼顾压缩比和速度 |
| **并行引擎** | 基于 Intel oneTBB 的可扩展 Producer-Consumer 流水线 |
| **随机访问** | 原生 block-based 格式（类似 BGZF），支持任意位置快速访问 |
| **现代工程** | C++20 标准，Modern CMake + Conan 2.x + GitHub Actions CI/CD |

## 为什么选择 fq-compressor？

传统 FASTQ 压缩工具将 reads 视为独立字符串，依赖通用压缩算法。**fq-compressor** 采用本质不同的方法：

1. **Reads 是基因组的片段** — 我们通过重排序和组装 reads 来利用生物学冗余性。
2. **每种数据流使用专用压缩器** — 序列用 ABC，质量值用 SCM，标识符用 tokenization + delta 编码。
3. **归档格式面向实际使用** — 独立 block 支持随机访问、并行解压和故障隔离。

## 性能概览

| 编译器 | 压缩速度 | 解压速度 | 压缩比 |
|--------|---------|---------|--------|
| GCC    | 11.30 MB/s | 60.10 MB/s | 3.97x |
| Clang  | 11.90 MB/s | 62.30 MB/s | 3.97x |

*测试环境：Intel Core i7-9700 @ 3.00GHz（8 核），2.27M 条 Illumina reads（511 MB 未压缩）*

## 快速导航

- [安装指南](installation.md) — 从源码构建
- [快速开始](quickstart.md) — 压缩你的第一个 FASTQ 文件
- [命令行参考](cli-reference.md) — 所有命令和选项
- [架构设计](design.md) — 深入了解工作原理
