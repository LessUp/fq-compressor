# Design Document: fq-compressor

## Overview

fq-compressor 是一个高性能 FASTQ 文件压缩工具，结合 Spring 的先进压缩算法和 fastq-tools 的现代化框架。本设计文档描述系统架构、组件接口、数据模型和实现策略。

### 设计目标

1.  **高压缩比**: 复用 **Spring/Mincom** 的 **Assembly-based Compression (ABC)** 核心（minimizer bucketing / local consensus / delta / arithmetic），参考 **Repaq** 的紧凑设计。
2.  **高性能**: 基于 **Intel TBB** 的并行流水线处理 (参考 **Pigz** 的 Producer-Consumer 模型)。
3.  **模块化**: 清晰的接口设计，支持多种压缩后端。
4.  **现代化**: C++20 标准，RAII 内存管理，Concepts 约束。
5.  **随机访问**: 支持基于 Block 的随机读取 (Scheme A)。
6.  **内存可控**: 显式内存预算控制，支持超大文件分治处理。

### 技术栈

| 类别 | 技术选型 | 说明 |
|------|----------|------|
| 语言标准 | **C++20** | Concepts, Ranges, Coroutines |
| 构建系统 | **CMake 3.20+** | Modern CMake 最佳实践 |
| 并发框架 | **Intel oneTBB** | `parallel_pipeline`, `task_group` |
| CLI 解析 | **CLI11** | 现代化、功能丰富的命令行库 |
| 日志系统 | **Quill** | 极低延迟异步日志 (Low Latency Logging) |
| 压缩库 | **Spring (Core)**, fqzcomp5-style (Quality), libdeflate, libbz2, liblzma, zstd | 混合压缩策略 |
| 测试框架 | Google Test + RapidCheck | 单元测试 + 属性测试 |
| 依赖管理 | Conan 2.x | 依赖包管理 |

## Architecture

### 系统架构图

```mermaid
graph TD
    subgraph CLI["CLI Layer (CLI11)"]
        A[main.cpp] --> B[CompressCommand]
        A --> C[DecompressCommand]
        A --> D[InfoCommand]
        A --> V[VerifyCommand]
    end

    subgraph Core["Core Layer"]
        B --> E[CompressionEngine]
        C --> F[DecompressionEngine]
        V --> G[VerifyEngine]
        
        E --> H[FastqParser]
        E --> I[SequenceCompressor (SpringABC)]
        E --> N[IDCompressor]
        E --> K[QualityCompressor (SCM)]
        E --> J[BlockEncoder]
        
        F --> L[BlockDecoder]
        F --> M[SequenceDecompressor (SpringABC)]
        F --> O[IDDecompressor]
        F --> P[QualityDecompressor (SCM)]
    end

    subgraph Pipeline["Pipeline Layer (TBB)"]
        E --> Q[TBBPipeline]
        F --> Q
        Q --> R[WorkerPool]
        note[Reference Pigz Model: Read -> Compress -> Write]
    end

    subgraph Storage["Storage Layer"]
        J --> S[FQCWriter]
        L --> T[FQCReader]
        S --> U[BlockIndex]
    end
```

### 关键参考项目架构借鉴

1.  **Spring / Mincom**: 复用其 **ABC (Assembly-based Compression)** 核心（minimizer bucketing / local consensus / delta / arithmetic），并通过 vendor/fork 集成其 encoder/decoder。采用 **两阶段压缩策略** 解决 Block-based 随机访问与全局优化的矛盾（详见下文）。
2.  **Pigz**: 借鉴其并行设计。主线程读文件 -> 分块提交给 TBB 线程池压缩 -> 输出线程按顺序写入文件。
3.  **Repaq / FQZComp5**: 参考其算术编码和上下文模型优化，特别是对 metadata (IDs) 的压缩处理。

### 两阶段压缩策略 (Two-Phase Compression)

**问题**: Spring 的 ABC 算法依赖全局 Reads 重排序（Approximate Hamiltonian Path）来构建局部共识，与 Block-based 随机访问存在本质冲突。

