# 2026-03-22 — Explicit unsupported-feature handling

## Changed
- `decompress` 命令在参数校验阶段对 `--original-order` 显式报错，避免当前版本静默暴露未完成的恢复逻辑
- 更新 `main.cpp` 中 `--original-order` 的 CLI 帮助文案，明确该选项保留给未来的 reorder-map 恢复能力
- 清理 `include/fqc/commands/compress_command.h` 中与当前实现不符的 placeholder 注释
- 在 `README.md`、`README.zh-CN.md`、GitBook Quick Start 与 FAQ 中补充输入格式支持说明，明确当前支持 plain/gzip/bzip2/xz，`.zst` FASTQ 输入暂未支持
