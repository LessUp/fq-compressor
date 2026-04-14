# fq-compressor

[![Docs](https://img.shields.io/badge/Docs-GitBook-blue?logo=gitbook)](https://lessup.github.io/fq-compressor/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/Standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey.svg)](https://github.com/LessUp/fq-compressor/releases)

English | [简体中文](README.zh-CN.md) | [Rust Version (fq-compressor-rust)](https://github.com/LessUp/fq-compressor-rust)

> **跨语言实现**：本项目同时提供 **[Rust 实现](https://github.com/LessUp/fq-compressor-rust)**，共享相同的 `.fqc` 归档格式与 ABC/SCM 压缩算法，以 Rayon + crossbeam 替代 Intel TBB 并引入异步 I/O。

**Documentation**

 - [GitBook / Docs Site](https://lessup.github.io/fq-compressor/)
 - [中文文档总览](docs/README.md)
 - [Benchmark Documentation](docs/benchmark/README.md)
 - [Latest Benchmark Report](docs/benchmark/results/report-latest.md)

> Note: FASTQ input stream support currently covers plain text, gzip (`.gz`), bzip2 (`.bz2`), and
> xz (`.xz`). Zstandard-compressed FASTQ input (`.zst`) is not supported yet.

**fq-compressor** is a high-performance, next-generation FASTQ compression tool designed for the sequencing era. It combines state-of-the-art **Assembly-based Compression (ABC)** strategies with robust industrial-grade engineering to deliver extreme compression ratios, fast parallel processing, and native random access.

## Table of Contents

- [Key Features](#-key-features)
- [Quick Start](#-quick-start)
- [Core Algorithms](#-core-algorithms-the-why-it-works)
- [Engineering Optimizations](#-engineering-optimizations)
- [Installation](#-installation)
- [Usage](#-usage)
- [Performance Benchmarking](#-performance-benchmarking)
- [Distribution & External Compression](#-distribution--external-compression)
- [References & Acknowledgements](#-references--acknowledgements)
- [License](#license)

---

## 🚀 Key Features

*   **Extreme Compression**: Approaching the theoretical limit using "Assembly-based" reordering and consensus generation (similar to Spring/Mincom).
*   **Hybrid Quality Compression**: Uses **Statistical Context Mixing (SCM)** (similar to fqzcomp5) for quality scores, balancing ratio and speed.
*   **Parallel Powerhouse**: Built on **Intel oneTBB** with a scalable Producer-Consumer pipeline, maximizing multicore utilization.
*   **Random Access**: Native **Block-based** format (like BGZF) allows instant access to any part of the file (Scatter-Gather ready).
*   **Standard Compliant**: Written in **C++23**, using Modern CMake and efficient I/O.

---

## 🎯 Quick Start

### Download Pre-built Binary

```bash
# Download from GitHub Releases
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.1.0-linux-x86_64-musl/fqc /usr/local/bin/
```

### Basic Usage

```bash
# Compress FASTQ file
fqc compress -i input.fastq -o output.fqc

# Decompress
fqc decompress -i output.fqc -o restored.fastq

# View archive information
fqc info output.fqc

# Verify integrity
fqc verify output.fqc

# Partial decompression (random access)
fqc decompress -i output.fqc --range 1000:2000 -o subset.fastq
```

---

## 🔬 Core Algorithms (The "Why it works")

> **Technical Deep Dive**: This section explains the theoretical foundations of our compression approach.

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
2. **Breaks Random Access**: The `.fqc` format supports O(1) random access to any part of the file (Scatter-Gather ready). External compression destroys this capability.
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

## 🔧 Installation

### Prerequisites
- C++23 compatible compiler (GCC 13+, Clang 16+)
- CMake 3.20+
- Conan 2.x package manager

### Option 1: Pre-built Binary (Recommended)

Download from [GitHub Releases](https://github.com/LessUp/fq-compressor/releases):

| Platform | Architecture | File | Notes |
|----------|--------------|------|-------|
| Linux | x86_64 | `fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz` | **Static linking, runs anywhere** |
| Linux | x86_64 | `fq-compressor-v0.1.0-linux-x86_64-glibc.tar.gz` | Dynamic linking (glibc) |
| Linux | aarch64 | `fq-compressor-v0.1.0-linux-aarch64-musl.tar.gz` | **Static linking, runs anywhere** |
| macOS | x86_64 | `fq-compressor-v0.1.0-macos-x86_64.tar.gz` | Intel Mac |
| macOS | arm64 | `fq-compressor-v0.1.0-macos-arm64.tar.gz` | Apple Silicon |

### Option 2: Build from Source

```bash
# Clone the repository
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor

# Install dependencies
conan install . --build=missing -of=build

# Build
cmake --preset release
cmake --build build/build/Release -j$(nproc)

# Run tests
ctest --test-dir build/build/Release
```

The binary will be at `build/build/Release/bin/fqc`.

### Option 3: Docker

```bash
# Build image
docker build -f docker/Dockerfile -t fq-compressor .

# Run compression
docker run --rm -v $(pwd):/data fq-compressor compress -i /data/reads.fastq -o /data/reads.fqc

# Run decompression
docker run --rm -v $(pwd):/data fq-compressor decompress -i /data/reads.fqc -o /data/reads.fastq
```

---

## 📖 Usage

### Compression

```bash
# Basic compression
fqc compress -i input.fastq -o output.fqc

# With custom threads and memory limit
fqc compress -i input.fastq -o output.fqc --threads 8 --memory-limit 4096

# Paired-end mode (interleaved)
fqc compress -i paired.fastq -o paired.fqc --paired

# Paired-end mode (separate files)
fqc compress -i reads_1.fastq -i2 reads_2.fastq -o paired.fqc --paired
```

### Decompression

```bash
# Full decompression
fqc decompress -i output.fqc -o restored.fastq

# Partial decompression (random access)
fqc decompress -i output.fqc --range 1000:2000 -o subset.fastq

# Header-only extraction
fqc decompress -i output.fqc --header-only -o headers.txt
```

### Information & Verification

```bash
# View archive metadata
fqc info output.fqc

# Verify integrity
fqc verify output.fqc
```

---

## Documentation Map

- **Chinese project docs**: [docs/README.md](docs/README.md)
- **GitBook (bilingual)**: [https://lessup.github.io/fq-compressor/](https://lessup.github.io/fq-compressor/)
- **Specs and design notes**: [docs/specs/README.md](docs/specs/README.md)
- **Research notes**: [docs/research/](docs/research/)

## 📊 Performance Benchmarking

fq-compressor includes a comprehensive benchmarking framework for evaluating compiler optimizations (GCC vs Clang) and multi-dimensional performance analysis (compression speed, decompression speed, compression ratio, memory usage, and parallel scalability).

### Quick Start

```bash
# Build both GCC and Clang Release versions
./scripts/build.sh gcc-release 4
./scripts/build.sh clang-release 4

# Run compiler performance comparison with visualization
python3 benchmark/compiler_benchmark.py \
  --input fq-data/E150035817_L01_1201_1.sub10.fq.gz \
  --gcc-binary build/gcc-release/src/fqc \
  --clang-binary build/clang-release/src/fqc \
  -t 1 4 8 \
  -r 3 \
  --output-dir docs/benchmark \
  --visualize
```

### Generated Reports

The benchmarking framework automatically generates:

- **benchmark-report.html** - Comprehensive HTML report with embedded visualizations
- **benchmark-report.md** - Markdown format report
- **benchmark-results.json** - Raw data in JSON format
- **PNG Charts** - Performance visualizations:
  - `compression-speed.png` - Compression throughput comparison
  - `decompression-speed.png` - Decompression throughput comparison
  - `compression-ratio.png` - Compression efficiency comparison
  - `memory-usage.png` - Peak memory usage analysis
  - `scalability.png` - Parallel scalability metrics

### Key Metrics Evaluated

| Metric | Description |
|--------|-------------|
| **Compression Speed (MB/s)** | Throughput during compression |
| **Decompression Speed (MB/s)** | Throughput during decompression |
| **Compression Ratio** | Compressed size / Original size |
| **Bits per Base** | Compressed bits per DNA base |
| **Peak Memory (MB)** | Maximum memory during operation |
| **Scalability** | Performance gains with multiple threads |
| **Parallel Efficiency** | Actual speedup / Ideal speedup |

### For Detailed Information

See [docs/benchmark/README.md](docs/benchmark/README.md) for:
- Complete benchmark framework documentation
- Usage examples for different tools
- How to interpret results
- Continuous integration setup

---

## 🧪 Testing

```bash
# Run unit tests
ctest --test-dir build/build/Release

# Run end-to-end CLI tests
./tests/e2e/test_cli.sh

# Run performance benchmarks
./tests/e2e/test_performance.sh

# Compare with Spring
./scripts/compare_spring.sh input.fastq
```

---

## 🗺️ Documentation Map

| Resource | Description |
|----------|-------------|
| [GitBook (Bilingual)](https://lessup.github.io/fq-compressor/) | Official documentation site |
| [Chinese Docs](docs/README.md) | Chinese documentation hub |
| [Specs & Design](docs/specs/README.md) | Technical specifications |
| [Research Notes](docs/research/) | Algorithm analysis and references |
| [Benchmark Results](docs/benchmark/results/report-latest.md) | Latest performance data |

---

## License
Project-authored code in this repository is licensed under the [MIT License](LICENSE).

The vendored code under [`vendor/spring-core/`](vendor/spring-core/) remains under Spring's
"Non-exclusive Research Use License for SPRING Software" and is **not** covered by the MIT
license above. The default build keeps this directory disabled in `CMakeLists.txt`, but you
should still review the third-party terms before redistributing or using that code commercially.


## 📊 Benchmark Results

**Latest benchmark**: [report-latest.md](docs/benchmark/results/report-latest.md)
**Visualization**: [charts-latest.html](docs/benchmark/results/charts-latest.html)

### Performance Summary

| Compiler | Compression | Decompression | Compression Ratio |
|----------|-------------|---------------|-------------------|
| GCC      | 11.30 MB/s | 60.10 MB/s | 3.9712x        |
| Clang    | 11.90 MB/s | 62.30 MB/s | 3.9712x        |

*Tested on Intel(R) Core(TM) i7-9700 CPU @ 3.00GHz with 8 cores, using 2.27M Illumina reads (511 MB uncompressed)*

See [docs/benchmark/](docs/benchmark/) for detailed results and historical data.
