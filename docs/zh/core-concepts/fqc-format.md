---
title: FQC 归档格式
description: .fqc 归档格式的技术规范
version: 0.2.0
last_updated: 2026-04-22
language: zh
---

# FQC 归档格式规范

> .fqc 文件格式的技术规范

## 概述

FQC（FASTQ 压缩）格式是针对基因 FASTQ 数据的领域特定归档格式。其特性包括：

- **基于块的存储**: 独立压缩块，支持并行访问
- **列式流**: ID、序列和质量的分离存储
- **随机访问**: 对任何 reads 范围的 O(1) 访问
- **完整性验证**: 多级校验和
- **重排序支持**: 用于原始顺序恢复的 reads 排序映射

## 文件结构

```
┌─────────────────────────────────────────────────────────────────┐
│                        FQC 归档                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 固定头部 (256 字节)                                      │   │
│  │   - 魔数: "FQCC" (4 字节)                               │   │
│  │   - 版本: uint16_t                                      │   │
│  │   - 格式标志: uint16_t                                  │   │
│  │   - 块数: uint32_t                                      │   │
│  │   - 总 reads: uint64_t                                  │   │
│  │   - 总碱基: uint64_t                                    │   │
│  │   - 保留: 填充到 256 字节                               │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 压缩参数块 (可变)                                        │   │
│  │   - 算法参数                                            │   │
│  │   - 质量压缩设置                                        │   │
│  │   - 块大小配置                                          │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 块 0                                                    │   │
│  │   ┌─────────────────────────────────────────────────┐   │   │
│  │   │ 块头部 (64 字节)                                │   │   │
│  │   │   - 块 ID、原始大小、压缩后大小                 │   │   │
│  │   │   - reads 数、碱基数                            │   │   │
│  │   │   - 校验和 (CRC32C)                             │   │   │
│  │   └─────────────────────────────────────────────────┘   │   │
│  │   ┌─────────────────────────────────────────────────┐   │   │
│  │   │ 压缩数据                                        │   │   │
│  │   │   ┌─────────────┐ ┌─────────────┐ ┌─────────┐  │   │   │
│  │   │   │ ID 流       │ │ 序列流      │ │ 质量    │  │   │   │
│  │   │   │ (zstd)      │ │ (zstd)      │ │ 流      │  │   │   │
│  │   │   │             │ │             │ │ (zstd)  │  │   │   │
│  │   │   └─────────────┘ └─────────────┘ └─────────┘  │   │   │
│  │   └─────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 块 1 ...                                               │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 块 N-1                                                 │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 页脚 (可变)                                             │   │
│  │   - 块索引表                                           │   │
│  │   - 重排序映射 (如启用)                                │   │
│  │   - 全局元数据 (可选)                                  │   │
│  │   - 页脚校验和与大小                                   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## 固定头部格式

```c
struct FQCHeader {
    // 魔数
    char magic[4] = {'F', 'Q', 'C', 'C'};  // "FQCC"
    
    // 版本信息
    uint16_t version_major = 0;
    uint16_t version_minor = 1;
    uint16_t version_patch = 0;
    
    // 格式标志 (位掩码)
    uint16_t format_flags;
    // 位 0: 双端测序数据
    // 位 1: 已重排序 (有重排序映射)
    // 位 2: 保留原始顺序
    // 位 3-15: 保留
    
    // 文件统计
    uint32_t block_count;
    uint64_t total_reads;
    uint64_t total_bases;
    
    // 未来使用保留
    uint8_t reserved[224];  // 填充到 256 字节
};
```

## 块格式

### 块头部

```c
struct BlockHeader {
    uint32_t block_id;           // 顺序块号
    uint32_t flags;              // 块特定标志
    
    // 大小信息
    uint64_t original_size;      // 未压缩数据大小
    uint64_t compressed_size;    // 压缩后数据大小
    
    // 内容统计
    uint32_t read_count;         // 块中 reads 数
    uint64_t base_count;         // 块中碱基数
    
    // 校验和
    uint32_t crc32c_data;        // 压缩数据 CRC32C
    uint32_t crc32c_uncompressed; // 未压缩数据 CRC32C
    
    // 保留
    uint8_t reserved[16];
};
```

### 块数据布局

每个块包含三个独立的压缩流：

```c
struct BlockData {
    // ID 流 (标记化 + 增量编码)
    uint64_t id_stream_offset;
    uint64_t id_stream_size;
    
    // 序列流 (ABC 编码)
    uint64_t seq_stream_offset;
    uint64_t seq_stream_size;
    
    // 质量流 (SCM 编码)
    uint64_t qual_stream_offset;
    uint64_t qual_stream_size;
    
    // 压缩数据紧随其后
    uint8_t data[];
};
```

#### ID 流格式

```c
struct IDStream {
    // 头部
    uint32_t compression_type;   // 0 = zstd, 1 = lz4
    uint32_t token_count;        // 令牌数
    
