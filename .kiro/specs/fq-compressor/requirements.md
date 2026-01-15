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

## Non-Functional Requirements

### Performance Targets

| 指标 | Baseline | Target | Stretch Goal |
|------|----------|--------|--------------|
| **压缩率** (bits/base) | 1.5-2.0 | 0.4-0.6 | 0.3-0.4 |
| **压缩速度** (MB/s/thread) | 50-100 | 20-50 | 10-20 |
| **解压速度** (MB/s/thread) | 200-400 | 100-200 | 50-100 |
| **内存峰值** (GB) | < 2 | < 4 | < 8 |

### Comparison with Existing Tools

| 工具 | 压缩率 (bits/base) | 压缩速度 (MB/s) | 解压速度 (MB/s) |
|------|-------------------|----------------|----------------|
| gzip -9 | ~2.0 | ~20 | ~200 |
| Spring | ~0.3 | ~5 | ~50 |
| repaq | ~0.5 | ~100 | ~200 |
| **fqc (Target)** | **0.4-0.6** | **20-50** | **100-200** |

### Test Datasets (Recommended)

| 数据集 | 类型 | 大小 | 来源 |
|--------|------|------|------|
| SRR000001 | Illumina, 36bp SE | 200MB | NCBI SRA |
| SRR1770413 | Illumina HiSeq, 100bp PE | 2GB | NCBI SRA |
| ERR000589 | NovaSeq, 150bp PE | 20GB | ENA |
| PacBio HiFi | PacBio, 10-20KB | 5GB | PacBio 官网 |

## Requirements

### Requirement 1.1: 混合压缩策略 (Hybrid Compression Strategy - Strategy B)
**User Story:** 追求极致压缩率，采用学术界最前沿的 **Assembly-based Compression (ABC)** 路线，并以 **Spring Core Integration** 作为工程落地方式。

#### Acceptance Criteria
1.  **Requirement 1.1.1: I/O 支持**
    -   支持 `.fastq`, `.fq`, `.gz` 输入（Phase 1: plain/gzip）。
    -   支持 `.bz2`, `.xz` 输入。
    -   支持输出 `.fqc`。
    -   不提供外层封装输出（例如 `.fqc.gz`）：外层压缩对压缩率提升有限、会增加耗时且会破坏随机访问语义；如确有分发需求可由用户在归档外部自行二次压缩（使用前需先解包）。
2.  **Requirement 1.1.2: 流分离与核心算法路线 (State-of-the-Art)**
    -   **流分离 (Stream Separation)**: 必须将 FASTQ 数据拆分为独立的逻辑流：**Identifier Stream**, **Sequence Stream**, **Quality Stream**。
    -   **Sequence**: 采用 **ABC 策略** (Minimizer Bucketing -> Local Consensus -> Delta Encoding -> Arithmetic Coding；桶内可选 Reordering)。
        -   *Implementation*: **Vendor/Fork Spring Core**（encoder/decoder）。直接集成修改版的 **Spring** 核心算法库，而非从零重写，以确保正确性和性能。
    -   **Quality**: 采用 **Statistical Context Mixing (SCM)** (参考 Fqzcomp5)。
        -   使用 Order-1 或 Order-2 上下文算术编码。
        -   *Non-goal*: SCM 仅用于质量值；Sequence 不采用 fqzcomp5 的纯统计模型作为主路线。
    -   **IDs**: 采用 **Delta + Tokenization**。
    -   **技术落地**:
        -   必须处理 Spring 代码的模块化封装，使其适配 Block-based 架构。
        -   必须解决 Spring 的 license (Non-commercial) 兼容性问题（仅供自用学习/非商用发布）。
    -   **风险与约束 (Risks/Constraints)**:
        -   ABC 属于高复杂度路线，CPU/内存开销显著高于纯统计模型；必须以 Block 分治控制内存峰值与端到端延迟。
        -   必须保证 Round-trip Lossless，并提供与原始 Spring 的对照验证以降低正确性风险。
