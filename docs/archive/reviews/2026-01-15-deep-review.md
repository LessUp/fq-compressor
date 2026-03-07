# fq-compressor 设计方案深度审查报告

**审查日期**: 2026-01-15  
**审查范围**: `.kiro/specs/fq-compressor/` 目录下的 `design.md`, `requirements.md`, `tasks.md`, `ref.md`  
**审查目标**: 检查设计一致性、边界条件、模糊定义和潜在错误

---

## 审查摘要

| 类别 | 数量 | 状态 |
|------|------|------|
| 高优先级问题 | 3 | 🔴 需要修复 |
| 中优先级问题 | 7 | 🟡 建议修复 |
| 低优先级问题 | 5 | 🟢 可选修复 |
| 已确认正确 | 7 | ✅ 无需修改 |

---

## 第一部分：高优先级问题 (必须修复)

### 问题 #6: 采样策略边界条件未定义

**位置**: 
- `requirements.md` - Long Read 选项
- `tasks.md` - 18.1 实现长读检测
- `design.md` - 自动检测策略

**问题描述**:

文档中描述 "采样前 1000 条 Reads 计算 median 和 max length"，但存在以下边界条件未定义：

1. **文件少于 1000 条 reads**: 如果输入文件只有 500 条 reads，采样策略是什么？
2. **采样后发现超长读**: 如果前 1000 条都是 100bp 短读（归类为 SHORT），但第 1001 条是 50KB 长读，会导致：
   - Spring ABC 尝试处理超过 511bp 的数据 → 崩溃或数据损坏
   - 压缩策略与实际数据不匹配
3. **流式模式下的采样**: stdin 输入时无法预先采样，如何处理？

**影响分析**:

```
场景: 用户压缩一个混合数据文件（前 1000 条是 Illumina 短读，后面是 PacBio 长读）
预期: 系统应检测到长读并使用 Zstd
实际: 系统可能错误地使用 ABC，导致数据损坏或崩溃
```

**推荐修复方案**:

1. 采样数量改为 `min(1000, total_reads)`
2. 对于文件输入，增加可选的全文件扫描 max_length（`--scan-all-lengths`）
3. 流式模式下，默认使用 MEDIUM 策略（保守）或要求用户显式指定 `--long-read-mode`
4. 在压缩过程中动态检测超长 reads，遇到时报错并建议重新压缩

---

### 问题 #7: --range 参数语义不完整

**位置**: 
- `requirements.md` - decompress 子命令

**问题描述**:

`--range <start:end>` 参数描述为 "闭区间；以归档存储顺序计数"，但缺少以下关键定义：

1. **索引基数**: 是 0-based (程序员习惯) 还是 1-based (用户友好)？
2. **边界错误处理**:
   - `start > end` 时的行为？
   - `start` 或 `end` 超出 `TotalReadCount` 时的行为？
   - `start` 为负数时的行为？
3. **省略语法**: 文档说 "end 可省略表示直到文件末尾"，但 start 能否省略？

**影响分析**:

```
用户执行: fqc decompress -i file.fqc --range 0:100
问题: 用户期望获取第 1-101 条 reads (0-based) 还是第 0-100 条 (1-based)?
```

**推荐修复方案**:

1. 明确使用 **1-based 索引**（与 SAMtools、bedtools 等生物信息工具一致）
2. 定义错误处理：
   - `start > end`: 报错 `EXIT_USAGE_ERROR (1)`
   - `start < 1`: 报错 `EXIT_USAGE_ERROR (1)`
   - `end > TotalReadCount`: 自动截断到 `TotalReadCount`，发出警告
3. 支持省略语法：
   - `:end` 等价于 `1:end`
   - `start:` 等价于 `start:TotalReadCount`

---

### 问题 #11: --streams 非 all 模式输出格式未定义

**位置**: 
- `requirements.md` - decompress 子命令

**问题描述**:

`--streams <id|seq|qual|all>` 允许选择性解码子流，但 FASTQ 格式要求四行完整：
```
@ID
SEQUENCE
+
QUALITY
```

如果用户指定 `--streams seq`，输出格式是什么？

