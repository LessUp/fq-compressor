# 2026-03-22 — Low-risk cleanup and wording alignment

## Changed
- 将 `tests/placeholder_test.cpp` 重新表述为构建烟雾测试，并将 CMake 测试目标名称改为 `build_smoke_test`
- 同步更新 `.vscode/launch.json` 中的测试可执行文件名称，避免继续引用旧的 `placeholder_test`
- 更新 `include/fqc/commands/decompress_command.h` 中 `originalOrder` 字段注释，明确其当前为预留能力，实际会在校验阶段被拒绝
- 修正 `docs/specs/requirements.md` 与 `docs/specs/design.md` 中对 `--original-order` 的描述，使其与当前实现保持一致
