# 快速开始

本指南将带你完成第一个 FASTQ 文件的压缩和解压。

## 压缩 FASTQ 文件

```bash
fqc compress -i input.fastq -o output.fqc
```

读取 `input.fastq`，依次执行完整压缩流水线（序列用 ABC、质量值用 SCM、标识符用 tokenization），输出压缩归档 `output.fqc`。

### 常用选项

```bash
# 指定线程数
fqc compress -i input.fastq -o output.fqc -t 8

# 设置压缩级别（1-9，默认：5）
fqc compress -i input.fastq -o output.fqc -l 9

# 设置内存限制（MB）
fqc compress -i input.fastq -o output.fqc --memory-limit 4096

# 直接压缩 gzip 格式的 FASTQ
fqc compress -i input.fastq.gz -o output.fqc
```

## 解压归档

```bash
fqc decompress -i output.fqc -o restored.fastq
```

### 随机访问解压

fq-compressor 的核心特性之一是 O(1) 随机访问：

```bash
# 提取第 1000 到 2000 条 reads
fqc decompress -i output.fqc --range 1000:2000 -o subset.fastq
```

## 查看归档信息

```bash
fqc info output.fqc
```

显示压缩归档的元信息：
- Read 条数
- 原始大小和压缩后大小
- 压缩比和 bits per base
- Block 数量和索引信息
- Read 长度统计

## 校验完整性

```bash
fqc verify output.fqc
```

执行全面的完整性检查：
- 文件头和文件尾校验和
- Block 级别 CRC 验证
- 索引一致性检查

## 双端数据（Paired-End）

```bash
# 压缩双端文件
fqc compress -i read1.fastq -I read2.fastq -o paired.fqc

# 解压双端文件
fqc decompress -i paired.fqc -o read1_out.fastq -O read2_out.fastq
```

## 流水线模式

处理大文件时，使用流水线模式获得最大吞吐量：

```bash
fqc compress -i input.fastq -o output.fqc --pipeline
fqc decompress -i output.fqc -o restored.fastq --pipeline
```

启用完整的 3 阶段并行流水线（读取 → 压缩 → 写入），基于 Intel oneTBB 实现。
