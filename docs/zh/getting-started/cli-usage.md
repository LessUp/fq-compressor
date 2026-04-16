---
title: CLI 使用指南
description: fq-compressor 命令行接口完整参考
version: 0.1.0
last_updated: 2026-04-16
language: zh
---

# CLI 使用参考

> fq-compressor 命令行接口完整参考

## 全局选项

这些选项适用于所有命令：

| 选项 | 简写 | 描述 | 默认值 |
|------|------|------|--------|
| `--help` | `-h` | 显示帮助信息 | - |
| `--version` | `-v` | 显示版本信息 | - |
| `--verbose` | `-v` | 启用详细输出 | false |
| `--threads` | `-t` | 使用的线程数 | 自动 |
| `--memory-limit` | - | 内存限制（MB） | 自动 |
| `--no-progress` | - | 禁用进度输出 | false |

---

## 命令

### `compress` - 压缩 FASTQ 到 FQC

将一个或多个 FASTQ 文件压缩为 `.fqc` 格式。

```bash
fqc compress [选项] -i <输入> -o <输出>
```

#### 选项

| 选项 | 简写 | 描述 | 默认值 |
|------|------|------|--------|
| `--input` | `-i` | 输入 FASTQ 文件（必需） | - |
| `--input2` | `-i2` | 双端第二个输入文件 | - |
| `--output` | `-o` | 输出 FQC 文件（必需） | - |
| `--paired` | - | 启用双端模式 | false |
| `--mode` | `-m` | 压缩模式（fast/balanced/best） | balanced |
| `--block-size` | - | 块大小（碱基数） | 10000000 |
| `--scan-all-lengths` | - | 扫描所有序列长度 | false |
| `--pipeline` | - | 使用流水线模式 | false |

#### 示例

```bash
# 基本压缩
fqc compress -i reads.fastq -o reads.fqc

# 使用 8 个线程并行压缩
fqc compress -i reads.fastq -o reads.fqc -t 8

# 双端测序（分离文件）
fqc compress -i reads_1.fastq -i2 reads_2.fastq -o paired.fqc --paired

# 双端测序（交错文件）
fqc compress -i interleaved.fastq -o paired.fqc --paired

# 最佳压缩模式（较慢）
fqc compress -i reads.fastq -o reads.fqc --mode best

# 快速压缩模式
fqc compress -i reads.fastq -o reads.fqc --mode fast

# 从压缩输入直接压缩
fqc compress -i reads.fastq.gz -o reads.fqc
```

#### 退出代码

| 代码 | 含义 |
|------|------|
| 0 | 成功 |
| 1 | 一般错误 |
| 2 | 无效参数 |
| 3 | I/O 错误 |
| 4 | 格式错误 |
| 5 | 内存错误 |

---

### `decompress` - 解压 FQC 到 FASTQ

将 `.fqc` 归档解压回 FASTQ 格式。

```bash
fqc decompress [选项] -i <输入> -o <输出>
```

#### 选项

| 选项 | 简写 | 描述 | 默认值 |
|------|------|------|--------|
| `--input` | `-i` | 输入 FQC 文件（必需） | - |
| `--output` | `-o` | 输出 FASTQ 文件（必需） | - |
| `--range` | - | 要解压的 reads 范围 | 全部 |
| `--range-pairs` | - | 双端模式范围 | 全部 |
| `--original-order` | - | 恢复原始 reads 顺序 | false |
| `--header-only` | - | 仅提取头部 | false |
| `--sequence-only` | - | 仅提取序列 | false |
| `--quality-only` | - | 仅提取质量分数 | false |

#### 示例

```bash
# 完整解压
fqc decompress -i reads.fqc -o reads.fastq

# 解压第 1000-2000 条 reads
fqc decompress -i reads.fqc --range 1000:2000 -o subset.fastq

# 解压前 1000 条 reads
fqc decompress -i reads.fqc --range :1000 -o first_1000.fastq

# 从第 5000 条解压到末尾
fqc decompress -i reads.fqc --range 5000: -o from_5000.fastq

# 恢复原始顺序（如果压缩时重排序）
fqc decompress -i reads.fqc -o reads.fastq --original-order

# 仅提取头部
fqc decompress -i reads.fqc --header-only -o headers.txt
```

#### 范围语法

| 语法 | 含义 |
|------|------|
| `1000:2000` | 第 1000 到 2000 条 reads（含） |
| `:1000` | 前 1000 条 reads（0-999） |
| `5000:` | 从第 5000 条到末尾 |
| `1000` | 仅第 1000 条 read |

