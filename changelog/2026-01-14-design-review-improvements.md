# 设计评审改进 - 2026-01-14

## 概述

基于设计评审发现的问题，对 fq-compressor 规格文档进行了全面更新，解决了架构矛盾、格式设计缺陷、参数定义不完整等问题。

## 修改文件

- `.kiro/specs/fq-compressor/design.md`
- `.kiro/specs/fq-compressor/requirements.md`
- `.kiro/specs/fq-compressor/tasks.md`

---

## 重要改进

### 1. 两阶段压缩策略 (Two-Phase Compression)

**问题**: Spring 的 ABC 算法依赖全局 Reads 重排序，与 Block-based 随机访问存在本质冲突。

**解决方案**:
- **Phase 1 (全局分析)**: Minimizer 提取 → 全局 Bucketing → 全局 Reordering → 生成 Reorder Map
- **Phase 2 (分块压缩)**: 按 Block 并行压缩，每个 Block 独立编码

**优势**:
- 压缩率: 全局重排序保证接近 Spring 原版
- 随机访问: Block 独立性保证 O(1) 随机访问
- 内存控制: Phase 1 仅需 O(N) 的 Minimizer 索引

### 2. Reorder Map 支持

新增 `HAS_REORDER_MAP` 标志和 `ReorderMap` 结构，支持：
- 用户查询"原始第 N 条 Read 在归档中的位置"
- 解压时按原始顺序输出 (`--original-order`)
- Delta + Varint 压缩存储，约 2 bytes/read

### 3. 格式字段类型修复

- **Stream Offsets**: `uint32_t` → `uint64_t` (支持大 Block)
- **Flags**: `uint32` → `uint64` (预留更多空间)
- **BlockHeader**: 新增 `compressed_size` 和各 Stream 的 `size_*` 字段

### 4. Codec 版本兼容策略

明确规定：
- **Family 变更**: 不兼容，必须报错 `EXIT_UNSUPPORTED_CODEC (5)`
- **Version 变更**: 向后兼容，新版本 Reader 可读取旧版本数据

预定义 Codec Family:
| Family | Name | Description |
|--------|------|-------------|
| 0x0 | RAW | 无压缩 |
| 0x1 | ABC_V1 | Spring ABC (Sequence) |
| 0x2 | SCM_V1 | Statistical Context Mixing (Quality) |
| 0x3 | DELTA_LZMA | Delta + LZMA (IDs) |
| 0x4 | DELTA_ZSTD | Delta + Zstd (IDs) |

### 5. Block Index 完善

```cpp
struct IndexEntry {
    uint64_t offset;           // Block 绝对偏移
    uint64_t compressed_size;  // Block 压缩后大小
    uint64_t archive_id_start; // 起始 Read ID (归档顺序)
    uint32_t read_count;       // Read 数量
};
```

### 6. 校验流程优化

推荐校验顺序:
1. 读取 Footer，验证 `magic_end`
2. 验证 `global_checksum`（快速检测截断/损坏）
3. 读取 Block Index
4. 逐 Block 验证 `block_xxhash64`

---

## Paired-End 支持设计

### PE 存储布局
- **Interleaved (交错)**: R1_0, R2_0, R1_1, R2_1, ... (默认)
- **Consecutive (连续)**: R1_0, ..., R1_N, R2_0, ..., R2_N

### CLI 参数
- `-1, --read1 <path>`: R1 文件
- `-2, --read2 <path>`: R2 文件
- `--interleaved`: 交错格式输入
- `--pe-layout <interleaved|consecutive>`: 存储布局
- `--split-pe`: 解压时分离输出

---

## Long Read 模式设计

### 自动检测
- 采样前 1000 条 Reads
- 阈值: median length > 10KB

### 压缩策略差异
| 组件 | Short Read | Long Read |
|------|------------|-----------|
| Sequence | ABC (Spring) | Overlap-based 或 Zstd |
| Quality | SCM Order-2 | SCM Order-1 |
| Reordering | 启用 | 禁用 |
| Block Size | 100K reads | 10K reads |

---

## 内存管理设计

### 内存预算
- 默认上限: 8192MB (`--memory-limit`)
- Phase 1 估算: ~24 bytes/read
- Phase 2 估算: ~50 bytes/read × block_size

### 超大文件分治
当文件超过内存预算时，自动启用 Chunk-wise Compression。

---

## 任务计划更新

### 时间估算
| Phase | 预估时间 | 风险等级 |
|-------|----------|----------|
| Phase 1 | 2-3 周 | 低 |
| Phase 2 | 4-6 周 | **高** |
| Phase 3 | 2-3 周 | 中 |
| Phase 4 | 2-3 周 | 中 |
| Phase 5 | 2 周 | 低 |

**总计**: 12-17 周（约 3-4 个月）

### 新增任务
- 8.1 实现 Phase 1: 全局分析模块
- 8.2 实现 Reorder Map 存储
- 8.3 实现 Phase 2: 分块压缩模块
- 8.4 实现内存管理模块
- 18.3 编写长读属性测试
- 19.3 实现 PE 解压功能
- 19.4 编写 PE 属性测试
