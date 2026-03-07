# fq-compressor

[English](README.md) | [简体中文](README.zh-CN.md)

**fq-compressor** 是一个面向 FASTQ 数据的高性能压缩工具，目标是在二代/三代测序场景下，同时兼顾高压缩比、可维护工程架构、并行处理能力与随机访问能力。

## 文档入口

- [中文文档总览](docs/README.md)
- [GitBook 双语文档](https://lessup.github.io/fq-compressor/)
- [Benchmark 说明](docs/BENCHMARK.md)
- [最新 Benchmark 报告](docs/benchmark/results/report-latest.md)
- [规格与设计文档](docs/specs/README.md)

## 项目定位

本项目采用混合压缩策略：

- **Sequence**: 基于 Spring / Mincom 思路的 **Assembly-based Compression (ABC)**
- **Quality**: 借鉴 fqzcomp5 的 **Statistical Context Mixing (SCM)**
- **Identifiers**: **Tokenization + Delta**
- **容器格式**: 自定义 **`.fqc`**，支持分块存储与随机访问

## 核心特性

- **高压缩比**
  - 面向 FASTQ 数据特征进行专项建模，而非只依赖通用压缩算法。

- **随机访问**
  - 采用 block-based 归档设计，可按范围快速解压。

- **并行流水线**
  - 基于 Intel oneTBB 组织 `read -> compress -> write` 流水线。

- **现代工程化**
  - C++23、Modern CMake、Conan 2.x、GitHub Actions。

- **多模式支持**
  - 支持单端、双端、保序/重排、无损/部分有损质量值处理。

## 快速开始

### 构建

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor
conan install . --build=missing -of=build/clang-release -s build_type=Release -s compiler.cppstd=20
cmake --preset clang-release
cmake --build --preset clang-release -j$(nproc)
```

### 测试

```bash
./scripts/test.sh clang-debug
```

### 基本使用

```bash
# 压缩
fqc compress -i input.fastq -o output.fqc

# 解压
fqc decompress -i output.fqc -o restored.fastq

# 查看归档信息
fqc info output.fqc

# 校验归档完整性
fqc verify output.fqc
```

## Benchmark 概览

当前仓库已包含 benchmark 框架和报告输出能力，重点关注：

- 压缩速度
- 解压速度
- 压缩率 / bits per base
- 峰值内存
- 多线程扩展性

最新公开结果可见：

- [docs/benchmark/results/report-latest.md](docs/benchmark/results/report-latest.md)
- [docs/benchmark/results/charts-latest.html](docs/benchmark/results/charts-latest.html)

## 目录治理建议

- **`docs/`**
  - 作为正式文档主目录，中文为主。

- **`docs/gitbook/`**
  - 作为 GitBook / Honkit 双语站点源码。

- **`tests/`**
  - 正式测试源码目录，应保留。

- **`Testing/`**
  - CTest 运行产物目录，不属于源码资产。

- **`ref-projects/`**
  - 更适合作为本地参考缓存，而非正式仓库结构；建议仅在文档中保留分析结果。

- **`.spec-workflow` 与 `.kiro/specs/fq-compressor`**
  - 已建议向 `docs/specs/` 收敛，后续可删除原目录。

## 文档分层

- `docs/README.md`: 中文文档入口
- `docs/specs/`: 需求、设计、实施计划
- `docs/design/`: 参考项目与架构说明
- `docs/benchmark/`: 基准测试结果
- `docs/review/`: 评审记录
- `docs/gitbook/`: GitBook 双语站点

## 发布与自动化

本仓库已规划：

- GitHub Actions 持续集成
- 推送语义化 tag 后自动创建 GitHub Release
- 主分支变更后自动构建并发布 GitBook 到 GitHub Pages

## 许可证

当前许可证仍需你最终确认。考虑到部分参考实现和集成策略，建议在正式对外发布前再次核对兼容性。
