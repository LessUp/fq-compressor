# fq-compressor

<p align="center">
  <b>A high-performance FASTQ compressor with O(1) random access</b>
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

- [Overview](#-overview)
- [Installation](#-installation)
- [Usage](#-usage)
- [Performance](#-performance)
- [Documentation](#-documentation)
- [Development](#-development)
- [Contributing](#-contributing)
- [License](#-license)
- [Acknowledgments](#-acknowledgments)

---

## 🎯 Overview

**fq-compressor** is a FASTQ compression tool written in C++23. It combines **Assembly-based Compression (ABC)** with **Statistical Context Mixing (SCM)** to reach near-entropy compression ratios while keeping **O(1) random access** into the compressed archive.

Notable properties:
- Random access to reads without full decompression
- Intel oneTBB parallel pipeline
- Reads `.gz` FASTQ inputs directly, no pre-decompression
- Reproducible benchmarks via `./scripts/benchmark.sh` (tracked) and `./scripts/benchmark_v2.sh` (local comparison)

---

## 📦 Installation

### Build from Source (Recommended)

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor

# Conan install + CMake build in one step
./scripts/build.sh gcc-release

# Binary: build/gcc-release/src/fqc
```

**Other presets:** `clang-debug` (development), `clang-release`, `gcc-debug`, `clang-asan`, `clang-tsan`. See [AGENTS.md](AGENTS.md) for the full list.

**Requirements:** GCC 14+ or Clang 18+, CMake 3.28+, Conan 2.x

### Pre-built Binaries

Pre-built binaries for Linux (glibc/musl, x86_64/aarch64) and macOS (x86_64/arm64) are available on the [releases page](https://github.com/LessUp/fq-compressor/releases/latest).

> **Note:** v0.2.0 was released without binary assets. Use a newer release or build from source.

---

## 🚀 Usage

### Compress and Decompress

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
# Random access — extract reads 1000-2000
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

## 📊 Performance

- **3.97× compression ratio** on Illumina data
- **11.9 MB/s** compression, **62.3 MB/s** decompression (multithreaded)
- **O(1) random access** without full decompression
- Archive inspection and verification via `fqc info` and `fqc verify`
- Direct `.gz` FASTQ input handling

These figures come from the tracked benchmark. To reproduce them and compare against peer tools, run `./scripts/benchmark.sh` (results written to `benchmark_v2/results/`).

For architecture and file-format details, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## 📚 Documentation

| Document | Contents |
|----------|----------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design, pipeline, format, random access |
| [AGENTS.md](AGENTS.md) | Build commands, code style, development workflow |
| [Releases](https://github.com/LessUp/fq-compressor/releases) | Pre-built binaries |

---

## 🛠️ Development

`fq-compressor` is in **closeout mode**. The development workflow is intentionally small:

```bash
./scripts/build.sh clang-debug
./scripts/lint.sh format-check
./scripts/test.sh clang-debug
```

### Release Checks

Run the acceptance script before tagging a release:

```bash
./scripts/acceptance.sh
```

To regenerate the tracked benchmark:

```bash
./scripts/benchmark.sh \
  --dataset err091571-local-supported \
  --build \
  --tools fqc,gzip,spring \
  --quick
```

`./scripts/benchmark_v2.sh` runs a lighter local comparison for quick checks.

See [AGENTS.md](AGENTS.md) for full project rules and architecture.

---

## 🤝 Contributing

Contributions are welcome, particularly:

- documentation cleanup
- bug fixes with regression tests
- workflow and tooling simplification

See [AGENTS.md](AGENTS.md) for the repository workflow and coding standards.

---

## 📄 License

Project code is MIT-licensed — see [LICENSE](LICENSE).

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
