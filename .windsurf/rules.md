---
description: fq-compressor C++ 项目规则
---

# fq-compressor: C++ 项目规则

> C++23 实现的高性能 FASTQ 压缩器。CMake + Conan 2.x，TBB 并行流水线。
> **当前进度约 60%**：核心算法完成，集成层缺失（compress/decompress 命令为占位符）。

## 验证命令

```bash
./scripts/build.sh gcc-release              # 构建
./scripts/test.sh gcc-release               # 测试 (34 单元测试)
./scripts/lint.sh format-check              # 格式检查
./scripts/lint.sh lint clang-debug          # clang-tidy 静态分析
./scripts/lint.sh all                       # 全部检查
```

Sanitizer 构建: `clang-asan` / `clang-tsan` / `coverage`

## 项目布局

```
include/fqc/          # 公共 API 头文件
src/
├── algo/             # 压缩算法 (block_compressor, global_analyzer, quality_compressor, id_compressor, pe_optimizer)
├── io/               # I/O 层 (fastq_parser, compressed_stream, paired_end_parser, async_io)
├── format/           # FQC 格式 (fqc_reader, fqc_writer, reorder_map)
├── pipeline/         # TBB 并行管道
├── common/           # 基础设施 (error, logger, memory_budget)
└── commands/         # CLI 命令 (compress, decompress, info, verify)
tests/                # Google Test + RapidCheck
benchmark/            # 性能基准测试
scripts/              # 构建/测试/lint 脚本
```

## 构建系统

- **CMake + Ninja**，preset 定义在 `CMakePresets.json`
- 构建目录: `build/<preset>` (如 `build/gcc-release`, `build/clang-debug`, `build/clang-asan`)
- **Conan 2.x**: `scripts/build.sh` 自动安装依赖
- 二进制输出: `build/<preset>/src/fqc`

## 编码规范

- **C++23**: 使用 Ranges、Concepts 等现代特性
- **代码风格**: `.clang-format` (100 列宽, 4 空格缩进)
- **静态分析**: `.clang-tidy`
- **命名**: 文件/目录 `snake_case`; 类 `PascalCase`; 函数/变量 `camelCase`; 文档 `kebab-case`
- **Include 顺序**: 项目 (`fqc/`) → 标准库 → 第三方
- **错误处理**: `Result<T>` 模式，不用异常
- **日志**: quill 库，不用 `std::cout` 做状态输出

## 依赖 (Conan)

| 依赖 | 版本 | 用途 |
|------|------|------|
| CLI11 | 2.4.2 | CLI 解析 |
| quill | 11.0.2 | 高性能日志 |
| oneTBB | 2022.3.0 | 并行流水线 |
| zlib-ng | 2.3.2 | 压缩 |
| zstd | 1.5.7 | 压缩 |
| libdeflate | 1.25 | 压缩 |

## 当前开发重点

**核心问题**: 有优秀的核心引擎(4735行算法代码)，但缺少集成胶水代码。

**待完成**:
- compress 命令核心逻辑 (目前只写 8 字节魔数)
- decompress 命令核心逻辑 (目前只写注释)
- FQCWriter/Reader 实际集成
- TBB 并行流水线节点逻辑

详见: `docs/issues.md`, `docs/04-implementation-status.md`
