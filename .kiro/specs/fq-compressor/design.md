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

**流式模式 (Streaming Mode)**:
当输入来自 stdin 或显式指定 `--streaming` 时，跳过 Phase 1 的全局分析：
- 禁用全局重排序（`PRESERVE_ORDER=1`）
- 禁用块内重排序（保持原始顺序）
- 压缩率降低约 10-20%，但支持流式处理
- 自动设置 `STREAMING_MODE` 标志位，并强制 `HAS_REORDER_MAP=0`

**Reorder Map 存储**:
- 若 `PRESERVE_ORDER=0`，Reorder Map 作为可选元数据存入文件（位于 Block Index 之前）
- 采用双向映射设计，支持正向查询和反向输出
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
| Reorder Map    |  (Optional, Variable Length)
+----------------+
|   Block Index  |  (Variable Length)
+----------------+
|  File Footer   |  (Fixed Length)
+----------------+
```

### 1. Magic Header
Magic Header = Magic (8 bytes) + Version (1 byte)
- `Magic`: `0x89 'F' 'Q' 'C' 0x0D 0x0A 0x1A 0x0A` (参考 PNG/XZ 风格魔法数)
- `Version`: 1 byte (`major:4bit, minor:4bit`)
    - Major 变更：格式不兼容（需新版本 Reader）
    - Minor 变更：向后兼容（Reader 可跳过未知字段）

### 2. Global Header
包含全局元数据：
- `GlobalHeaderSize` (uint32): Global Header 总长度（bytes，包含自身字段），用于前向兼容跳过未知扩展字段。
- `Flags` (uint64): 扩展为 64 位，预留更多空间
    - bit 0: `IS_PAIRED`: 配对端数据
    - bit 1: `PRESERVE_ORDER`: 是否保留原始 Reads 顺序 (0=Reordered, 1=Original)
    - bit 2: `LEGACY_LONG_READ_MODE`: 保留位（历史兼容），**必须为 0**
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
    - bit 8-9: `PE_LAYOUT`: Paired-End 布局（**仅当 IS_PAIRED=1 时有效**）
        - 00: Interleaved (默认，R1/R2 交替存储)
        - 01: Consecutive (所有 R1 后跟所有 R2；**注意：此模式会导致 Sequence 压缩率下降，因无法利用 R1/R2 互补性**)
        - 10-11: Reserved
    - bit 10-11: `READ_LENGTH_CLASS`: Read 长度分类（判定时参考 median/max length）
        - 00: Short (median < 1KB 且 max <= 511，使用 ABC + 全局 Reordering)
        - 01: Medium (1KB <= median < 10KB 或 max > 511，使用 Zstd，禁用 Reordering)
        - 10: Long (median >= 10KB 或 max >= 10KB，禁用 Reordering)
        - 11: Reserved
    - bit 12: `STREAMING_MODE`: 流式压缩模式（禁用全局与块内重排）
        - 强制 `PRESERVE_ORDER=1`
        - 强制 `HAS_REORDER_MAP=0`
    - bit 13-63: Reserved
- `CompressionAlgo` (uint8): 主要策略族 ID（用于兼容/快速识别；具体以每个 BlockHeader 的 codec_* 为准）
- `ChecksumType` (uint8): 校验算法（默认：xxhash64；预留升级空间）
- `TotalReadCount` (uint64): 总 Reads 数量（即 Sequence Count；若为 PE 数据，R1 和 R2 分别计数，Total = 2 * Pairs）
- `OriginalFilenameLength` (uint16)
- `OriginalFilename` (string)
- `Timestamp` (uint64)

**Forward Compatibility**:
- Reader 读取 `GlobalHeaderSize` 后，若发现未知字段，可按长度跳过；不理解的 `Flags` 位应忽略。
- `Version` 仅用于格式重大变更；字段扩展优先通过 `GlobalHeaderSize` + 预留字段/新增字段实现。

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
    uint8_t  codec_ids;           // e.g., 0x30 = Family 3, Version 0 (Delta + LZMA) -> For Identifier Stream
    uint8_t  codec_seq;           // e.g., 0x10 = Family 1, Version 0 (Spring ABC + Arithmetic)
    uint8_t  codec_qual;          // e.g., 0x20 = Family 2, Version 0 (SCM + Arithmetic)
    uint8_t  codec_aux;           // e.g., 0x50 = Family 5, Version 0 (Delta + Varint for lengths)
    uint8_t  reserved1;           // Padding for alignment
    uint16_t reserved2;           // Padding for alignment
    uint64_t block_xxhash64;      // xxhash64 of uncompressed logical streams (ID+SEQ+QUAL+Aux)
    uint32_t uncompressed_count;  // Number of reads in this block
    uint32_t uniform_read_length; // If all reads same length, store here; 0 = variable (use Aux)
    uint64_t compressed_size;     // Total compressed payload size (bytes)
    
    // Stream Offsets (Relative to Payload Start) - 使用 uint64_t 支持大 Block
    uint64_t offset_ids;
    uint64_t offset_seq;
    uint64_t offset_qual;
    uint64_t offset_aux;          // Read lengths if variable (size_aux > 0)
    
    // Stream Sizes (Compressed) - 便于选择性解码
    uint64_t size_ids;
    uint64_t size_seq;
    uint64_t size_qual;
    uint64_t size_aux;            // 0 = uniform length (use uniform_read_length)
};
// Total: 4 + 4 + 1 + 1 + 1 + 1 + 1 + 1 + 2 + 8 + 4 + 4 + 8 + (8*4) + (8*4) = 104 bytes
```