3.  **Requirement 1.1.3: 压缩模式与数据类型**
    -   **Reorder (Default)**: 允许重排序 reads 以获得最高压缩比（参考 Spring/Repaq）。
    -   **Order-Preserving**: 强制保留原始 reads 顺序（压缩比略低）。
    -   **Streaming Mode**: 流式压缩模式，禁用全局重排序，支持 stdin 输入。
    -   支持 **Short Reads** (median < 1KB 且 max <= 511，Spring ABC 兼容)。
    -   支持 **Medium Reads** (1KB <= median < 10KB 或 max > 511，如 PacBio HiFi)。
    -   支持 **Long Reads** (median >= 10KB，如 Nanopore，自动禁用重排序)。
    -   **Spring 兼容性**: 未扩展 Spring ABC 时，max_read_length > 511 的数据不得进入 Short 模式，应切换到 Medium/Long 或通用压缩。
    -   支持 **SE (Single-End)** 和 **PE (Paired-End)**。
    -   **断点续压**: **不支持**断点续压（No Resume），简化设计。
    -   **stdin 输入限制**: 
        -   stdin 输入时自动启用流式模式（禁用全局重排序）
        -   若需要全局重排序，必须使用文件输入
        -   stdin 输入时发出警告提示压缩率可能降低

### Requirement 2: 解压缩与随机访问
**User Story:** 用户希望快速解压，并能随机访问文件的特定部分。

#### Acceptance Criteria
1.  **Requirement 2.1: 随机访问 (Scheme A)**:
    -   压缩文件必须按 **Block** 组织。
    -   支持通过 Block Index 快速定位并解压指定范围的 reads。
    -   解压语义："第 X 到 Y 条 Reads" 的编号以归档存储顺序为准（开启重排时按重排后的顺序）。
2.  **Requirement 2.2: 输出格式**: 支持解压为 `.fastq` 或直接流式输出到标准输出。
3.  **Requirement 2.3: 部分解压 (Header-only / Column-selective)**:
    -   支持仅解压 header（Identifier Stream），用于快速预览/统计。
    -   支持只解压 Sequence 或 Quality 子流（依赖列式子流拆分与索引），避免无关子流解码开销。

### Requirement 3: 质量值处理 (Quality Scores)
**User Story:** 用户希望在文件大小和数据精度之间做权衡。

#### Acceptance Criteria
1.  **Requirement 3.1: Lossless (Default)**: 无损保留所有质量值。
2.  **Requirement 3.2: QVZ Lossy**: 支持 **QVZ** 算法进行有损压缩，提供 configurable compression ratio。
3.  **Requirement 3.3: Illumina Binning**: 支持标准的 8-level binning。
4.  **Requirement 3.4: No Quality**: 选项丢弃质量值（仅保留序列和 ID）。

### Requirement 4: 高性能架构
**User Story:** 用户希望充分利用多核 CPU，加速处理 massive datasets。

#### Acceptance Criteria
1.  **Requirement 4.1: 并行模型**: 使用 **Intel TBB**。
    -   支持 `pipeline` 模式（IO bound 场景）。
    -   支持 `parallel_for` / 任务图模式（Compute bound 场景）。
2.  **Requirement 4.2: 日志系统**: 使用 **Quill** 异步日志库，确保日志记录不阻塞关键路径。
3.  **Requirement 4.3: 内存管理**: 显式控制内存使用上限，避免 OOM（特别是 Spring 算法内存消耗较大）。
    -   提供 `--memory-limit <MB>` 参数控制内存上限（默认：8192MB）。
    -   Phase 1 (全局分析) 内存估算：~24 bytes/read（Minimizer 索引 + Reorder Map）。
    -   Phase 2 (分块压缩) 内存估算：~50 bytes/read × block_size。
    -   超大文件自动启用分治模式（Chunk-wise Compression）。

### Requirement 5: 数据完整性
**User Story:** 数据安全第一，必须能检测损坏。

#### Acceptance Criteria
1.  **Requirement 5.1: Global Checksum**: 文件尾部包含全局 `xxhash64`。
2.  **Requirement 5.2: Block Checksum**: 每个 Block 包含独立的 `xxhash64`，支持定位损坏块。
3.  **Requirement 5.3: Verify Mode**: 提供单独的 `verify` 命令，不解压落盘即可验证文件完整性。
4.  **Requirement 5.4: 校验顺序**: 推荐校验流程：
    1. 读取 Footer，验证 `magic_end`（快速检测格式）
    2. 验证 `global_checksum`（快速检测文件截断/损坏）
    3. 读取 Block Index
    4. 逐 Block 验证 `block_xxhash64`（完整模式）

