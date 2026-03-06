# fq-compressor 规格文档

本目录用于承接原先分散在 `.kiro/specs/fq-compressor/` 与相关流程目录中的正式规格内容，作为 `docs/` 下统一管理的设计与需求文档入口。

## 文档列表

- [requirements.md](requirements.md)
  - 项目需求、非功能指标、CLI 设计、兼容性与边界条件。

- [design.md](design.md)
  - 总体架构、`.fqc` 文件格式、并行流水线与长读策略。

- [tasks.md](tasks.md)
  - 分阶段实施计划、阶段目标与完成情况。

- [reference-projects.md](reference-projects.md)
  - 参考项目及其对本项目的借鉴点。

## 迁移说明

本目录的目标不是简单堆叠历史文件，而是把：

- 需求
- 设计
- 实施计划
- 参考项目分析

统一收敛到 `docs/` 体系中，便于：

- GitBook 对外展示
- GitHub Pages 发布
- 后续继续维护与增量更新

## 清理建议

当你确认此目录已满足使用需求后，可以考虑删除以下原始目录：

- `.kiro/specs/fq-compressor/`
- `.spec-workflow/`

如果仍需要保留历史痕迹，也建议将其降级为归档目录，而不是作为主工作目录继续暴露在仓库根目录。
