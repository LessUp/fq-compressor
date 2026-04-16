---
title: 核心算法
description: fq-compressor 压缩算法深入解析 - ABC 和 SCM
version: 0.1.0
last_updated: 2026-04-16
language: zh
---

# 核心算法

> fq-compressor 压缩算法的深入解析

fq-compressor 采用**混合压缩策略**：
- **ABC**（基于组装的压缩）用于序列
- **SCM**（统计上下文混合）用于质量分数
- **Delta + 标记化**用于 reads 标识符

---

## ABC：基于组装的压缩

### 概述

ABC 将测序 reads 视为底层基因组的片段，而非独立的字符串。这种方法通过利用基因组数据固有的冗余性实现卓越的压缩效果。

**核心洞察**：在典型的测序运行中，多个 reads 来自相同的基因组区域。通过将相似的 reads 分组并对相对共识的差异进行编码，我们实现了接近熵极限的压缩。

### ABC 流水线

```
输入 Reads
    │
    ▼
┌─────────────────┐
│ 1. Minimizer    │  提取 k-mer 签名
│    索引         │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 2. 分桶         │  按共享 minimizer 分组 reads
│                 │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 3. 重排序       │  对 reads 排序以最大化
│    (TSP)        │  相邻相似性
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 4. 共识         │  为每个桶生成局部共识
│    生成         │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 5. 增量         │  将 reads 编码为
│    编码         │  相对共识的编辑
└────────┬────────┘
         │
         ▼
   压缩输出
```

### 1. Minimizer 索引

**定义**：序列的 minimizer 是 w 个连续 k-mer 窗口中字典序最小的 k-mer。

**算法**：
```cpp
// 对于每个 read
for each window of w k-mers:
    minimizer = min(window)  // 字典序
    index[minimizer].add(read_id)
```

**参数**：
- k = 21（k-mer 大小）
- w = 11（窗口大小）

**优势**：
- 与存储所有 k-mer 相比减少内存使用
- 保持相似 reads 的敏感性
- 具有生化相关性（minimizer 倾向于聚集）

### 2. 分桶

共享 minimizer 的 reads 被分组到桶中。

**桶大小管理**：
```cpp
// 目标桶大小以实现最优压缩
const size_t kTargetBucketSize = 10000;

// 如有需要，拆分大桶
if (bucket.size() > kTargetBucketSize * 2):
    split_by_secondary_minimizer(bucket)
```

**工作原理**：
- 相同基因组区域的 reads 共享 minimizer
- 局部共识比全局共识更准确
- 支持每个桶的并行处理

### 3. 重排序（旅行商问题）

在每个桶内，对 reads 重排序以最小化连续 reads 之间的编辑距离。

**贪心 TSP 近似**：
```python
def reorder_reads(bucket):
    ordered = [bucket.pop_random()]
    
    while bucket:
        last = ordered[-1]
        # 寻找最近邻
        next_read = min(bucket, key=lambda r: edit_distance(last, r))
        ordered.append(next_read)
        bucket.remove(next_read)
    
    return ordered
```

**复杂度**：对于大小为 n 的桶为 O(n²)

**结果**：Reads 形成近似哈密顿路径，相邻 reads 相似。

### 4. 共识生成

从重排序的 reads 构建位置级共识。

**算法**：
```python
def generate_consensus(ordered_reads):
    # 将 reads 对齐到第一条
    alignment = multi_align(ordered_reads)
    
    # 位置级多数投票
    consensus = []
    for position in alignment.columns:
        counts = Counter(position.bases)
        consensus.append(counts.most_common(1)[0][0])
    
    return ''.join(consensus)
```

**变体**：
- 高覆盖度使用简单多数
- 噪声数据使用质量加权投票

### 5. 增量编码

将每个 read 编码为相对共识的一系列编辑。

**编辑类型**：
```cpp
enum class EditType {
    Match,      // 与共识相同
    Subst,      // 替换
    Insert,     // 插入
    Delete      // 删除
};

struct Edit {
    EditType type;
    uint32_t position;
    char base;      // 用于替换/插入
};
```

**编码示例**：
```
共识:  ACGTACGTACGTACGT
Read:  ACGTAAGTACGTACGT

编辑：
  - [5]: 替换 A -> A（无变化，匹配）
  - [6]: 替换 C -> A

结果：位置 6 的 1 次替换：A
```

**压缩比**：典型基因组序列 3-5 倍

---

## SCM：统计上下文混合

### 概述

SCM 使用高阶上下文建模和算术编码压缩质量分数。质量分数具有挑战性，因为它们：
- 具有高熵（噪声）
- 不易"组装"（不共享结构）
- 具有位置依赖模式（reads 末端质量下降）

### 上下文建模

压缩器基于先前上下文预测下一个质量值。

**上下文定义**：
```
上下文 = (prev_qv1, prev_qv2, current_base, position_bucket)
```