**解决方案**: 采用两阶段压缩策略，在保持高压缩率的同时支持随机访问：

```
Phase 1: Global Analysis (全局分析)
├── 1.1 扫描所有 Reads，提取 Minimizers
├── 1.2 构建 Minimizer -> Bucket 映射
├── 1.3 全局重排序决策 (Approximate Hamiltonian Path)
├── 1.4 生成 Reorder Map: original_id -> archive_id
└── 1.5 划分 Block 边界 (每 Block 默认 100K reads)

Phase 2: Block-wise Compression (分块压缩)
├── 2.1 按 Block 并行压缩 (TBB parallel_pipeline)
├── 2.2 每个 Block 独立编码 (Local Consensus + Delta)
├── 2.3 Block 内状态隔离，支持独立解压
└── 2.4 写入归档文件
```

**优势**:
- **压缩率**: Phase 1 的全局重排序保证接近 Spring 原版的压缩率
- **随机访问**: Phase 2 的 Block 独立性保证 O(1) 随机访问
- **内存控制**: Phase 1 仅需 O(N) 的 Minimizer 索引，Phase 2 内存受 Block 大小限制

**Reorder Map 存储**:
- 若 `PRESERVE_ORDER=0`，Reorder Map 作为可选元数据存入文件（位于 Block Index 之前）
- 支持用户查询"原始第 N 条 Read 在归档中的位置"

## Archive Format Design (.fqc)

文件结构设计为支持流式写入和随机访问读取。

### File Layout

```
+----------------+
|  Magic Header  |  (9 bytes)
+----------------+
| Global Header  |  (Variable Length)
+----------------+
|    Block 0     |
+----------------+
|    Block 1     |
+----------------+
|      ...       |
+----------------+
|    Block N     |
+----------------+
|   Block Index  |  (Variable Length)
+----------------+
|  File Footer   |  (Fixed Length)
+----------------+
```

### 1. Magic Header
Magic Header = Magic (8 bytes) + Version (1 byte)
- `Magic`: `0x89 'F' 'Q' 'C' 0x0D 0x0A 0x1A 0x0A` (参考 PNG/XZ 风格魔法数)
- `Version`: 1 byte (e.g., 0x01)

### 2. Global Header
包含全局元数据：
- `GlobalHeaderSize` (uint32): Global Header 总长度（bytes，包含自身字段），用于前向兼容跳过未知扩展字段。
- `Flags` (uint64): 扩展为 64 位，预留更多空间
    - bit 0: `IS_PAIRED`: 配对端数据
    - bit 1: `PRESERVE_ORDER`: 是否保留原始 Reads 顺序 (0=Reordered, 1=Original)
    - bit 2: `LONG_READ_MODE`: 长读模式
    - bit 3-4: `QUALITY_MODE`:
        - 00: Lossless (Default)
        - 01: Illumina Binning 8
        - 10: QVZ Lossy
        - 11: Discard Quality
    - bit 5-6: `ID_MODE`:
        - 00: Preserve Exact (Default)
        - 01: Tokenized/Reconstruct (Split static/dynamic parts)
        - 10: Discard/Indexed (Replace with 1,2,3...)
    - bit 7: `HAS_REORDER_MAP`: 是否包含 Reorder Map（用于原始顺序回溯）
    - bit 8-9: `PE_LAYOUT`: Paired-End 布局
        - 00: Single-End
        - 01: Interleaved (R1, R2 交替存储)
        - 10: Consecutive (所有 R1 后跟所有 R2)
    - bit 10-63: Reserved
- `CompressionAlgo` (uint8): 主要策略族 ID（用于兼容/快速识别；具体以每个 BlockHeader 的 codec_* 为准）
- `ChecksumType` (uint8): 校验算法（默认：xxhash64；预留升级空间）
- `TotalReadCount` (uint64): 总 Read 数量（用于快速统计和进度显示）
- `OriginalFilenameLength` (uint16)
- `OriginalFilename` (string)
- `Timestamp` (uint64)

