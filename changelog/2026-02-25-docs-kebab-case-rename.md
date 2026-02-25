# docs 目录重构：统一 kebab-case 命名

**日期**: 2026-02-25
**类型**: refactor
**范围**: docs, benchmark, scripts

## 概述

将 `docs/` 目录下所有文件和目录统一为小写字母 + 中划线（kebab-case）命名规范，并同步更新全部交叉引用。

## 文件重命名（32 个）

### docs/ 根目录（16 个）
- `01_feasibility_analysis.md` → `01-feasibility-analysis.md`
- `02_strategy_evaluation.md` → `02-strategy-evaluation.md`
- `03_algorithm_selection.md` → `03-algorithm-selection.md`
- `04_implementation_status.md` → `04-implementation-status.md`
- `05_boundary_conditions.md` → `05-boundary-conditions.md`
- `BENCHMARK.md` → `benchmark.md`
- `BENCHMARK_SUMMARY.md` → `benchmark-summary.md`
- `COMPREHENSIVE_DIAGNOSIS.md` → `comprehensive-diagnosis.md`
- `ISSUES.md` → `issues.md`
- `ISSUES_DETAILED.md` → `issues-detailed.md`
- `PHASE3_SUMMARY.md` → `phase3-summary.md`
- `PROJECT_SUMMARY.md` → `project-summary.md`
- `REPAIR_PROGRESS.md` → `repair-progress.md`
- `TEST_RESULTS.md` → `test-results.md`
- `phase4_tbb_parallel_implementation.md` → `phase4-tbb-parallel-implementation.md`
- `phase4_usage_guide.md` → `phase4-usage-guide.md`

### docs/benchmark/（7 个）
- `benchmark_report.html` → `benchmark-report.html`
- `benchmark_report.md` → `benchmark-report.md`
- `benchmark_results.json` → `benchmark-results.json`
- `compression_ratio.png` → `compression-ratio.png`
- `compression_speed.png` → `compression-speed.png`
- `decompression_speed.png` → `decompression-speed.png`
- `memory_usage.png` → `memory-usage.png`

### docs/benchmark/results/（9 个）
- 所有 `_` 分隔的文件名改为 `-` 分隔

### 目录重命名
- `docs/reivew/` → `docs/review/`（同时修正拼写错误）

## 交叉引用更新（约 15 个文件）

- `CLAUDE.md` — docs 路径引用
- `README.md` — docs 路径和报告链接
- `.claude/index.json` — docs 路径
- `.windsurf/rules.md` — 命名规范描述
- `scripts/benchmark.sh` — 输出文件名模式
- `benchmark/visualize_benchmark.py` — 图表键名和输出文件名
- `benchmark/compiler_benchmark.py` — 默认输出文件名
- `docs/04-implementation-status.md` — 内部引用
- `docs/phase4-usage-guide.md` — 内部引用
- `docs/phase3-summary.md` — 内部引用
- `docs/project-summary.md` — 内部引用
- `docs/benchmark-summary.md` — 文件名引用
- `docs/benchmark.md` — 文件名引用
- `docs/benchmark/results/*-latest.*` — 符号引用更新

## 未变更

- `docs/dev/` 下 3 个文件（已是 kebab-case）
- `docs/review/` 下 4 个文件（已是 kebab-case）
- `docs/benchmark/README.md`（平台约定保留大写）
