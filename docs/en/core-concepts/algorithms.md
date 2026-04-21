---
title: Core Algorithms
description: Deep dive into fq-compressor compression algorithms - ABC and SCM
version: 0.2.0
last_updated: 2026-04-22
language: en
---

# Core Algorithms

> Deep dive into the compression algorithms that power fq-compressor

fq-compressor uses a **hybrid compression strategy**:
- **ABC** (Assembly-based Compression) for sequences
- **SCM** (Statistical Context Mixing) for quality scores
- **Delta + Tokenization** for read identifiers

---

## ABC: Assembly-based Compression

### Overview

ABC treats sequencing reads as fragments of an underlying genome, rather than independent strings. This approach achieves superior compression by exploiting the inherent redundancy in genomic data.

**Key Insight**: In a typical sequencing run, multiple reads come from the same genomic regions. By grouping similar reads and encoding differences from a consensus, we achieve near-entropy compression.

### ABC Pipeline

```
Input Reads
    │
    ▼
┌─────────────────┐
│ 1. Minimizer    │  Extract k-mer signatures
│    Indexing     │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 2. Bucketing    │  Group reads by shared minimizers
│                 │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 3. Reordering   │  Order reads to maximize similarity
│    (TSP)        │  between neighbors
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 4. Consensus    │  Generate local consensus per bucket
│    Generation   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 5. Delta        │  Encode reads as edits from consensus
│    Encoding     │
└────────┬────────┘
         │
         ▼
   Compressed Output
```

### 1. Minimizer Indexing

**Definition**: A minimizer of a sequence is the lexicographically smallest k-mer in a window of w consecutive k-mers.

**Algorithm**:
```cpp
// For each read
for each window of w k-mers:
    minimizer = min(window)  // Lexicographically
    index[minimizer].add(read_id)
```

**Parameters**:
- k = 21 (k-mer size)
- w = 11 (window size)

**Benefits**:
- Reduces memory usage vs. storing all k-mers
- Maintains sensitivity for similar reads
- Biochemically relevant (minimizers tend to cluster)

### 2. Bucketing

Reads sharing minimizers are grouped into buckets.

**Bucket Size Management**:
```cpp
// Target bucket size for optimal compression
const size_t kTargetBucketSize = 10000;

// Split large buckets if needed
if (bucket.size() > kTargetBucketSize * 2):
    split_by_secondary_minimizer(bucket)
```

**Why it works**:
- Reads in the same genomic region share minimizers
- Local consensus is more accurate than global
- Enables parallel processing per bucket

### 3. Reordering (Traveling Salesman)

Within each bucket, reorder reads to minimize edit distance between consecutive reads.

**Greedy TSP Approximation**:
```python
def reorder_reads(bucket):
    ordered = [bucket.pop_random()]
    
    while bucket:
        last = ordered[-1]
        # Find nearest neighbor
        next_read = min(bucket, key=lambda r: edit_distance(last, r))
        ordered.append(next_read)
        bucket.remove(next_read)
    
    return ordered
```

**Complexity**: O(n²) for bucket of size n

**Result**: Reads form an approximate Hamiltonian path where adjacent reads are similar.

### 4. Consensus Generation

Build a position-wise consensus from reordered reads.

**Algorithm**:
```python
def generate_consensus(ordered_reads):
    # Align reads to first read
    alignment = multi_align(ordered_reads)
    
    # Position-wise majority vote
    consensus = []
    for position in alignment.columns:
        counts = Counter(position.bases)
        consensus.append(counts.most_common(1)[0][0])
    
    return ''.join(consensus)
```

**Variants**:
- Simple majority for high coverage
- Quality-weighted voting for noisy data

### 5. Delta Encoding

Encode each read as a series of edits from the consensus.

**Edit Types**:
```cpp
enum class EditType {
    Match,      // Same as consensus
    Subst,      // Substitution
    Insert,     // Insertion
    Delete      // Deletion
};

struct Edit {
    EditType type;
    uint32_t position;
    char base;      // For Subst/Insert
};
```

**Encoding Example**:
```
Consensus:  ACGTACGTACGTACGT
Read:       ACGTAAGTACGTACGT

Edits:
  - [5]: Subst A -> A (no change, match)
  - [6]: Subst C -> A

Result: 1 substitution at position 6: A
```

**Compression Ratio**: Typical 3-5x for genomic sequences

---

## SCM: Statistical Context Mixing

### Overview

