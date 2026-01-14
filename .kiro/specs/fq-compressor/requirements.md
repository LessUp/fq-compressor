# Requirements Document

## Introduction

fq-compressor 是一个高性能 FASTQ 文件压缩工具，结合了 Spring 的先进压缩算法和 fastq-tools 的现代化高性能框架。该项目旨在提供：

- **核心算法**: 借鉴 **Spring** (reads 重排序, 有损/无损) 和 **Repaq** (紧凑重排) 的设计。
- **高性能框架**: 基于 **fastq-tools** (C++20, Modern CMake) 和 **Intel TBB** (并行流水线)。
- **借鉴项目**:
    - **@Spring**: 核心压缩算法（重排序、编码）。
    - **@fastq-tools**: 基础框架、IO、代码风格。
    - **@repaq**: 极致压缩比设计参考。
    - **@fqzcomp5 / @DSRC**: 高性能压缩和算术编码参考。
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
| **压缩算法** | Spring (核心) + External | Spring 用于重排序/编码，外部库 (libdeflate/lzma) 用于通用流 |
| **随机访问** | **Scheme A** | 分块独立压缩 (Block-based)，支持 O(1) 随机访问 |

### 架构兼容性

1.  **IO 层**: 复用 fastq-tools 的设计，增加块级 (Block-level) 读写支持。
2.  **处理流水线**: 采用 TBB `parallel_pipeline`，模仿 **pigz** 的 "Read -> Compress -> Write" 模型。
3.  **压缩核心**: 将 Spring 的算法解耦为 "Block Context"，使其支持无状态/重置状态的压缩。
4.  **扩展性**: 格式设计预留 Flag 位，支持多种压缩后端。

## Requirements

### Requirement 1: 核心功能与压缩
**User Story:** 用户希望获得极高的压缩比，且能灵活选择保序或重排。

#### Acceptance Criteria
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
    -   解压语义：提供 "第 N 个 Block" 或 "第 X 到 Y 条 Reads" 的解压能力。
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
1.  **Global Checksum**: 文件尾部包含全局 `xxhash64` 或 `crc32c`。
2.  **Block Checksum**: 每个 Block 包含独立的 Checksum，支持定位损坏块。
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