**Codec Versioning Convention**:
- `codec_*` 采用 `(family, version)` 的注册表语义：高 4 bits = family，低 4 bits = version。
- **兼容性规则**：
    - **Family 变更**: 不兼容。Reader 遇到未知 family 必须报错 `EXIT_UNSUPPORTED_CODEC (5)`。
    - **Version 变更**: 向后兼容。新版本 Reader 可读取旧版本数据；旧版本 Reader 遇到更高 version 应报警告但尝试解码。
- **预定义 Codec Family**：

    | Family | Name | Description | 适用流 |
    |--------|------|-------------|--------|
    | 0x0 | RAW | 无压缩 (用于调试) | 所有 |
    | 0x1 | ABC_V1 | Spring ABC (Sequence) | Sequence |
    | 0x2 | SCM_V1 | Statistical Context Mixing (Quality) | Quality |
    | 0x3 | DELTA_LZMA | Delta + LZMA | IDs |
    | 0x4 | DELTA_ZSTD | Delta + Zstd | IDs |
    | 0x5 | DELTA_VARINT | Delta + Varint 编码 | Aux (lengths) |
    | 0x6 | OVERLAP_V1 | Overlap-based (Long Read) | Sequence |
    | 0x7 | ZSTD_PLAIN | Plain Zstd | Sequence (fallback) |
    | 0x8 | SCM_ORDER1 | SCM Order-1 (低内存) | Quality |
    | 0xE | EXTERNAL | 外部/自定义 Codec | 所有 |
    | 0xF | RESERVED | 保留 | - |

**Codec 参数存储**:
- 大多数 Codec 无需额外参数
- 需要参数的 Codec（如 SCM Order）在 GlobalHeader 末尾添加可选的 `CodecParams` 段
- `CodecParams` 格式: `[codec_family:1][param_len:1][params:N]...`，以 `0xFF` 结束

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
    -   **Medium/Long Read 策略** (max > 511 或 median >= 1KB):
        -   禁用 Minimizer Reordering（中长读相似度低，重排收益有限）
        -   **主要策略**: Zstd 通用压缩（简单、稳定、推荐）
        -   **可选优化**: NanoSpring 风格的 overlap-based 压缩（仅适用于 Long；需适配 Block 模式，复杂度高）
        -   具体分类与 Block 限制见「Long Read 模式设计」
3.  **Quality Stream**:
    -   *Strategy*: **Statistical Context Mixing (SCM)** (Ref: Fqzcomp5).
    -   *Why*: 质量值具有高频率波动特性，Assembly-based 方法效果不佳。Fqzcomp5 的上下文模型 (Context Model) 是目前处理质量值的最佳实践。我们将实现一个类似的 Order-1/Order-2 上下文模型。
    -   **Discard 模式**: `codec_qual=RAW` 且 `size_qual=0`，解压时填充占位符质量值（默认 `'!'`）以保持 FASTQ 四行格式
