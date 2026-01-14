# Requirements Document

## Introduction

fq-compressor 是一个高性能 FASTQ 文件压缩工具，结合了 Spring 的先进压缩算法和 fastq-tools 的现代化高性能框架。该项目旨在提供：

- **核心算法**: **Sequence** 采用 **Assembly-based Compression (ABC)**（Spring/Mincom：compress the edits），**Quality** 采用 **fqzcomp5 风格 SCM**（上下文算术编码），**IDs** 采用 **Tokenization + Delta**。
- **高性能框架**: 基于 **fastq-tools** (C++20, Modern CMake) 和 **Intel TBB** (并行流水线)。
- **借鉴项目**:
    - **@Spring / @Mincom**: ABC 核心算法（minimizer bucketing / local consensus / delta）。
    - **@fastq-tools**: 基础框架、IO、代码风格。
    - **@repaq**: 极致压缩比设计参考。
    - **@fqzcomp5 / @DSRC**: 算术编码、容器格式与质量值压缩参考。
    - **@pigz**: 并行压缩模型（Producer-Consumer）参考。
    - **@HARC**: 参考其架构设计。
- **支持场景**: 二代测序 (Illumina) 和 三代测序 (Nanopore/PacBio)。
- **目标**: 自用学习，定义新格式，支持随机访问。

## 可行性评估

### 技术可行性分析

| 评估维度 | 方案选择 | 理由/来源 |
|---------|----------|-----------|
| **C++ 标准** | C++20 | 现代特性（Concepts, Ranges），符合 fastq-tools 风格 |
| **并行框架** | Intel oneTBB | 强大的任务图和流水线支持（Pipeline/Task Graph） |
| **CLI 库** | **CLI11** | Header-only，功能丰富，优于 Boost.PO 的重型依赖 |
| **日志库** | **Quill** | 极低延迟异步日志，适合高性能 IO 密集型应用 |
| **压缩算法** | Spring Core (ABC) + fqzcomp5-style (Quality) + External | Sequence: Spring/Mincom ABC；Quality: SCM；External (libdeflate/lzma/...) 用于通用子流 |
| **随机访问** | **Scheme A** | 分块独立压缩 (Block-based)，支持 O(1) 随机访问 |

### 核心策略选型评估 (Strategy Evaluation)

针对 **有/无参考序列** 和 **有损/无损** 的深度评估与决策：

#### 1. 参考序列依赖 (Reference-based vs Reference-free)
| 模式 | 优势 | 劣势 | **本项目决策** |
|-----|-----|-----|---------------|
| **有参考 (Ref-based)**<br>(类 CRAM) | 极高压缩比 (只需存变异) | 依赖外部 Reference 文件，管理困难（Reference丢失=数据丢失）；压缩速度受比对限制；不适用 De novo 或宏基因组。 | **不采用 (或作为远期扩展)**<br>理由：作为通用归档工具，自包含性 (Self-contained) 至关重要。 |
| **无参考 (Ref-free)**<br>(类 Spring/Mincom) | **自包含**，无依赖，适用任何数据；管理简单。 | 理论上限略低于 Ref-based。 | **✅ 采用 (Primary)**<br>理由：Spring 算法本身通过 **Reads重排序** 构建"内部参考"，已能达到近乎 Ref-based 的压缩率，且无外部依赖。 |

#### 2. 有损 vs 无损 (Lossy vs Lossless)
| 数据类型 | 策略选型 | 方案详情 |
|---------|----------|----------|
| **序列 (Sequence)** | **必须无损 (Mandatory Lossless)** | 序列包含核心生物学信息，除用户显式要求 Trim/Filter 外，压缩过程严禁修改碱基。 |
| **质量值 (Quality)** | **混合模式 (Hybrid)** | **默认无损**：用于高精度变异检测。<br>**可选有损**：<br>1. **Illumina 8-bin**: 官方推荐的有损，几乎不影响下游。<br>2. **QVZ/Spring-Lossy**: 基于模型的有损，压缩比极高。 |
| **ID (Headers)** | **可配置 (Configurable)** | **默认无损**：保留原始 Header。<br>**结构化重建**：对于标准 Illumina Header，仅存元数据，解压时重建（无损但压缩率高）。<br>**丢弃/索引化**：仅保留顺序 ID (1, 2, 3...)，用于极度压缩。 |


### 架构兼容性

1.  **IO 层**: 复用 fastq-tools 的设计，增加块级 (Block-level) 读写支持。
2.  **处理流水线**: 采用 TBB `parallel_pipeline`，模仿 **pigz** 的 "Read -> Compress -> Write" 模型。
3.  **压缩核心**: 将 Spring 的算法解耦为 "Block Context"，使其支持无状态/重置状态的压缩。
4.  **扩展性**: 格式设计预留 Flag 位，支持多种压缩后端。

## Requirements

### Requirement 1.1: 混合压缩策略 (Hybrid Compression Strategy - Strategy B)
**User Story:** 追求极致压缩率，采用学术界最前沿的 **Assembly-based Compression (ABC)** 路线，并以 **Spring Core Integration** 作为工程落地方式。

#### Acceptance Criteria
1.  **流分离 (Stream Separation)**:
    -   必须将 FASTQ 数据拆分为独立的逻辑流：**Identifier Stream**, **Sequence Stream**, **Quality Stream**。
