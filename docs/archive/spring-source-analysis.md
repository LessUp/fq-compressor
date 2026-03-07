# Spring 源码结构分析

> **Task 7.1**: 分析 Spring 源码结构
> **Requirements**: 1.1.2 (流分离与核心算法路线)
> **Date**: 2025-01-14

## 1. 概述

Spring 是一个高性能 FASTQ 压缩工具，采用 **Assembly-based Compression (ABC)** 算法。本文档分析其源码结构，为 fq-compressor 项目的 Spring 核心算法集成提供参考。

### 1.1 目录结构

```
ref-projects/Spring/src/
├── main.cpp                          # CLI 入口
├── spring.h/cpp                      # 主压缩/解压缩接口
├── params.h                          # 全局参数定义
├── preprocess.h/cpp                  # FASTQ 预处理
├── reorder.h                         # Reads 重排序 (模板实现)
├── encoder.h/cpp                     # 序列编码 (Consensus + Delta)
├── decompress.h/cpp                  # 解压缩实现
├── reorder_compress_streams.h/cpp    # 流重排序与压缩
├── reorder_compress_quality_id.h/cpp # 质量值与ID压缩
├── pe_encode.h/cpp                   # Paired-End 编码
├── util.h/cpp                        # 工具函数
├── bitset_util.h                     # 位集操作与字典构建
├── BooPHF.h                          # 布谷鸟哈希 (Minimal Perfect Hash)
├── call_template_functions.h/cpp     # 模板函数调用
├── id_compression/                   # ID 压缩模块 (算术编码)
├── qvz/                              # 质量值压缩模块 (QVZ 算法)
└── libbsc/                           # BSC 通用压缩库
```

## 2. 核心模块分析

### 2.1 Minimizer Bucketing (字典构建)

**文件**: `bitset_util.h`

**核心数据结构**:
```cpp
class bbhashdict {
public:
    boophf_t *bphf;        // 布谷鸟哈希 (Minimal Perfect Hash Function)
    int start;             // 字典起始位置
    int end;               // 字典结束位置
    uint32_t numkeys;      // 键数量
    uint32_t dict_numreads; // 字典中的 reads 数量
    uint32_t *startpos;    // 起始位置数组
    uint32_t *read_id;     // Read ID 数组
    bool *empty_bin;       // 空桶标记
};
```

**关键函数**:
- `constructdictionary<bitset_size>()`: 构建字典，使用 BooPHF 进行最小完美哈希
- `generateindexmasks<bitset_size>()`: 生成索引掩码
- `stringtobitset<bitset_size>()`: 字符串转位集

**全局状态依赖**:
- 字典构建依赖全局 `basemask` 数组
- 使用 OpenMP 并行构建，需要线程安全的锁机制

**内存分配模式**:
- 字典大小与 reads 数量成正比
- 每个字典需要 `numkeys + 1` 个 `uint32_t` 的 `startpos` 数组
- 每个字典需要 `dict_numreads` 个 `uint32_t` 的 `read_id` 数组

### 2.2 Reordering (重排序)

**文件**: `reorder.h` (模板实现)

**核心数据结构**:
```cpp
template <size_t bitset_size>
struct reorder_global {
    uint32_t numreads;
    uint32_t numreads_array[2];
    int maxshift, num_thr, max_readlen;
    const int numdict = NUM_DICT_REORDER;  // 默认 2
    
    std::bitset<bitset_size> **basemask;   // 碱基掩码数组
    std::bitset<bitset_size> mask64;       // 64位掩码
    
    // 文件路径
    std::string basedir;
    std::string infile[2];
    std::string outfile;
    // ... 更多文件路径
};
```

**关键函数**:
- `reorder_main<bitset_size>()`: 重排序主入口
- `reorder<bitset_size>()`: 核心重排序算法
- `search_match<bitset_size>()`: 搜索匹配 reads
- `updaterefcount<bitset_size>()`: 更新参考序列计数

**算法流程**:
1. 读取所有 reads 到内存
2. 构建字典 (2个字典，覆盖不同位置)
3. 多线程并行搜索相似 reads
4. 使用 Hamming 距离阈值 (`THRESH_REORDER = 4`) 判断匹配
5. 输出重排序后的 reads 和位置信息