4.  **Aux Stream** (可选):
    -   *用途*: 存储可变长度 Reads 的长度信息
    -   *Strategy*: Delta + Varint 编码
    -   *存储条件*:
        -   当 Block 内所有 Reads 长度相同时: `size_aux = 0`，长度存储在 `BlockHeader.uniform_read_length`
        -   当 Reads 长度不同时: 存储 Delta + Varint 编码的长度序列
    -   *编码方式*: 第一个长度存储原值，后续存储与前一个的差值（相邻 Reads 长度通常相近）


### 4. Block Index (At End)
支持快速定位。
```cpp
struct BlockIndex {
    uint32_t header_size;     // BlockIndex header size (bytes)
    uint32_t entry_size;      // IndexEntry size (bytes), for forward compatibility
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
- **PE 计数语义**: Read ID 以 **单条 Read 记录** 计数（PE 总数 = 2 * Pairs）
- 若需查询"原始第 N 条 Read"：
    1. 查询 Reorder Map (Forward) 得到 `archive_id`
    2. 二分查找 Block Index 找到包含该 `archive_id` 的 Block
    3. 解压并定位

### 4.1 Reorder Map (Optional)
当 `PRESERVE_ORDER=0` 且 `HAS_REORDER_MAP=1` 时存在，支持原始顺序回溯。

采用**双向映射**设计，同时支持正向查询和反向输出：

```cpp
struct ReorderMap {
    uint64_t total_reads;
    uint64_t forward_map_size;    // Forward Map 压缩后大小
    uint64_t reverse_map_size;    // Reverse Map 压缩后大小
    
    // Forward Map: original_id -> archive_id
    // 用途: 查询"原始第 N 条 Read 在归档中的位置"
    // 编码: Delta + Varint，通常压缩到 ~2 bytes/read
    uint8_t forward_map[];
    
    // Reverse Map: archive_id -> original_id
    // 用途: 按原始顺序输出时，快速查找每条 Read 的原始位置
    // 编码: Delta + Varint，通常压缩到 ~2 bytes/read
    uint8_t reverse_map[];
};
```

**设计说明**:
- **Forward Map**: 支持 `--range` 按原始 ID 范围解压
- **Reverse Map**: 支持 `--original-order` 高效输出
- **空间开销**: ~4 bytes/read（两个映射各 ~2 bytes/read）
- **性能**: 映射解压到内存后两种查询方向都是 O(1)；若按需解码需额外块索引

**查询流程**:
1. 按归档顺序解压: 无需 Reorder Map
2. 按原始顺序输出: 使用 Reverse Map，按 archive_id 顺序遍历，输出到 original_id 位置
3. 查询原始 Read 位置: 使用 Forward Map

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

**Global Checksum 校验范围**:
- 校验范围: `[文件起始, Footer 起始位置)`
- 包含: Magic Header, Global Header, All Blocks, Reorder Map (if exists), Block Index
- 不包含: File Footer 本身
- 计算方式: 流式计算，边写边更新 xxHash64 状态

**校验流程 (推荐顺序)**:
1. 读取 Footer，验证 `magic_end`（快速检测格式）
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
- **范围语义**: 所有范围/索引的 Read ID 以单条 Read 计数（PE 总数 = 2 * Pairs）

## Long Read 模式设计

### Spring 511 长度限制深度分析

**问题根源**: Spring 的 `MAX_READ_LEN = 511` 是**编译时硬编码常量**，深度嵌入其核心数据结构：

```cpp
// Spring 源码 (params.h)
const uint16_t MAX_READ_LEN = 511;
const uint32_t MAX_READ_LEN_LONG = 4294967290;  // -l 模式使用

