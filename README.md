# fq-compressor

<p align="center">
  <a href="https://lessup.github.io/fq-compressor/"><img src="https://img.shields.io/badge/Docs-GitBook-blue?logo=gitbook" alt="Docs"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License"></a>
  <a href="https://en.cppreference.com/w/cpp/23"><img src="https://img.shields.io/badge/Standard-C%2B%2B23-blue.svg" alt="C++23"></a>
  <a href="https://github.com/LessUp/fq-compressor/releases"><img src="https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey.svg" alt="Platform"></a>
  <a href="https://github.com/LessUp/fq-compressor/releases/latest"><img src="https://img.shields.io/github/v/release/LessUp/fq-compressor?include_prereleases" alt="Release"></a>
</p>

<p align="center">
  <b>High-performance FASTQ compression for the sequencing era</b>
</p>

<p align="center">
  English | <a href="README.zh-CN.md">简体中文</a> | <a href="https://github.com/LessUp/fq-compressor-rust">Rust Implementation</a>
</p>

---

## 🚀 Quick Start

### Installation

```bash
# Linux / macOS (x86_64)
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.1.0-linux-x86_64-musl/fqc /usr/local/bin/
```

See [Installation Guide](docs/en/getting-started/installation.md) for all platforms.

### Basic Usage

```bash
# Compress
fqc compress -i reads.fastq -o reads.fqc

# Verify & Decompress
fqc verify reads.fqc
fqc decompress -i reads.fqc -o restored.fastq

# Partial extraction (random access)
fqc decompress -i reads.fqc --range 1000:2000 -o subset.fastq
```

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| 🧬 **ABC Algorithm** | Assembly-based Compression achieves 3-5x ratio via minimizer bucketing and delta encoding |
| 📊 **SCM Quality** | Statistical Context Mixing for optimal quality score compression |
| ⚡ **Parallel Processing** | Intel oneTBB pipeline scales to all available cores |
| 🎯 **Random Access** | O(1) block-level access for partial extraction without full decompression |
| 🔧 **Modern C++23** | Type-safe, high-performance, maintainable codebase |
| 📦 **Multiple Formats** | Transparent read/write of .gz, .bz2, .xz files |

---

## 📊 Performance

Benchmark: 2.27M Illumina reads (511 MB uncompressed)

| Metric | GCC 15.2 | Clang 21 |
|--------|----------|----------|
| **Compression** | 11.30 MB/s | 11.90 MB/s |
| **Decompression** | 60.10 MB/s | 62.30 MB/s |
| **Ratio** | 3.97x | 3.97x |

*Platform: Intel i7-9700 (8 cores), Linux 6.8*

See [Benchmarks](docs/en/performance/benchmark.md) for detailed results.

---

## 🔬 Core Algorithms

### Assembly-based Compression (ABC)

Treats reads as genome fragments rather than independent strings:

1. **Minimizer Bucketing** - Groups reads by shared k-mer signatures
2. **TSP Reordering** - Orders reads to maximize neighbor similarity
3. **Consensus Generation** - Builds local consensus per bucket
4. **Delta Encoding** - Stores edits from consensus

### Statistical Context Mixing (SCM)

High-order context modeling for quality scores:

- Context: previous QVs + current base + position
- Adaptive arithmetic coding
- Near-entropy compression

---

## 📚 Documentation

| Resource | Description |
|----------|-------------|
| [📖 Full Documentation](https://lessup.github.io/fq-compressor/) | GitBook site (bilingual) |
| [🚀 Quick Start](docs/en/getting-started/quickstart.md) | Get started in 5 minutes |
| [📦 Installation](docs/en/getting-started/installation.md) | All platforms and methods |
| [⌨️ CLI Reference](docs/en/getting-started/cli-usage.md) | Complete command reference |
| [🏗️ Architecture](docs/en/core-concepts/architecture.md) | High-level design |
| [🧬 Algorithms](docs/en/core-concepts/algorithms.md) | Compression algorithms deep-dive |
| [📋 Format Spec](docs/en/core-concepts/fqc-format.md) | FQC archive format spec |

---

## 🛠️ Building from Source

### Prerequisites

- GCC 14+ or Clang 18+
- CMake 3.28+
- Conan 2.x

### Build

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor

conan install . --build=missing -of=build/clang-release \
    -s build_type=Release -s compiler.cppstd=23

cmake --preset clang-release
cmake --build --preset clang-release -j$(nproc)

# Binary at: build/clang-release/src/fqc
```

See [Build Guide](docs/en/development/build-from-source.md) for details.

---

## 🐳 Docker

```bash
docker build -f docker/Dockerfile -t fq-compressor .
docker run --rm -v $(pwd):/data fq-compressor compress -i /data/reads.fastq -o /data/reads.fqc
```

---

## 🧪 Testing

```bash
# Run all tests
./scripts/test.sh clang-release

# Run specific test
cmake --build --preset clang-release --target memory_budget_test
build/clang-release/tests/memory_budget_test
```

---

## 🗺️ Roadmap

- [ ] Long-read optimization (PacBio/Nanopore)
- [ ] Streaming compression mode
- [ ] Additional lossy quality modes
- [ ] AWS S3/GCS native integration
- [ ] Python bindings

---

## 🤝 Contributing

Contributions are welcome! Please see our [Contributing Guide](docs/en/development/contributing.md).

---

## 📄 License

- **Project Code**: MIT License - see [LICENSE](LICENSE)
- `vendor/spring-core/`: Spring's original research license (not MIT)

---

## 🙏 Acknowledgments

- **Spring** ([Chandak et al., 2019](https://doi.org/10.1101/gr.234583.119)) - ABC algorithm inspiration
- **fqzcomp5** (Bonfield) - Quality compression reference
- **Intel oneTBB** - Parallel computing framework

---

<p align="center">
  <a href="https://github.com/LessUp/fq-compressor/releases">Releases</a> •
  <a href="https://lessup.github.io/fq-compressor/">Documentation</a> •
  <a href="CHANGELOG.md">Changelog</a>
</p>
