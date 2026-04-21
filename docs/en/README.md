---
title: fq-compressor Documentation
description: Official documentation for fq-compressor - a high-performance FASTQ compression tool
version: 0.2.0
last_updated: 2026-04-22
language: en
---

# fq-compressor Documentation

> 🧬 High-performance FASTQ compression for the sequencing era

Welcome to the official documentation for **fq-compressor**, a next-generation FASTQ compression tool that combines state-of-the-art compression algorithms with industrial-grade engineering.

## 📚 Documentation Structure

| Section | Description |
|---------|-------------|
| [Getting Started](./getting-started/) | Installation, quick start, and CLI usage |
| [Core Concepts](./core-concepts/) | Architecture, algorithms, and FQC format |
| [Development](./development/) | Contributing, coding standards, and build guides |
| [Performance](./performance/) | Benchmarking and optimization |
| [Reference](./reference/) | Changelog, FAQ, and API reference |

## 🚀 Quick Links

- **[Installation Guide](./getting-started/installation.md)** - Get started in minutes
- **[Quick Start](./getting-started/quickstart.md)** - Your first compression
- **[CLI Reference](./getting-started/cli-usage.md)** - Complete command reference

## 🌐 Language

- [English (current)](./README.md)
- [简体中文](../zh/README.md)

## 📖 Getting Started

```bash
# Quick installation (Linux x86_64)
wget https://github.com/LessUp/fq-compressor/releases/download/v0.2.0/fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.2.0-linux-x86_64-musl/fqc /usr/local/bin/

# Compress your first FASTQ file
fqc compress -i input.fastq -o output.fqc

# Verify and decompress
fqc verify output.fqc
fqc decompress -i output.fqc -o restored.fastq
```

## 🏆 Key Features

| Feature | Description | Benefits |
|---------|-------------|----------|
| **ABC Algorithm** | Assembly-based Compression for sequences | Near-entropy compression ratios |
| **SCM Quality** | Statistical Context Mixing for quality scores | Optimal quality compression |
| **Parallel Processing** | Intel oneTBB pipeline | Maximize multicore utilization |
| **Random Access** | Block-based format | O(1) partial decompression |
| **C++23 Modern** | Latest C++ features | High performance, maintainable code |

## 📊 Performance Highlights

Comparison on 2.27M Illumina reads (511 MB uncompressed):

| Metric | GCC | Clang |
|--------|-----|-------|
| Compression Speed | 11.30 MB/s | 11.90 MB/s |
| Decompression Speed | 60.10 MB/s | 62.30 MB/s |
| Compression Ratio | 3.97x | 3.97x |

See the [Benchmark Guide](./performance/benchmark.md) for detailed results.

## 🔧 Technical Specifications

- **Language**: C++23
- **Minimum Compilers**: GCC 14+, Clang 18+
- **Build System**: CMake 3.28+ with Conan 2.x
- **Parallel Library**: Intel oneTBB
- **Logging**: Quill
- **CLI**: CLI11

## 📝 License

Project-authored code is licensed under the [MIT License](../../LICENSE).

The vendored code under `vendor/spring-core/` remains under Spring's original license.

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](./development/contributing.md) for details.

---

**Version**: 0.2.0 | **Last Updated**: 2026-04-22
