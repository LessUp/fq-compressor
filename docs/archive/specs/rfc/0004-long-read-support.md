# RFC-0004: Long Read Support

> **Status**: Adopted  
> **Author**: Project Team  
> **Date**: 2026-02-01

---

## Summary

This RFC describes the strategy for handling long read data (PacBio HiFi, Nanopore), including automatic classification, compression algorithm selection, and block size adaptation.

---

## Background: Spring 511bp Limit

### The Problem

Spring's `MAX_READ_LEN = 511` is a compile-time constant embedded in:

```cpp
// Spring source (params.h)
const uint16_t MAX_READ_LEN = 511;

// Dependent structures (reorder.cpp)
typedef std::bitset<2*MAX_READ_LEN> bitset;  // Fixed 1022 bits
bitset basemask[MAX_READ_LEN][128];          // Static array
bitset positionmask[MAX_READ_LEN];
char s[MAX_READ_LEN + 1];                    // Fixed buffer
```

### Extension Analysis

| Approach | Difficulty | Impact |
|----------|------------|--------|
| Increase constant | Low | Memory ×2 per doubling |
| Dynamic allocation | High | Major refactor |
| Algorithm adaptation | Limited | ABC ineffective for long reads |

### Key Insight

Spring's `-l` (long read) mode **completely bypasses ABC**:
> "Supports variable length long reads... This mode directly applies general purpose compression (BSC)"

**Conclusion**: Don't extend ABC for long reads. Use dedicated strategy.

---

## Read Length Classification

### Three-Tier System

| Class | Criteria | Typical Data |
|-------|----------|--------------|
| **Short** | max ≤ 511 AND median < 1KB | Illumina (50-300bp) |
| **Medium** | max > 511 OR median ≥ 1KB | PacBio HiFi (1-25KB) |
| **Long** | max ≥ 10KB | Nanopore, PacBio CLR |
| **Ultra-long** | max ≥ 100KB | Nanopore ultra-long |

### Detection Algorithm

```
function classify_reads(sample_reads, total_reads):
    sample_size = min(1000, total_reads)
    
    lengths = [len(r) for r in sample_reads]
    max_len = max(lengths)
    median_len = median(lengths)
    
    # Priority order (strict)
    if max_len >= 100KB:
        return LONG  # Ultra-long strategy
    if max_len >= 10KB:
        return LONG
    if max_len > 511:
        return MEDIUM  # Spring compatibility guard
    if median_len >= 1KB:
        return MEDIUM
    return SHORT
```

### Conservative Strategy

**Key Rule**: Never truncate reads to fit Spring ABC.

If ANY read exceeds 511bp, the entire file is classified as MEDIUM or higher. This ensures:
- Data integrity (no information loss)
- Correct codec selection
- Appropriate block sizing

### stdin Handling

stdin input cannot be sampled (no seeking):
- Default to MEDIUM strategy (conservative)
- Emit warning: "stdin input, using MEDIUM strategy"
- User can override with `--long-read-mode`

---

## Compression Strategies

### Short Read: ABC + Reordering

```
Algorithm: Spring ABC
- Minimizer bucketing
- Global reordering (TSP)
- Local consensus
- Delta encoding

Block Size: 100,000 reads
Quality SCM: Order-2
Reordering: Enabled
```

### Medium Read: Zstd

```
Algorithm: Zstd
- General-purpose compression
- No reordering (limited benefit)

Block Size: 50,000 reads
Quality SCM: Order-2
Reordering: Disabled
```

### Long Read: Zstd + Conservative

```
Algorithm: Zstd
- Level 5 default
- No reordering

Block Size: 10,000 reads
Quality SCM: Order-1 (memory reduction)
Block Bases Limit: 200MB
Reordering: Disabled
```

### Ultra-long Read: Zstd + Strict Limits

```
Algorithm: Zstd
- Level 5

Block Size: 10,000 reads (soft)
Block Bases Limit: 50MB (hard)
Quality SCM: Order-1
Reordering: Disabled

Actual reads per block:
    = min(10000, 50MB / max(1, median_length))
```

---

## Why Not NanoSpring?

### NanoSpring Analysis

| Feature | Spring (ABC) | NanoSpring |
|---------|-------------|------------|
| Core algorithm | Minimizer + Reordering | MinHash + Minimap2 + Graph |
| Target | Short reads (<500bp) | Long reads (10KB+) |
| Data compressed | Full FASTQ | **Sequence only** |
| Error tolerance | ~0.1% | 5-15% |
| Block mode | Needs adaptation | Native global |

### Integration Challenges

1. **No Quality/ID support**: NanoSpring ignores quality scores and read IDs
2. **Global processing**: Designed for whole-file compression, not blocks
3. **Complex dependency**: Requires minimap2 alignment
4. **High complexity**: Block adaptation is non-trivial

### Design Decision

**Use Zstd for long reads**:
- Simple, stable, fast
- Full FASTQ support
- Native streaming
- Block-compatible
- Sufficient compression ratio

NanoSpring-style overlap compression is a potential future optimization.

---

## Block Size Adaptation

### Dynamic Calculation

```cpp
size_t calculate_block_size(ReadLengthClass cls, size_t median_len) {
    switch (cls) {
        case SHORT:
            return 100000;
        case MEDIUM:
            return 50000;
        case LONG:
            if (median_len >= 100000) {
                // Ultra-long: limit by bases
                size_t reads_by_bases = 50'000'000 / median_len;
                return std::min(10000UL, reads_by_bases);
            }
            size_t reads_by_bases = 200'000'000 / median_len;
            return std::min(10000UL, reads_by_bases);
    }
}
```

### Rationale

| Metric | Short | Medium | Long | Ultra-long |
|--------|-------|--------|------|------------|
| Reads/block | 100K | 50K | 10K | ≤10K |
| Bases/block | ~50MB | ~250MB | ~100MB | ≤50MB |
| Memory/block | ~5MB | ~25MB | ~10MB | ≤5MB |

---

## CLI Options

### Long Read Mode

```
--long-read-mode <auto|short|medium|long>
    auto     Automatic detection (default)
    short    Force short read mode (error if max > 511)
    medium   Force medium mode
    long     Force long mode
```

### Block Configuration

```
--block-reads <N>
    Override default block size (reads per block)

--max-block-bases <bytes>
    Maximum bases per block (Long/Ultra-long only)
    Default: 200MB (Long), 50MB (Ultra-long)
```

### All-Length Scan

```
--scan-all-lengths
    Scan entire file for max_length detection
    More accurate but slower
    Only for file input (not stdin)
```

---

## Mixed Length Handling

### Reality Check

**Mixed short/long reads in a single FASTQ are extremely rare**:
- Different sequencing platforms (Illumina vs Nanopore) produce separate files
- Each run outputs its own FASTQ
- Downstream analysis pipelines expect uniform data

### Design Decision

No special handling for mixed lengths. Classification is file-wide:
- If any read exceeds 511bp → MEDIUM or higher
- Compression ratio may be suboptimal for mixed files (acceptable trade-off)

---

## Performance Expectations

| Data Type | Compression Ratio | Speed (Compress) | Speed (Decompress) |
|-----------|-------------------|------------------|-------------------|
| Illumina 150bp | 3-5x | 20-50 MB/s | 100-200 MB/s |
| PacBio HiFi | 2-3x | 50-100 MB/s | 150-300 MB/s |
| Nanopore | 2-3x | 50-100 MB/s | 150-300 MB/s |
| Ultra-long | 2-3x | 30-80 MB/s | 100-200 MB/s |

*Compression ratio for long reads is lower because ABC reordering benefits are unavailable*
