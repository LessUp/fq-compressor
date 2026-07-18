# fq-compressor

**A C++23 FASTQ archiver with compress, decompress, and verify commands.**

[![CI](https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml/badge.svg)](https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml)
[![GitHub Release](https://img.shields.io/github/v/release/LessUp/fq-compressor)](https://github.com/LessUp/fq-compressor/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)
![CMake 3.28+](https://img.shields.io/badge/CMake-3.28%2B-blue.svg)
![Conan 2.x](https://img.shields.io/badge/Conan-2.x-blue.svg)

[中文](README.md) · [English](README.en.md) · [Quick Start](#quick-start) · [Architecture](ARCHITECTURE.md) · [Releases](https://github.com/LessUp/fq-compressor/releases)

## Features

* `compress`: pack single-end or paired-end FASTQ into an FQC archive.
* `decompress`: restore an FQC archive to a canonical interleaved FASTQ stream.
* `verify`: run the same full integrity checks as decompression without writing FASTQ.

## Quick Start

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor
./scripts/build.sh clang-release
./build/clang-release/src/fqc --help
```

Compress:

```bash
./build/clang-release/src/fqc compress -i reads.fastq.gz -o reads.fqc
```

Decompress:

```bash
./build/clang-release/src/fqc decompress -i reads.fqc -o restored.fastq
```

Verify:

```bash
./build/clang-release/src/fqc verify reads.fqc
```

Paired-end:

```bash
./build/clang-release/src/fqc compress \
  -i reads_R1.fastq.gz \
  -2 reads_R2.fastq.gz \
  -o paired.fqc
```

Run `./build/clang-release/src/fqc --help` for all options. The archive byte layout is in [ARCHITECTURE.md](ARCHITECTURE.md).

## Tech Stack

| Layer | Technology |
|---|---|
| Language | C++23 (GCC 14+ / Clang 18+) |
| Build | CMake 3.28+ + Ninja |
| Package | Conan 2.x |
| Compression | Zstd |
| Checksumming | xxHash |
| Testing | GoogleTest |

## Design

* Sequential frames: independent, self-checking frames; one engine handles compression, decompression, and verification.
* Compact encoding: uppercase A/C/G/T packed to 2 bits; other IUPAC symbols and lowercase bases stored as exact-position exceptions.
* Bounded memory: default 16 GiB budget, minimum 64 MiB; conservative per-frame peak estimation before allocation.
* Pipeline-friendly: supports stdin/stdout; regular-file output is staged and atomically replaced on success.
* Paired-end adjacency: R1/R2 pairs are stored together and never split across frame boundaries.
* Layered checksumming: XXH64 over the global header, every logical frame, and the footer.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full layout.

## Performance

Environment: AMD Ryzen 7 5800H (WSL2, 8C/16T), Clang 21 Release, 64 MiB memory budget, randomised synthetic data, median of 3 runs.

| Scenario | Compress | Decompress | Max RSS |
|---|---|---:|---:|
| Randomised Illumina-like 150 bp | 53.15 MiB/s | 182.40 MiB/s | 31.4 MiB |
| Randomised ONT-like 20 kbp | 55.66 MiB/s | 215.22 MiB/s | 25.5 MiB |

Note: WSL2 wall-clock numbers are noisy, so these figures are best used for relative comparison on the same machine. Full data is in [performance/INDEX.md](performance/INDEX.md).

## When to use

Good for:

* Sequential FASTQ archiving and exact recovery with bounded memory.
* A single format and single set of constraints for compression, decompression, and verification.
* Pipelines and batch workflows.

Not for:

* Random access or range extraction.
* Lossy compression or original-order reordering.
* Non-FASTQ data formats.

## Build Requirements

* C++23 compiler: **GCC 14+** or **Clang 18+**
* **CMake 3.28+**
* **Conan 2.x**
* Linux / macOS; Windows via WSL or Docker

## Quality

CI covers:

* Formatting: clang-format
* Static analysis: clang-tidy
* Builds: GCC Release, Clang Release
* Sanitizers: ASan, TSan, UBSan
* Tests: unit, integration, end-to-end

## Documentation

| Want to | Read |
|---|---|
| Build and first run | This file |
| CLI options | `./build/clang-release/src/fqc --help` |
| Archive layout | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Performance data | [performance/INDEX.md](performance/INDEX.md) |
| Changelog | [CHANGELOG.md](CHANGELOG.md) |

## License

[MIT License](LICENSE).
