# fq-compressor

<p align="center">
  <b>High-performance FASTQ compression for the sequencing era</b>
</p>

<p align="center">
  <a href="https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml">
    <img src="https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml/badge.svg" alt="CI Status">
  </a>
  <a href="https://github.com/LessUp/fq-compressor/actions/workflows/quality.yml">
    <img src="https://github.com/LessUp/fq-compressor/actions/workflows/quality.yml/badge.svg" alt="Code Quality">
  </a>
  <a href="https://github.com/LessUp/fq-compressor/releases/latest">
    <img src="https://img.shields.io/github/v/release/LessUp/fq-compressor?include_prereleases&label=Release" alt="Latest Release">
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License">
  </a>
  <a href="https://en.cppreference.com/w/cpp/23">
    <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  </a>
  <a href="https://lessup.github.io/fq-compressor/">
    <img src="https://img.shields.io/badge/Docs-Online-blue?logo=gitbook" alt="Documentation">
  </a>
</p>

<p align="center">
  <a href="README.md">English</a> •
  <a href="README.zh-CN.md">简体中文</a> •
  <a href="https://github.com/LessUp/fq-compressor-rust">Rust Implementation</a>
</p>

---

## 🎯 What is fq-compressor?

**fq-compressor** is a high-performance FASTQ compression tool that leverages **Assembly-based Compression (ABC)** and **Statistical Context Mixing (SCM)** to achieve near-entropy compression ratios while maintaining **O(1) random access** to compressed data.

**Key highlights:**
- 🧬 **3.97× compression ratio** on Illumina data
- ⚡ **11.9 MB/s** compression, **62.3 MB/s** decompression (multithreaded)
- 🎯 **Random access** without full decompression
- 🚀 **Intel oneTBB** parallel pipeline
- 📦 **Transparent support** for .gz, .bz2, .xz inputs

---

## 📦 Quick Installation

### Pre-built Binaries (Recommended)

**Linux (x86_64, static binary):**
```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.2.0/fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.2.0-linux-x86_64-musl/fqc /usr/local/bin/
```

**macOS (Homebrew):**
```bash
# Coming soon
```

