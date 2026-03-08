# fq-compressor 文档总览

## 快速入口

- [项目中文 README](../README.zh-CN.md)
- [项目英文 README](../README.md)
- [GitBook 双语文档](https://lessup.github.io/fq-compressor/)

## 文档结构

```
docs/
├── specs/          # 权威规格文档（需求、设计、任务、边界条件、参考项目）
├── research/       # 技术研究与参考项目分析
├── dev/            # 开发指南（环境搭建、编码规范、并行流水线）
├── benchmark/      # 性能基准框架（生成的报告和图表已 gitignore）
├── gitbook/        # GitBook 双语用户文档（中文/英文）
└── archive/        # 历史开发记录归档
```

### 1. 规格文档 (`specs/`)

项目的**权威设计与需求文档**。

| 文件 | 内容 |
|------|------|
| [requirements.md](specs/requirements.md) | 完整需求定义、CLI 设计、性能目标 |
| [design.md](specs/design.md) | 总体架构、`.fqc` 文件格式、两阶段压缩策略、长读支持 |
| [tasks.md](specs/tasks.md) | 分阶段实施计划与完成情况 |
| [references.md](specs/references.md) | 参考项目（Spring/fqzcomp5/pigz 等）及其借鉴点 |
| [boundary-conditions.md](specs/boundary-conditions.md) | 边界条件处理规范（空文件、单条Read、超长Read 等） |

### 2. 研究报告 (`research/`)

技术可行性分析、策略选型评估与参考项目分析。

| 文件 | 内容 |
|------|------|
| [01-feasibility-analysis.md](research/01-feasibility-analysis.md) | 项目可行性与架构分析 |
| [02-strategy-evaluation.md](research/02-strategy-evaluation.md) | 参考/无参考、有损/无损策略评估 |
| [03-algorithm-selection.md](research/03-algorithm-selection.md) | 算法选型与随机访问设计 |
| [04-spring-source-analysis.md](research/04-spring-source-analysis.md) | Spring 源码结构与核心算法分析 |

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
| [README.md](benchmark/README.md) | Benchmark 框架说明、工具使用、指标解读 |

> 运行 benchmark 脚本后会在此目录生成 PNG 图表、HTML/MD 报告和 JSON 数据，
> 这些生成文件已在 `.gitignore` 中排除。

### 5. GitBook 用户文档 (`gitbook/`)

面向用户的双语文档，可通过 GitBook / Honkit 生成静态站点。

- **中文**: `gitbook/zh/`
- **English**: `gitbook/en/`

### 6. 历史归档 (`archive/`)

开发过程中的阶段总结、诊断报告和代码评审记录，仅供参考。

| 文件 | 内容 |
|------|------|
| `reviews/` | 代码评审记录（2026-01） |
| `comprehensive-diagnosis.md` | 项目综合诊断（占位符分析、差异对比、修复路线图） |
| `repair-progress.md` | 修复进度跟踪（P0/P1/P2 任务完成记录） |
| `project-summary.md` | 项目完成总结（功能、性能、代码统计） |
| `phase3-summary.md` | Phase 3 阶段总结 |
| `phase4-usage-guide.md` | Phase 4 使用指南 |
| `test-results.md` | 测试结果记录 |

## 维护原则

- **正式说明优先写入 `specs/`**
- **对外展示同步到 `gitbook/`**
- **开发者指南维护在 `dev/`**
- **参考项目分析归入 `research/`**
- **阶段性草稿与历史记录归入 `archive/`**
