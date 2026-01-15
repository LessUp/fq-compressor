# 参考项目文档

本项目在开发过程中参考了以下开源项目。这些项目在 FASTQ 处理、序列压缩和并行计算方面提供了重要的技术思路和实现参考。

## 1. fastq-tools
- **路径**: `ref-projects/fastq-tools`
- **简介**: 一个面向 FASTQ 文件的现代 C++ 高性能处理工具集。
- **核心特性**:
  - 专注于生物信息学场景中的质控、过滤和统计分析。
  - 基于 **Intel oneTBB** 的并行流水线处理架构。
  - 使用 C++20 标准。
  - 提供了 `stat`（统计分析）和 `filter`（读段过滤与剪切）等核心命令。

## 2. fqzcomp5
- **路径**: `ref-projects/fqzcomp5`
- **简介**: 基于 `htscodecs` 库重新实现的 FASTQ 压缩工具（fqzcomp-4.x 的继承者）。
- **核心特性**:
  - 采用基于块（Block-based）的文件格式（通常为 100MB 到 1GB），支持更激进的多线程处理。
  - 引入了基于 rANS 和位打包（bit-packing）的快速压缩模式。
  - 支持自适应范围编码（adaptive range coding）模型，显著降低质量得分的存储空间。

## 3. HARC (Hash-based Read Compressor)
- **路径**: `ref-projects/HARC`
- **简介**: 一种专门用于基因组读取序列（Read Sequences）压缩的哈希工具。
- **核心特性**:
  - 能够实现近乎最优的压缩比和极快的解压缩速度。
  - 仅压缩序列部分，支持固定长度的 Read（最高支持 42.9 亿个 Read）。
  - 需要大量的内存（约 50 Bytes/read）来构建哈希表。

## 4. minicom
- **路径**: `ref-projects/minicom`
- **简介**: 通过索引 `(w, k)-minimizer` 覆盖重叠部分，生成长 contig 以提高 FASTQ 压缩比。
- **核心特性**:
  - 仅压缩 DNA 序列，不支持压缩整个 FASTQ 文件。
  - 利用最小化子串（Minimizers）来查找重叠并组装。
  - 提供了保序（Order-preserving）和非保序两种压缩模式。

## 5. Spring
- **路径**: `ref-projects/Spring`
- **简介**: 一个高性能的 FASTQ 压缩工具，支持高达 42.9 亿个 Read。
- **核心特性**:
  - 针对单端和双端数据提供近乎最优的压缩比。
  - 支持 **随机访问**（解压缩特定范围的 Read）。
  - 支持质量得分的各种量化处理（QVZ, Illumina 8-level binning 等）。
  - 可以通过重新排序 Read（保持配对关系）来进一步提升压缩率。
- **关键限制**:
  - **MAX_READ_LEN = 511**: 短读模式（无 `-l` 标志）的硬编码上限，深度嵌入 bitset 数据结构。
  - **长读模式 (`-l`)**: 完全绕过 ABC 算法，直接使用 BSC 通用压缩，压缩率较低。
  - 这说明 Spring 作者认为 ABC 算法对长读的收益不值得扩展。
- **本项目使用策略**:
  - 仅用于 `max_read_length <= 511` 的短读数据。
  - 超过 511bp 的数据使用 Zstd fallback 或 NanoSpring 风格的 Overlap-based 策略。

## 6. repaq
- **路径**: `ref-projects/repaq`
- **简介**: 针对 NovaSeq 数据优化的超高速 FASTQ 压缩工具。
- **核心特性**:
  - 支持将 FASTQ 压缩为 `.rfq`（极速）或 `.rfq.xz`（极高压缩比）格式。
  - 特别针对双端数据进行优化，将其压缩为单个文件以提高压缩率。
  - 压缩时间通常不到 `gzip` 的 1/5，且压缩比远高于 `gzip`。

## 7. pigz (Parallel Implementation of GZip)
- **路径**: `ref-projects/pigz`
- **简介**: `gzip` 的完全并行替代实现。
- **核心特性**:
  - 充分利用多处理器和多核心来加速压缩。
  - 与 `gzip` 的文件格式完全兼容。
  - 由 Mark Adler 编写，是处理大规模压缩任务的事实标准。

## 8. NanoSpring
- **路径**: `ref-projects/NanoSpring`
- **简介**: 专门用于纳米孔（Nanopore）长读段 FASTQ 格式的压缩工具。
- **核心特性**:
  - 仅压缩读段序列（忽略质量得分和标识符），适合与 FQC 的分流架构结合。
  - 采用 **Overlap-based** 策略（与 Spring 的 Assembly-based 不同，更适合高噪声长读）。
  - 压缩比比 `gzip` 高 3 倍以上。
  - 专门优化了对长读段和高错误率数据的处理能力。
- **与 Spring 的关系**:
  - **NanoSpring 不是 Spring 的扩展**，而是完全不同的算法。
  - Spring 使用 Minimizer + Reordering + Consensus (ABC)，适合低错误率短读。
  - NanoSpring 使用 MinHash + Minimap2 + Consensus Graph (Overlap-based)，适合高错误率长读。
- **核心算法差异**:

  | 特性 | Spring (ABC) | NanoSpring (Overlap-based) |
  |------|-------------|---------------------------|
  | 核心策略 | Minimizer + Reordering + Consensus | MinHash + Minimap2 + Consensus Graph |
  | 适用场景 | 短读 (Illumina, <500bp) | 长读 (Nanopore, 10KB+) |
  | 压缩内容 | 完整 FASTQ (Seq + Qual + ID) | 仅序列 (忽略 Qual/ID) |
  | 错误率容忍 | 低 (~0.1%) | 高 (~5-15%) |
  | 重排序 | 全局重排序 (关键优化) | 无重排序 |

- **本项目使用策略**:
  - 对于 Long Read (median >= 10KB)，**Zstd 通用压缩是推荐首选**（简单、稳定）。
  - Overlap-based 压缩作为可选优化（需适配为 Block-based 模式以支持随机访问，实现复杂度高）。
  - 不推荐在 Phase 1-3 实现 Overlap-based，可作为 Phase 5+ 的优化项。