**Other platforms:** See [Installation Guide](https://lessup.github.io/fq-compressor/en/getting-started/installation)

### Build from Source

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor

# Install dependencies via Conan
conan install . --build=missing -of=build/gcc-release \
    -s build_type=Release -s compiler.cppstd=23

# Build
cmake --preset gcc-release
cmake --build --preset gcc-release -j$(nproc)

# Binary: build/gcc-release/src/fqc
```

**Requirements:** GCC 14+ or Clang 18+, CMake 3.28+, Conan 2.x

---

## 🚀 Basic Usage

### Compress & Decompress

```bash
# Compress FASTQ to FQC format
fqc compress -i reads.fastq -o reads.fqc

# Verify archive integrity
fqc verify reads.fqc

# Full decompression
fqc decompress -i reads.fqc -o restored.fastq
```

### Advanced Features

```bash
# Random access - extract reads 1000-2000
fqc decompress -i reads.fqc --range 1000:2000 -o subset.fastq

# Multi-threaded compression (8 threads)
fqc compress -i reads.fastq -o reads.fqc -t 8 -v

# Paired-end data
fqc compress -i reads_1.fastq -i2 reads_2.fastq \
  -o paired.fqc --paired

# Archive inspection
fqc info reads.fqc
```

---

## 📊 Performance Benchmarks

**Test dataset:** 2.27M Illumina reads (511 MB uncompressed)  
**Hardware:** Intel Core i7-9700 @ 3.00GHz (8 cores)

| Compiler | Compression | Decompression | Ratio |
|----------|-------------|---------------|-------|
| **GCC 15.2** | 11.30 MB/s | 60.10 MB/s | **3.97×** |
| **Clang 21** | 11.90 MB/s | 62.30 MB/s | **3.97×** |

### Comparison with Other Tools

| Tool | Ratio | Compress | Decompress | Parallel | Random Access |
|------|-------|----------|------------|----------|---------------|
| **fq-compressor** | **3.97×** | 11.9 MB/s | 62 MB/s | ✓ | ✓ |
| Spring | 4.5× | 3 MB/s | 25 MB/s | ✓ | ✗ |
| fqzcomp5 | 3.5× | 8 MB/s | 35 MB/s | ✗ | ✗ |
| gzip | 2.1× | 45 MB/s | 180 MB/s | ✗ | ✗ |
| zstd | 2.5× | 220 MB/s | 500 MB/s | ✗ | ✗ |

*See [detailed benchmarks](https://lessup.github.io/fq-compressor/en/performance/benchmark) for more comparisons.*

---

## 🔬 Core Algorithms

### Assembly-based Compression (ABC)

fq-compressor treats reads as **genome fragments** rather than independent strings:

1. **Minimizer Bucketing** — Groups reads by shared k-mer signatures
2. **TSP Reordering** — Maximizes neighbor similarity via Traveling Salesman optimization
3. **Consensus Generation** — Builds local consensus per bucket
4. **Delta Encoding** — Stores only edits from consensus sequence

### Statistical Context Mixing (SCM)

For quality scores, fq-compressor uses:

- **Context:** Previous QVs + current base + position
- **Prediction:** High-order context modeling
- **Coding:** Adaptive arithmetic coding
- **Result:** Near-entropy compression for quality data

---

## 📚 Documentation

| Resource | Description |
|----------|-------------|
| [📖 Online Docs](https://lessup.github.io/fq-compressor/) | Full documentation (EN/ZH) |
| [🚀 Quick Start](https://lessup.github.io/fq-compressor/en/getting-started/quickstart) | Get started in 5 minutes |
| [⌨️ CLI Reference](https://lessup.github.io/fq-compressor/en/getting-started/cli-usage) | Complete command reference |
| [🏗️ Architecture](https://lessup.github.io/fq-compressor/en/core-concepts/architecture) | High-level design |
| [🧬 Algorithms](https://lessup.github.io/fq-compressor/en/core-concepts/algorithms) | Compression algorithms |
| [📋 FQC Format](https://lessup.github.io/fq-compressor/en/core-concepts/fqc-format) | Archive format specification |
| [🤝 Contributing](https://lessup.github.io/fq-compressor/en/development/contributing) | Developer guide |

---

## 🛠️ Development

### Quick Build

```bash
# Debug build with all features
./scripts/build.sh gcc-debug

# Release build
./scripts/build.sh gcc-release

# Run tests
./scripts/test.sh gcc-release
```

### Build Presets

| Preset | Compiler | Mode | Sanitizers |
|--------|----------|------|------------|
| `gcc-debug` | GCC 15.2 | Debug | None |
| `gcc-release` | GCC 15.2 | Release | None |
| `clang-debug` | Clang 21 | Debug | None |
| `clang-asan` | Clang 21 | Debug | AddressSanitizer |
| `clang-tsan` | Clang 21 | Debug | ThreadSanitizer |
| `coverage` | GCC 15.2 | Debug | Coverage |

### Code Quality

```bash
# Format code
./scripts/lint.sh format

# Run linters
./scripts/lint.sh lint clang-debug

# Check all
./scripts/lint.sh all clang-debug
```

---

## 🐳 Docker Usage

```bash
# Build image
docker build -f docker/Dockerfile -t fq-compressor .

# Compress data
docker run --rm -v $(pwd):/data \
  fq-compressor compress -i /data/reads.fastq -o /data/reads.fqc

# Decompress data
docker run --rm -v $(pwd):/data \
  fq-compressor decompress -i /data/reads.fqc -o /data/restored.fastq
```

---

## 🗺️ Roadmap

- [x] v0.1.0 - Initial release with ABC + SCM
- [x] v0.2.0 - Paired-end optimization, long-read support
- [ ] Streaming compression mode
- [ ] Additional lossy quality modes
- [ ] AWS S3/GCS native integration
- [ ] Python bindings

---

## 🤝 Contributing

Contributions are welcome! Please see our [Contributing Guide](https://lessup.github.io/fq-compressor/en/development/contributing) for details.

**Ways to contribute:**
- 🐛 Report bugs and issues
- 💡 Suggest new features
- 📝 Improve documentation
- 🔧 Submit pull requests
- 🧪 Test on different datasets

---

## 📄 License

- **Project Code:** MIT License — see [LICENSE](LICENSE)
- **vendor/spring-core/:** Spring's original research license (not MIT)

---

## 🙏 Acknowledgments

- **Spring** ([Chandak et al., 2019](https://doi.org/10.1101/gr.234583.119)) — ABC algorithm inspiration
- **fqzcomp5** (Bonfield) — Quality compression reference
- **Intel oneTBB** — Parallel computing framework
- **Contributors** — Everyone who has helped improve this project

---

<p align="center">
  <a href="https://github.com/LessUp/fq-compressor/releases">Releases</a> •
  <a href="https://lessup.github.io/fq-compressor/">Documentation</a> •
  <a href="CHANGELOG.md">Changelog</a> •
  <a href="https://github.com/LessUp/fq-compressor/discussions">Discussions</a>
</p>