SCM compresses quality scores using high-order context modeling with arithmetic coding. Quality scores are challenging because they:
- Have high entropy (noisy)
- Are not "assemblable" (don't share structure)
- Have position-dependent patterns (quality drops near read ends)

### Context Modeling

The compressor predicts the next quality value based on previous context.

**Context Definition**:
```
Context = (prev_qv1, prev_qv2, current_base, position_bucket)
```

**Order-2 Model Example**:
```cpp
// P(Q_i | Q_{i-1}, Q_{i-2}, base_i)
struct Context {
    uint8_t qv_minus_1;      // Previous quality value
    uint8_t qv_minus_2;      // Quality before that
    char current_base;        // Current sequence base (A/C/G/T/N)
    uint8_t position_bucket;  // Quantized position (0-9)
};
```

### Multiple Model Mixing

Combine predictions from multiple context models:

```python
# Order-0: P(Q_i) - marginal probability
# Order-1: P(Q_i | Q_{i-1})
# Order-2: P(Q_i | Q_{i-1}, Q_{i-2})
# Order-s: P(Q_i | current_sequence_context)

# Weighted mixing
final_prediction = sum(
    weight[i] * model[i].predict(context[i])
    for i in range(num_models)
)
```

**Static vs. Adaptive Weights**:
- Static: Pre-trained weights from training data
- Adaptive: Online weight adjustment per sequence

### Arithmetic Coding

Convert predictions into compressed bits using arithmetic coding.

**Principle**:
- Quality values encoded as intervals
- Interval subdivided by probability distribution
- High-probability values get larger intervals (fewer bits)

**Example**:
```
Q values: [33, 34, 35, 36, 37]  # Phred scores
Counts:   [100, 200, 400, 200, 100]  # Empirical distribution

P(35) = 400/1000 = 0.4
Bits(35) = -log2(0.4) ≈ 1.32 bits
```

### Quality Value Binning (Optional)

For lossy compression, bin quality values:

| Method | Bins | Bits/QV | Quality Impact |
|--------|------|---------|----------------|
| Lossless | 94 (33-126) | ~5 | None |
| Illumina 8 | 8 | 3 | Minimal |
| 4-bin | 4 | 2 | Moderate |
| 2-bin | 2 | 1 | High |

**Illumina 8-bin Strategy**:
```cpp
// Illumina recommended bins
int bin_quality(int q) {
    if (q <= 9) return q;        // Keep low QVs
    if (q <= 19) return 15;      // Bin 10-19
    if (q <= 24) return 22;      // Bin 20-24
    if (q <= 29) return 27;      // Bin 25-29
    if (q <= 34) return 33;      // Bin 30-34
    if (q <= 39) return 37;      // Bin 35-39
    return 40;                    // Bin 40+
}
```

---

## ID Compression

### Tokenization

Read identifiers often follow patterns like:
```
@E150035817L1C001R0010000000/1
@E150035817L1C001R0010000001/1
@E150035817L1C001R0010000002/1
```

**Tokenization Strategy**:
```python
def tokenize_id(read_id):
    # Split on pattern transitions (letter->digit, digit->letter)
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

# Result: ['E', '150035817', 'L', '1', 'C', '001', 'R', '001', '0000000', '/', '1']
```

### Delta Encoding

For monotonically increasing numeric tokens:

```
Original: [001, 002, 003, 004, 005, ...]
Delta:    [001, 001, 001, 001, 001, ...]  // Much smaller!
```

### Dictionary Compression

Build a dictionary of common token patterns:

```cpp
// First pass: build dictionary
std::unordered_map<std::string, uint32_t> dict;
for (const auto& token : all_tokens):
    if (frequency[token] > threshold):
        dict[token] = next_id++;

// Second pass: encode
for (auto& token : tokens):
    if (dict.contains(token)):
        emit(dict_id_marker, dict[token]);
    else:
        emit(literal_marker, token);
```

---

## Performance Characteristics

| Algorithm | Speed | Compression | Memory | Parallel |
|-----------|-------|-------------|--------|----------|
| ABC | ~10 MB/s | 3-5x | Medium | Bucket-level |
| SCM | ~15 MB/s | 2-3x | Low | Yes |
| ID | ~50 MB/s | 5-10x | Low | Yes |

**Combined**: ~11 MB/s compression, 3.97x typical ratio

---

## References

### Academic Papers

1. **SPRING**: Chandak, S., et al. (2019). "SPRING: a next-generation FASTQ compressor." *Bioinformatics*.
2. **Mincom**: Liu, Y., et al. "Mincom: hash-based minimizer bucketing for genome sequence compression."
3. **fqzcomp**: Bonfield, J. K. "Compression of FASTQ and SAM format sequencing data."

### Related Tools

- [Spring](https://github.com/shubhamchandak94/Spring) - Original ABC implementation
- [fqzcomp5](https://github.com/jkbonfield/fqzcomp5) - Quality compression reference

---

**🌐 Language**: [English (current)](./algorithms.md) | [简体中文](../../zh/core-concepts/algorithms.md)
