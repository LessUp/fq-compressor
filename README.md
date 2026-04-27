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
fqc compress -i reads_1.fastq -2 reads_2.fastq \
  -o paired.fqc --paired

# Archive inspection
fqc info reads.fqc
```

---

## 📊 Proof Points

- **3.97× compression** on Illumina data with **O(1) random access**
- **11.9 MB/s compression** and **62.3 MB/s decompression** in multithreaded runs
- **Archive inspection and verification** via `fqc info` and `fqc verify`
- **Transparent input handling** for `.gz`, `.bz2`, and `.xz` FASTQ inputs

For deeper benchmark data, algorithm notes, and file-format details, use the maintained docs rather
than this repository entry page.

---

## 📚 Documentation & Project Surfaces

| Surface | Role |
|---------|------|
| [📖 GitHub Pages](https://lessup.github.io/fq-compressor/) | Public landing page and EN/ZH entry paths |
| [🚀 English docs](https://lessup.github.io/fq-compressor/en/) | Installation, CLI, architecture, performance |
| [简体中文文档](https://lessup.github.io/fq-compressor/zh/) | 安装、命令参考、架构说明、性能基准 |
| [📦 Releases](https://github.com/LessUp/fq-compressor/releases) | Prebuilt binaries |
| [🤝 Contributing Guide](https://lessup.github.io/fq-compressor/en/development/contributing) | Closeout-oriented development workflow |

---

## 🛠️ Development

`fq-compressor` is **OpenSpec-driven** and currently in **closeout mode**. The expected local loop is:

```bash
./scripts/dev/preflight.sh
openspec list --json
./scripts/build.sh clang-debug
./scripts/lint.sh format-check
./scripts/test.sh clang-debug
```

Use `openspec/specs/` as the living requirements source. Treat `specs/` and `docs/archive/` as
historical reference only.

---

## 🤝 Contributing

Focused contributions are welcome, especially for:

- documentation cleanup and ownership tightening
- evidence-driven bug fixes with regression coverage
- workflow and tooling simplification
- archive-readiness polish

See the [Contributing Guide](https://lessup.github.io/fq-compressor/en/development/contributing) for
the repository workflow.

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
