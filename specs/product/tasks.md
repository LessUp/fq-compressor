# Implementation Plan

> **Version**: 1.0  
> **Last Updated**: 2026-04-17  
> **Status**: ✅ Complete

---

## Overview

Implementation is organized into 5 phases. All core functionality has been implemented.

### Timeline Summary

| Phase | Duration | Risk | Status |
|-------|----------|------|--------|
| Phase 1: Skeleton & Format | 2-3 weeks | Low | ✅ Complete |
| Phase 2: Spring Integration | 4-6 weeks | High | ✅ Complete |
| Phase 3: TBB Pipeline | 2-3 weeks | Medium | ✅ Complete |
| Phase 4: Optimization | 2-3 weeks | Medium | ✅ Complete |
| Phase 5: Verification | 2 weeks | Low | ✅ Complete |

**Total**: ~12-17 weeks (3-4 months)

---

## Phase 1: Skeleton & Format ✅

### 1.1 Project Setup
- [x] Directory structure (`include/fqc/`, `src/`, `tests/`, `vendor/`)
- [x] CMake build system with presets
- [x] Conan dependency management
- [x] Code style configuration (`.clang-format`, `.clang-tidy`)
- [x] Dev Container / Docker environment

### 1.2 Infrastructure
- [x] Logger module (Quill)
- [x] Common types (`ReadRecord`, `QualityMode`, `IDMode`)
- [x] Error handling framework (`FQCException`, `Result<T>`)

### 1.3 FQC Format
- [x] Format constants and data structures
- [x] `FQCWriter` class
- [x] `FQCReader` class with random access
- [x] Block Index with forward compatibility
- [x] Property tests for format roundtrip

### 1.4 FASTQ Parser
- [x] Streaming parser
- [x] Compressed input support (gzip, bzip2, xz)
- [x] PE format support
- [x] Property tests for parsing

### 1.5 CLI Framework
- [x] Main entry with subcommands
- [x] `compress`, `decompress`, `info`, `verify` commands
- [x] Global options (`-t`, `-v`, `--memory-limit`)

---

## Phase 2: Compression Algorithms ✅

### 2.1 Spring Core Integration
- [x] Source code analysis
- [x] Core algorithm extraction to `vendor/spring-core/`
- [x] License compatibility review

### 2.2 Two-Phase Compression
- [x] Phase 1: Global Analysis (Minimizer extraction, Bucketing, Reordering)
- [x] Phase 2: Block Compression (Consensus, Delta, Arithmetic)
- [x] Reorder Map storage (bidirectional)
- [x] Memory budget management

### 2.3 Quality Compression
- [x] SCM context model (Order-1/Order-2)
- [x] Illumina 8-bin lossy mode
- [x] Arithmetic encoder integration

### 2.4 ID Compression
- [x] Tokenizer for Illumina headers
- [x] Delta encoding
- [x] General compressor integration (LZMA/Zstd)

---

## Phase 3: Parallel Pipeline ✅

### 3.1 Pipeline Architecture
- [x] `IPipelineStage` interface
- [x] `ReaderFilter` (Serial) - chunked FASTQ reading
- [x] `CompressFilter` (Parallel) - block compression
- [x] `WriterFilter` (Serial) - ordered block writing

### 3.2 Engines
- [x] `CompressionEngine` - assembles TBB pipeline
- [x] `DecompressionEngine` - parallel decompression
- [x] Range-based extraction
- [x] Stream selection

---

## Phase 4: Extensions ✅

### 4.1 I/O Optimization
- [x] Async I/O with double buffering
- [x] External compression advisory documentation

### 4.2 Input Formats
- [x] bzip2 support
- [x] xz support

### 4.3 Long Read Support
- [x] Automatic length detection (sampling)
- [x] Classification: Short/Medium/Long/Ultra-long
- [x] Strategy selection per class
- [x] Block size adaptation

### 4.4 Paired-End Support
- [x] Dual file input (`-1`, `-2`)
- [x] Interleaved format
- [x] PE layout options
- [x] R1/R2 complementary optimization

---

## Phase 5: Verification & Release ✅

### 5.1 Integrity Verification
- [x] Global checksum validation
- [x] Block-level verification
- [x] `--fail-fast` option
- [x] Error isolation (`--skip-corrupted`)

### 5.2 Testing
- [x] Unit tests (Google Test)
- [x] Property tests (RapidCheck)
- [x] E2E tests
- [x] Performance benchmarks

### 5.3 Documentation
- [x] README (bilingual)
- [x] CHANGELOG
- [x] API documentation (Doxygen)
- [x] GitBook website

---

## Appendix: Property Test Generators

### DNA Sequence Generator
```cpp
// Parameters
size_t min_length = 50;
size_t max_length = 500;
double n_ratio = 0.01;
// Distribution: A/C/G/T each 25%, N configurable
```

### Quality Value Generator
```cpp
// Phred33 range: '!' (0) to 'J' (41)
// Distribution simulates real data:
// - First 20%: high quality (30-41)
// - Middle 60%: medium quality (20-35)
// - Last 20%: low quality (10-25)
```

### Read ID Generator
```cpp
// Illumina format: @<instrument>:<run>:<flowcell>:<lane>:<tile>:<x>:<y>
// Strategy: Fixed instrument, variable numeric fields (delta-friendly)
```