**可能的解释**:
1. 输出纯序列（每行一条，无 ID/Quality）
2. 输出 FASTA 格式（>ID + SEQUENCE）
3. 输出 TSV 格式（ID\tSEQUENCE）
4. 报错（不支持单独输出）

**影响分析**:

```
用户执行: fqc decompress -i file.fqc --streams seq -o output.txt
问题: output.txt 的格式是什么？下游工具能否解析？
```

**推荐修复方案**:

1. 定义输出格式规则：
   - `--streams all`: 标准 FASTQ 四行格式
   - `--streams id`: 每行一个 ID（不含 `@` 前缀）
   - `--streams seq`: 每行一条序列（纯碱基）
   - `--streams qual`: 每行一条质量值（纯 ASCII）
   - `--streams id,seq`: FASTA 格式（`>ID\nSEQUENCE`）
   - `--streams id,seq,qual`: 标准 FASTQ 格式
2. 添加 `--output-format <fastq|fasta|tsv|raw>` 参数控制输出格式
3. 在帮助信息中明确说明各组合的输出格式

---

## 第二部分：中优先级问题 (建议修复)

### 问题 #2: ReorderMap 结构缺少前向兼容字段

**位置**: `design.md` - 4.1 Reorder Map (Optional)

**问题描述**:

`BlockIndex` 结构包含 `header_size` 和 `entry_size` 用于前向兼容，但 `ReorderMap` 结构没有类似字段：

```cpp
// BlockIndex - 有兼容性字段
struct BlockIndex {
    uint32_t header_size;     // ✓ 前向兼容
    uint32_t entry_size;      // ✓ 前向兼容
    uint64_t num_blocks;
    IndexEntry entries[];
};

// ReorderMap - 缺少兼容性字段
struct ReorderMap {
    uint64_t total_reads;
    uint64_t forward_map_size;
    uint64_t reverse_map_size;
    uint8_t forward_map[];
    uint8_t reverse_map[];
};
```

**影响分析**:

如果未来需要在 ReorderMap 中添加新字段（如分块索引、压缩算法标识），旧版本 Reader 将无法正确跳过未知字段。

**推荐修复方案**:

```cpp
struct ReorderMap {
    uint32_t header_size;         // 新增：ReorderMap header 大小
    uint32_t version;             // 新增：ReorderMap 版本号
    uint64_t total_reads;
    uint64_t forward_map_size;
    uint64_t reverse_map_size;
    uint8_t forward_map[];
    uint8_t reverse_map[];
};
```

---

### 问题 #3: GlobalHeader 缺少完整结构体定义

**位置**: `design.md` - 2. Global Header

**问题描述**:

GlobalHeader 描述了各字段的语义，但缺少：
1. 完整的 C++ 结构体定义
2. 字段的精确字节布局
3. `OriginalFilename` 的编码方式（UTF-8? Latin-1?）
4. 字符串是否包含 null 终止符

**推荐修复方案**:

添加完整的结构体定义：

```cpp
struct GlobalHeader {
    uint32_t header_size;           // 偏移 0, 包含自身的总大小
    uint64_t flags;                 // 偏移 4
    uint8_t  compression_algo;      // 偏移 12
    uint8_t  checksum_type;         // 偏移 13
    uint16_t reserved;              // 偏移 14, 对齐填充
    uint64_t total_read_count;      // 偏移 16
    uint16_t original_filename_len; // 偏移 24
    // uint8_t original_filename[original_filename_len]; // 偏移 26, UTF-8 编码, 无 null 终止
    // uint64_t timestamp;          // 偏移 26 + original_filename_len
    // uint8_t codec_params[];      // 可选, 以 0xFF 结束
};
// 最小大小: 26 + 8 = 34 bytes (无文件名)
```

---

### 问题 #4: CodecParams 存在检测机制模糊

**位置**: `design.md` - Codec 参数存储

**问题描述**:

文档说 "需要参数的 Codec 在 GlobalHeader 末尾添加可选的 `CodecParams` 段"，但：
1. 如何知道 CodecParams 是否存在？
2. CodecParams 是否计入 `GlobalHeaderSize`？
3. 如果 `GlobalHeaderSize` 大于已知字段总和，剩余部分是 CodecParams 还是未知扩展字段？

