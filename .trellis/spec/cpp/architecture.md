# Architecture

> Module organization, namespaces, and pipeline architecture.

---

## Module Organization

| Namespace | Purpose |
|-----------|---------|
| `fqc::algo` | Compression algorithms (ABC, SCM) |
| `fqc::commands` | CLI command implementations |
| `fqc::common` | Shared utilities (errors, logging, types) |
| `fqc::format` | FQC archive format definition |
| `fqc::io` | I/O abstractions (FASTQ, compressed streams) |
| `fqc::pipeline` | TBB-based parallel processing pipelines |

---

## Directory Structure

```
include/fqc/           Public header files (API)
  algo/               Compression algorithms (ABC, SCM)
  commands/           CLI command APIs
  common/             Errors, logging, types, memory budget
  format/             FQC archive format, reader/writer, reorder map
  io/                 FASTQ parsing, compressed stream I/O, async I/O
  pipeline/           TBB pipeline types and nodes

src/                   Implementation files
  algo/               Algorithm implementations
  commands/           Command implementations
  common/             Common utilities
  format/             Format implementations
  io/                 I/O implementations
  pipeline/           Pipeline node implementations
  main.cpp            CLI entry point
```

---

## Compression Pipeline

```
FASTQ Input → FastqParser → GlobalAnalyzer → BlockCompressor → FQC Writer
                                              (TBB parallel pipeline)
```

### Pipeline Stages

1. **Reader Node**: Parse FASTQ input, emit `ReadChunk` tokens
2. **Compressor Node**: Compress each chunk in parallel, emit `CompressedBlock` tokens
3. **Writer Node**: Write blocks to FQC archive in order

---

## Decompression Pipeline

```
FQC Input → FQC Reader → Decompressor → FastqWriter → FASTQ Output
              (TBB parallel pipeline)
```

---

## Key Algorithms

### Assembly-based Compression (ABC)

For short read sequences:

1. **Minimizer Bucketing** — groups reads by shared k-mer signatures
2. **TSP Reordering** — maximizes neighbor similarity
3. **Consensus Generation** — builds local consensus per bucket
4. **Delta Encoding** — stores only edits from consensus

### Statistical Context Mixing (SCM)

For quality scores:

1. **Context** — Previous QVs + current base + position
2. **Prediction** — High-order context modeling
3. **Coding** — Adaptive arithmetic coding

---

## Codec Families

| Family | Name | Use |
|--------|------|-----|
| 0x0 | RAW | No compression (debugging) |
| 0x1 | ABC_V1 | Spring ABC (Sequence) |
| 0x2 | SCM_V1 | Context Mixing (Quality) |
| 0x3 | DELTA_LZMA | IDs |
| 0x4 | DELTA_ZSTD | IDs |
| 0x5 | DELTA_VARINT | Auxiliary data (lengths) |
| 0x6 | OVERLAP_V1 | Long read overlap compression |
| 0x7 | ZSTD_PLAIN | Fallback (Sequence) |
| 0x8 | SCM_ORDER1 | Low-memory quality compression |
| 0xE | EXTERNAL | External/custom codec |
| 0xF | RESERVED | Reserved for future |

---

## Read Length Classification

| Class | Criteria | Block Size | Algorithm |
|-------|----------|------------|-----------|
| SHORT | max ≤ 511 AND median < 1KB | 100K | ABC + reorder |
| MEDIUM | max > 511 OR median ≥ 1KB | 50K | Zstd |
| LONG | max ≥ 10KB (includes ultra-long ≥ 100KB) | 10K | Zstd |

Note: ULTRA_LONG (≥100KB) is a sub-category of LONG with smaller block bases limits (50MB vs 200MB).

---

## Key Types

### ReadRecord

Core data structure for a FASTQ read:

```cpp
// include/fqc/common/types.h
struct ReadRecord {
    std::string id;        // Read identifier (without '@' prefix)
    std::string comment;   // Optional comment after ID
    std::string sequence;  // DNA sequence (A, C, G, T, N)
    std::string quality;   // Quality scores (Phred+33 encoded)
};

struct ReadRecordView {
    std::string_view id;
    std::string_view comment;
    std::string_view sequence;
    std::string_view quality;
};
```

### CompressedBlock

Pipeline token for compressed data (contains separate streams):

```cpp
// include/fqc/pipeline/pipeline.h
struct CompressedBlock {
    BlockId blockId;
    ReadId startReadId;
    std::vector<std::uint8_t> idStream;    // Compressed ID data
    std::vector<std::uint8_t> seqStream;   // Compressed sequence data
    std::vector<std::uint8_t> qualStream;  // Compressed quality data
    std::vector<std::uint8_t> auxStream;   // Auxiliary data
    std::uint32_t readCount;
    std::uint32_t uniformReadLength;       // 0 if variable length
    std::uint64_t checksum;                // Block checksum
    std::uint8_t codecIds;                 // Codec family for ID stream
    std::uint8_t codecSeq;                 // Codec family for sequence stream
    std::uint8_t codecQual;                // Codec family for quality stream
    std::uint8_t codecAux;                 // Codec family for auxiliary stream
    bool isLast;
};
```

---

## Dependencies

| Purpose | Library | Version |
|---------|---------|---------|
| CLI parsing | CLI11 | 2.4.2 |
| Logging | Quill | 11.0.2 |
| Formatting | fmt | 12.1.0 |
| Compression | zlib-ng, bzip2, xz_utils | various |
| Compression | zstd, libdeflate | 1.5.7, 1.25 |
| Checksums | xxHash | 0.8.3 |
| Parallelism | Intel oneTBB | 2022.3.0 |
| Testing | GTest | 1.12.1 |
| Property Testing | RapidCheck | optional |

---

## Logging

Use Quill-based logging from `include/fqc/common/logger.h`:

```cpp
FQC_LOG_DEBUG("Processing block {}", blockId);
FQC_LOG_INFO("Compressed {} reads", totalReads);
FQC_LOG_ERROR("Failed to open file: {}", path);
```

**Do NOT use `std::cout` for status/debug logging.**

> See [Error Handling](./error-handling.md) for error handling patterns.

---

## Adding a New Command

1. Add header in `include/fqc/commands/`
2. Add implementation in `src/commands/`
3. Register in `src/main.cpp`
4. Add tests in `tests/commands/`

---

## Common Patterns

### PIMPL Idiom

Many classes use the PIMPL idiom for binary compatibility and compilation firewall:

```cpp
// Header
class FQCWriter {
public:
    FQCWriter();
    ~FQCWriter();
    // ...
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Implementation
class FQCWriter::Impl {
public:
    // Actual implementation...
};
```
