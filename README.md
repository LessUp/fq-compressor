# fq-compressor

<p align="center">
  <b>High-performance FASTQ compression for the sequencing era</b>
</p>

<p align="center">
  <a href="https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml">
    <img src="https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml/badge.svg" alt="CI Status">
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
</p>

<p align="center">
  <a href="README.md">English</a> •
  <a href="README.zh-CN.md">简体中文</a> •
  <a href="https://github.com/LessUp/fq-compressor-rust">Rust Implementation</a>
</p>

---

## Contents

- [What is fq-compressor?](#-what-is-fq-compressor)
- [Installation](#-installation)
- [Basic Usage](#-basic-usage)
- [Proof Points](#-proof-points)
- [Documentation](#-documentation)
- [Development](#-development)
- [Contributing](#-contributing)
- [License](#-license)
- [Acknowledgments](#-acknowledgments)

---

## 🎯 What is fq-compressor?

**fq-compressor** is a high-performance FASTQ compression tool that leverages **Assembly-based Compression (ABC)** and **Statistical Context Mixing (SCM)** to achieve near-entropy compression ratios while maintaining **O(1) random access** to compressed data.

**Key highlights:**
- 🎯 **Random access** without full decompression
- 🚀 **Intel oneTBB** parallel pipeline
- 📦 **Transparent support** for .gz inputs
- 🧪 **Evidence-first benchmarking** via `./scripts/benchmark.sh` (tracked) and `./scripts/benchmark_v2.sh` (local comparison)

---

## 📦 Installation

### Build from Source (Recommended)

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor

# Build (handles Conan install + CMake in one step)
./scripts/build.sh gcc-release

# Binary: build/gcc-release/src/fqc
```

**Other presets:** `clang-debug` (development), `clang-release`, `gcc-debug`, `clang-asan`, `clang-tsan`. See [AGENTS.md](AGENTS.md) for the full list.

**Requirements:** GCC 14+ or Clang 18+, CMake 3.28+, Conan 2.x

### Pre-built Binaries

Pre-built binaries are published on the [releases page](https://github.com/LessUp/fq-compressor/releases/latest) for Linux (glibc/musl, x86_64/aarch64) and macOS (x86_64/arm64).

> **Note:** The v0.2.0 release shipped without binary assets. Use the latest release or build from source above.

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

- **3.97× compression ratio** on Illumina data
- **11.9 MB/s** compression, **62.3 MB/s** decompression (multithreaded)
- **O(1) random access** without full decompression
- **Archive inspection and verification** via `fqc info` and `fqc verify`
- **Transparent input handling** for `.gz` FASTQ inputs

> Figures above are tracked benchmark results. For reproducible evidence and peer standing, run `./scripts/benchmark.sh` (results written to `benchmark_v2/results/`).

For deeper architecture and file-format details, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## 📚 Documentation

| Surface | Role |
|---------|------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design, pipeline, format, and random access |
| [AGENTS.md](AGENTS.md) | Build commands, code style, development workflow |
| [📦 Releases](https://github.com/LessUp/fq-compressor/releases) | Prebuilt binaries |

---

## 🛠️ Development

`fq-compressor` is in **closeout mode**. Simple development workflow:

```bash
./scripts/build.sh clang-debug
./scripts/lint.sh format-check
./scripts/test.sh clang-debug
```

### Release checks

Contributors should use the single acceptance runner:

```bash
./scripts/acceptance.sh
```

Generate reproducible tracked benchmark evidence with:

```bash
./scripts/benchmark.sh \
  --dataset err091571-local-supported \
  --build \
  --tools fqc,gzip,spring \
  --quick
```

Use `./scripts/benchmark_v2.sh` for local comparison runs and smoke-scale exploratory workloads.

See AGENTS.md for full project rules and architecture.

---

## 🤝 Contributing

Focused contributions are welcome, especially for:

- documentation cleanup and ownership tightening
- evidence-driven bug fixes with regression coverage
- workflow and tooling simplification
- archive-readiness polish

See [AGENTS.md](AGENTS.md) for the repository workflow and coding standards.

---

## 📄 License

- **Project Code:** MIT License — see [LICENSE](LICENSE)

---

## 🙏 Acknowledgments

- **Spring** ([Chandak et al., 2019](https://doi.org/10.1101/gr.234583.119)) — ABC algorithm inspiration
- **fqzcomp5** (Bonfield) — Quality compression reference
- **Intel oneTBB** — Parallel computing framework
- **Contributors** — Everyone who has helped improve this project

---

<p align="center">
  <a href="https://github.com/LessUp/fq-compressor/releases">Releases</a> •
  <a href="ARCHITECTURE.md">Architecture</a> •
  <a href="CHANGELOG.md">Changelog</a> •
  <a href="https://github.com/LessUp/fq-compressor/discussions">Discussions</a>
</p>
