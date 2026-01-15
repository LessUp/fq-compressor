# 设计一致性修复（2026-01-15）

## 概述

修复设计/需求/任务文档中关于版本语义、索引兼容、流式模式约束、分治顺序与校验范围等不一致点，明确 PE 计数语义与质量丢弃行为。

## 修改文件

- `.kiro/specs/fq-compressor/design.md`
- `.kiro/specs/fq-compressor/requirements.md`
- `.kiro/specs/fq-compressor/tasks.md`

## 变更内容

1. **版本与标志语义明确**
   - 定义 `Version` 的 major/minor 编码语义与兼容规则。
   - 约束 `LEGACY_LONG_READ_MODE` 保留位，明确 Streaming 强制 `PRESERVE_ORDER` 且不保存 Reorder Map。
2. **索引与校验一致性**
   - BlockIndex 增加 `header_size`/`entry_size` 前向兼容字段。
   - 明确 Block 校验基于未压缩逻辑流 (ID/SEQ/QUAL/AUX)。
3. **随机访问与 PE 计数语义**
   - 规定 Read ID 以单条 Read 计数（PE 总数 = 2 * Pairs）。
4. **分治与 Reorder Map 全局连续性**
   - 分治模式下按 Chunk 拼接 Reorder Map 并累加偏移，保证全局 `archive_id` 连续。
5. **质量值丢弃行为**
   - 明确丢弃质量值时解压填充占位符质量值，保持 FASTQ 四行格式。
6. **流式模式附录对齐**
   - 统一描述：流式模式禁用块内重排序、强制 PRESERVE_ORDER 且不写入 Reorder Map。
7. **细节澄清补充**
   - 质量丢弃模式默认占位符为 `'!'`。
   - BlockIndex `entry_size` 兼容规则：大于结构大小跳过尾部字段，小于必须字段报错。
   - Reorder Map 超大映射允许分块索引或 mmap 方式按需解码。
