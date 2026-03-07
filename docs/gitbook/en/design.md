# Architecture

## System Overview

fq-compressor is organized into four major layers:

```
┌─────────────────────────────────────────────────┐
│                  CLI Layer (CLI11)               │
├──────────┬──────────┬──────────┬────────────────┤
│ compress │decompress│   info   │     verify     │
├──────────┴──────────┴──────────┴────────────────┤
│            Parallel Pipeline (oneTBB)            │
│         read → compress/decompress → write       │
├──────────┬──────────┬───────────────────────────┤
│ Sequence │ Quality  │      Identifier           │
│  (ABC)   │  (SCM)   │  (Token + Delta)          │
├──────────┴──────────┴───────────────────────────┤
│        FQC Format Layer (Block I/O)              │
│     Header │ Blocks │ Reorder Map │ Footer       │
├─────────────────────────────────────────────────┤
│        Infrastructure                            │
│  Error Handling │ Logging │ Memory Management    │
└─────────────────────────────────────────────────┘
```

## Module Status

| Module | Responsibility | Status |
|--------|---------------|--------|
| **FASTQ Parser** | Parse and validate FASTQ format | ✅ Complete |
| **FQC Format** | Custom compressed format with random access | ✅ Complete |
| **Sequence Compression (ABC)** | Spring-style reordering and consensus encoding | ⚠️ 80% |
| **Quality Compression (SCM)** | fqzcomp5-style context mixing | ⚠️ 90% |
| **ID Compression** | Delta + Tokenization | ⚠️ 90% |
| **TBB Pipeline** | Intel oneTBB parallel framework | ⚠️ 40% |
| **CLI Framework** | CLI11 integration | ✅ Complete |
| **Error Handling** | ExitCode enum + Result system | ✅ Complete |
| **Logging** | Quill high-performance logger | ✅ Complete |
| **Memory Management** | Budget tracking and auto-chunking | ✅ Complete |

## Design Principles

1. **Columnar Stream Separation** — Each data type (sequence, quality, identifier) is compressed independently with its optimal algorithm.
2. **Block Independence** — Each block can be compressed/decompressed independently, enabling random access and parallel processing.
3. **Adaptive Strategy** — Read length classification automatically selects the best compression strategy (ABC for short reads, Zstd for long reads).
4. **Memory Budget Awareness** — Dynamic chunking adapts to available system memory.

## Source Code Layout

```
src/
├── main.cpp              # Entry point
├── cli/                  # CLI commands (compress, decompress, info, verify)
├── core/                 # Core compression algorithms
│   ├── block_compressor  # Sequence ABC engine
│   ├── quality_compressor# SCM quality engine
│   ├── id_compressor     # Identifier compression
│   └── global_analyzer   # Read analysis and classification
├── format/               # FQC file format (reader, writer, index)
├── parser/               # FASTQ parser (SE, PE, interleaved)
├── pipeline/             # TBB parallel pipeline
├── io/                   # Async I/O, compressed streams
├── memory/               # Memory budget management
└── utils/                # Error handling, logging, helpers
```

## Detailed Design

- [Core Algorithms](algorithms.md) — ABC, SCM, and tokenization details
- [FQC File Format](fqc-format.md) — Binary format specification
- [Parallel Pipeline](pipeline.md) — TBB pipeline architecture
