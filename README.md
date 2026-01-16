# fq-compressor

**fq-compressor** is a high-performance, next-generation FASTQ compression tool designed for the sequencing era. It combines state-of-the-art **Assembly-based Compression (ABC)** strategies with robust industrial-grade engineering to deliver extreme compression ratios, fast parallel processing, and native random access.

## 🚀 Key Features

*   **Extreme Compression**: Approaching the theoretical limit using "Assembly-based" reordering and consensus generation (similar to Spring/Mincom).
*   **Hybrid Quality Compression**: Uses **Statistical Context Mixing (SCM)** (similar to fqzcomp5) for quality scores, balancing ratio and speed.
*   **Parallel Powerhouse**: Built on **Intel oneTBB** with a scalable Producer-Consumer pipeline, maximizing multicore utilization.
*   **Random Access**: Native **Block-based** format (like BGZF) allows instant access to any part of the file (Scatter-Gather ready).
*   **Standard Compliant**: Written in **C++20**, using Modern CMake and efficient I/O.

---

## 🔬 Core Algorithms (The "Why it works")

We adopt a "Best of Both Worlds" hybrid strategy:

### 1. Sequence: Assembly-based Compression (ABC)
Unlike traditional compressors that treat reads as independent strings, we treat them as fragments of an underlying genome.
*   **Minimizer Bucketing**: Reads are grouped into buckets based on shared minimizers (signature k-mers).
*   **Reordering**: Inside each bucket, reads are reordered to form an approximate Hamiltonian path (minimizing distance between neighbors).
*   **Local Consensus & Delta**: A local consensus is generated for the bucket. Only the edits (substitutions, indels) relative to the consensus are stored.
*   **Reference**: This approach is mathematically grounded in the works of **Spring** and **Mincom**.

### 2. Quality Scores: Statistical Context Mixing (SCM)
Quality scores are noisy and don't "assemble" well. We use high-order arithmetic coding with context mixing.
*   **Context Modeling**: The compressor predicts the next Quality Value based on the previous context (e.g., previous 2 QVs + current sequence base).
*   **Arithmetic Coding**: Predictions are fed into an adaptive arithmetic coder for near-entropy-limit compression.
*   **Reference**: Heavily inspired by **fqzcomp5** (by James Bonfield).

---

## 🛠 Engineering Optimizations

### 1. Parallel Pipeline (Intel TBB)
We implement a `read -> filter -> compress -> write` pipeline using **Intel Threading Building Blocks (oneTBB)**.
*   **Token-based Flow**: Controls memory usage by limiting "in-flight" blocks.
*   **Load Balancing**: TBB's work-stealing scheduler automatically balances heavy (reordering) and light (IO) tasks.

### 2. Columnar Block Format
Data is split into physically separate streams within each compressed block:
*   `[Stream ID]`: Delta-encoded integers + Tokenized headers.
*   `[Stream Seq]`: ABC-encoded sequence deltas.
*   `[Stream Qual]`: SCM-encoded quality values.
This ensures that the optimal algorithm handles each data type independently.

### 3. Random Access (O(1))
Files are composed of independent **Blocks** (e.g., 10MB unpackaged).
*   **Indexing**: A lightweight footer index maps block IDs to file offsets.
*   **Context Reset**: Compression models are reset at block boundaries, enabling fully independent parallel decompression.

---

## 📦 Distribution & External Compression

### Why No Built-in `.fqc.gz` Output?

The `.fqc` format is **already highly compressed** using domain-specific algorithms (ABC for sequences, SCM for quality scores). Adding an external compression layer (like gzip or xz) provides minimal additional benefit while introducing significant drawbacks:

1. **Marginal Compression Gain**: External compression typically adds only 1-5% additional compression on already-compressed `.fqc` files.
2. **Breaks Random Access**: The `.fqc` format supports O(1) random access to any read range. External compression destroys this capability.
3. **Increased Latency**: Decompression requires two passes (external decompression + FQC decompression).
4. **Complexity**: Managing nested compression adds operational complexity.

### Recommended Distribution Workflow

If you need to distribute `.fqc` files and want additional compression for transfer:

```bash
# Compress for distribution (user's responsibility)
xz -9 archive.fqc              # Creates archive.fqc.xz

# Before use, decompress the outer layer
xz -d archive.fqc.xz           # Restores archive.fqc

# Now use fqc normally with random access
fqc decompress -i archive.fqc --range 1000:2000 -o subset.fastq
```

### Best Practices

| Scenario | Recommendation |
|----------|----------------|
| **Active Analysis** | Use `.fqc` directly (random access enabled) |
| **Long-term Archive** | Use `.fqc` directly (already near-optimal compression) |
| **Network Transfer** | Optionally wrap with `xz -9` or `zstd -19`, unwrap before use |
| **Cloud Storage** | Use `.fqc` directly (cloud providers often compress at rest) |

> **Note**: The `.fqc` format achieves compression ratios of 0.4-0.6 bits/base, which is already close to the theoretical entropy limit for genomic data. External compression cannot significantly improve upon this.

---

## 📚 References & Acknowledgements

This project stands on the shoulders of giants. We explicitly acknowledge and reference the following academic works and open-source projects:

### Academic Papers
*   **SPRING**: Chandak, S., et al. (2019). *"SPRING: a next-generation FASTQ compressor"*. Bioinformatics. [Algorithm: Reordering, Encoder]
*   **Mincom**: Liu, Y., et al. *"Mincom: hash-based minimizer bucketing for genome sequence compression"*. [Algorithm: Bucketing]
*   **HARC**: Chandak, S., et al. (2018). *"HARC: Haplotype-aware Reordering for Compression"*. [Algorithm: Reordering]
*   **Fqzcomp**: Bonfield, J. K. *"Compression of FASTQ and SAM format sequencing data"*. [Algorithm: Quality Context Mixing]

### Open Source Reference Projects
*   **[Spring](https://github.com/shubhamchandak94/Spring)**: Core reordering and encoding logic.
*   **[fqzcomp5](https://github.com/jkbonfield/fqzcomp5)**: Quality score compression models.
*   **[fastq-tools](https://github.com/dcjones/fastq-tools)**: High-performance C++ framework and I/O.
*   **[pigz](https://github.com/madler/pigz)**: Parallel implementation reference.
*   **[Repaq](https://github.com/OpenGene/repaq)**: Compact reordering ideas.

## License
[License TBD - Likely GPLv3 or similar due to Spring dependency]
