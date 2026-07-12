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
- Tracked benchmarks with peer-tool comparison (see [Performance](#-performance))

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

Compression ratio and throughput measured on tracked benchmark workloads
(results in `benchmark_v2/results/`). fqc runs with 4 threads; peer tools run
single-threaded with their defaults.

### Compression Ratio

| Workload | fqc | gzip | xz | bzip2 |
|----------|-----|------|----|-------|
| small20k-single | 4.04× | 3.25× | 4.05× | 3.98× |
| big100k-single | 4.86× | 3.89× | 5.16× | 4.88× |
| small20k-paired | 3.94× | — | — | — |
| big100k-paired | 4.68× | — | — | — |

fqc holds the competitive compression-ratio tier, ahead of gzip and on par with
xz and bzip2 on single-end data.

### Throughput (big100k-single, 4 threads)

| Tool | Compress (MiB/s) | Decompress (MiB/s) |
|------|------------------|---------------------|
| fqc | 0.10 | 2.12 |
| gzip | 4.24 | 79.07 |
| xz | 0.24 | 27.56 |
| bzip2 | 7.93 | 4.27 |

Throughput is the known short board: fqc trades speed for ratio. The current
pipeline is not yet tuned for throughput; compression spends most of its time
in the ABC reordering and consensus stages.

### Reproduce

```bash
# Tracked benchmark (dataset-driven, auto-prepares data, writes to benchmark_v2/results/)
./scripts/benchmark.sh --dataset err091571-local-supported --build --prepare --quick

# Local workload comparison via the v2 CLI
./scripts/benchmark_v2.sh prepare \
  --workload big100k-single --data-root benchmark_v2/data \
  --output-dir benchmark_v2/data
./scripts/benchmark_v2.sh run \
  --workload big100k-single --data-root benchmark_v2/data \
  --tools fqc,gzip,xz,bzip2 --threads 4 --runs 1 \
  --json benchmark_v2/results/big100k-single.json \
  --report benchmark_v2/results/big100k-single.md
```

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
  --tools fqc,gzip,xz,bzip2 \
  --quick
```

For workload-level local comparison, use `./scripts/benchmark_v2.sh run` (see
[Performance → Reproduce](#-performance)).

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
