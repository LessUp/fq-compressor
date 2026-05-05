# Product Requirements Document (PRD)

> **Version**: 1.0  
> **Last Updated**: 2026-04-17  
> **Status**: Core Features Complete

---

## Introduction

fq-compressor is a high-performance FASTQ compression tool combining **Assembly-based Compression (ABC)** with industrial-grade engineering practices. It achieves high compression ratios, fast parallel processing, and native random access capability.

### Core Algorithm Strategy

| Stream | Algorithm | Reference |
|--------|-----------|-----------|
| **Sequence** | Assembly-based Compression (ABC) | Spring/Mincom |
| **Quality** | Statistical Context Mixing (SCM) | fqzcomp5 |
| **IDs** | Delta + Tokenization | Custom |

### Technology Stack

| Component | Technology |
|-----------|------------|
| Language | C++23 |
| Build System | CMake 3.28+ + Ninja |
| Dependency Management | Conan 2.x |
| Parallel Framework | Intel oneTBB |
| CLI Parser | CLI11 |
| Logging | Quill |

---

## Requirements

### Requirement 1: Compression

**User Story**: Compress FASTQ files with high ratio and speed.

#### Acceptance Criteria

1. **R1.1 Input Format Support**
   - `.fastq`, `.fq`, `.gz`, `.bz2`, `.xz` input
   - `.fqc` output format
   - No outer wrapper compression (preserves random access)

2. **R1.2 Stream Separation**
   - Independent compression of ID, Sequence, Quality streams
   - Optional Aux stream for variable-length reads

3. **R1.3 Compression Modes**
   - **Reorder (Default)**: Allow read reordering for maximum compression
   - **Order-Preserving**: Keep original read order
   - **Streaming Mode**: Support stdin input (no global reordering)

4. **R1.4 Read Length Support**
   - **Short** (max ≤ 511, median < 1KB): ABC + global reordering
   - **Medium** (max > 511 or median ≥ 1KB): Zstd, no reordering
   - **Long** (median ≥ 10KB): Zstd, no reordering, conservative block size

5. **R1.5 Paired-End Support**
   - Dual file input (`-1`, `-2`)
   - Interleaved format support
   - PE layout options: `interleaved` | `consecutive`

---

### Requirement 2: Decompression & Random Access

**User Story**: Fast decompression with random access capability.

#### Acceptance Criteria

1. **R2.1 Block-based Architecture**
   - Files organized as independent blocks
   - O(1) random access via Block Index
   - 1-based indexing (consistent with SAMtools/bedtools)

2. **R2.2 Output Options**
   - Full file decompression
   - Range extraction (`--range start:end`)
   - Header-only extraction (`--header-only`)
   - Stream selection (`--streams id|seq|qual|all`)

3. **R2.3 PE-specific**
   - `--range-pairs` for pair-based extraction
   - `--split-pe` for separate R1/R2 output

---

### Requirement 3: Quality Score Handling

**User Story**: Balance file size and data precision.

#### Acceptance Criteria

1. **R3.1 Lossless (Default)**: Preserve all quality values
2. **R3.2 Illumina Binning**: Standard 8-level binning
3. **R3.3 Quality Discard**: Drop quality values, use placeholder on decompress

---

### Requirement 4: High Performance

**User Story**: Utilize multi-core CPUs for massive datasets.

#### Acceptance Criteria

1. **R4.1 Parallelism**: Intel oneTBB pipeline
2. **R4.2 Memory Control**: `--memory-limit <MB>` parameter
3. **R4.3 Chunk Processing**: Automatic for files exceeding memory budget

---

### Requirement 5: Data Integrity

**User Story**: Detect and handle corrupted files.

#### Acceptance Criteria

1. **R5.1 Checksums**: xxhash64 for global and per-block verification
2. **R5.2 Verify Command**: Standalone integrity verification
3. **R5.3 Error Isolation**: Single corrupted block doesn't affect others
4. **R5.4 Recovery**: `--skip-corrupted` option for partial decompression

---

### Requirement 6: CLI Usability

**User Story**: Intuitive command-line interface.

#### Commands

```
fqc <subcommand> [options]

Subcommands:
  compress    Compress FASTQ to FQC
  decompress  Decompress FQC to FASTQ
  info        Display archive metadata
  verify      Verify archive integrity
```

#### Exit Codes

| Code | Name | Meaning |
|------|------|---------|
| 0 | SUCCESS | Operation completed |
| 1 | ARGUMENT_ERROR | Invalid CLI arguments |
| 2 | IO_ERROR | File I/O failure |
| 3 | FORMAT_ERROR | Invalid file format |
| 4 | CHECKSUM_ERROR | Checksum mismatch |
| 5 | UNSUPPORTED_CODEC | Unknown codec family |

---

### Requirement 7: Development & Build

**User Story**: Easy to build and maintain.

#### Acceptance Criteria

1. **R7.1 Build System**: CMake 3.20+ with presets
2. **R7.2 Dependencies**: Conan 2.x or git submodules
3. **R7.3 Code Style**: Automated via clang-format/clang-tidy
4. **R7.4 CI/CD**: GitHub Actions for build, test, quality checks
5. **R7.5 Containerization**: Dev Container / Docker environment

---

### Requirement 8: Atomic Write & Recovery

**User Story**: Interrupted compression doesn't corrupt output.

#### Acceptance Criteria

1. **R8.1 Atomic Write**: Use `.fqc.tmp` + rename
2. **R8.2 Signal Handling**: Cleanup on SIGINT/SIGTERM

---

### Requirement 9: Version Compatibility

**User Story**: New versions read old files.

#### Acceptance Criteria

1. **R9.1 Backward Compatible**: New readers read all old versions
2. **R9.2 Forward Compatible**: Old readers skip unknown fields via header_size
3. **R9.3 Version Encoding**: (major:4bit, minor:4bit)

---

## Non-Functional Requirements

### Performance Targets

| Metric | Target | Stretch Goal |
|--------|--------|--------------|
| Compression ratio | 0.4-0.6 bits/base | 0.3-0.4 |
| Compression speed | 20-50 MB/s/thread | 50-100 |
| Decompression speed | 100-200 MB/s/thread | 200-400 |
| Memory peak | < 4 GB | < 2 GB |

### Comparison with Existing Tools

| Tool | Ratio (bits/base) | Compress Speed | Decompress Speed |
|------|-------------------|----------------|------------------|
| gzip -9 | ~2.0 | ~20 MB/s | ~200 MB/s |
| Spring | ~0.3 | ~5 MB/s | ~50 MB/s |
| repaq | ~0.5 | ~100 MB/s | ~200 MB/s |
| **fqc (Target)** | 0.4-0.6 | 20-50 MB/s | 100-200 MB/s |
