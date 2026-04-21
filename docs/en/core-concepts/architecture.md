---
title: Architecture Overview
description: High-level architecture and design principles of fq-compressor
version: 0.2.0
last_updated: 2026-04-22
language: en
---

# Architecture Overview

> Understanding the high-level design and components of fq-compressor

## Design Goals

fq-compressor was designed with the following key goals:

1. **High Compression Ratio**: Approach theoretical entropy limits for genomic data
2. **Parallel Performance**: Maximize utilization of modern multi-core processors
3. **Random Access**: Enable O(1) access to any subset of reads
4. **Industrial Quality**: Production-ready error handling, logging, and memory management
5. **Maintainability**: Clean, modern C++23 code with clear separation of concerns

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        fq-compressor                            │
├─────────────────────────────────────────────────────────────────┤
│  CLI Layer    │  Command Layer   │  Pipeline Layer              │
│  ───────────  │  ─────────────   │  ─────────────               │
│  main.cpp     │  CompressCmd     │  TBB Pipeline                │
│  CLI11        │  DecompressCmd   │    ┌─────────┐               │
│               │  InfoCmd         │    │Producer │               │
│               │  VerifyCmd       │    │  Node   │               │
│               │                  │    └────┬────┘               │
│               │                  │         ▼                    │
│               │                  │    ┌─────────┐               │
│               │                  │    │Compressor│              │
│               │                  │    │  Node    │              │
│               │                  │    └────┬────┘               │
│               │                  │         ▼                    │
│               │                  │    ┌─────────┐               │
│               │                  │    │ Writer  │               │
│               │                  │    │  Node   │               │
│               │                  │    └─────────┘               │
├─────────────────────────────────────────────────────────────────┤
│  Format Layer    │  Algorithm Layer                             │
│  ─────────────   │  ─────────────                               │
│  FQCReader       │  BlockCompressor                             │
│  FQCWriter       │    ├── SequenceCompressor (ABC)              │
│  ReorderMap      │    ├── QualityCompressor (SCM)               │
│  BlockIndex      │    └── IDCompressor (Delta+Token)            │
│                  │  GlobalAnalyzer                              │
├─────────────────────────────────────────────────────────────────┤
│  I/O Layer                                                      │
│  ─────────                                                      │
│  FastqParser (plain/gz/bz2/xz)                                  │
│  CompressedOutputStream (zstd/zlib/lz4)                         │
│  Async I/O with prefetch                                        │
└─────────────────────────────────────────────────────────────────┘
```

## Component Layers

### 1. CLI Layer

The CLI layer handles command-line argument parsing and command routing.

**Key Components:**
- `main.cpp`: Entry point, global option handling
- CLI11 library: Type-safe command-line parsing

**Responsibilities:**
- Parse command-line arguments
- Validate inputs
- Dispatch to appropriate command handler
- Handle global options (threads, verbosity, etc.)

### 2. Command Layer

Implements individual CLI commands as separate classes.

**Commands:**
- `CompressCommand`: Single-pass and two-phase compression
- `DecompressCommand`: Full and partial decompression
- `InfoCommand`: Archive metadata inspection
- `VerifyCommand`: Integrity verification
- `SplitPeCommand`: Paired-end splitting

**Design Pattern:** Command pattern with unified error handling

### 3. Pipeline Layer

Parallel processing using Intel oneTBB.

**Pipeline Stages:**
1. **Producer Node**: Reads FASTQ records from input
2. **Compressor Node**: Compresses blocks in parallel
3. **Writer Node**: Writes compressed blocks to output

**Key Features:**
- Token-based flow control (limits memory usage)
- Work-stealing scheduler for load balancing
- Exception propagation from worker threads

### 4. Format Layer

FQC archive format implementation.

**Key Components:**
- `FQCWriter`: Serializes compressed blocks to FQC format
- `FQCReader`: Deserializes FQC archives
- `ReorderMap`: Manages read reordering for compression/restore
- `BlockIndex`: Enables O(1) random access

**FQC Format Features:**
- Block-based storage (typically 10MB uncompressed)
- Columnar streams (separate ID, sequence, quality streams)
- Checksums for integrity verification
- Reorder map for original order restoration

### 5. Algorithm Layer

Core compression algorithms.

#### Sequence Compression (ABC)
- `GlobalAnalyzer`: Minimizer-based bucketing, read reordering
- `SequenceCompressor`: Delta encoding from consensus

#### Quality Compression (SCM)
- `QualityCompressor`: Statistical context mixing
- `ArithmeticCoder`: High-precision entropy coding

#### ID Compression
- `IDCompressor`: Tokenization and delta encoding

### 6. I/O Layer

High-performance I/O with format abstraction.

**Features:**
- Transparent decompression of input files (.gz, .bz2, .xz)
- Block-based compressed output (zstd, zlib)
- Asynchronous I/O with prefetch
- Buffered writing

## Data Flow

### Compression Flow

```
FASTQ Input
    │
    ▼