// 依赖此常量的数据结构 (reorder.cpp)
typedef std::bitset<2*MAX_READ_LEN> bitset;     // 1022 bits 固定大小
bitset basemask[MAX_READ_LEN][128];             // 静态数组
bitset positionmask[MAX_READ_LEN];              // 静态数组
char s[MAX_READ_LEN + 1];                       // 固定缓冲区
```

**扩展可行性评估**:

| 扩展方案 | 难度 | 说明 |
|---------|------|------|
| 修改 `MAX_READ_LEN` 常量 | **低** | 只需改一个值，但会显著增加内存（bitset 大小翻倍） |
| 动态长度支持 | **高** | 需重构所有 bitset 为 `std::vector<bool>` 或动态容器，涉及大量代码 |
| 算法适用性 | **有限** | ABC 对长读收益有限（见下文分析） |

**关键发现**: Spring 的 `-l` (long read) 模式**完全绕过 ABC 算法**：
> "Supports variable length long reads of arbitrary length (with -l flag). This mode **directly applies general purpose compression (BSC)** to reads and so compression gains might be lower."

这说明 Spring 作者认为 **ABC 算法对长读的收益不值得扩展**。

### NanoSpring vs Spring 对比分析

**NanoSpring 不是 Spring 的扩展**，而是**完全不同的算法**：

| 特性 | Spring (ABC) | NanoSpring (Overlap-based) |
|------|-------------|---------------------------|
| **核心策略** | Minimizer + Reordering + Consensus | MinHash + Minimap2 + Consensus Graph |
| **适用场景** | 短读 (Illumina, <500bp) | 长读 (Nanopore, 10KB+) |
| **压缩内容** | 完整 FASTQ (Seq + Qual + ID) | **仅序列** (忽略 Qual/ID) |
| **错误率容忍** | 低 (~0.1%) | **高** (~5-15%) |
| **重排序** | 全局重排序 (关键优化) | 无重排序 |
| **内存模型** | O(N) Minimizer 索引 | O(N) MinHash + Consensus Graph |

**为什么 ABC 对长读效果有限**:
1. **高错误率**: Nanopore ~5-15% 错误率导致 Minimizer 匹配困难
2. **序列相似度低**: 长读间难以找到足够相似的序列进行 Consensus
3. **重排序收益低**: 无法有效聚集相似 reads

### Read 长度分类

采用三级分类系统，基于采样的 median 和 max read length 自动判断：

| 分类 | 判定条件（按优先级） | 压缩策略 | Block Size |
|------|----------------------|----------|------------|
| **Long (Ultra)** | max >= 100KB | Zstd + max_block_bases 限制，禁用 Reordering | 10K reads |
| **Long** | max >= 10KB | Zstd，禁用 Reordering | 10K reads |
| **Medium** | max > 511 或 median >= 1KB | Zstd，禁用 Reordering | 50K reads |
| **Short** | max <= 511 且 median < 1KB | ABC + 全局 Reordering | 100K reads |

**超长读 (Ultra-long) 定义**:
- 当 `max_read_length >= 100KB`（可配置）时视为超长读。
- 仍归类为 `READ_LENGTH_CLASS=LONG`，但采用更保守策略：
  - 强制 Zstd（不启用 overlap-based）
  - 更小的 Block 基准大小与基于碱基数的上限

**Spring 兼容性约束**:
- Spring ABC 原版对可变长度 short reads 的上限约 511bp
- 若未对 Spring 做扩展，进入 ABC 的 reads 必须满足 `max_read_length <= 511`
- 超过 511bp 的数据自动切换到 Medium/Long 策略

### 自动检测策略
```cpp
enum class ReadLengthClass {
    SHORT,      // max <= 511 且 median < 1KB (Spring ABC 兼容)
    MEDIUM,     // max > 511 或 1KB <= median < 10KB
    LONG        // max >= 10KB (包含 Ultra-long: max >= 100KB)
};

// 自动检测逻辑 (优先级从高到低，严格按此顺序判定):
// 1. max_length >= 100KB → LONG (Ultra-long 策略：强制 Zstd + 10K block + max_block_bases)
// 2. max_length >= 10KB → LONG (Zstd，禁重排，10K block)
// 3. max_length > 511 → MEDIUM (Spring 兼容保护，即使 median < 1KB)
// 4. median >= 1KB → MEDIUM (Zstd，禁重排，50K block)
// 5. 其余 → SHORT (ABC + 全局 Reordering，100K block)

