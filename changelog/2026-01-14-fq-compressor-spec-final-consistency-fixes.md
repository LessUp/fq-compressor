# fq-compressor 最终校对：规格一致性修复（2026-01-14）

## 背景

- 在完成 ABC（Spring/Mincom）+ SCM（fqzcomp5 风格）路线确认后，需要对 `requirements.md` / `design.md` / `tasks.md` 做最后一次一致性校对。
- 目标：确保需求编号可追踪、设计描述与任务计划一致、并消除会误导实现的过时/矛盾表述。

## 变更

- `.kiro/specs/fq-compressor/design.md`
  - 修正架构图与模块命名：补齐 `VerifyCommand/VerifyEngine`，并将序列/质量/ID 组件显式区分为 `SpringABC/SCM/ID`。
  - 修复格式说明的自相矛盾：
    - Magic Header 长度统一为 9 bytes（Magic 8 + Version 1）。
    - 删除 `CompressionAlgo` 的重复条目。
  - 对齐依赖与示例：将压缩库表述统一为 `libdeflate/libbz2/liblzma`；Identifier Stream 示例通用压缩器统一为 `LZMA`。

- `.kiro/specs/fq-compressor/requirements.md`
  - 调整 `Requirement 2.3`：由“按特定 ID 解压”改为与列式子流设计一致的“Header-only/子流选择性解压”。
  - 补齐 `Requirement 6.2` 的解压相关参数说明：加入 `--range`、`--header-only`。
  - 扩展 `Requirement 1.1.1`：由“输入支持”升级为 “I/O 支持”，并补充可选的 `.fqc.gz` 外层 gzip 输出能力（Phase 4: Optional）。

- `.kiro/specs/fq-compressor/tasks.md`
  - 对齐 `Requirement 2.3`：在 `FQCReader`/`DecompressionEngine` 任务中补充 Header-only 与子流选择性解码的实现点。
  - 对齐 CLI：在 `DecompressCommand` 解析选项中加入 `--header-only`。
  - 修复需求追踪：将端到端测试的 `_Requirements: 1.1, 2, 3_` 改为可追踪的 `2.1/2.2/2.3` 与 `3.1~3.4`。

## 影响

- 需求-设计-任务三份文档的追踪链路更闭环：`requirements` 的编号体系能在 `tasks` 中一一引用。
- 解压侧能力定义更贴近可实现的列式子流架构：优先支持 `--range`、`--header-only`、以及“只解码特定子流”。

## 回退方案

- 回滚本次变更涉及的三份 spec 文件与本 changelog 文件即可。