**全局状态依赖**:
- `reorder_global` 结构体包含所有全局状态
- 使用 OpenMP 锁 (`omp_lock_t`) 保护字典访问
- 依赖临时文件进行中间数据存储

**内存分配模式**:
- `std::bitset<bitset_size> *read`: 所有 reads 的位集表示
- `uint16_t *read_lengths`: 所有 reads 的长度
- `bool *remainingreads`: 剩余 reads 标记
- 内存估算: ~24 bytes/read (位集 + 长度 + 标记)

### 2.3 Consensus/Delta Encoding (共识/差分编码)

**文件**: `encoder.h/cpp`

**核心数据结构**:
```cpp
struct encoder_global {
    uint32_t numreads, numreads_s, numreads_N;
    int numdict_s = NUM_DICT_ENCODER;  // 默认 2
    int max_readlen, num_thr;
    
    // 文件路径
    std::string basedir;
    std::string infile, infile_flag, infile_pos;
    std::string outfile_seq, outfile_pos, outfile_noise, outfile_noisepos;
    
    char enc_noise[128][128];  // 噪声编码表
};

struct contig_reads {
    std::string read;
    int64_t pos;
    char RC;
    uint32_t order;
    uint16_t read_length;
};
```

**关键函数**:
- `encoder_main<bitset_size>()`: 编码主入口
- `encode<bitset_size>()`: 核心编码算法
- `buildcontig()`: 构建共识序列
- `writecontig()`: 写入共识序列和差分信息

**算法流程**:
1. 读取重排序后的 reads
2. 构建 contig (共识序列)
3. 计算每个 read 与共识序列的差异 (noise)
4. 输出共识序列、位置、噪声信息

**全局状态依赖**:
- `encoder_global` 和 `encoder_global_b<bitset_size>` 结构体
- 噪声编码表 `enc_noise` 基于 Minoche et al. 的替换统计

**内存分配模式**:
- `std::list<contig_reads>`: 当前 contig 的 reads 列表
- 限制列表大小 (`list_size > 10000000`) 防止内存溢出
- 内存估算: ~50 bytes/read × block_size

### 2.4 Arithmetic Coding (算术编码)

**文件**: `libbsc/` 目录

Spring 使用 **BSC (Block Sorting Compressor)** 进行最终压缩，而非传统的算术编码。

**BSC 组件**:
- **BWT (Burrows-Wheeler Transform)**: 块排序变换
- **LZP (Longest Previous Factor)**: 预处理
- **QLFC (Quantized Length-Frequency Coding)**: 量化编码

**关键接口**:
```cpp
namespace bsc {
    int BSC_compress(const char *infile, const char *outfile);
    int BSC_decompress(const char *infile, const char *outfile);
    int BSC_str_array_compress(const char *outfile, std::string *str_array,
                               uint32_t num_strings, uint32_t *str_len_array);
    int BSC_str_array_decompress(const char *infile, std::string *str_array,
                                 uint32_t num_strings, uint32_t *str_len_array);
}
```

### 2.5 ID 压缩

**文件**: `id_compression/` 目录

**算法**: 算术编码 + 统计模型

**关键接口**:
```cpp
void compress_id_block(const char *outfile_name, std::string *id_array,
                       const uint32_t &num_ids);
void decompress_id_block(const char *infile_name, std::string *id_array,
                         const uint32_t &num_ids);
```

### 2.6 Quality 压缩

**文件**: `qvz/` 目录

**支持模式**:
- **Lossless**: 无损压缩
- **QVZ**: 向量量化有损压缩
- **Illumina 8-bin**: 8级分箱
- **Binary**: 二值化阈值

**关键接口**:
```cpp
namespace qvz {
    void encode(struct qv_options_t *opts, uint32_t max_readlen,
                uint32_t numreads, std::string *quality_string_array,
                uint32_t *str_len_array);
}
```

## 3. 压缩流程分析

### 3.1 Short Read 压缩流程

