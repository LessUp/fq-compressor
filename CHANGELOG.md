# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added

- Comprehensive documentation restructuring with bilingual support (EN/ZH)
- Professional changelog following Keep a Changelog standards
- Release notes automation for GitHub releases

---

## [0.1.0] - 2026-04-16

### Overview

First stable release of fq-compressor with complete FASTQ compression pipeline, parallel processing, and random access capabilities.

### Added

#### Core Compression Features

- **ABC (Assembly-based Compression)** for genomic sequences
  - Minimizer-based read bucketing (k=21, w=11)
  - TSP-based read reordering within buckets
  - Local consensus generation and delta encoding
  - Achieves 3.5-5x compression ratio on genomic data

- **SCM (Statistical Context Mixing)** for quality scores
  - High-order context modeling (up to order-2)
  - Adaptive arithmetic coding
  - Optional quality binning (Illumina 8-bin strategy)
  - Balances compression ratio and information preservation

- **ID Stream Compression**
  - Tokenization of read identifiers
  - Delta encoding for monotonic numeric tokens
  - Dictionary compression for common patterns

- **FQC Archive Format**
  - Block-based storage architecture (default ~10MB blocks)
  - Columnar streams (separate ID/Seq/Qual compression)
  - Native random access with O(1) block lookup
  - Checksum integrity verification at multiple levels
  - Reorder map for original-order restoration

#### Parallel Processing

- Producer-Consumer pipeline using Intel oneTBB
- Configurable thread pool with automatic core detection
- Token-based flow control for memory management
- Load balancing via work-stealing scheduler
- Thread-safe block-level parallel compression

#### Command Line Interface

- `compress` - FASTQ to FQC compression with multiple modes
- `decompress` - FQC to FASTQ with full and partial extraction
- `info` - Archive metadata inspection (text and JSON output)
- `verify` - Integrity verification
- `split-pe` - Paired-end archive splitting

#### I/O Capabilities

- Transparent input decompression (.gz, .bz2, .xz)
- Configurable output compression (zstd, zlib)
- Asynchronous I/O with prefetch buffers
- Paired-end support (interleaved and separate files)
- Memory-limited operation for large datasets

#### Engineering & Tooling

- **Build System**: Modern CMake 3.28+ with Conan 2.x
- **Language**: C++23 (GCC 14+, Clang 18+)
- **Testing**: Google Test with 34+ test cases
- **Benchmarking**: Automated performance framework with visualization
- **Containerization**: Docker and DevContainer support
- **CI/CD**: GitHub Actions for build, test, and release

#### Documentation

- Comprehensive README with quick start guide
- Technical specification documents
- Algorithm deep-dives and references
- Bilingual documentation (English/Chinese)
- GitBook-powered documentation site

### Performance

Benchmarked on 2.27M Illumina reads (511 MB uncompressed):

| Compiler | Compression Speed | Decompression Speed | Compression Ratio |
|----------|-------------------|---------------------|-------------------|
| GCC 15.2 | 11.30 MB/s | 60.10 MB/s | 3.97x |
| Clang 21 | 11.90 MB/s | 62.30 MB/s | 3.97x |

- Single-threaded performance
- Multi-threaded scaling up to 8 cores
- Peak memory usage: <2GB for 100MB+ files

### Changed

- Optimized sequence reordering algorithm for better cache locality
- Improved memory budget allocation across subsystems
- Refined CLI argument parsing for consistency

### Fixed

- Race condition in pipeline token management
- Memory leak in quality compressor context initialization
- Edge case handling for empty FASTQ files

---

## [0.1.0-alpha.3] - 2026-02-25

### Fixed

- CLI option propagation for `--threads`, `--memory-limit`, `--no-progress`
- Verbosity flag `-v` log level adjustment
- `verify` command integration with `VerifyCommand` class
- DevContainer SSH agent detection logic

### Changed

- Eliminated duplicate `QualityCompressionMode` enum, unified to `QualityMode`
- Resolved anonymous namespace type shadowing issues
- Documentation file naming standardized to kebab-case
- Shell scripts unified with `#!/usr/bin/env bash` shebang

---

## [0.1.0-alpha.2] - 2026-01-29

### Fixed

- CMake preset toolchain paths for cross-compilation
- `FastqParser::open()` gzip input support
- 21 type conversion warnings
- Switch statement exhaustiveness warnings
- Clang-specific diagnostic warnings

### Changed

- Removed `-stdlib=libc++` from Clang presets, using `libstdc++`
- Changed LTO flags from `-flto` to `-flto=thin`

---

## [0.1.0-alpha.1] - 2026-01-23

### Added

- CLion-specific DevContainer configuration
- Docker Compose with configurable host data mounts
- Additional VS Code extensions (GitLens, Git Graph, Docker)

### Changed

- DevContainer features to include `common-utils`

---

## [0.1.0-alpha.0] - 2026-01-14

### Added

- Initial project structure and architecture design
- Core compression algorithm prototypes
- Reference project analysis (8 projects)
- Pre-commit hooks configuration
- EditorConfig and formatting rules

---

## Release Notes Format

Each release includes:

1. **Binary Artifacts**
   - Linux x86_64 (static musl, dynamic glibc)
   - Linux ARM64 (static musl)
   - macOS x86_64 (Intel)
   - macOS ARM64 (Apple Silicon)

2. **Documentation**
   - Installation guide
   - Quick start tutorial
   - CLI reference
   - Performance benchmarks

3. **Verification**
   - SHA-256 checksums for all artifacts
   - GPG signatures (from v1.0.0)

---

[Unreleased]: https://github.com/LessUp/fq-compressor/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.3...v0.1.0
[0.1.0-alpha.3]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.2...v0.1.0-alpha.3
[0.1.0-alpha.2]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.1...v0.1.0-alpha.2
[0.1.0-alpha.1]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.0...v0.1.0-alpha.1
[0.1.0-alpha.0]: https://github.com/LessUp/fq-compressor/releases/tag/v0.1.0-alpha.0