**2 阶模型示例**：
```cpp
// P(Q_i | Q_{i-1}, Q_{i-2}, base_i)
struct Context {
    uint8_t qv_minus_1;      // 前一个质量值
    uint8_t qv_minus_2;      // 再前一个质量值
    char current_base;        // 当前序列碱基 (A/C/G/T/N)
    uint8_t position_bucket;  // 量化位置 (0-9)
};
```

### 多模型混合

组合来自多个上下文模型的预测：

```python
# 0 阶: P(Q_i) - 边缘概率
# 1 阶: P(Q_i | Q_{i-1})
# 2 阶: P(Q_i | Q_{i-1}, Q_{i-2})
# s 阶: P(Q_i | current_sequence_context)

# 加权混合
final_prediction = sum(
    weight[i] * model[i].predict(context[i])
    for i in range(num_models)
)
```

**静态 vs 自适应权重**：
- 静态：从训练数据预训练的权重
- 自适应：每个序列的在线权重调整

### 算术编码

使用算术编码将预测转换为压缩位。

**原理**：
- 质量值编码为区间
- 按概率分布细分区间
- 高概率值获得更大区间（更少位）

**示例**：
```
Q 值: [33, 34, 35, 36, 37]  // Phred 分数
计数: [100, 200, 400, 200, 100]  // 经验分布

P(35) = 400/1000 = 0.4
Bits(35) = -log2(0.4) ≈ 1.32 位
```

### 质量值分箱（可选）

用于有损压缩，对质量值进行分箱：

| 方法 | 分箱数 | 每 QV 位数 | 质量影响 |
|--------|------|---------|--------|
| 无损 | 94 (33-126) | ~5 | 无 |
| Illumina 8 | 8 | 3 | 最小 |
| 4 分箱 | 4 | 2 | 中等 |
| 2 分箱 | 2 | 1 | 高 |

**Illumina 8 分箱策略**：
```cpp
// Illumina 推荐分箱
int bin_quality(int q) {
    if (q <= 9) return q;        // 保留低 QV
    if (q <= 19) return 15;      // 分箱 10-19
    if (q <= 24) return 22;      // 分箱 20-24
    if (q <= 29) return 27;      // 分箱 25-29
    if (q <= 34) return 33;      // 分箱 30-34
    if (q <= 39) return 37;      // 分箱 35-39
    return 40;                    // 分箱 40+
}
```

---

## ID 压缩

### 标记化

Reads 标识符通常遵循如下模式：
```
@E150035817L1C001R0010000000/1
@E150035817L1C001R0010000001/1
@E150035817L1C001R0010000002/1
```

**标记化策略**：
```python
def tokenize_id(read_id):
    # 在模式转换处分割（字母->数字，数字->字母）
    tokens = []
    current = read_id[0]
    prev_type = char_type(read_id[0])
    
    for c in read_id[1:]:
        curr_type = char_type(c)
        if curr_type != prev_type or c in '_/':
            tokens.append(current)
            current = c
            prev_type = curr_type
        else:
            current += c
    
    tokens.append(current)
    return tokens

# 结果: ['E', '150035817', 'L', '1', 'C', '001', 'R', '001', '0000000', '/', '1']
```

### 增量编码

对于单调递增的数字令牌：

```
原始: [001, 002, 003, 004, 005, ...]
增量: [001, 001, 001, 001, 001, ...]  // 小得多！
```

### 字典压缩

构建常见令牌模式字典：

```cpp
// 第一遍：构建字典
std::unordered_map<std::string, uint32_t> dict;
for (const auto& token : all_tokens):
    if (frequency[token] > threshold):
        dict[token] = next_id++;

// 第二遍：编码
for (auto& token : tokens):
    if (dict.contains(token)):
        emit(dict_id_marker, dict[token]);
    else:
        emit(literal_marker, token);
```

---

## 性能特性

| 算法 | 速度 | 压缩 | 内存 | 并行 |
|-----------|-------|-------------|--------|----------|
| ABC | ~10 MB/s | 3-5x | 中等 | 桶级 |
| SCM | ~15 MB/s | 2-3x | 低 | 是 |
| ID | ~50 MB/s | 5-10x | 低 | 是 |

**综合**: ~11 MB/s 压缩, 典型比 3.97x

---

## 参考文献

### 学术论文

1. **SPRING**: Chandak, S., et al. (2019). "SPRING: a next-generation FASTQ compressor." *Bioinformatics*.
2. **Mincom**: Liu, Y., et al. "Mincom: hash-based minimizer bucketing for genome sequence compression."
3. **fqzcomp**: Bonfield, J. K. "Compression of FASTQ and SAM format sequencing data."

### 相关工具

- [Spring](https://github.com/shubhamchandak94/Spring) - 原始 ABC 实现
- [fqzcomp5](https://github.com/jkbonfield/fqzcomp5) - 质量压缩参考

---

**🌐 语言**: [English](../../en/core-concepts/algorithms.md) | [简体中文（当前）](./algorithms.md)