---

### `info` - 显示归档信息

显示 FQC 归档的详细信息。

```bash
fqc info [选项] <归档>
```

#### 选项

| 选项 | 简写 | 描述 | 默认值 |
|------|------|------|--------|
| `--json` | - | JSON 格式输出 | false |
| `--detailed` | `-d` | 显示详细块信息 | false |

#### 示例

```bash
# 基本信息
fqc info archive.fqc

# 脚本用 JSON 输出
fqc info archive.fqc --json

# 详细块信息
fqc info archive.fqc --detailed
```

#### 示例输出

```
归档: archive.fqc
  版本: 1
  格式标志: 0x0003
  压缩模式: balanced
  
  概要:
    总块数: 12
    总 reads: 2270000
    总碱基: 227000000
    双端测序: 是
    重排序: 是
    
  大小:
    压缩后: 128.5 MB
    原始: 511.0 MB
    压缩比: 3.97x
    位/碱基: 4.53
    
  块:
    块 0: 189000 reads, 10.7 MB
    块 1: 189000 reads, 10.5 MB
    ...
```

---

### `verify` - 验证归档完整性

验证 FQC 归档的完整性。

```bash
fqc verify [选项] <归档>
```

#### 选项

| 选项 | 简写 | 描述 | 默认值 |
|------|------|------|--------|
| `--verbose` | `-v` | 详细输出 | false |

#### 示例

```bash
# 验证归档
fqc verify archive.fqc

# 详细验证
fqc verify archive.fqc -v
```

#### 示例输出

```
验证 archive.fqc...

结构检查: 通过
块数量: 12
块校验和: 12/12 通过
重排映射: 存在, 有效
元数据: 有效

归档完整性: 已验证 ✓
```

---

### `split-pe` - 分离双端测序归档

将双端测序 FQC 归档分离为两个 FASTQ 文件。

```bash
fqc split-pe [选项] -i <输入> -1 <输出1> -2 <输出2>
```

#### 选项

| 选项 | 简写 | 描述 | 默认值 |
|------|------|------|--------|
| `--input` | `-i` | 输入双端 FQC（必需） | - |
| `--output1` | `-1` | 第一个配对输出文件（必需） | - |
| `--output2` | `-2` | 第二个配对输出文件（必需） | - |
| `--range` | - | 要提取的配对范围 | 全部 |
| `--original-order` | - | 恢复原始顺序 | false |

#### 示例

```bash
# 分离双端测序归档
fqc split-pe -i paired.fqc -1 reads_1.fastq -2 reads_2.fastq

# 带范围分离
fqc split-pe -i paired.fqc -1 r1.fastq -2 r2.fastq --range 1000:2000
```

---

## 压缩模式

| 模式 | 描述 | 速度 | 压缩比 |
|------|------|------|--------|
| `fast` | 最快压缩 | ~15 MB/s | ~3.5x |
| `balanced` | 平衡性能 | ~11 MB/s | ~4.0x |
| `best` | 最佳压缩 | ~6 MB/s | ~4.5x |

---

## 环境变量

| 变量 | 描述 | 示例 |
|------|------|------|
| `FQC_THREADS` | 默认线程数 | `FQC_THREADS=8` |
| `FQC_MEMORY_LIMIT` | 默认内存限制（MB） | `FQC_MEMORY_LIMIT=4096` |
| `FQC_LOG_LEVEL` | 日志级别（trace/debug/info/warn/error） | `FQC_LOG_LEVEL=debug` |

---

## 常用工作流程

### 标准压缩工作流程

```bash
# 1. 压缩
fqc compress -i input.fastq -o output.fqc -t 8

# 2. 验证
fqc verify output.fqc

# 3. 查看信息
fqc info output.fqc

# 4. 需要时解压
fqc decompress -i output.fqc -o restored.fastq
```

### 双端测序工作流程

```bash
# 压缩双端测序数据
fqc compress -i reads_1.fastq -i2 reads_2.fastq -o paired.fqc --paired -t 8

# 分离回独立文件
fqc split-pe -i paired.fqc -1 out_1.fastq -2 out_2.fastq
```

### 随机访问工作流程

```bash
# 压缩一次
fqc compress -i large.fastq -o large.fqc

# 按需提取子集
fqc decompress -i large.fqc --range 0:10000 -o chunk_0.fastq
fqc decompress -i large.fqc --range 10000:20000 -o chunk_1.fastq
```

---

**🌐 语言**: [English](../../en/getting-started/cli-usage.md) | [简体中文（当前）](./cli-usage.md)
