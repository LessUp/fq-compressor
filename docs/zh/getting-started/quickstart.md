---
title: 快速入门
description: 5 分钟内开始使用 fq-compressor - 从第一次压缩到高级用法
version: 0.2.0
last_updated: 2026-04-22
language: zh
---

# 快速入门

> 🚀 在 5 分钟内压缩您的第一个 FASTQ 文件

## 前置条件

- [已安装 fq-compressor](./installation.md)
- 一个 FASTQ 文件用于压缩（或使用下面的示例）

## 第 1 步：创建测试文件

如果手头没有 FASTQ 文件，可以创建一个小型测试文件：

```bash
# 创建一个最简 FASTQ 文件
cat > sample.fastq << 'EOF'
@read_1
ACGTACGTACGTACGTACGTACGTACGTACGTACGT
+
IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
@read_2
TGCATGCATGCATGCATGCATGCATGCATGCATGCA
+
IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
@read_3
AAAATTTTCCCCGGGGAAAATTTTCCCCGGGGAAAA
+
IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
EOF
```

## 第 2 步：压缩

将 FASTQ 文件压缩为 `.fqc` 格式：

```bash
# 基本压缩
fqc compress -i sample.fastq -o sample.fqc

# 带进度输出
fqc compress -i sample.fastq -o sample.fqc -v

# 输出：
# [2026-04-16 10:30:15] INFO: 开始压缩...
# [2026-04-16 10:30:15] INFO: 压缩完成: 108 字节 -> 67 字节 (1.61x)
```

## 第 3 步：验证

验证压缩文件的完整性：

```bash
fqc verify sample.fqc

# 输出：
# [2026-04-16 10:30:20] INFO: 验证成功
# 归档: sample.fqc
#   总块数: 1
#   总 reads: 3
#   校验和: 通过
```

## 第 4 步：查看信息

获取归档的详细信息：

```bash
fqc info sample.fqc

# 输出：
# 归档: sample.fqc
#   版本: 1
#   块数: 1
#   总 reads: 3
#   总碱基: 108
#   压缩后大小: 67 字节
#   原始大小: 108 字节
#   压缩比: 1.61x
```

JSON 输出格式（适用于脚本）：

```bash
fqc info sample.fqc --json
```

## 第 5 步：解压

将原始 FASTQ 文件还原：

```bash
# 完整解压
fqc decompress -i sample.fqc -o restored.fastq

# 与原始文件对比
diff sample.fastq restored.fastq
# （无输出表示文件相同）
```

## 高级快速示例

### 双端测序压缩

```bash
# 分离文件
fqc compress -i reads_R1.fastq -i2 reads_R2.fastq -o paired.fqc --paired

# 交错文件
fqc compress -i interleaved.fastq -o paired.fqc --paired
```

### 并行压缩（更快）

```bash
# 使用 8 个线程
fqc compress -i large.fastq -o large.fqc --threads 8

# 限制内存使用
fqc compress -i large.fastq -o large.fqc --threads 8 --memory-limit 4096
```

### 随机访问（部分解压）

```bash
# 仅解压第 1000-2000 条 reads
fqc decompress -i large.fqc --range 1000:2000 -o subset.fastq

# 仅解压前 1000 条 reads
fqc decompress -i large.fqc --range :1000 -o first_1000.fastq

# 从第 5000 条解压到末尾
fqc decompress -i large.fqc --range 5000: -o from_5000.fastq
```

### 压缩输入支持

```bash
# 直接从 .gz 或 .bz2 文件压缩
fqc compress -i input.fastq.gz -o output.fqc
fqc compress -i input.fastq.bz2 -o output.fqc
fqc compress -i input.fastq.xz -o output.fqc
```

## 工作流程总结

```bash
# 典型工作流程
fqc compress -i input.fastq -o output.fqc          # 1. 压缩
fqc verify output.fqc                                # 2. 验证
fqc info output.fqc                                  # 3. 检查
fqc decompress -i output.fqc -o restored.fastq      # 4. 解压
```

## 常用选项参考

| 选项 | 描述 | 示例 |
|------|------|------|
| `-i, --input` | 输入 FASTQ 文件 | `-i reads.fastq` |
| `-o, --output` | 输出文件 | `-o reads.fqc` |
| `-i2` | 双端测序第二个输入 | `-i2 reads_2.fastq` |
| `--paired` | 启用双端模式 | `--paired` |
| `-t, --threads` | 线程数 | `-t 8` |
| `--memory-limit` | 内存限制（MB） | `--memory-limit 4096` |
| `--range` | 部分解压范围 | `--range 1000:2000` |
| `-v, --verbose` | 详细输出 | `-v` |
| `-h, --help` | 显示帮助 | `-h` |

## 性能提示

1. **对大型文件使用多线程**: `--threads 8`
2. **必要时设置内存限制**: `--memory-limit 4096`
3. **直接从 .gz 压缩** 以避免中间文件
4. **使用随机访问** 而非完整解压，当您只需要子集时

## 下一步

- [CLI 使用指南](./cli-usage.md) - 完整命令参考
- [核心算法](../core-concepts/algorithms.md) - 了解压缩原理
- [性能基准测试](../performance/benchmark.md) - 查看详细性能指标

---

**🌐 语言**: [English](../../en/getting-started/quickstart.md) | [简体中文（当前）](./quickstart.md)