```
spring::compress()
├── preprocess()                    # FASTQ 解析与清理
│   ├── 读取 FASTQ 文件
│   ├── 分离 clean reads 和 N reads
│   └── 输出二进制格式
├── call_reorder()                  # 重排序
│   ├── 构建字典 (BooPHF)
│   ├── 多线程搜索相似 reads
│   └── 输出重排序结果
├── call_encoder()                  # 编码
│   ├── 构建共识序列
│   ├── 计算差分 (noise)
│   └── 输出编码结果
├── reorder_compress_quality_id()   # 质量值与ID压缩 (非保序模式)
│   ├── 重排序质量值
│   ├── 重排序 ID
│   └── BSC 压缩
├── pe_encode()                     # PE 编码 (非保序模式)
│   └── 编码配对信息
├── reorder_compress_streams()      # 流压缩
│   ├── 重排序各流
│   └── BSC 压缩各块
└── tar 打包                        # 输出 .spring 文件
```

### 3.2 Long Read 压缩流程

```
spring::compress() with long_flag=true
├── preprocess()                    # FASTQ 解析
└── 直接 BSC 压缩                   # 跳过重排序和编码
    ├── 压缩 reads
    ├── 压缩 quality
    └── 压缩 id
```

**关键发现**: Long Read 模式 (`-l` 标志) **完全绕过 ABC 算法**，直接使用 BSC 通用压缩。

## 4. 关键参数 (params.h)

```cpp
const uint16_t MAX_READ_LEN = 511;           // 短 reads 最大长度 (硬编码)
const uint32_t MAX_READ_LEN_LONG = 4294967290; // 长 reads 最大长度
const int NUM_DICT_REORDER = 2;              // 重排序字典数量
const int MAX_SEARCH_REORDER = 1000;         // 最大搜索次数
const int THRESH_REORDER = 4;                // Hamming 距离阈值
const int NUM_LOCKS_REORDER = 0x1000000;     // 锁数量 (16M)
const float STOP_CRITERIA_REORDER = 0.5;     // 停止条件
const int NUM_DICT_ENCODER = 2;              // 编码字典数量
const int MAX_SEARCH_ENCODER = 1000;         // 编码最大搜索次数
const int THRESH_ENCODER = 24;               // 编码 Hamming 阈值
const int NUM_READS_PER_BLOCK = 256000;      // 短 reads 每块数量
const int NUM_READS_PER_BLOCK_LONG = 10000;  // 长 reads 每块数量
const int BSC_BLOCK_SIZE = 64;               // BSC 块大小 (MB)
```

## 5. 全局状态依赖分析

### 5.1 全局状态清单

| 模块 | 全局状态 | 类型 | 说明 |
|------|----------|------|------|
| reorder | `reorder_global<bitset_size>` | 结构体 | 重排序全局状态 |
| reorder | `basemask` | 二维数组 | 碱基掩码 |
| reorder | `dict_lock`, `read_lock` | OpenMP 锁 | 并发控制 |
| encoder | `encoder_global` | 结构体 | 编码全局状态 |
| encoder | `encoder_global_b<bitset_size>` | 结构体 | 编码位集状态 |
| encoder | `enc_noise` | 二维数组 | 噪声编码表 |
| all | 临时文件 | 文件系统 | 中间数据存储 |

### 5.2 Block-based 适配挑战

1. **全局重排序**: Spring 的重排序是全局的，需要改造为两阶段策略
2. **临时文件依赖**: 大量使用临时文件，需要改为内存缓冲
3. **模板参数**: `bitset_size` 是编译时常量，需要运行时适配
4. **OpenMP 依赖**: 需要替换为 TBB 并行原语

## 6. 内存分配模式分析

### 6.1 Phase 1 (全局分析) 内存估算

| 组件 | 每 Read 内存 | 说明 |
|------|-------------|------|
| Minimizer 索引 | ~16 bytes | BooPHF + startpos + read_id |
| Reorder Map | ~8 bytes | original_id -> archive_id |
| **总计** | **~24 bytes/read** | |

### 6.2 Phase 2 (分块压缩) 内存估算

| 组件 | 每 Read 内存 | 说明 |
|------|-------------|------|
| Read 位集 | ~128 bytes | `std::bitset<1024>` (511bp × 2bits) |
| Read 长度 | 2 bytes | `uint16_t` |
| Contig 列表 | ~50 bytes | `contig_reads` 结构体 |
| **总计** | **~180 bytes/read** | 单个 Block 内 |

### 6.3 内存峰值控制

- Block 大小: 256,000 reads (短读) / 10,000 reads (长读)
- 单 Block 内存: ~46 MB (短读) / ~1.8 MB (长读)
- 建议 `--memory-limit`: 8 GB (支持 ~300M reads 的全局分析)

