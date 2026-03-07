# 分发与最佳实践

## 为什么不提供内置 `.fqc.gz` 输出？

`.fqc` 格式已经使用领域特定算法进行了**高度压缩**。添加外部压缩层带来的收益极小，但会引入显著缺陷：

1. **边际压缩增益** — 外部压缩通常只能在已压缩的 `.fqc` 文件上增加 1-5% 的额外压缩。
2. **破坏随机访问** — `.fqc` 格式支持 O(1) 随机访问，外部压缩会破坏这一能力。
3. **增加延迟** — 解压需要两次通道（外部解压 + FQC 解压）。
4. **增加复杂性** — 管理嵌套压缩增加运维负担。

## 推荐工作流

### 网络传输

如果需要分发 `.fqc` 文件并希望传输时获得额外压缩：

```bash
# 为分发进行压缩
xz -9 archive.fqc              # 生成 archive.fqc.xz

# 使用前解压外层
xz -d archive.fqc.xz           # 恢复 archive.fqc

# 正常使用 fqc 的随机访问
fqc decompress -i archive.fqc --range 1000:2000 -o subset.fastq
```

### 场景指南

| 场景 | 建议 |
|------|------|
| **日常分析** | 直接使用 `.fqc`（随机访问可用） |
| **长期存档** | 直接使用 `.fqc`（已接近最优压缩） |
| **网络传输** | 可选用 `xz -9` 或 `zstd -19` 包装，使用前解包 |
| **云存储** | 直接使用 `.fqc`（云服务商通常会在存储时压缩） |

## 压缩效率

`.fqc` 格式实现了 **0.4–0.6 bits/base** 的压缩比，已接近基因组数据的理论熵极限。外部压缩无法显著改善。

### 与通用压缩器的对比

| 工具 | Bits/Base | 随机访问 | 并行 |
|------|-----------|---------|------|
| **fqc** | 0.4–0.6 | ✅ 是 | ✅ 是 |
| gzip | 1.8–2.2 | ❌ 否 | ❌ 否 |
| pigz | 1.8–2.2 | ❌ 否 | ✅ 是 |
| xz | 1.2–1.6 | ❌ 否 | ❌ 否 |
| zstd | 1.4–1.8 | ❌ 否 | ✅ 是 |

## 集成建议

### 生信分析流水线

```bash
# 测序后压缩
fqc compress -i raw_reads.fastq -o reads.fqc -t 8

# 提取子集用于 QC
fqc decompress -i reads.fqc --range 0:10000 -o qc_subset.fastq
fastqc qc_subset.fastq

# 完整解压用于比对
fqc decompress -i reads.fqc -o reads.fastq
bwa mem ref.fa reads.fastq | samtools sort -o aligned.bam
```

### 双端工作流

```bash
# 压缩双端数据
fqc compress -i R1.fastq.gz -I R2.fastq.gz -o sample.fqc

# 解压双端数据
fqc decompress -i sample.fqc -o R1.fastq -O R2.fastq
```
