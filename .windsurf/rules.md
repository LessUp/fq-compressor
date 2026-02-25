---
description: fq-compressor project rules for Cascade/Windsurf
---

# fq-compressor: 项目规则（Windsurf/Cascade）

## 项目概述
fq-compressor 是一个高性能 FASTQ 压缩器，结合：
- **核心算法**: Spring (ABC 策略 - Assembly-based Compression)
- **质量压缩**: fqzcomp5 风格的 SCM (Statistical Context Mixing)
- **并行框架**: Intel oneTBB + Producer-Consumer Pipeline
- **格式特性**: 块级随机访问支持

## 权威入口（优先使用）
- **构建**: `./scripts/core/build`
- **测试**: `./scripts/core/test`
- **代码质量**: `./scripts/core/lint`
- **依赖安装**: `./scripts/core/install-deps`
- **Benchmark 系统**: `./scripts/benchmark`

## 禁止/避免
- **不要使用** `scripts/deprecated/` 目录下脚本。
- 仓库历史上存在旧脚本，若与 `scripts/core/*` 冲突，以 `scripts/core/*` 为准。

## 构建系统约定
- **CMake + Ninja**（`CMakePresets.json` 也可参考，但常用入口仍是 `scripts/core/build`）。
- 默认构建目录命名：`build-<compiler>-<type>`，例如：
  - `build-clang-debug`
  - `build-clang-release`
  - `build-clang-coverage`
- Conan：`scripts/core/build` 会在可用时自动 `conan install` 生成 toolchain。

## 代码风格（权威文档）
- 以 `docs/dev/coding-standards.md` 为准。
- C++20；clang-format/clang-tidy 配置来自 `.clang-format` / `.clang-tidy`。
- 源码文件/目录：`snake_case`；文档文件：`kebab-case`（小写+中划线）；类：`PascalCase`；函数/变量：`camelCase`。
- include 顺序：标准库 -> 第三方 -> 本项目。

## 目录约定
- 公共 API：`include/fqcomp/`
- 实现：`src/`
- CLI：`src/cli/`
- 测试：`tests/`
- Benchmark：`tools/benchmark/` + `benchmark_results/`

## 核心模块
- `src/abc/` - Assembly-based Compression (序列压缩)
- `src/scm/` - Statistical Context Mixing (质量压缩)
- `src/io/` - 块级 IO 和随机访问
- `src/pipeline/` - TBB 并行管道

## 常用质量检查命令（建议优先级）
- `./scripts/core/lint format-check`
- `./scripts/core/lint tidy -b build-clang-release`
- `./scripts/core/lint cppcheck`（可选）
- `./scripts/core/lint iwyu -b build-clang-release`（可选，建议人工审阅输出）

## 分析/诊断工具
- Valgrind memcheck: `./scripts/tools/valgrind-memcheck`
- Valgrind cachegrind: `./scripts/tools/valgrind-cachegrind`
- Sanitizers: `./scripts/core/build --sanitizer asan|tsan|ubsan`（建议 Debug，且常搭配 `--no-lto`）
