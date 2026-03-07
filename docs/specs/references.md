# 参考项目文档

本项目在开发过程中参考了以下开源项目。这些项目在 FASTQ 处理、序列压缩和并行计算方面提供了重要的技术思路和实现参考。

## 1. Spring

- **来源**: https://github.com/shubhamchandak94/Spring
- **简介**: 高性能 FASTQ 压缩工具，支持高达 42.9 亿个 Read。
- **核心特性**:
  - 针对单端和双端数据提供近乎最优的压缩比。
  - 支持**随机访问**（解压缩特定范围的 Read）。
  - 支持质量得分的各种量化处理（QVZ, Illumina 8-level binning 等）。
  - 可以通过重新排序 Read（保持配对关系）来进一步提升压缩率。
- **关键限制**:
  - `MAX_READ_LEN = 511`: 短读模式的硬编码上限，深度嵌入 bitset 数据结构。
  - 长读模式 (`-l`): 完全绕过 ABC 算法，直接使用 BSC 通用压缩。
- **本项目借鉴**:
  - 短读场景下 ABC 路线的核心算法参考（Minimizer Bucketing / Local Consensus / Delta Encoding）。
  - 仅用于 `max_read_length <= 511` 的短读数据。
  - 对 block-based 随机访问设计提出了适配挑战。

## 2. fastq-tools

- **来源**: 现代 C++ FASTQ 高性能处理工具集
- **简介**: 面向生物信息学场景的质控、过滤和统计分析工具。
- **核心特性**:
  - 基于 **Intel oneTBB** 的并行流水线处理架构。
  - 使用 C++20 标准。
  - 提供 `stat`（统计分析）和 `filter`（读段过滤与剪切）等核心命令。
- **本项目借鉴**:
  - 工程化基线：`.clang-format/.clang-tidy/.editorconfig` 体系。
  - 脚本组织、代码风格和流水线结构。
  - IO 层设计（扩展为块级读写）。

## 3. fqzcomp5

- **来源**: 基于 `htscodecs` 库重新实现的 FASTQ 压缩工具。
- **简介**: fqzcomp-4.x 的继承者，采用统计上下文混合压缩。
- **核心特性**:
  - 基于块（Block-based）的文件格式，支持多线程处理。
  - 引入基于 rANS 和位打包的快速压缩模式。
  - 支持自适应范围编码模型，显著降低质量得分存储空间。
- **本项目借鉴**:
  - 质量值压缩的上下文建模与统计压缩思路（SCM 策略）。
  - 范围编码实现参考。

## 4. Mincom / minicom

- **来源**: 通过索引 `(w, k)-minimizer` 覆盖重叠部分生成长 contig 以提高压缩比。
- **核心特性**:
  - 仅压缩 DNA 序列，不支持完整 FASTQ。
  - 利用 Minimizers 查找重叠并组装。
  - 提供保序和非保序两种压缩模式。
- **本项目借鉴**:
  - Minimizer Bucketing 方向的重要启发。

## 5. repaq

- **来源**: https://github.com/OpenGene/repaq
- **简介**: 针对 NovaSeq 数据优化的超高速 FASTQ 压缩工具。
- **核心特性**:
  - 支持 `.rfq`（极速）或 `.rfq.xz`（极高压缩比）格式。
  - 特别针对双端数据优化。
  - 压缩时间通常不到 gzip 的 1/5，且压缩比远高于 gzip。
- **本项目借鉴**:
  - 压缩比与性能权衡的实现思路。
  - 极致压缩比设计参考。

## 6. pigz

- **来源**: https://github.com/madler/pigz
- **简介**: gzip 的完全并行替代实现。
- **核心特性**:
  - 通过多线程实现接近线性的加速比。
  - 采用 Producer-Consumer 模型的并行架构。
- **本项目借鉴**:
  - "Read -> Compress -> Write" 三阶段 Producer-Consumer 并行架构。
  - 并行压缩模型参考。

## 7. HARC (Hash-based Read Compressor)

- **来源**: 专门用于基因组读取序列压缩的哈希工具。
- **核心特性**:
  - 近乎最优的压缩比和极快的解压缩速度。
  - 仅压缩序列部分，支持固定长度 Read。
  - 需要大量内存（约 50 Bytes/read）构建哈希表。
- **本项目借鉴**:
  - 架构设计参考。
  - 针对序列压缩与 hash 路径的补充视角。

## 8. NanoSpring

- **来源**: 面向长读数据（Nanopore）的压缩工具。
- **核心特性**:
  - 使用 MinHash + Minimap2 + Consensus Graph（与 Spring 完全不同的算法）。
  - 适用于长读（10KB+），容忍 5-15% 错误率。
  - 仅压缩序列，不处理 Quality/ID。
- **本项目借鉴**:
  - 长读 overlap-based 路线对照。
  - 反向证明：长读与短读不应强行套用同一 ABC 路线。
  - 适配为 Block 模式的复杂度分析。

## 目录治理说明

- 仓库中曾存在 `ref-projects/` 目录保存参考项目源码副本，已从正式项目结构中移除。
- 参考项目的**结论与借鉴点**保留在本文档中。
- 真正需要集成的外部代码通过 `vendor/` 管理，配套许可与构建说明。
