# fq-compressor 文档总览

## 快速入口

- [项目中文 README](../README.zh-CN.md)
- [项目英文 README](../README.md)
- [GitBook 双语文档](https://lessup.github.io/fq-compressor/)

## 文档结构

```
docs/
├── specs/          # 权威规格文档（需求、设计、任务、参考项目）
├── research/       # 早期研究与可行性评估报告
├── dev/            # 开发指南（环境搭建、编码规范、并行流水线）
├── benchmark/      # 性能基准框架与测试结果
├── gitbook/        # GitBook 双语用户文档（中文/英文）
└── archive/        # 历史开发记录归档
```

### 1. 规格文档 (`specs/`)

项目的**权威设计与需求文档**。

| 文件 | 内容 |
|------|------|
| [requirements.md](specs/requirements.md) | 完整需求定义、CLI 设计、性能目标、边界条件 |
| [design.md](specs/design.md) | 总体架构、`.fqc` 文件格式、两阶段压缩策略、长读支持 |
| [tasks.md](specs/tasks.md) | 分阶段实施计划与完成情况 |
| [references.md](specs/references.md) | 参考项目（Spring/fqzcomp5/pigz 等）及其借鉴点 |

### 2. 研究报告 (`research/`)

项目早期的技术可行性分析与策略选型评估。

| 文件 | 内容 |
|------|------|
| [01-feasibility-analysis.md](research/01-feasibility-analysis.md) | 项目可行性与架构分析 |
| [02-strategy-evaluation.md](research/02-strategy-evaluation.md) | 参考/无参考、有损/无损策略评估 |
| [03-algorithm-selection.md](research/03-algorithm-selection.md) | 算法选型与随机访问设计 |

### 3. 开发指南 (`dev/`)

面向开发者的实用工程文档。

| 文件 | 内容 |
|------|------|
| [project-setup.md](dev/project-setup.md) | 环境搭建、CMake 配置、Conan 依赖、静态分析 |
| [coding-standards.md](dev/coding-standards.md) | 编码规范与命名约定 |
| [tbb-parallel-pipeline.md](dev/tbb-parallel-pipeline.md) | TBB 三阶段并行流水线实现详解 |

### 4. 性能基准 (`benchmark/`)

| 文件 | 内容 |
|------|------|
| [BENCHMARK.md](benchmark/BENCHMARK.md) | Benchmark 框架说明 |
| [results/report-latest.md](benchmark/results/report-latest.md) | 最新 Benchmark 报告 |
| [results/](benchmark/results/) | 历史结果数据（JSON/HTML/Markdown） |

### 5. GitBook 用户文档 (`gitbook/`)

面向用户的双语文档，可通过 GitBook / Honkit 生成静态站点。

- **中文**: `gitbook/zh/`
- **English**: `gitbook/en/`

### 6. 历史归档 (`archive/`)

开发过程中的阶段总结、问题追踪和代码评审记录，仅供参考。

- `archive/reviews/` — 代码评审记录
- `archive/implementation-status.md` — 实现状态跟踪
- `archive/phase3-summary.md` / `phase4-usage-guide.md` — 阶段总结
- `archive/issues.md` / `issues-detailed.md` — 问题追踪
- `archive/spring-source-analysis.md` — Spring 源码分析

## 维护原则

- **正式说明优先写入 `specs/`**
- **对外展示同步到 `gitbook/`**
- **开发者指南维护在 `dev/`**
- **阶段性草稿与历史记录归入 `archive/`**
