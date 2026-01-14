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
