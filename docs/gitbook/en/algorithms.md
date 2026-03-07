# Core Algorithms

fq-compressor adopts a **"Best of Both Worlds"** hybrid strategy: each FASTQ data stream is compressed with a specialized algorithm optimized for its characteristics.

## 1. Sequence: Assembly-based Compression (ABC)

Unlike traditional compressors that treat reads as independent strings, fq-compressor treats them as **fragments of an underlying genome**.

### How It Works

```
Raw Reads ──► Minimizer Bucketing ──► Read Reordering ──► Local Consensus ──► Delta Encoding
```

#### Step 1: Minimizer Bucketing

Reads are grouped into buckets based on shared **minimizers** (signature k-mers). Reads sharing a minimizer are likely from the same genomic region.

#### Step 2: Read Reordering

Inside each bucket, reads are reordered to form an approximate **Hamiltonian path**, minimizing the edit distance between neighboring reads.

#### Step 3: Local Consensus Generation

A local consensus sequence is generated for each bucket, representing the "average" read in that group.

#### Step 4: Delta Encoding

Only the edits (substitutions, insertions, deletions) relative to the consensus are stored. Since reads in the same bucket are highly similar, deltas are very small.

### Read Length Classification

The ABC strategy is most effective for short reads. fq-compressor automatically classifies reads:

| Type | Condition | Strategy |
|------|-----------|----------|
| **Short** | max ≤ 511 bp, median < 1 KB | Full ABC with global reordering |
| **Medium** | max > 511 bp or 1 KB ≤ median < 10 KB | Zstd, reordering disabled |
| **Long** | median ≥ 10 KB or max ≥ 10 KB | Zstd, reordering disabled |

### References

- **Spring**: Chandak, S., et al. (2019). *"SPRING: a next-generation FASTQ compressor"*. Bioinformatics.
- **Mincom**: Liu, Y., et al. *"Mincom: hash-based minimizer bucketing for genome sequence compression"*.
- **HARC**: Chandak, S., et al. (2018). *"HARC: Haplotype-aware Reordering for Compression"*.

---

## 2. Quality Scores: Statistical Context Mixing (SCM)

Quality scores are noisy and don't "assemble" well — they require a fundamentally different approach.

### How It Works

```
Quality Values ──► Context Modeling ──► Prediction ──► Arithmetic Coding
```

#### Context Modeling

The compressor predicts the next quality value based on a **context** composed of:
- Previous 2 quality values
- Current sequence base (A/C/G/T)
- Position within the read

#### Adaptive Arithmetic Coding

Predictions are fed into an adaptive arithmetic coder that approaches the **entropy limit** of the quality stream.

### Why Not Compress Quality with ABC?

Quality scores are inherently noisy and don't exhibit the same biological redundancy as DNA sequences. Context-based statistical modeling is more effective because:
- Quality scores are position-dependent
- They correlate with the underlying base call
- Neighboring quality values are strongly correlated

### References

- **fqzcomp5**: Bonfield, J. K. *"Compression of FASTQ and SAM format sequencing data"*.

---

## 3. Identifiers: Tokenization + Delta

FASTQ read identifiers follow structured patterns (e.g., `@INSTRUMENT:RUN:FLOWCELL:LANE:TILE:X:Y`).

### How It Works

1. **Tokenization** — Split identifiers into typed tokens (instrument, run ID, coordinates, etc.)
2. **Delta Encoding** — For numeric fields, store only the difference from the previous read
3. **Backend Compression** — Tokenized streams are compressed with a general-purpose compressor

This approach exploits the highly regular structure of Illumina-style identifiers, where consecutive reads differ only in coordinate fields.
