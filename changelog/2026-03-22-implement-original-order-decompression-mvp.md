# 2026-03-22 — Implement original-order decompression MVP

## Changed
- 在 `decompress` 命令中实现基于 reorder map 的 `--original-order` 单线程恢复路径，并在启用该选项时绕过并行 pipeline，优先保证输出正确性
- `--range` / `--range-pairs` 与 `--original-order` 的组合语义落实为：先按 archive 顺序选择子集，再按 original order 输出该子集
- 为 `split-pe` 的 original-order 路径增加 paired archive 校验，并限制当前仅支持 interleaved paired-end 归档
- 在 `FQCReader::loadReorderMap()` 中将磁盘上的 reorder map 归一化为运行时 1-based 语义，并补充 forward/reverse 一致性校验
- 新增 `tests/commands/original_order_command_test.cpp`，覆盖 original-order 顺序恢复、线程回落和 archive 子集重排输出
- 增强 `tests/format/fqc_writer_test.cpp`，验证 reorder map 读回后的 lookup API 使用 1-based 运行时语义