**Forward Compatibility**:
- Reader 读取 `GlobalHeaderSize` 后，若发现未知字段，可按长度跳过；不理解的 `Flags` 位应忽略。
- `Version` 仅用于格式重大变更；字段扩展优先通过 `GlobalHeaderSize` + 预留字段/新增字段实现。

### 3. Block Structure
为了支持 "Scheme A" (Random Access)，数据被分割为独立的块。每个块可以独立解压（除了可能的共享静态字典）。

### 3. Block Structure (Columnar Storage)
采用**列式存储** (Columnar Storage) 思想，将 Block Payload 划分为独立的子流 (Sub-streams)，每个子流单独压缩。

```cpp
struct Block {
    BlockHeader header;
    // Concat of compressed streams:
    // [Stream ID][Stream Seq][Stream Qual][Stream Aux]
};

struct BlockHeader {
    uint32_t header_size;         // BlockHeader total size (bytes), for forward compatibility
    uint32_t block_id;
    uint8_t  checksum_type;       // Same domain as GlobalHeader.ChecksumType
    uint8_t  codec_id;            // e.g., 0x10 = Family 1, Version 0 (Delta + LZMA)
    uint8_t  codec_seq;           // e.g., 0x10 = Family 1, Version 0 (Spring ABC + Arithmetic)
    uint8_t  codec_qual;          // e.g., 0x10 = Family 1, Version 0 (SCM + Arithmetic)
    uint64_t block_xxhash64;
    uint32_t uncompressed_count;  // Number of reads in this block
    uint32_t reserved;            // Padding for alignment
    uint64_t compressed_size;     // Total compressed payload size (bytes)
    
    // Stream Offsets (Relative to Payload Start) - 使用 uint64_t 支持大 Block
    uint64_t offset_id;
    uint64_t offset_seq;
    uint64_t offset_qual;
    uint64_t offset_aux;          // Optional (e.g., Read lengths if variable)
    
    // Stream Sizes (Compressed) - 便于选择性解码
    uint64_t size_id;
    uint64_t size_seq;
    uint64_t size_qual;
    uint64_t size_aux;
};
// Total: 4 + 4 + 1 + 1 + 1 + 1 + 8 + 4 + 4 + 8 + (8*4) + (8*4) = 100 bytes
```

**Codec Versioning Convention**:
- `codec_*` 采用 `(family, version)` 的注册表语义：高 4 bits = family，低 4 bits = version。
- **兼容性规则**：
    - **Family 变更**: 不兼容。Reader 遇到未知 family 必须报错 `EXIT_UNSUPPORTED_CODEC (5)`。
    - **Version 变更**: 向后兼容。新版本 Reader 可读取旧版本数据；旧版本 Reader 遇到更高 version 应报警告但尝试解码。
- **预定义 Codec Family**：

    | Family | Name | Description |
    |--------|------|-------------|
    | 0x0 | RAW | 无压缩 (用于调试) |
    | 0x1 | ABC_V1 | Spring ABC (Sequence) |
    | 0x2 | SCM_V1 | Statistical Context Mixing (Quality) |
    | 0x3 | DELTA_LZMA | Delta + LZMA (IDs) |
    | 0x4 | DELTA_ZSTD | Delta + Zstd (IDs) |
    | 0xF | RESERVED | 保留 |

**Stream Definitions**:
1.  **Identifier Stream**:
    -   *Strategy*: Tokenization -> Delta Encoding -> General Compressor (LZMA/Zstd).
    -   *Why*: IDs (e.g., `@Illumina...:1:1:1:1`) often differ only by last integer. Delta encoding handles this efficiently.
2.  **Sequence Stream**:
    -   *Strategy*: **Assembly-based Compression (ABC)** (State-of-the-Art).
    -   *Implementation Path*: **Spring Core Fork** + **Two-Phase Strategy**.
        -   **Phase 1 (Global)**: Minimizer 提取 -> 全局 Bucketing -> 全局 Reordering
        -   **Phase 2 (Per-Block)**: Block 内 Consensus & Delta -> Arithmetic Coding
    -   *Why*: 核心思想：Don't just compress reads, compress the edits。两阶段策略在保持高压缩率的同时实现随机访问。
    -   **Long Read 策略** (read length > 10KB):
        -   禁用 Minimizer Reordering（长读序列相似度低，重排收益有限）
        -   使用 NanoSpring 风格的 overlap-based 压缩
        -   或回退到 SCM + Zstd 通用策略