## 7. 接口边界分析

### 7.1 需要提取的核心接口

```cpp
// 1. 字典构建
template <size_t bitset_size>
void constructdictionary(std::bitset<bitset_size> *read, bbhashdict *dict,
                         uint16_t *read_lengths, const int numdict,
                         const uint32_t &numreads, const int bpb,
                         const std::string &basedir, const int &num_thr);

// 2. 重排序
template <size_t bitset_size>
void reorder(std::bitset<bitset_size> *read, bbhashdict *dict,
             uint16_t *read_lengths, const reorder_global<bitset_size> &rg);

// 3. 编码
template <size_t bitset_size>
void encode(std::bitset<bitset_size> *read, bbhashdict *dict, uint32_t *order_s,
            uint16_t *read_lengths_s, const encoder_global &eg,
            const encoder_global_b<bitset_size> &egb);

// 4. 共识构建
std::string buildcontig(std::list<contig_reads> &current_contig,
                        const uint32_t &list_size);

// 5. BSC 压缩
namespace bsc {
    int BSC_compress(const char *infile, const char *outfile);
    int BSC_str_array_compress(const char *outfile, std::string *str_array,
                               uint32_t num_strings, uint32_t *str_len_array);
}
```

### 7.2 需要适配的接口

| 原接口 | 适配方向 | 说明 |
|--------|----------|------|
| 临时文件 I/O | 内存缓冲 | 消除文件系统依赖 |
| OpenMP 并行 | TBB 并行 | 统一并行框架 |
| 全局重排序 | 两阶段策略 | 支持 Block-based 随机访问 |
| 模板 bitset_size | 运行时适配 | 支持可变读长 |

## 8. 依赖关系图

```
┌─────────────────────────────────────────────────────────────────┐
│                         spring.cpp                               │
│                    (主压缩/解压缩入口)                            │
└─────────────────────────────────────────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
┌───────────────┐      ┌───────────────┐      ┌───────────────┐
│  preprocess   │      │    reorder    │      │   encoder     │
│  (FASTQ解析)  │      │   (重排序)    │      │   (编码)      │
└───────────────┘      └───────────────┘      └───────────────┘
        │                       │                       │
        │                       ▼                       │
        │              ┌───────────────┐                │
        │              │  bitset_util  │                │
        │              │  (字典构建)   │                │
        │              └───────────────┘                │
        │                       │                       │
        │                       ▼                       │
        │              ┌───────────────┐                │
        │              │    BooPHF     │                │
        │              │ (完美哈希)    │                │
        │              └───────────────┘                │
        │                                               │
        └───────────────────────┬───────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
┌───────────────┐      ┌───────────────┐      ┌───────────────┐
│    libbsc     │      │id_compression │      │      qvz      │
│  (BSC压缩)    │      │  (ID压缩)     │      │ (质量值压缩)  │
└───────────────┘      └───────────────┘      └───────────────┘
```

## 9. 风险与建议

### 9.1 高风险点

1. **MAX_READ_LEN = 511 硬编码**: 深度嵌入 bitset 大小，扩展困难
2. **全局状态依赖**: 需要大量重构以支持 Block 独立性
3. **临时文件依赖**: 需要改为内存缓冲或流式处理
4. **OpenMP 锁机制**: 需要替换为 TBB 并发原语

### 9.2 建议适配策略

1. **两阶段压缩**: 
   - Phase 1: 全局 Minimizer 提取 + 重排序决策
   - Phase 2: Block 内独立压缩

2. **状态隔离**:
   - 将全局状态封装为 `CompressionContext` 类
   - 每个 Block 使用独立的 `BlockContext`

3. **内存管理**:
   - 实现 `MemoryBudget` 类控制内存使用
   - 超大文件自动启用分治模式

4. **并行框架**:
   - 使用 TBB `parallel_pipeline` 替代 OpenMP
   - 保持 Block 级别并行

## 10. 下一步行动

1. **Task 7.2**: 提取 Spring 核心算法到 `vendor/spring-core/`
2. **Task 8.1**: 实现 Phase 1 全局分析模块
3. **Task 8.3**: 实现 Phase 2 分块压缩模块

---

**License 注意**: Spring 采用 "Non-exclusive Research Use License"，仅供非商业/研究使用。fq-compressor 项目需遵守此限制。
