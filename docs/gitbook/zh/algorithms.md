# 核心算法

fq-compressor 采用 **"两全其美"** 的混合策略：每种 FASTQ 数据流使用针对其特征优化的专用算法进行压缩。

## 1. 序列：基于组装的压缩（ABC）

与传统压缩工具将 reads 视为独立字符串不同，fq-compressor 将 reads 视为**基因组的片段**。

### 工作原理

```
原始 Reads ──► Minimizer 分桶 ──► Read 重排序 ──► 局部共识 ──► Delta 编码
```

#### 步骤一：Minimizer 分桶

根据共有的 **minimizer**（签名 k-mer）将 reads 分组到桶中。共享 minimizer 的 reads 很可能来自同一基因组区域。

#### 步骤二：Read 重排序

在每个桶内，reads 被重新排列以形成近似**哈密顿路径**，最小化相邻 reads 之间的编辑距离。

#### 步骤三：局部共识生成

为每个桶生成一条局部共识序列，代表该组 reads 的"平均值"。

#### 步骤四：Delta 编码

仅存储相对于共识的编辑操作（替换、插入、删除）。由于同一桶内的 reads 高度相似，delta 非常小。

### Read 长度分类

ABC 策略对短读最有效。fq-compressor 自动对 reads 进行分类：

| 类型 | 条件 | 策略 |
|------|------|------|
| **短读** | max ≤ 511 bp，中位数 < 1 KB | 完整 ABC + 全局重排序 |
| **中等** | max > 511 bp 或 1 KB ≤ 中位数 < 10 KB | Zstd，禁用重排序 |
| **长读** | 中位数 ≥ 10 KB 或 max ≥ 10 KB | Zstd，禁用重排序 |

### 参考文献

- **Spring**: Chandak, S., et al. (2019). *"SPRING: a next-generation FASTQ compressor"*. Bioinformatics.
- **Mincom**: Liu, Y., et al. *"Mincom: hash-based minimizer bucketing for genome sequence compression"*.
- **HARC**: Chandak, S., et al. (2018). *"HARC: Haplotype-aware Reordering for Compression"*.

---

## 2. 质量值：统计上下文混合（SCM）

质量值具有噪声特性，不适合"组装"——需要根本不同的方法。

### 工作原理

```
质量值 ──► 上下文建模 ──► 预测 ──► 算术编码
```

#### 上下文建模

压缩器根据由以下元素组成的**上下文**预测下一个质量值：
- 前 2 个质量值
- 当前序列碱基（A/C/G/T）
- read 内的位置

#### 自适应算术编码

预测结果被送入自适应算术编码器，逼近质量值流的**熵极限**。

### 为什么不用 ABC 压缩质量值？

质量值本质上是噪声的，不具有与 DNA 序列相同的生物学冗余性。基于上下文的统计建模更有效，因为：
- 质量值是位置相关的
- 它们与底层碱基调用相关
- 相邻质量值之间有强相关性

### 参考文献

- **fqzcomp5**: Bonfield, J. K. *"Compression of FASTQ and SAM format sequencing data"*.

---

## 3. 标识符：Tokenization + Delta

FASTQ read 标识符遵循结构化模式（例如 `@INSTRUMENT:RUN:FLOWCELL:LANE:TILE:X:Y`）。

### 工作原理

1. **Tokenization** — 将标识符拆分为类型化 token（仪器名、运行 ID、坐标等）
2. **Delta 编码** — 对于数值字段，仅存储与前一条 read 的差值
3. **后端压缩** — tokenized 流使用通用压缩器进行压缩

这种方法利用了 Illumina 风格标识符的高度规则结构，连续 reads 通常仅在坐标字段上有差异。