### Requirement 6: CLI 与易用性
**User Story:** 命令行接口应符合现有 Linux 工具习惯。

#### Acceptance Criteria
1.  **Requirement 6.1: 库选型**: 使用 **CLI11**。
2.  **Requirement 6.2: 命令与参数设计**:

    **命令结构**:
    - `fqc <subcommand> [options]`
    - 子命令：`compress`, `decompress`, `info`, `verify`

    **I/O 约定**:
    - `-i -` 表示从 `stdin` 读取（支持管道）。
    - `-o -` 表示输出到 `stdout`。
    - 若未指定 `-o/--output`：
        - `compress`: 默认输出到 `<input>.fqc`（input 为 `-` 时要求显式指定 `-o`）。
        - `decompress`: 默认输出到 `stdout`。

    **全局选项（对所有子命令生效）**:
    - `-t, --threads <N>`: 线程数（默认：物理核数或 `std::thread::hardware_concurrency()`）。
    - `-v, --verbose`: 增加日志详细程度（可重复）。
    - `--log-file <path>`: 写入日志文件（默认：stderr）。
    - `--log-level <trace|debug|info|warning|error>`: 日志等级（默认：info）。
    - `--no-progress`: 关闭进度显示。
    - `--json`: 将 `info/verify` 输出切换为 JSON（默认：人类可读）。
    - `--version`: 打印版本并退出。

    **compress 子命令**:
    - `-i, --input <path>`: 输入 FASTQ（支持 `.fastq/.fq/.gz`；也支持 `-`）。
    - `-o, --output <path>`: 输出 `.fqc` 文件。
    - `-l, --level <1-9>`: 压缩等级（默认：5）。
    - `--block-reads <N>`: 每个 Block 的 reads 数（默认：100000；Long Read 模式默认：10000）。
    - `--reorder / --no-reorder`: 是否重排 reads（默认：`--reorder`）。
    - `--preserve-order`: 强制保留原始顺序（等价于 `--no-reorder`，二者互斥）。
    - `--save-reorder-map / --no-save-reorder-map`: 是否保存 Reorder Map 用于原始顺序回溯（默认：`--save-reorder-map`）。
    - `--id-mode <exact|tokenize|discard>`: ID 处理模式（默认：exact）。
    - `--quality-mode <lossless|illumina8|qvz|discard>`: 质量值模式（默认：lossless）。
    - `--lossy-quality <spec>`: 有损质量参数（仅当 `--quality-mode` 为 `illumina8` 或 `qvz` 时允许）。
    - `--memory-limit <MB>`: 内存使用上限（默认：8192MB）。
    - `--checksum <xxh64>`: 校验算法（默认：xxh64）。
    
    **Paired-End 选项**:
    - `--paired`: 输入为 Paired-End。
    - `-1, --read1 <path>`: PE 模式下的 R1 文件。
    - `-2, --read2 <path>`: PE 模式下的 R2 文件。
    - `--interleaved`: PE 输入为交错格式（R1, R2 在同一文件中交替出现）。
    - `--pe-layout <interleaved|consecutive>`: PE 存储布局（默认：interleaved）。
    
    **Long Read 选项**:
    - `--long-read-mode <auto|short|medium|long>`: 长读模式（默认：auto）。
    - 自动检测：采样前 1000 条 Reads，计算 median 与 max length：
        - max_length > 10KB: Long
        - max_length > 511: Medium (Spring 兼容保护)
        - 其余：median < 1KB -> Short；1KB <= median < 10KB -> Medium
    
    **流式模式选项**:
    - `--streaming`: 强制启用流式模式（禁用全局重排序，支持 stdin）。
    - 当 `-i -` (stdin) 时自动启用，并发出警告。

    **decompress 子命令**:
    - `-i, --input <path>`: 输入 `.fqc`。
    - `-o, --output <path>`: 输出 FASTQ（默认：stdout）。
    - `--range <start:end>`: 仅解压指定 reads 范围（闭区间；以归档存储顺序计数；`end` 可省略表示直到文件末尾）。
    - `--original-order`: 按原始顺序输出（需要归档包含 Reorder Map；若不存在则报错）。
    - `--header-only`: 仅输出 header（Identifier Stream），不解码 Sequence/Quality。
    - `--streams <id|seq|qual|all>`: 子流选择性解码（默认：all；与 `--header-only` 互斥）。
    - `--verify / --no-verify`: 解压时是否校验 checksum（默认：`--verify`）。
    - `--split-pe`: PE 模式下分别输出 R1/R2 到两个文件（需配合 `-o` 指定前缀）。
    - `--skip-corrupted`: 跳过损坏的 Block 继续解压（默认：遇到损坏则报错退出）。
    - `--corrupted-placeholder <skip|empty>`: 损坏 Block 的处理方式（默认：skip）。

    **info 子命令**:
    - `-i, --input <path>`: 输入 `.fqc`。
    - `--show-index`: 输出索引摘要（Block 数、范围、偏移）。
    - `--show-codecs`: 输出各子流 codec 统计与分布。

    **verify 子命令**:
    - `-i, --input <path>`: 输入 `.fqc`。
    - `--mode <quick|full>`: quick 仅验证元数据/索引一致性；full 逐块校验（默认：full）。
    - `--fail-fast`: 一旦发现损坏立即退出。
