# fq-compressor 规格/设计决策更新：归档格式、随机访问、校验与并行模型（2026-01-14）

## 背景

- 现有设计将 `.fqc` 表达为一组归档内文件（例如 `cp.bin`、`read_seq.bin` 等）。但本项目希望定义一个正式、可扩展的单文件容器格式，以便支持随机访问与完整性校验。
- 参考实现：DSRC（块级 CRC32 与 footer 索引）、fqzcomp5（块内分段与策略字段）、repaq（chunk-based 容器 + compare 验证）、pigz（并行 gzip 思路）。

## 变更

- `.kiro/specs/fq-compressor/requirements.md`
  - 明确随机访问语义：read range 以归档的存储顺序为准（可能已重排）。
  - 移除 MVP 中的 checkpoint/resumable compression 需求。
  - I/O 压缩格式分阶段：Phase 1（plain/gzip），Phase 2（bzip2/xz）。
  - 数据完整性：全局校验 + 块级校验（xxhash64），并在解压时验证。
- `.kiro/specs/fq-compressor/design.md`
  - 将“归档内文件列表”替换为正式 `.fqc` 容器布局：Header + Metadata + Block 区 + Index/Trailer + checksums。
  - 并行模型以 oneTBB 为核心（pipeline + task-based 混合），不强制单一流水线形态。
  - 记录 CLI 解析库与日志库的选型（CLI11；Quill）。
  - 记录 pigz 风格并行 gzip 的可行集成点（作为 IO 层加速策略）。

## 影响

- 文档层面：更清晰的随机访问与完整性语义，后续实现可据此落地 `.fqc` reader/writer 与索引生成。
- 行为预期变化：开启重排时，随机访问按重排后的顺序定位；若需要“原始顺序随机访问”则必须额外保存映射（非 MVP）。

## 回退方案

- 回滚上述两份文档到变更前版本即可。
