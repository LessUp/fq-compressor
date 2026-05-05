# RFC-0002: Compression Algorithms

> **Status**: Adopted  
> **Author**: Project Team  
> **Date**: 2026-01-15

---

## Summary

This RFC describes the three core compression algorithms used in fq-compressor:
- **ABC (Assembly-based Compression)** for sequences
- **SCM (Statistical Context Mixing)** for quality scores
- **Delta + Tokenization** for read IDs

---

## Sequence Compression: ABC

### Overview

Assembly-based Compression treats reads as genome fragments rather than independent strings. The key insight: "Don't compress reads, compress the edits."

### Pipeline

```
┌─────────────────────────────────────────────────────────┐
│  1. Minimizer Extraction                                 │
│     - Slide window over each read                        │
│     - Extract (w,k)-minimizers as signatures             │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  2. Bucketing                                            │
│     - Group reads by shared minimizers                   │
│     - Similar reads end up in same bucket                │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  3. Reordering (TSP Approximation)                       │
│     - Order reads to maximize neighbor similarity        │
│     - Approximate Hamiltonian Path                       │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  4. Consensus Generation                                 │
│     - Build local consensus per bucket                   │
│     - Reference for delta encoding                       │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  5. Delta Encoding                                       │
│     - Store edits from consensus                         │
│     - Position + substitution encoding                   │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  6. Arithmetic Coding                                    │
│     - Final entropy compression                          │
└─────────────────────────────────────────────────────────┘
```

### Key Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `k` | 10 | Minimizer k-mer size |
| `w` | 10 | Minimizer window size |
| Block size | 100K | Reads per block (short) |

### Spring 511bp Limit

Spring's `MAX_READ_LEN = 511` is a compile-time constant deeply embedded in data structures:
- Fixed-size bitset for base masks
- Static arrays for position masks

**Design Decision**: Do NOT extend Spring ABC for long reads. Use Zstd fallback instead.

---

## Quality Compression: SCM

### Overview

Statistical Context Mixing uses context models to predict quality score distributions, achieving near-entropy compression.

### Context Model

```
Context = {
    previous quality scores (order-1 or order-2),
    current base,
    position in read,
    run-length of identical scores
}
```

### Pipeline

```
┌─────────────────────────────────────────────────────────┐
│  1. Context Extraction                                   │
│     - Previous QVs (last 1-2 positions)                  │
│     - Current base (A/C/G/T/N)                           │
│     - Position                                           │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  2. Probability Estimation                               │
│     - Look up context in model                           │
│     - Get probability distribution                       │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  3. Adaptive Update                                      │
│     - Update model with observed value                   │
│     - Learn quality patterns                             │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  4. Range Coding                                         │
│     - Encode using predicted distribution                │
└─────────────────────────────────────────────────────────┘
```

### Order Selection

| Mode | Context Depth | Memory | Use Case |
|------|---------------|--------|----------|
| Order-1 | 1 previous QV | Low | Long reads |
| Order-2 | 2 previous QVs | Medium | Short reads (default) |

### Lossy Modes

1. **Illumina 8-bin**: Map 41 quality levels to 8 bins
   ```
   [0-9] → 0, [10-19] → 1, [20-24] → 2, [25-29] → 3
   [30-34] → 4, [35-37] → 5, [38-40] → 6, [41] → 7
   ```

2. **Discard**: Drop quality values entirely
   - Decompress with placeholder `!` (Phred 0)
   - Configurable via `--placeholder-qual`

---

## ID Compression: Delta + Tokenization

### Overview

Read IDs often follow patterns with incremental numeric fields. Delta encoding exploits this structure.

### Illumina Header Format

```
@<instrument>:<run>:<flowcell>:<lane>:<tile>:<x>:<y>
Example: @SIM:1:FCX:1:1:1001:2001
         @SIM:1:FCX:1:1:1001:2002  ← Only last field changes
```

### Pipeline

```
┌─────────────────────────────────────────────────────────┐
│  1. Tokenization                                         │
│     - Split ID into components                           │
│     - Identify static vs dynamic parts                   │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  2. Static Part Storage                                  │
│     - Store once per block                               │
│     - Template for reconstruction                        │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  3. Delta Encoding (Dynamic Parts)                       │
│     - Store differences: x_n = x_(n-1) + delta           │
│     - Use variable-length encoding                       │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  4. LZMA/Zstd Compression                                │
│     - Final compression of delta stream                  │
└─────────────────────────────────────────────────────────┘
```

### Modes

| Mode | Description | Compression | Use Case |
|------|-------------|-------------|----------|
| `exact` | Full preservation | Lower | Clinical data |
| `tokenize` | Structure-aware | Medium | Standard Illumina |
| `discard` | Sequential IDs | Highest | Anonymous archives |

### ID Reconstruction (discard mode)

- SE: `@{archive_id}` → `@1`, `@2`, `@3`
- PE Interleaved: `@{pair_id}/{read_num}` → `@1/1`, `@1/2`
- Configurable prefix via `--id-prefix`

---

## Codec Selection Logic

```
if (max_read_length <= 511 && median_length < 1KB) {
    // Short read: Spring ABC
    sequence_codec = ABC_V1;
    enable_reordering = true;
    block_size = 100000;
} else if (max_read_length < 10000) {
    // Medium read: Zstd
    sequence_codec = ZSTD_PLAIN;
    enable_reordering = false;
    block_size = 50000;
} else {
    // Long read: Zstd with conservative settings
    sequence_codec = ZSTD_PLAIN;
    enable_reordering = false;
    block_size = 10000;
    apply_max_block_bases_limit = true;
}

// Quality codec
quality_codec = SCM_V1;  // Order-2 for short, Order-1 for long

// ID codec
id_codec = DELTA_ZSTD;  // Or DELTA_LZMA for higher compression
```

---

## Performance Characteristics

| Algorithm | Compress Speed | Decompress Speed | Ratio |
|-----------|---------------|------------------|-------|
| ABC (Sequence) | 5-10 MB/s | 30-50 MB/s | ~0.3 bits/base |
| SCM (Quality) | 50-100 MB/s | 100-200 MB/s | ~0.8 bits/QV |
| Delta (ID) | 100+ MB/s | 200+ MB/s | ~2 bytes/ID |

---

## References

- Spring paper: Chandak et al., 2019, Genome Research
- fqzcomp5: Bonfield, htscodecs library
- Range coding: Martin, 1979
