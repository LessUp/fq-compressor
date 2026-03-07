# fq-compressor 文档总览

本目录是 **fq-compressor 的正式文档主目录**，默认以中文为主；若需要对外展示的双语说明，请优先查看 `docs/gitbook/` 生成的 GitBook / Honkit 站点。

## 快速入口

- [项目中文 README](../README.zh-CN.md)
- [项目英文 README](../README.md)
- [GitBook 双语文档](https://lessup.github.io/fq-compressor/)
- [Benchmark 框架说明](benchmark/BENCHMARK.md)
- [最新 Benchmark 报告](benchmark/results/report-latest.md)
- [规格文档入口](specs/README.md)

## 文档结构

### 1. 设计与规格 (`specs/`, `design/`)

- `specs/requirements.md`
- `specs/design.md`
- `specs/tasks.md`
- `specs/reference-projects.md`
- `design/reference-projects.md`
- `design/01-feasibility-analysis.md`
- `design/02-strategy-evaluation.md`
- `design/03-algorithm-selection.md`

### 2. Benchmark 与性能 (`benchmark/`)

- `benchmark/BENCHMARK.md`
- `benchmark/benchmark-summary.md`
- `benchmark/README.md`
- `benchmark/results/`

### 3. 开发与工程化 (`dev/`)

- `dev/project-setup.md`
- `dev/coding-standards.md`
- `dev/spring-source-analysis.md`
- `dev/project-summary.md`
- `dev/04-implementation-status.md`
- `dev/05-boundary-conditions.md`
- `dev/phase3-summary.md`
- `dev/phase4-tbb-parallel-implementation.md`
- `dev/phase4-usage-guide.md`
- `dev/test-results.md`
- `dev/ISSUES.md`
- `dev/issues-detailed.md`
- `dev/comprehensive-diagnosis.md`
- `dev/repair-progress.md`

### 4. 评审与历史记录 (`review/`)

- `review/`

## 目录治理结论

- **`tests/`**
  - 正式测试源码目录，必须保留。

- **`Testing/`**
  - CTest 的运行产物目录，通常不应作为仓库内容维护。

- **`ref-projects/`**
  - 适合作为本地调研缓存，不适合继续作为正式仓库目录。
  - 建议保留文档分析结果，逐步移除目录依赖。

- **`.spec-workflow/`**
  - 更偏流程模板目录；若工作流已稳定迁移，可从主仓库结构中退出。

- **`.kiro/specs/fq-compressor/`**
  - 已适合并入 `docs/specs/`，作为正式规格文档归档。

## 推荐维护原则

- **正式说明优先写入 `docs/`**
- **对外展示优先同步到 GitBook**
- **阶段性草稿、review 记录与最终说明分层存放**
- **Benchmark 结论与原始结果分开存放**

## 后续建议

如果你准备继续清理仓库，可以按以下顺序进行：

1. 确认 `docs/specs/` 内容满足使用需要。
2. 删除 `.kiro/specs/fq-compressor/` 与 `.spec-workflow/` 原目录。
3. 确认 `ref-projects/` 不再作为本地依赖。
4. 删除仓库内遗留的 `Testing/` 目录内容。
