# 2026-03-22 — Documentation and licensing alignment

## Changed
- 修复 `README.md` 与 `README.zh-CN.md` 中的 Benchmark 链接，统一指向 `docs/benchmark/README.md`
- 修复英文 README 的文档章节标题乱码，并将失效的参考文档链接调整为现有 `docs/research/` 目录
- 更新中文 README 的文档分层说明，使其与当前仓库目录结构一致
- 明确项目许可证口径：项目自研代码采用 MIT，`vendor/spring-core/` 继续保留其原始 Spring 非商业研究用途许可证
- 同步更新 `CLAUDE.md` 中的许可证说明，避免与仓库现状不一致
- 为仓库新增根级 `LICENSE` 文件，并在 `conanfile.py` 中补充许可证字段注释，说明 vendored 第三方代码不在 MIT 授权范围内
