# fq-compressor 文档

fq-compressor 是一个面向 FASTQ 数据的高性能压缩项目，强调以下能力：

- 面向测序数据特征的专项压缩
- 基于 block 的随机访问
- oneTBB 并行流水线
- 可维护的现代 C++ 工程体系
- 面向短读与长读的差异化策略

## 阅读路径

- [设计概览](design.md)
- [使用说明](usage.md)
- [Benchmark 说明](benchmark.md)

## 仓库文档入口

- [中文 README](../../../README.zh-CN.md)
- [Docs 总览](../../README.md)
- [规格文档](../../specs/README.md)
- [Benchmark 原始文档](../../BENCHMARK.md)

## 目录治理结论

- `docs/` 是正式文档主目录
- `tests/` 是正式测试源码目录
- `Testing/` 是 CTest 产物目录
- `ref-projects/` 建议只保留分析结论，不再保留为正式目录
- `.kiro/specs/fq-compressor` 与 `.spec-workflow` 建议并入 `docs/specs/`
