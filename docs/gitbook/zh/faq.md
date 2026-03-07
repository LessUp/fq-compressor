# 常见问题

## 基本问题

### fq-compressor 是什么？

fq-compressor 是一个高性能 FASTQ 压缩工具，使用领域特定算法（序列使用基于组装的压缩，质量值使用统计上下文混合）实现接近理论极限的压缩比，同时支持随机访问和并行处理。

### 与 gzip/zstd 相比如何？

通用压缩器将 FASTQ 数据视为任意文本。fq-compressor 利用测序数据的生物学结构，实现比通用压缩器高 3-5 倍的压缩比。详见[性能基准](benchmark.md)页面。

### 是无损的吗？

是的。fq-compressor 默认执行完全无损压缩。每个碱基、质量值和标识符都会被精确保留。

---

## 兼容性

### 支持哪些 FASTQ 格式？

- 标准 4 行 FASTQ（`.fastq`、`.fq`）
- Gzip 压缩的 FASTQ（`.fastq.gz`、`.fq.gz`）
- 单端（SE）和双端（PE）数据
- Illumina、BGI 和其他常见标识符格式

### 支持哪些 read 长度？

fq-compressor 处理所有 read 长度，但使用不同策略：

| Read 长度 | 策略 |
|-----------|------|
| 短读（≤ 511 bp） | 完整 ABC 压缩 + 重排序 |
| 中等（512 bp – 10 KB） | Zstd 压缩，无重排序 |
| 长读（> 10 KB） | Zstd 压缩，无重排序 |

### `.fqc` 文件可以被其他工具使用吗？

不能 — `.fqc` 是自定义格式。需要使用 `fqc decompress` 解压后才能使用标准生信工具。随机访问功能使得无需完整解压即可方便地提取子集。

---

## 性能

### 应该使用多少线程？

为获得最佳效果，使用物理 CPU 核心数。对于大于 100 MB 的文件，建议使用 `--pipeline` 标志。

```bash
fqc compress -i reads.fastq -o reads.fqc -t 8 --pipeline
```

### 需要多少内存？

内存使用取决于 block 大小和在途 block 数量。默认内存限制为 8 GB，可通过 `--memory-limit` 调整：

```bash
fqc compress -i reads.fastq -o reads.fqc --memory-limit 4096  # 4 GB
```

### 为什么压缩比解压慢？

压缩涉及计算密集的操作（minimizer 提取、read 重排序、共识生成），这些操作在解压时没有对应的等价操作。这种不对称是设计使然 — 压缩一次，解压多次。

---

## 故障排除

### 解压时出现"格式错误"

确保文件由 fq-compressor 创建且未被损坏：

```bash
fqc verify archive.fqc
```

### 压缩时内存不足

减少内存限制以强制使用更小的 block 大小：

```bash
fqc compress -i reads.fastq -o reads.fqc --memory-limit 2048
```

### 构建失败提示"TBB not found"

确保 Conan 正确安装了依赖：

```bash
conan install . --build=missing -of=build
```

---

## 学术引用

### 如何引用 fq-compressor？

如果您在研究中使用了 fq-compressor，请引用 GitHub 仓库：

```
LessUp. fq-compressor: High-performance FASTQ compression with random access.
https://github.com/LessUp/fq-compressor
```

### 基于哪些算法？

- **Spring** (Chandak et al., 2019) — 基于组装的序列压缩
- **Mincom** (Liu et al.) — Minimizer 分桶
- **fqzcomp5** (Bonfield) — 质量值上下文混合
- **HARC** (Chandak et al., 2018) — 单倍型感知的重排序