3.  **Requirement 6.3: 帮助**: 提供详细的 `--help` 信息。

#### 退出码约定

- `0`: 成功
- `1`: 参数/用法错误
- `2`: I/O 错误（文件不存在、读写失败等）
- `3`: 格式错误或版本不兼容
- `4`: 校验失败（checksum mismatch）
- `5`: 不支持的算法/codec

### Requirement 7: 开发与构建
**User Story:** 项目应易于构建和维护。

#### Acceptance Criteria
1.  **Requirement 7.1: 构建系统**: CMake 3.20+。
2.  **Requirement 7.2: 依赖管理**: 使用 Conan 或 Git Submodules (vendor)。
3.  **Requirement 7.3: 代码风格**: 严格遵循 C++20 标准和 fastq-tools 代码风格。

#### 推荐开发环境（容器）

- GCC 15 + libstdc++（生产编译首选）
- Clang 21 + libc++（开发调试首选）
- CMake 4.x
- Conan 2.x

### Requirement 8: 原子写入与错误恢复
**User Story:** 用户希望压缩过程中断时不会产生损坏的输出文件。

#### Acceptance Criteria
1.  **Requirement 8.1: 原子写入**: 压缩输出使用临时文件 (`.fqc.tmp`)，完成后原子重命名。
2.  **Requirement 8.2: 信号处理**: 捕获 `SIGINT`/`SIGTERM`，清理临时文件后退出。
3.  **Requirement 8.3: 磁盘空间检查**: 压缩前检查目标磁盘可用空间（可选警告）。
4.  **Requirement 8.4: 断点续压**: **不支持**（简化设计，与 Requirement 1.1.3 一致）。
5.  **Requirement 8.5: 错误隔离与恢复**:
    -   单个 Block 损坏不应影响其他 Block 的解压。
    -   `verify` 命令应报告所有损坏 Block 的位置和数量。
    -   `decompress` 支持 `--skip-corrupted` 选项，跳过损坏 Block 继续解压。
    -   损坏 Block 的 Reads 在输出中用占位符替代或跳过（由用户选择）。

### Requirement 9: 版本兼容性
**User Story:** 用户希望新版本工具能读取旧版本创建的文件。

#### Acceptance Criteria
1.  **Requirement 9.1: 向后兼容**: 新版本 Reader 必须能读取所有旧版本创建的 `.fqc` 文件。
2.  **Requirement 9.2: 前向兼容**: 旧版本 Reader 遇到未知字段时应跳过（利用 `header_size`），遇到未知 Codec Family 时报错 `EXIT_UNSUPPORTED_CODEC (5)`。
3.  **Requirement 9.3: 版本语义**:
    - `Version` 字段采用 `(major:4bit, minor:4bit)` 编码
    - Major 变更：格式不兼容（需要新版本工具）
    - Minor 变更：向后兼容（新增可选字段）
4.  **Requirement 9.4: 升级路径**: 提供 `fqc upgrade` 命令将旧版本文件升级到新格式（可选，Phase 5+）。