    // 令牌字典 (如使用)
    uint32_t dict_entries;
    // 后跟字典项...
    
    // 压缩令牌数据
    uint8_t compressed_data[];
};
```

#### 序列流格式

```c
struct SequenceStream {
    // 头部
    uint32_t compression_type;
    uint32_t consensus_count;    // 共识序列数
    
    // 共识序列 (变长)
    uint32_t consensus_lengths[];  // 每个共识长度
    char consensus_data[];         // 拼接的共识序列
    
    // 增量编码 reads
    uint8_t compressed_deltas[];
};

struct DeltaEntry {
    uint32_t consensus_id;       // 映射到的共识
    uint32_t edit_count;         // 编辑数
    Edit edits[];                // 编辑数组
};

struct Edit {
    uint8_t type;                // 0=匹配, 1=替换, 2=插入, 3=删除
    uint32_t position;           // 共识中的位置
    char base;                   // 用于替换/插入
};
```

#### 质量流格式

```c
struct QualityStream {
    // 头部
    uint32_t compression_type;   // 0 = SCM + 算术编码
    uint32_t context_order;      // 上下文模型阶数
    
    // 模型参数 (SCM)
    uint32_t model_weights[];    // 上下文模型权重
    
    // 压缩质量分数
    uint8_t compressed_data[];
};
```

## 页脚格式

### 块索引表

```c
struct BlockIndex {
    uint32_t entry_count;        // = block_count
    
    struct BlockIndexEntry {
        uint64_t offset;         // 块文件偏移
        uint64_t size;           // 总块大小 (头部 + 数据)
        uint32_t read_count;     // 块中 reads 数
        uint64_t first_read_id;  // 首 reads 全局 ID
    } entries[];
};
```

### 重排序映射

用于在压缩重排序后恢复原始 reads 顺序。

```c
struct ReorderMap {
    uint32_t format_version = 1;
    uint64_t read_count;
    uint32_t compression_type;   // 0 = zstd, 1 = 无压缩
    
    // 映射: compressed_order -> original_order
    // 存储为增量编码 varint 以压缩
    uint8_t compressed_mapping[];
};
```

**增量编码**:
```
原始: [1000, 5, 999, 4, 998, 3, ...]
增量: [1000, -995, 994, -995, 994, -995, ...]
Varint: 紧凑整数编码
```

### 页脚尾部

```c
struct FooterTrailer {
    uint64_t footer_size;        // 页脚大小（字节）
    uint64_t footer_offset;      // 页脚开始的文件偏移
    uint64_t index_offset;       // 块索引偏移
    uint64_t reorder_map_offset; // 重排序映射偏移（0 如无）
    uint32_t crc32c_footer;      // 页脚数据校验和
};
```

## 格式标志

| 位 | 名称 | 描述 |
|-----|------|------|
| 0 | `PAIRED_END` | 包含双端测序数据 |
| 1 | `REORDERED` | 压缩时重排序了 reads |
| 2 | `ORIG_ORDER_PRESERVED` | 可恢复原始顺序 |
| 3 | `HAS_METADATA` | 包含全局元数据块 |
| 4-15 | 保留 | 必须为 0 |

## 版本历史

| 版本 | 日期 | 变更 |
|---------|------|--------- |
| 0.1.0 | 2026-04-16 | 初始发布 |

## 实现说明

### 块大小选择

默认块大小：每块 ~10MB 未压缩

- **较小块**: 更好的随机访问粒度，更多开销
- **较大块**: 更好压缩，更差随机访问

建议：
- 随机访问工作负载：5-10MB 块
- 顺序工作负载：10-20MB 块

### 压缩级别

| 模式 | ZSTD 级别 | 使用场景 |
|------|------------|----------|
| fast | 1 | 速度优先 |
| balanced | 3 | 默认 |
| best | 19 | 存储优先 |

### 内存映射

格式支持内存映射读取：

```cpp
// 块对齐确保块头部在可预测偏移处
constexpr size_t kBlockAlignment = 4096;  // 页面大小

// 总块大小对齐到对齐边界
padded_size = ((actual_size + kBlockAlignment - 1) / kBlockAlignment) * kBlockAlignment;
```

### 校验和

- **CRC32C**: 用于数据完整性（x86 硬件加速）
- **覆盖**: 块数据、页脚结构
- **验证**: 读取时和通过 `fqc verify` 显式验证

## 兼容性

### 向前兼容

新版本读取器应该：
- 忽略未知格式标志
- 跳过未知页脚部分
- 处理比预期大的头部

### 向后兼容

旧版本读取器遇到新版本格式应该：
- 拒绝更高主版本号的文件
- 警告更高次版本号
- 使用兼容功能子集正常工作

---

**🌐 语言**: [English](../../en/core-concepts/fqc-format.md) | [简体中文（当前）](./fqc-format.md)
