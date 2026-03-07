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
- **构建**: `./scripts/build.sh <preset>`
- **测试**: `./scripts/test.sh <preset>` 或 `./scripts/run_tests.sh -p <preset>`
- **代码质量**: `./scripts/lint.sh {format|format-check|lint|all}`
- **依赖安装**: `./scripts/install_deps.sh <preset>`

## 构建系统约定
- **CMake + Ninja**，通过 `CMakePresets.json` 定义所有 preset。
- 构建目录命名：`build/<preset>`，例如：
  - `build/clang-debug`
  - `build/gcc-release`
  - `build/clang-asan`
- Conan 2.x：`scripts/build.sh` 会在需要时自动调用 `install_deps.sh` 安装依赖。

## 代码风格（权威文档）
- 以 `docs/dev/coding-standards.md` 为准。
- C++23；clang-format/clang-tidy 配置来自 `.clang-format` / `.clang-tidy`。
- 源码文件/目录：`snake_case`；文档文件：`kebab-case`（小写+中划线）；类：`PascalCase`；函数/变量：`camelCase`。
- include 顺序：项目头文件 (`fqc/`) -> 标准库 -> 第三方。

## 目录约定
- 公共 API 头文件：`include/fqc/`
- 实现源码：`src/`
- CLI 命令头文件：`include/fqc/commands/`
- CLI 命令实现：`src/commands/`
- 测试：`tests/`
- Benchmark：`benchmark/`

## 核心模块
- `src/algo/` - 压缩算法（block_compressor, global_analyzer, quality_compressor, id_compressor, pe_optimizer）
- `src/io/` - I/O 层（fastq_parser, compressed_stream, paired_end_parser, async_io）
- `src/format/` - FQC 格式（fqc_reader, fqc_writer, reorder_map）
- `src/pipeline/` - TBB 并行管道
- `src/common/` - 公共基础设施（error, logger, memory_budget）
- `src/commands/` - CLI 命令实现

## 常用质量检查命令
- `./scripts/lint.sh format-check` - 格式检查
- `./scripts/lint.sh lint clang-debug` - clang-tidy 静态分析
- `./scripts/lint.sh format` - 自动格式化
- `./scripts/lint.sh all` - 全部检查

## Sanitizer 构建
- ASan: `./scripts/build.sh clang-asan`
- TSan: `./scripts/build.sh clang-tsan`
- Coverage: `./scripts/build.sh coverage`
