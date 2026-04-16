---
title: 性能基准测试
description: fq-compressor 综合基准测试指南
version: 0.1.0
last_updated: 2026-04-16
language: zh
---

# 性能基准测试

> 评估 fq-compressor 在您的数据上的性能

## 概述

fq-compressor 包含综合基准测试工具，用于评估：

- **压缩比**: 压缩后数据小多少？
- **速度**: 压缩和解压的吞吐量
- **内存使用**: 峰值内存消耗
- **可扩展性**: 线程数增加时的性能变化

## 基准测试套件

### 快速基准测试

```bash
# 简单单轮基准测试
./scripts/benchmark.sh --quick --input data.fastq
```

### 完整基准测试

```bash
# 多轮综合基准测试
./scripts/benchmark.sh --full --input data.fastq
```

## 性能指标

### 压缩指标

| 指标 | 描述 | 目标 |
|--------|-------------|--------|
| **压缩比** | 原始 / 压缩后 | 3.5-5x |
| **每碱基位数** | 压缩位每核苷酸 | 0.4-0.6 |
| **速度** | 未压缩数据 MB/s | >10 MB/s |
| **内存** | 峰值 RAM 使用 | <2 GB (100MB 文件) |

### 解压指标

| 指标 | 描述 | 目标 |
|--------|-------------|--------|
| **速度** | 压缩数据 MB/s | >50 MB/s |
| **内存** | 峰值 RAM 使用 | <500 MB |

## GCC vs Clang 对比

基准测试框架可以比较编译器版本：

```bash
# 构建两个版本
./scripts/build.sh gcc-release
./scripts/build.sh clang-release

# 运行对比
python3 benchmark/compiler_benchmark.py \
    --input test_data.fastq.gz \
    --gcc-binary build/gcc-release/src/fqc \
    --clang-binary build/clang-release/src/fqc \
    --threads 1 4 8 \
    --runs 3 \
    --visualize
```

### 预期结果

| 编译器 | 压缩 | 解压 | 说明 |
|----------|-------------|---------------|-------|
| GCC 15.2 | ~11.3 MB/s | ~60 MB/s | 稳定 |
| Clang 21 | ~11.9 MB/s | ~62 MB/s | 快 5% |

## 多工具对比

将 fq-compressor 与其他工具对比：

```bash
python3 benchmark/benchmark.py \
    --input data.fastq \
    --tools fqc,gzip,pigz,zstd \
    --threads 1,4,8 \
    --output-dir results/
```

### 典型对比

| 工具 | 压缩比 | 压缩 | 解压 | 并行 |
|------|-------|----------|------------|----------|
| fq-compressor | 3.97x | 11.9 MB/s | 62.3 MB/s | ✓ |
| gzip | 2.1x | 45 MB/s | 180 MB/s | ✗ |
| pigz | 2.1x | 150 MB/s | 180 MB/s | ✓ |
| zstd | 2.5x | 220 MB/s | 500 MB/s | ✗ |

## 结果解读

### 速度与压缩比的权衡

fq-compressor 优先考虑压缩比而非原始速度：

```
高压缩 ──────────────────► 快速解压
     │                                    │
  fq-compressor                    gzip
  Spring                           pigz
  fqzcomp5                         zstd
```

### 内存扩展

内存使用大致随线程数线性扩展：

```
线程数:  1    2    4    8    16
内存:   500M 800M 1.2G 1.8G 2.5G (示例)
```

使用 `--memory-limit` 来限制使用量。

## 持续基准测试

### CI 集成

添加到 GitHub Actions：

```yaml
- name: Benchmark
  run: |
    python3 benchmark/compiler_benchmark.py \
        --input benchmarks/test_data.fq.gz \
        --gcc-binary build/gcc-release/src/fqc \
        --clang-binary build/clang-release/src/fqc \
        --output-dir benchmark-results/
    
- name: Upload Results
  uses: actions/upload-artifact@v4
  with:
    name: benchmark-results
    path: benchmark-results/
```

### 性能回归检测

```bash
# 与基准对比
python3 benchmark/compare_baseline.py \
    --baseline results/baseline.json \
    --current results/current.json \
    --threshold 5%  # 如果衰退 >5% 则失败
```

## 基准测试最佳实践

### 数据选择

- 使用来自您领域的代表性数据集
- 包括小文件 (<100MB) 和大文件 (>1GB)
- 测试双端和单端数据

### 环境

- 关闭其他 CPU 密集型应用程序
- 确保足够的 RAM（避免交换）
- 使用热缓存（计时前运行一次）
- 多次迭代以提高准确性

### 报告

报告中应包括：
- 硬件规格（CPU、RAM、存储）
- 软件版本（操作系统、编译器、fqc）
- 数据集特征（大小、reads 数、reads 长度）
- 使用的命令行选项

## 故障排除

### 结果不稳定

- 确保禁用 CPU 频率调节
- 检查是否发生热节流
- 使用 `taskset` 固定到特定核心

### 内存不足

```bash
# 降低并行度或块大小
fqc compress -i large.fastq -o out.fqc --threads 4 --memory-limit 4096
```

---

**🌐 语言**: [English](../../en/performance/benchmark.md) | [简体中文（当前）](./benchmark.md)