┌─────────────┐
│ FastqParser │ (transparent decompression)
└──────┬──────┘
       │
       ▼
┌───────────────┐     ┌───────────────────┐
│GlobalAnalyzer │────▶│ Reorder Map       │
│ (bucketing)   │     │ (saved to archive)│
└───────┬───────┘     └───────────────────┘
        │
        ▼
┌───────────────┐
│ BlockBuilder  │ (group into blocks)
└───────┬───────┘
        │
        ▼
┌───────────────────────────────────────┐
│ BlockCompressor                       │
│   ├── IDCompressor (delta+token)     │
│   ├── SequenceCompressor (ABC)       │
│   └── QualityCompressor (SCM)        │
└───────────────┬───────────────────────┘
                │
                ▼
┌─────────────┐
│ FQCWriter   │ (serialize with index)
└──────┬──────┘
       │
       ▼
    .fqc File
```

### Decompression Flow

```
.fqc File
    │
    ▼
┌─────────────┐
│ FQCReader   │ (parse header, load index)
└──────┬──────┘
       │
       ▼
┌───────────────────────────────────────┐
│ BlockDecompressor                     │
│   ├── IDDecompressor                 │
│   ├── SequenceDecompressor           │
│   └── QualityDecompressor            │
└───────────────┬───────────────────────┘
                │
                ▼
┌───────────────┐     ┌───────────────────┐
│ RecordBuilder │◀────│ Reorder Map       │
│               │     │ (optional restore)│
└───────┬───────┘     └───────────────────┘
        │
        ▼
┌─────────────┐
│ FASTQ Output│
└─────────────┘
```

## Memory Management

### Memory Budget System

fq-compressor features an automatic memory budget system:

1. **Detection**: Automatically detects available system memory
2. **Allocation**: Distributes budget across subsystems
3. **Throttling**: Uses token-based backpressure in pipeline
4. **Limits**: Respects user-defined `--memory-limit`

### Memory Usage by Component

| Component | Typical Usage | Configurable |
|-----------|---------------|--------------|
| Pipeline tokens | ~10% of budget | Yes (indirect) |
| Block buffers | ~30% of budget | Yes (block-size) |
| Sorting/reordering | ~40% of budget | No |
| Compression contexts | ~20% of budget | No |

## Threading Model

### oneTBB Integration

- **Flow Graph**: Producer → Compressor → Writer pipeline
- **Parallel Algorithms**: parallel_for for block processing
- **Concurrent Containers**: concurrent_queue for work distribution

### Thread Safety Guarantees

- **Parsing**: Single-threaded (with lookahead)
- **Compression**: Thread-safe per block
- **Writing**: Thread-safe with ordering preservation

## Error Handling Strategy

### Error Categories

| Category | Handling | Example |
|----------|----------|---------|
| Parse errors | Fatal with location | Malformed FASTQ |
| I/O errors | Fatal with path | Permission denied |
| Memory errors | Fatal with stats | Out of memory |
| Format errors | Fatal with context | Corrupted archive |

### Error Reporting

- **Structured**: `Result<T>` / `VoidResult` types
- **Context**: Source location, file position
- **Logging**: Structured logging with Quill
- **CLI**: User-friendly error messages

## Extension Points

### Adding New Commands

1. Create class in `src/commands/`
2. Implement `execute()` method
3. Register in `main.cpp`

### Adding New Algorithms

1. Implement `CompressorInterface`
2. Update `BlockCompressor` to dispatch
3. Add version handling for backward compatibility

---

**🌐 Language**: [English (current)](./architecture.md) | [简体中文](../../zh/core-concepts/architecture.md)
