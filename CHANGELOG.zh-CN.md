# 变更日志

本变更日志保持**保守、可核验、以发布为中心**。在 closeout 清理过程中，低置信度的叙事性条目已被移除，只保留较容易核实的记录。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，项目遵循
[语义化版本](https://semver.org/lang/zh-CN/)。

## [未发布]

### 变更

- 恢复了默认仓库健康基线，使 `format-check` 与 `clang-debug` 单元测试再次通过。
- 增加了面向 closeout 的仓库工作流资产：`CLAUDE.md`、Copilot 指令、preflight/worktree
  脚本以及 clangd 项目配置。
- 重构了 GitHub Pages 入口面，并使仓库 About 元数据与项目落地叙事更接近一致。
- 引入了面向 baseline health、治理、文档/Pages、AI 协作、工程工具链与最终 archive
  readiness 的 OpenSpec closeout changes。

## [0.1.0] - 2026-04-16

### 新增

- `fq-compressor` 的首个公开 C++ 实现
- 核心 `compress`、`decompress`、`info`、`verify` 命令集
- FASTQ 压缩/解压流水线、FQC 归档格式与随机访问基础能力
- 基于 Google Test 的单元测试覆盖、GitHub Actions 与双语项目文档

## 说明

- 历史预发布阶段的叙事性细节已被移除，因为它们不足以作为权威 changelog 保留。
- 后续条目应保持简短、以发布为中心，并且有证据支撑。

[未发布]: https://github.com/LessUp/fq-compressor/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/LessUp/fq-compressor/releases/tag/v0.1.0