2.  **核心算法路线 (State-of-the-Art)**:
    -   **Sequence**: 采用 **ABC 策略** (Minimizer Bucketing -> Local Consensus -> Delta Encoding -> Arithmetic Coding；桶内可选 Reordering)。
        -   *Implementation*: **Vendor/Fork Spring Core**（encoder/decoder）。直接集成修改版的 **Spring** 核心算法库，而非从零重写，以确保正确性和性能。
    -   **Quality**: 采用 **Statistical Context Mixing (SCM)** (参考 Fqzcomp5)。
        -   使用 Order-1 或 Order-2 上下文算术编码。
        -   *Non-goal*: SCM 仅用于质量值；Sequence 不采用 fqzcomp5 的纯统计模型作为主路线。
    -   **IDs**: 采用 **Delta + Tokenization**。
3.  **技术落地**:
    -   必须处理 Spring 代码的模块化封装，使其适配 Block-based 架构。
    -   必须解决 Spring 的 license (Non-commercial) 兼容性问题（仅供自用学习/非商用发布）。
4.  **风险与约束 (Risks/Constraints)**:
    -   ABC 属于高复杂度路线，CPU/内存开销显著高于纯统计模型；必须以 Block 分治控制内存峰值与端到端延迟。
    -   必须保证 Round-trip Lossless，并提供与原始 Spring 的对照验证以降低正确性风险。

1.  **输入支持**: 支持 `.fastq`, `.fq`, `.gz`, `.bz2`, `.xz` 输入（Phase 1: plain/gzip; Phase 2: bz2/xz）。
2.  **压缩模式**:
    -   **Reorder (Default)**: 允许重排序 reads 以获得最高压缩比（参考 Spring/Repaq）。
    -   **Order-Preserving**: 强制保留原始 reads 顺序（压缩比略低）。
3.  **数据类型**:
    -   支持 **Short Reads** (Illumina, length < 512bp)。
    -   支持 **Long Reads** (Nanopore/PacBio, 任意长度，自动禁用重排序或使用专有算法)。
    -   支持 **SE (Single-End)** 和 **PE (Paired-End)**。
4.  **断点续压**: **不支持**断点续压（No Resume），简化设计。

### Requirement 2: 解压缩与随机访问
**User Story:** 用户希望快速解压，并能随机访问文件的特定部分。

#### Acceptance Criteria
1.  **随机访问 (Scheme A)**:
    -   压缩文件必须按 **Block** 组织。
    -   支持通过 Block Index 快速定位并解压指定范围的 reads。
    -   解压语义："第 X 到 Y 条 Reads" 的编号以归档存储顺序为准（开启重排时按重排后的顺序）。
2.  **输出格式**: 支持解压为 `.fastq` 或直接流式输出到标准输出。
3.  **单条/多条解压**: 支持仅解压 header 或特定 ID 的 reads（依赖索引）。

### Requirement 3: 质量值处理 (Quality Scores)
**User Story:** 用户希望在文件大小和数据精度之间做权衡。

#### Acceptance Criteria
1.  **Lossless (Default)**: 无损保留所有质量值。
2.  **QVZ Lossy**: 支持 **QVZ** 算法进行有损压缩，提供 configurable compression ratio。
3.  **Illumina Binning**: 支持标准的 8-level binning。
4.  **No Quality**: 选项丢弃质量值（仅保留序列和 ID）。

### Requirement 4: 高性能架构
**User Story:** 用户希望充分利用多核 CPU，加速处理 massive datasets。

#### Acceptance Criteria
1.  **并行模型**: 使用 **Intel TBB**。
    -   支持 `pipeline` 模式（IO bound 场景）。
    -   支持 `parallel_for` / 任务图模式（Compute bound 场景）。
2.  **日志系统**: 使用 **Quill** 异步日志库，确保日志记录不阻塞关键路径。
3.  **内存管理**: 显式控制内存使用上限，避免 OOM（特别是 Spring 算法内存消耗较大）。

### Requirement 5: 数据完整性
**User Story:** 数据安全第一，必须能检测损坏。

#### Acceptance Criteria
1.  **Global Checksum**: 文件尾部包含全局 `xxhash64`。
2.  **Block Checksum**: 每个 Block 包含独立的 `xxhash64`，支持定位损坏块。
3.  **Verify Mode**: 提供单独的 `verify` 命令，不解压落盘即可验证文件完整性。

### Requirement 6: CLI 与易用性
**User Story:** 命令行接口应符合现有 Linux 工具习惯。

#### Acceptance Criteria
1.  **库选型**: 使用 **CLI11**。
2.  **参数**:
    -   `-t/--threads`: 线程数。
    -   `-i/--input`: 输入文件。
    -   `-o/--output`: 输出文件。
    -   `-l/--level`: 压缩等级 (1-9)。
    -   `--reorder / --no-reorder`: 重排序开关。
    -   `--lossy-quality`: 有损模式及参数。
3.  **帮助**: 提供详细的 `--help` 信息。

### Requirement 7: 开发与构建
**User Story:** 项目应易于构建和维护。

#### Acceptance Criteria
1.  **构建系统**: CMake 3.20+。
2.  **依赖管理**: 使用 Conan 或 Git Submodules (vendor)。
3.  **代码风格**: 严格遵循 C++20 标准和 fastq-tools 代码风格。
