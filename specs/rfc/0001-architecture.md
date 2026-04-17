# RFC-0001: System Architecture

> **Status**: Adopted  
> **Author**: Project Team  
> **Date**: 2026-01-15

---

## Summary

This RFC describes the overall system architecture for fq-compressor, including component design, file format, and data flow.

---

## Design Goals

1. **High Compression Ratio**: Assembly-based Compression (ABC) via Spring integration
2. **High Performance**: Intel TBB parallel pipeline
3. **Random Access**: Block-based format with O(1) access
4. **Modularity**: Clear interfaces, pluggable compression backends
5. **Modern C++**: C++23 with RAII, Concepts, Ranges

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     CLI Layer (CLI11)                        │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │ Compress │ │Decompress│ │   Info   │ │  Verify  │       │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘       │
└───────┼────────────┼────────────┼────────────┼─────────────┘
        │            │            │            │
┌───────┼────────────┼────────────┼────────────┼─────────────┐
│       │       Core Layer       │            │              │
│  ┌────▼─────┐ ┌───▼──────┐ ┌───▼──────┐ ┌───▼──────┐      │
│  │Compress  │ │Decompress│ │  Global  │ │  Verify  │      │
│  │ Engine   │ │  Engine  │ │ Analyzer │ │  Engine  │      │
│  └────┬─────┘ └────┬─────┘ └──────────┘ └──────────┘      │
│       │            │                                       │
│  ┌────▼────────────▼─────┐                                │
│  │   Compression Alg.    │                                │
│  │ ┌────────┐ ┌────────┐ │                                │
│  │ │Block   │ │Quality │ │                                │
│  │ │Compress│ │Compress│ │                                │
│  │ └────────┘ └────────┘ │                                │
│  │ ┌────────┐ ┌────────┐ │                                │
│  │ │  ID    │ │ Global │ │                                │
│  │ │Compress│ │Reorder │ │                                │
│  │ └────────┘ └────────┘ │                                │
│  └───────────────────────┘                                │
└───────────────────────────────────────────────────────────┘
        │
┌───────┼───────────────────────────────────────────────────┐
│       │        Pipeline Layer (TBB)                       │
│  ┌────▼──────────────────────────────────────────────┐    │
│  │ Reader → Analyzer → Compressor → Writer           │    │
│  │ (Serial)  (Serial)   (Parallel)   (Serial)        │    │
│  └───────────────────────────────────────────────────┘    │
└───────────────────────────────────────────────────────────┘
        │
┌───────┼───────────────────────────────────────────────────┐
│       │        Storage Layer                              │
│  ┌────▼─────┐ ┌─────────────┐ ┌───────────┐              │
│  │ FQCWriter│ │  FQCReader  │ │ ReorderMap│              │
│  └──────────┘ └─────────────┘ └───────────┘              │
│  ┌──────────────────────────────────────────┐            │
│  │ Block Index | Checksums | Format Version │            │
│  └──────────────────────────────────────────┘            │
└───────────────────────────────────────────────────────────┘
```

---

## File Format (.fqc)

### Layout

```
┌─────────────────────────────────────────────┐
│  Magic Header (9 bytes)                      │
├─────────────────────────────────────────────┤
│  Global Header (Variable)                    │
├─────────────────────────────────────────────┤
│  Block 0                                     │
│  ├─ Block Header (104 bytes)                 │
│  ├─ ID Stream (compressed)                   │
│  ├─ Sequence Stream (compressed)             │
│  ├─ Quality Stream (compressed)              │
│  └─ Aux Stream (optional, lengths)           │
├─────────────────────────────────────────────┤
│  Block 1...N                                 │
├─────────────────────────────────────────────┤
│  Reorder Map (Optional)                      │
├─────────────────────────────────────────────┤
│  Block Index                                 │
├─────────────────────────────────────────────┤
│  File Footer (32 bytes)                      │
└─────────────────────────────────────────────┘
```

### Magic Header

```
Magic: 0x89 'F' 'Q' 'C' 0x0D 0x0A 0x1A 0x0A
Version: 1 byte (major:4bit, minor:4bit)
```

### Global Header

| Field | Type | Description |
|-------|------|-------------|
| `header_size` | uint32 | Total size for forward compatibility |
| `flags` | uint64 | Mode flags (paired, preserve_order, quality_mode, etc.) |
| `compression_algo` | uint8 | Primary codec family |
| `checksum_type` | uint8 | Checksum algorithm (default: xxhash64) |
| `total_read_count` | uint64 | Total reads (PE: 2 × pairs) |
| `original_filename` | string | UTF-8 encoded |

### Block Header (104 bytes)

| Field | Type | Description |
|-------|------|-------------|
| `header_size` | uint32 | For forward compatibility |
| `block_id` | uint32 | Block identifier |
| `codec_*` | uint8[4] | Codec for ID/SEQ/QUAL/AUX streams |
| `block_xxhash64` | uint64 | Checksum of uncompressed streams |
| `uncompressed_count` | uint32 | Reads in this block |
| `uniform_read_length` | uint32 | 0 if variable |
| `compressed_size` | uint64 | Total payload size |
| `offset_*` | uint64[4] | Stream offsets |
| `size_*` | uint64[4] | Stream sizes |

### Codec Families

| Family | Name | Use |
|--------|------|-----|
| 0x0 | RAW | No compression |
| 0x1 | ABC_V1 | Spring ABC (Sequence) |
| 0x2 | SCM_V1 | Context Mixing (Quality) |
| 0x3 | DELTA_LZMA | IDs |
| 0x4 | DELTA_ZSTD | IDs |
| 0x7 | ZSTD_PLAIN | Fallback (Sequence) |

---

## Two-Phase Compression Strategy

The core innovation enabling both high compression and random access:

### Phase 1: Global Analysis
```
1. Scan all reads → Extract Minimizers
2. Build Minimizer → Bucket mapping
3. Global Reordering (TSP approximation)
4. Generate Reorder Map: original_id → archive_id
5. Determine Block boundaries
```

### Phase 2: Block Compression
```
1. For each Block (parallel):
   - Generate local consensus
   - Delta encoding
   - Arithmetic coding
2. Write blocks sequentially
```

### Memory Model
- Phase 1: ~24 bytes/read (Minimizer index + Reorder Map)
- Phase 2: ~50 bytes/read × block_size

---

## Components

### CLI Layer
- `main.cpp`: Entry point, subcommand dispatch
- `compress_command.cpp`: Compression options
- `decompress_command.cpp`: Decompression options
- `info_command.cpp`: Archive inspection
- `verify_command.cpp`: Integrity check

### Core Layer
- `CompressionEngine`: Orchestrates TBB pipeline
- `DecompressionEngine`: Parallel decompression
- `GlobalAnalyzer`: Phase 1 processing
- `BlockCompressor`: Phase 2 processing

### Algorithm Layer
- `SequenceCompressor`: Spring ABC integration
- `QualityCompressor`: SCM implementation
- `IDCompressor`: Delta + Tokenization

### Format Layer
- `FQCWriter`: Streaming write with checksum
- `FQCReader`: Random access read
- `ReorderMap`: Bidirectional ID mapping

### I/O Layer
- `FastqParser`: Streaming FASTQ parsing
- `CompressedStream`: gzip/bzip2/xz support
- `AsyncReader/Writer`: Double buffering

---

## References

- Spring: https://github.com/shubhamchandak94/Spring
- fqzcomp5: https://github.com/jkbonfield/htscodecs
- TBB Pipeline: https://oneapi-src.github.io/oneTBB/