3.  **Quality Stream**:
    -   *Strategy*: **Statistical Context Mixing (SCM)** (Ref: Fqzcomp5).
    -   *Why*: 质量值具有高频率波动特性，Assembly-based 方法效果不佳。Fqzcomp5 的上下文模型 (Context Model) 是目前处理质量值的最佳实践。我们将实现一个类似的 Order-1/Order-2 上下文模型。


### 4. Block Index (At End)
支持快速定位。
```cpp
struct BlockIndex {
    uint64_t num_blocks;
    IndexEntry entries[];  // Array of IndexEntry
};

struct IndexEntry {
    uint64_t offset;           // Block 在文件中的绝对偏移
    uint64_t compressed_size;  // Block 压缩后大小 (冗余存储，便于快速扫描)
    uint64_t archive_id_start; // 该 Block 的起始 Read ID (归档顺序)
    uint32_t read_count;       // 该 Block 包含的 Read 数量
};
```

**语义说明**:
- `archive_id_start` 以归档存储顺序计数（若 `PRESERVE_ORDER=0` 则为重排后顺序）
- 若需查询"原始第 N 条 Read"，需配合 Reorder Map（存储于 Block Index 之前）

### 4.1 Reorder Map (Optional)
当 `PRESERVE_ORDER=0` 且 `HAS_REORDER_MAP=1` 时存在，支持原始顺序回溯。
```cpp
struct ReorderMap {
    uint64_t total_reads;
    // 压缩存储: original_id -> archive_id 映射
    // 使用 Delta + Varint 编码，通常压缩到 ~2 bytes/read
    uint8_t compressed_mapping[];
};
```

### 5. File Footer
```cpp
struct FileFooter {
    uint64_t index_offset;       // Block Index 开始的位置
    uint64_t reorder_map_offset; // Reorder Map 开始位置 (0 = 不存在)
    uint64_t global_checksum;    // 整个文件的 XXHASH64 (不含 Footer 本身)
    uint8_t  magic_end[8];       // "FQC_EOF\0" (0x46 0x51 0x43 0x5F 0x45 0x4F 0x46 0x00)
};
// Total: 32 bytes (8 + 8 + 8 + 8)
// 读取方式: fseek(file, -32, SEEK_END)
```

**校验流程 (推荐顺序)**:
1. 读取 Footer，验证 `magic_end`
2. 验证 `global_checksum`（快速检测文件截断/损坏）
3. 读取 Block Index
4. 逐 Block 验证 `block_xxhash64`（完整模式）

## Components and Interfaces

### 1. CLI 模块 (fqc_cli)
使用 **CLI11** 替代 cxxopts。

```cpp
#include <CLI/CLI.hpp>
// ...
CLI::App app{"fq-compressor: High performance FASTQ compressor"};
app.add_option("-i,--input", input_file, "Input file");
app.add_option("-t,--threads", threads, "Number of threads");
// ...
```

### 2. Logger 模块
使用 **Quill**。

```cpp
// include/fqc/common/logger.h
#include <string_view>
#include <quill/Quill.h>

namespace fqc::log {
    void init(std::string_view log_file, quill::LogLevel level);
    // 全局 Logger 实例
}
```

### 3. IO 与格式处理

- I/O 压缩格式分阶段支持：Phase 1（plain/gzip），Phase 2（bzip2/xz）。

```cpp
// include/fqc/format/fqc_writer.h
namespace fqc::format {
    class FQCWriter {
    public:
        void write_global_header(const GlobalHeader& header);
        void write_block(const BlockData& block); // Thread-safe or serialized
        void finalize(); // Write index and footer
    private:
        std::vector<IndexEntry> index_;
    };
}
```

