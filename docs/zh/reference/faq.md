---
title: 常见问题解答
description: 关于 fq-compressor 的常见问题和答案
version: 0.1.0
last_updated: 2026-04-16
language: zh
---

# 常见问题解答

## 一般问题

### 什么是 fq-compressor？

fq-compressor 是一款针对基因测序数据的高性能 FASTQ 压缩工具。它使用领域特定算法（ABC 用于序列，SCM 用于质量分数），相比 gzip 等通用压缩器可实现 2-3 倍的压缩比提升。

### 它与其他 FASTQ 压缩器相比如何？

| 工具 | 压缩比 | 速度 | 随机访问 | 并行 |
|------|-------|-------|---------------|----------|
| fq-compressor | 3.5-4x | 中等 | ✓ | ✓ |
| Spring | 4-5x | 慢 | ✗ | ✓ |
| fqzcomp5 | 3-4x | 中等 | ✗ | ✗ |
| gzip | 2-3x | 快 | ✗ | ✗ |

### 它是无损的吗？

是的，默认情况下 fq-compressor 是完全无损的。可选的有损质量压缩模式可用。

## 安装问题

### 支持哪些平台？

- Linux x86_64（glibc 2.31+，musl）
- Linux ARM64/AArch64
- macOS x86_64（Intel）
- macOS ARM64（Apple Silicon）

### 我需要从源码构建吗？

不需要。所有支持平台都有预构建的二进制文件。仅在以下情况需要从源码构建：
- 最新开发版本
- 自定义优化
- 需要调试符号

### 运行时依赖是什么？

静态构建（musl）**无运行时依赖**。动态构建（glibc）需要：
- glibc 2.31+（Linux）
- 标准 C++ 库

## 使用问题

### 我可以压缩已压缩的文件吗？

可以，fq-compressor 可以直接读取 .gz、.bz2 和 .xz 文件：

```bash
fqc compress -i reads.fastq.gz -o reads.fqc
```

### 如何使用双端测序数据？

支持两种模式：

```bash
# 分离文件
fqc compress -i reads_1.fastq -i2 reads_2.fastq -o paired.fqc --paired

# 交错文件
fqc compress -i interleaved.fastq -o paired.fqc --paired
```

### 应该使用什么压缩模式？

| 模式 | 使用场景 |
|------|----------|
| `fast` | 快速压缩，略低的压缩比 |
| `balanced` | 默认，良好权衡 |
| `best` | 最大压缩，较慢 |

## 性能问题

### 需要多少内存？

内存需求取决于：
- 数据集大小
- 线程数
- 块大小

经验法则：8 线程的典型数据集需要约 2-4GB。

使用 `--memory-limit` 来限制 RAM 使用。

### 我可以使用所有 CPU 核心吗？

可以，fq-compressor 随线程数扩展良好：

```bash
fqc compress -i data.fastq -o out.fqc --threads $(nproc)
```

### 为什么解压比压缩快？

压缩涉及：
- Minimizer 计算
- 重排序（TSP）
- 共识生成
- 增量编码

解压逆向这些过程，但操作更简单。

## 功能问题

### 什么是"随机访问"？

无需解压整个文件即可提取 reads 子集的能力：

```bash
# 仅解压第 1000-2000 条 reads
fqc decompress -i archive.fqc --range 1000:2000 -o subset.fastq
```

这是 O(1) - 无论归档大小如何，常数时间。

### 我可以恢复原始 reads 顺序吗？

可以，如果归档是在启用重排序的情况下创建的：

```bash
fqc decompress -i archive.fqc -o restored.fastq --original-order
```

### 格式稳定吗？

FQC 格式有用于向前兼容的版本标记。v0.1.0 创建的归档可被未来版本读取。

## 故障排除

### 安装后"找不到命令"

```bash
# 确保 fqc 在 PATH 中
echo $PATH | grep /usr/local/bin

# 或使用完整路径
/usr/local/bin/fqc --version
```

### "权限被拒绝"

```bash
chmod +x /usr/local/bin/fqc
```

### 压缩速度慢

- 确保使用发布构建（非调试）
- 使用多线程：`--threads 8`
- 对于速度关键型应用考虑 `fast` 模式

### 内存不足错误

```bash
# 限制内存使用
fqc compress -i large.fastq -o out.fqc --memory-limit 2048

# 减少线程数
fqc compress -i large.fastq -o out.fqc --threads 4
```

## 贡献

### 如何贡献？

请参阅我们的 [贡献指南](../development/contributing.md)。

### 在哪里报告错误？

[GitHub Issues](https://github.com/LessUp/fq-compressor/issues)

### 有开发路线图吗？

查看 [GitHub Discussions](https://github.com/LessUp/fq-compressor/discussions) 了解计划功能。

## 许可证

### 我可以将 fq-compressor 用于商业用途吗？

可以。项目代码是 MIT 许可证。

注意：`vendor/spring-core/` 包含第三方代码，使用单独的研究许可证。

### 专利情况如何？

fq-compressor 不实现任何专利算法。ABC 和 SCM 算法是已发表的学术研究。

---

**没有看到您的问题？** [提交 issue](https://github.com/LessUp/fq-compressor/issues/new) 或 [发起讨论](https://github.com/LessUp/fq-compressor/discussions/new)。