// 设计决策：保守策略
// - 不允许截断超长 reads 以适配 Spring ABC
// - 只要存在任何 read 超过 511bp，整个文件即归类为 MEDIUM 或更高
// - 这确保数据完整性，避免因截断导致的信息丢失
```

### Long Read 压缩策略

> [!IMPORTANT]
> **关于 Spring 511bp 限制与策略选择**:
> Spring 的 ABC 算法是为了短读长 (Illumina) 设计的，其图结构在处理超长且高错误率的 reads 时效率急剧下降。
> 因此，**绝对不应尝试强行扩展 Spring ABC 算法去支持长读**。
> 本项目采用 **分而治之** 的策略：
> *   **Short (<1KB, max<=511)**: 使用 Spring ABC (SOTA for short reads)。
> *   **Medium (1KB-10KB 或 max>511)**: Spring ABC 不支持。因此 **Zstd 通用压缩** 是该区间最稳妥、最高效的选择。
> *   **Long (>10KB)**: **Zstd 通用压缩** 作为主要策略（简单、稳定）；Overlap-based 作为可选优化（需适配为 Block 模式，复杂度高）。
> *   **NanoSpring 适配挑战**: NanoSpring 原生算法倾向于全局处理 (Global Graph/Index)，为了支持本项目要求的 **Block-based Random Access**，需要将其改造为处理独立的 Block。这会牺牲少量跨 Block 的重叠压缩收益，且实现复杂度高。因此 **Zstd 是 Long Read 的推荐首选**。

| 组件 | Short Read (<1KB, max<=511) | Medium Read (1KB-10KB 或 max>511) | Long Read (>10KB) |
|------|-------------------|------------------------|-------------------|
| **Sequence** | **ABC (Spring)**<br>*(limit: 511bp)* | **Zstd (General)**<br>*(Recommended for stability)* | **Zstd (Primary)**<br>*(Overlap-based 作为可选优化)* |
| **Quality** | **SCM Order-2**<br>*(高精度上下文)* | **SCM Order-2** | **SCM Order-1**<br>*(降低内存/计算开销)* |
| **Reordering** | **启用**<br>*(提升~20%)* | **禁用**<br>*(ABC 不可用)* | **禁用**<br>*(收益低，开销大)* |
| **Block Size** | 100K reads | 50K reads | 10K reads |

**Block Size 边界约束**:
- 对 Long/Ultra-long 追加 `max_block_bases` 上限（默认 200MB；Ultra-long 默认 50MB）。
- 实际 reads 数 = `min(reads_limit, max_block_bases / max(1, median_length))`，避免超长读导致单块过大。

### 混合长短读场景分析

**结论**: 在真实世界中，**几乎不存在**单个 FASTQ 文件混合长短读的场景。

**原因**:
1. **测序平台不同**: Illumina (短读) vs Nanopore/PacBio (长读) 是不同的仪器
2. **数据产出分离**: 每个测序 run 产出独立的 FASTQ 文件
3. **分析流程不同**: 短读和长读的下游分析完全不同
4. **即使混合测序**: 也会产出独立的文件（如 10x Genomics 的 linked-reads）

**唯一例外**: PacBio HiFi 数据可能有较大的长度变异（5-25KB），但仍属于"长读"范畴，统一使用 Long Read 策略。

**设计决策**: 本项目不做混合长短读的专门优化；若出现混合数据，按 `max_length` 优先级归类并提示可能的压缩率下降。

### 通用压缩算法选择：Zstd vs BSC

- **Zstd**: 低内存、解压快、支持流式与字典、生态成熟，便于 Block-based 随机访问与多线程。
- **BSC**: 压缩率可能更高，但内存占用大、解压慢、生态与部署成本高，且不利于小 Block 快速解码。
- **取舍**: 本项目优先保证随机访问与可维护性，因此默认选择 Zstd；BSC 可作为 `EXTERNAL` codec 的实验性选项。

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
2. 每 Chunk 独立执行 Two-Phase 压缩（仅在 Chunk 内重排序）
3. 归档顺序按 Chunk 顺序串联，`archive_id` 全局连续
4. Reorder Map 以 Chunk 为单位拼接并累加偏移，保证原始/归档 ID 可全局查询
5. Chunk 间不共享重排序上下文（压缩率略降 5-10%）

## Development Phases

1.  **Phase 1: Skeleton & Format** (~2-3 周): 搭建 CMake, 引入 CLI11/Quill, 定义文件格式读写器。
2.  **Phase 2: Integration** (~4-6 周, **高风险**): 移植/适配 Spring 核心算法，实现两阶段压缩策略。
3.  **Phase 3: Pipeline** (~2-3 周): 实现 TBB 流水线。
4.  **Phase 4: Optimization** (~2-3 周): 引入 pigz 思想优化 IO，引入 Repaq 思想优化重排，PE/Long Read 支持。
5.  **Phase 5: Verification** (~2 周): 全面测试 (Unit, Property, Integration)。