**推荐修复方案**:

1. CodecParams **计入** `GlobalHeaderSize`
2. 检测逻辑：
   ```cpp
   size_t known_fields_size = 34 + original_filename_len;
   if (header_size > known_fields_size) {
       // 存在 CodecParams 或扩展字段
       parse_codec_params(buffer + known_fields_size, header_size - known_fields_size);
   }
   ```
3. CodecParams 以 `0xFF` 结束，之后的数据为未知扩展字段（应跳过）

---

### 问题 #8: PE Consecutive 模式下 range 语义未定义

**位置**: `design.md` - PE 存储布局

**问题描述**:

Consecutive 模式存储布局为 `R1_0, R1_1, ..., R1_N, R2_0, R2_1, ..., R2_N`。

如果用户执行 `--range 1:100`，应该返回：
- **选项 A**: 前 100 条 R1（archive_id 1-100）
- **选项 B**: 前 50 对 (R1_1-R1_50 + R2_1-R2_50)

**推荐修复方案**:

1. `--range` 始终以 **archive_id** 计数，与存储布局无关
2. Consecutive 模式下 `--range 1:100` 返回 archive_id 1-100（即前 100 条 R1）
3. 如需按配对访问，使用 `--range-pairs 1:50`（新增参数）
4. 在文档中明确说明 Consecutive 模式下 range 的行为

---

### 问题 #9: 分治模式下 block_id 连续性未定义

**位置**: `design.md` - 超大文件分治

**问题描述**:

文档说 "archive_id 全局连续"，但没有说明 `block_id` 的分配策略。

如果每个 Chunk 的 block_id 都从 0 开始：
- Chunk 0: block_id 0, 1, 2
- Chunk 1: block_id 0, 1, 2  ← 冲突！

**推荐修复方案**:

1. `block_id` **全局连续**，跨 Chunk 递增
2. 分治模式下：
   - Chunk 0: block_id 0, 1, 2
   - Chunk 1: block_id 3, 4, 5
3. 在 Block Index 中，block_id 必须与数组索引一致

---

### 问题 #13: ID_MODE=discard 时的 ID 重建格式未定义

**位置**: `design.md` - Global Header Flags

**问题描述**:

当 `ID_MODE=10 (Discard/Indexed)` 时，解压输出的 ID 格式是什么？

可能的格式：
- `@1`, `@2`, `@3`
- `@read_1`, `@read_2`, `@read_3`
- `@1/1`, `@1/2` (PE 模式)
- `@fqc_1`, `@fqc_2`

**推荐修复方案**:

1. SE 模式: `@{archive_id}` (如 `@1`, `@2`)
2. PE Interleaved 模式: `@{pair_id}/{read_num}` (如 `@1/1`, `@1/2`, `@2/1`, `@2/2`)
3. PE Consecutive 模式: `@{archive_id}` (如 `@1`, `@2`, ..., `@N`, `@N+1`, ...)
4. 可通过 `--id-prefix <prefix>` 自定义前缀（默认为空）

---

### 问题 #5: Quality Discard 模式的 codec_qual 值语义混淆

**位置**: `design.md` - Quality Stream

**问题描述**:

文档说 "Discard 模式: `codec_qual=RAW` 且 `size_qual=0`"。

但 `RAW` (0x0) 通常表示 "无压缩的原始数据"，而不是 "无数据"。这可能导致：
- Reader 尝试读取 0 字节的 RAW 数据
- 语义混淆：RAW + size=0 vs 真正的空数据

**推荐修复方案**:

1. 保持现有设计，但明确文档化：
   - `codec_qual=RAW (0x0)` 且 `size_qual=0` 表示 "Quality 被丢弃"
   - 解压时填充占位符质量值（默认 `'!'`，可通过 `--placeholder-qual` 配置）
2. 或者定义专用值 `CODEC_DISCARDED = 0x0F`（使用 RESERVED 的一个值）

---

