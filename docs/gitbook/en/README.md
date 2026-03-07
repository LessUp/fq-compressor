# fq-compressor

**fq-compressor** is a high-performance, next-generation FASTQ compression tool designed for the sequencing era. It combines state-of-the-art **Assembly-based Compression (ABC)** strategies with robust industrial-grade engineering to deliver extreme compression ratios, fast parallel processing, and native random access.

## Key Features

| Feature | Description |
|---------|-------------|
| **Extreme Compression** | Approaching theoretical limits using Assembly-based reordering and consensus generation |
| **Hybrid Quality Compression** | Statistical Context Mixing (SCM) for quality scores, balancing ratio and speed |
| **Parallel Powerhouse** | Built on Intel oneTBB with a scalable Producer-Consumer pipeline |
| **Random Access** | Native block-based format (like BGZF) enables instant access to any part of the file |
| **Standard Compliant** | Written in C++20, using Modern CMake, Conan 2.x, and GitHub Actions CI/CD |

## Why fq-compressor?

Traditional FASTQ compressors treat reads as independent strings and rely on general-purpose compression algorithms. **fq-compressor** takes a fundamentally different approach:

1. **Reads are fragments of a genome** — we exploit the biological redundancy by reordering and assembling reads before compression.
2. **Each data stream gets a specialized compressor** — sequences use ABC, quality scores use SCM, and identifiers use tokenization + delta encoding.
3. **The archive format is designed for real-world use** — independent blocks enable random access, parallel decompression, and fault isolation.

## Performance at a Glance

| Compiler | Compression | Decompression | Compression Ratio |
|----------|-------------|---------------|-------------------|
| GCC      | 11.30 MB/s  | 60.10 MB/s    | 3.97x             |
| Clang    | 11.90 MB/s  | 62.30 MB/s    | 3.97x             |

*Tested on Intel Core i7-9700 @ 3.00GHz (8 cores), 2.27M Illumina reads (511 MB uncompressed)*

## Get Started

- [Installation](installation.md) — build from source
- [Quick Start](quickstart.md) — compress your first FASTQ file
- [CLI Reference](cli-reference.md) — all commands and options
- [Architecture](architecture.md) — how it works under the hood
