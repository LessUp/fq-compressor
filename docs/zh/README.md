---
title: fq-compressor 文档
description: fq-compressor 官方文档——高性能 FASTQ 压缩工具
version: 0.2.0
last_updated: 2026-04-22
language: zh
---

# fq-compressor 文档

> 🧬 面向测序时代的高性能 FASTQ 压缩工具

欢迎使用 **fq-compressor** 官方文档。fq-compressor 是一款新一代 FASTQ 压缩工具，将最先进的压缩算法与工业级工程实践相结合。

## 📚 文档结构

| 章节 | 描述 |
|------|------|
| [入门指南](./getting-started/) | 安装、快速入门和 CLI 使用 |
| [核心概念](./core-concepts/) | 架构、算法和 FQC 格式 |
| [开发指南](./development/) | 贡献指南、编码规范和构建指南 |
| [性能优化](./performance/) | 基准测试和性能优化 |
| [参考文档](./reference/) | 变更日志、常见问题解答和 API 参考 |

## 🚀 快速链接

- **[安装指南](./getting-started/installation.md)** - 数分钟内完成安装
- **[快速入门](./getting-started/quickstart.md)** - 您的第一次压缩
- **[CLI 参考](./getting-started/cli-usage.md)** - 完整命令参考

## 🌐 语言

- [English](../en/README.md)
- [简体中文（当前）](./README.md)

## 📖 快速开始

```bash
# 快速安装（Linux x86_64）
wget https://github.com/LessUp/fq-compressor/releases/download/v0.2.0/fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.2.0-linux-x86_64-musl/fqc /usr/local/bin/

# 压缩您的第一个 FASTQ 文件
fqc compress -i input.fastq -o output.fqc

# 验证并解压
fqc verify output.fqc
fqc decompress -i output.fqc -o restored.fastq
```

## 🏆 核心特性

| 特性 | 描述 | 优势 |
|------|------|------|
| **ABC 算法** | 面向序列的基于组装的压缩 | 接近熵极限的压缩比 |
| **SCM 质量分数** | 针对质量分数的统计上下文混合 | 最优质量分数压缩 |
| **并行处理** | 基于 Intel oneTBB 的流水线 | 最大化多核利用率 |
| **随机访问** | 基于块的数据格式 | O(1) 部分解压 |
| **现代 C++23** | 最新 C++ 特性 | 高性能、可维护代码 |

## 📊 性能亮点

在 2.27M Illumina reads（511 MB 未压缩）上的对比：

| 指标 | GCC | Clang |
|------|-----|-------|
| 压缩速度 | 11.30 MB/s | 11.90 MB/s |
| 解压速度 | 60.10 MB/s | 62.30 MB/s |
| 压缩比 | 3.97x | 3.97x |

详见 [基准测试指南](./performance/benchmark.md)。

## 🔧 技术规格

- **编程语言**: C++23
- **最低编译器版本**: GCC 14+，Clang 18+
- **构建系统**: CMake 3.28+，Conan 2.x
- **并行库**: Intel oneTBB
- **日志库**: Quill
- **CLI 库**: CLI11

## 📝 许可证

项目自研代码采用 [MIT 许可证](../../LICENSE)。

`vendor/spring-core/` 目录下的第三方代码遵循其原始许可证。

## 🤝 贡献

我们欢迎贡献！详情请参阅 [贡献指南](./development/contributing.md)。

---

**版本**: 0.2.0 | **最后更新**: 2026-04-22