## 第三部分：低优先级问题 (可选修复)

### 问题 #12: uniform_read_length=0 的歧义

**位置**: `design.md` - BlockHeader

**问题描述**:

`uniform_read_length = 0` 表示 "可变长度，使用 Aux"，但如果真的存在长度为 0 的 read（虽然实际不太可能），会产生歧义。

**推荐修复方案**:

1. 明确文档化：长度为 0 的 read 是无效的，解析时应报错
2. 或使用 `0xFFFFFFFF` 表示 "可变长度"

---

### 问题 #14: 空文件处理策略未定义

**位置**: 所有文档

**问题描述**:

没有说明如何处理空 FASTQ 文件（0 reads）。

**推荐修复方案**:

1. 空文件产生有效的 .fqc 文件（只有 header/footer，无 blocks）
2. `TotalReadCount = 0`
3. `num_blocks = 0`
4. 解压空 .fqc 产生空 FASTQ 文件

---

### 问题 #15: max_block_bases CLI 参数缺失

**位置**: `design.md` vs `requirements.md`

**问题描述**:

`design.md` 提到 "max_block_bases 上限（默认 200MB；Ultra-long 默认 50MB）"，但 `requirements.md` 的 CLI 参数中没有对应选项。

**推荐修复方案**:

1. 添加 `--max-block-bases <bytes>` 参数（高级选项）
2. 或明确这是内部参数，不暴露给用户

---

## 第四部分：已确认正确的部分 ✅

以下设计经审查确认正确，无需修改：

1. **Spring 511bp 限制** - 所有文档一致，明确说明 `max <= 511` 的约束
2. **Read 长度分类优先级** - design.md, requirements.md, tasks.md 三个文档完全一致
3. **PE 计数语义** - 明确为 "单条 Read 记录计数"（PE 总数 = 2 * Pairs）
4. **流式模式标志位关系** - `STREAMING_MODE=1` 强制 `PRESERVE_ORDER=1` 和 `HAS_REORDER_MAP=0`
5. **Block 校验范围** - 明确为 "未压缩逻辑流 (ID+SEQ+QUAL+Aux)"
6. **BlockHeader 大小计算** - 104 bytes 计算正确
7. **内存估算数值** - Phase 1: ~24 bytes/read (16+8) 一致

---

## 修复计划

| 优先级 | 问题 | 修复文件 | 状态 |
|--------|------|----------|------|
| 🔴 高 | #6 采样边界 | design.md, requirements.md, tasks.md | 待修复 |
| 🔴 高 | #7 --range 语义 | requirements.md | 待修复 |
| 🔴 高 | #11 --streams 输出格式 | requirements.md | 待修复 |
| 🟡 中 | #2 ReorderMap 兼容性 | design.md | 待修复 |
| 🟡 中 | #3 GlobalHeader 布局 | design.md | 待修复 |
| 🟡 中 | #4 CodecParams 检测 | design.md | 待修复 |
| 🟡 中 | #8 PE Consecutive range | design.md, requirements.md | 待修复 |
| 🟡 中 | #9 分治 block_id | design.md | 待修复 |
| 🟡 中 | #13 ID discard 格式 | design.md, requirements.md | 待修复 |
| 🟡 中 | #5 Quality Discard codec | design.md | 待修复 |
| 🟢 低 | #12 uniform_read_length | design.md | 待修复 |
| 🟢 低 | #14 空文件处理 | design.md, requirements.md | 待修复 |
| 🟢 低 | #15 max_block_bases CLI | requirements.md | 待修复 |

---

## 附录：审查方法论

本次审查采用以下方法：

1. **交叉验证**: 对比 design.md, requirements.md, tasks.md 中的相同概念，检查一致性
2. **边界分析**: 识别数值参数的边界条件（0, 负数, 最大值, 溢出）
3. **状态机分析**: 检查标志位组合的有效性和互斥关系
4. **用户场景模拟**: 模拟典型用户操作，检查是否有未定义行为
5. **格式兼容性**: 检查二进制格式的前向/后向兼容性设计

---

*审查人: Kiro AI Assistant*  
*审查版本: v1.0*