### 4. 并行压缩流水线 (Pipeline)

```cpp
// include/fqc/pipeline/compressor.h
class CompressionPipeline {
public:
    // TBB Filter Design
    // 1. ReaderFilter (Serial) -> Produces Chunks of Reads
    // 2. CompressFilter (Parallel) -> Compresses Chunks to Blocks (ABC/SCM/ID)
    // 3. WriterFilter (Serial) -> Writes Blocks to Disk
    void run();
};
```

## Correctness & Verification

### 完整性校验设计
1.  **输入完整性**: 读取 FASTQ 时校验基本格式。
2.  **块校验**: 每个 Block 解压后对比 `block_xxhash64`。
3.  **全局校验**: 整个文件流对比 `GlobalChecksum`。
4.  **Reference Verification**: `ref-projects/Spring` 的算法输出应与本工具核心算法输出一致（在由本工具适配 Block 接口后）。

## Paired-End 支持设计

### PE 存储布局
支持两种 PE 布局模式（由 `PE_LAYOUT` 标志控制）：

1.  **Interleaved (交错存储)** - 默认
    - R1_0, R2_0, R1_1, R2_1, ...
    - 优势：配对 Reads 物理相邻，利于压缩
    - 适用：大多数场景

2.  **Consecutive (连续存储)**
    - R1_0, R1_1, ..., R1_N, R2_0, R2_1, ..., R2_N
    - 优势：单端解压时无需跳过配对端
    - 适用：需要频繁单端访问的场景

### PE 压缩优化
- 利用 R1/R2 的互补性：若 R2 与 R1 的反向互补相似，仅存储差异
- Reordering 时保持配对关系：同时移动 (R1_i, R2_i)

## Long Read 模式设计

### 自动检测策略
```cpp
enum class ReadLengthMode {
    AUTO,       // 自动检测 (默认)
    SHORT,      // 强制短读模式 (< 512bp)
    LONG        // 强制长读模式 (>= 512bp)
};

// 自动检测逻辑:
// 采样前 1000 条 Reads，若 median length > 10KB 则启用 Long Read 模式
```

### Long Read 压缩策略
| 组件 | Short Read | Long Read |
|------|------------|-----------|
| Sequence | ABC (Spring) | Overlap-based 或 Zstd |
| Quality | SCM Order-2 | SCM Order-1 (降低内存) |
| Reordering | 启用 | 禁用 |
| Block Size | 100K reads | 10K reads (更大单条) |

## 内存管理设计

### 内存预算模型
```cpp
struct MemoryBudget {
    size_t max_total_mb = 8192;      // 总内存上限 (默认 8GB)
    size_t phase1_reserve_mb = 2048; // Phase 1 预留 (Minimizer 索引)
    size_t block_buffer_mb = 512;    // Block 缓冲池
    size_t worker_stack_mb = 64;     // 每 Worker 栈空间
};

// Phase 1 内存估算: ~16 bytes/read (Minimizer) + ~8 bytes/read (Reorder Map)
// Phase 2 内存估算: ~50 bytes/read * block_size
```

### 超大文件分治
当文件超过内存预算时，自动启用分治模式：
1. 将输入文件分割为 N 个 Chunk（每 Chunk 可放入内存）
2. 每 Chunk 独立执行 Two-Phase 压缩
3. Chunk 间不共享重排序上下文（压缩率略降 5-10%）

## Development Phases

1.  **Phase 1: Skeleton & Format** (~2-3 周): 搭建 CMake, 引入 CLI11/Quill, 定义文件格式读写器。
2.  **Phase 2: Integration** (~4-6 周, **高风险**): 移植/适配 Spring 核心算法，实现两阶段压缩策略。
3.  **Phase 3: Pipeline** (~2-3 周): 实现 TBB 流水线。
4.  **Phase 4: Optimization** (~2-3 周): 引入 pigz 思想优化 IO，引入 Repaq 思想优化重排，PE/Long Read 支持。
5.  **Phase 5: Verification** (~2 周): 全面测试 (Unit, Property, Integration)。
