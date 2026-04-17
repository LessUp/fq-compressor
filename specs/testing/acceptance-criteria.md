# Acceptance Criteria

> **Version**: 1.0  
> **Last Updated**: 2026-04-17

---

## Overview

This document defines testable acceptance criteria for fq-compressor features. Each criterion is linked to requirements in `/specs/product/requirements.md`.

---

## Compression Features

### AC-1.1: Basic Compression

**Requirement**: R1.1, R1.2

**Given** a valid FASTQ file
**When** user runs `fqc compress -i input.fq -o output.fqc`
**Then**:
- [ ] Output file `output.fqc` is created
- [ ] File size is smaller than input
- [ ] `fqc info output.fqc` shows correct read count
- [ ] `fqc verify output.fqc` passes

### AC-1.2: Compressed Input

**Requirement**: R1.1

**Given** a gzip-compressed FASTQ file `input.fq.gz`
**When** user runs `fqc compress -i input.fq.gz -o output.fqc`
**Then**:
- [ ] File is decompressed transparently
- [ ] Compression proceeds normally
- [ ] No intermediate files created

### AC-1.3: Streaming Mode

**Requirement**: R1.3

**Given** FASTQ data piped via stdin
**When** user runs `cat input.fq | fqc compress -i - -o output.fqc`
**Then**:
- [ ] Warning displayed about streaming mode
- [ ] Compression completes successfully
- [ ] Compression ratio may be reduced (no reordering)

---

## Decompression Features

### AC-2.1: Full Decompression

**Requirement**: R2.1, R2.2

**Given** a valid FQC file
**When** user runs `fqc decompress -i input.fqc -o output.fq`
**Then**:
- [ ] Output FASTQ is created
- [ ] All reads are restored
- [ ] Round-trip matches original (for lossless mode)

### AC-2.2: Range Extraction

**Requirement**: R2.2

**Given** an FQC file with 100,000 reads
**When** user runs `fqc decompress -i input.fqc --range 50000:60000 -o subset.fq`
**Then**:
- [ ] Only reads 50000-60000 are extracted
- [ ] Output has exactly 10,001 reads
- [ ] Decompression is faster than full file

### AC-2.3: Header-Only

**Requirement**: R2.2

**Given** an FQC file
**When** user runs `fqc decompress -i input.fqc --header-only -o headers.txt`
**Then**:
- [ ] Only read IDs are extracted
- [ ] No sequence/quality decoding performed
- [ ] Operation is faster than full decompression

---

## Random Access

### AC-RA.1: Block Independence

**Requirement**: R2.1

**Given** an FQC file with multiple blocks
**When** extracting any range
**Then**:
- [ ] Only necessary blocks are read
- [ ] Seek time is O(1)
- [ ] No sequential scan needed

### AC-RA.2: 1-Based Indexing

**Requirement**: R2.1

**Given** an FQC file
**When** user specifies `--range 1:100`
**Then**:
- [ ] First read is included (index 1)
- [ ] 100th read is included (index 100)
- [ ] Total 100 reads extracted

---

## Quality Score Handling

### AC-3.1: Lossless Quality

**Requirement**: R3.1

**Given** FASTQ with quality scores
**When** compressed and decompressed in lossless mode
**Then**:
- [ ] All quality scores match original
- [ ] No data loss

### AC-3.2: Quality Discard

**Requirement**: R3.3

**Given** FASTQ compressed with `--quality-mode discard`
**When** decompressed
**Then**:
- [ ] Quality lines are `!` repeated (or custom placeholder)
- [ ] Sequence and ID are preserved

---

## Performance

### AC-4.1: Multi-thread Scaling

**Requirement**: R4.1

**Given** a multi-core system
**When** comparing 1-thread vs 8-thread compression
**Then**:
- [ ] 8-thread speed is at least 5x faster
- [ ] Output is identical

### AC-4.2: Memory Limit

**Requirement**: R4.2

**Given** `--memory-limit 2048` (2GB)
**When** compressing large file
**Then**:
- [ ] Peak memory ≤ 2GB
- [ ] Compression completes (possibly in chunks)

---

## Data Integrity

### AC-5.1: Checksum Verification

**Requirement**: R5.1, R5.2

**Given** a valid FQC file
**When** `fqc verify input.fqc` runs
**Then**:
- [ ] Global checksum validated
- [ ] All block checksums validated
- [ ] Exit code 0 on success

### AC-5.2: Corruption Detection

**Requirement**: R5.1, R5.2

**Given** a corrupted FQC file
**When** `fqc verify corrupted.fqc` runs
**Then**:
- [ ] Corruption is detected
- [ ] Error message identifies affected block
- [ ] Exit code 4

### AC-5.3: Partial Recovery

**Requirement**: R5.4

**Given** an FQC with some corrupted blocks
**When** `fqc decompress --skip-corrupted input.fqc -o output.fq`
**Then**:
- [ ] Valid blocks are decompressed
- [ ] Corrupted blocks are skipped with warning
- [ ] Exit code 0 (success with warnings)

---

## CLI Usability

### AC-6.1: Help Message

**Requirement**: R6

**When** user runs `fqc --help` or `fqc compress --help`
**Then**:
- [ ] All options are documented
- [ ] Usage examples provided
- [ ] Exit code 0

### AC-6.2: Error Messages

**Requirement**: R6

**When** user provides invalid arguments
**Then**:
- [ ] Clear error message displayed
- [ ] Exit code 1
- [ ] Help suggestion provided

---

## Long Read Support

### AC-LR.1: Automatic Classification

**Requirement**: R1.4

**Given** FASTQ with median 5KB reads
**When** compressed
**Then**:
- [ ] Classified as LONG
- [ ] Zstd used for sequences
- [ ] No reordering attempted

### AC-LR.2: Ultra-long Handling

**Requirement**: R1.4

**Given** FASTQ with 200KB reads
**When** compressed
**Then**:
- [ ] Block bases limit applied
- [ ] Memory stays within bounds
- [ ] Compression completes

---

## Paired-End Support

### AC-PE.1: Dual File Input

**Requirement**: R1.5

**Given** R1.fq and R2.fq with matching pairs
**When** `fqc compress -1 R1.fq -2 R2.fq -o paired.fqc`
**Then**:
- [ ] Both files processed
- [ ] `fqc info` shows paired flag
- [ ] Decompression produces interleaved output by default

### AC-PE.2: Split Output

**Requirement**: R1.5

**Given** a paired FQC file
**When** `fqc decompress --split-pe input.fqc -o out`
**Then**:
- [ ] `out_R1.fastq` created
- [ ] `out_R2.fastq` created
- [ ] Files have matching pair counts

---

## Version Compatibility

### AC-9.1: Backward Compatibility

**Requirement**: R9.1

**Given** FQC file created with v1.0
**When** decompressed with v1.5
**Then**:
- [ ] File reads successfully
- [ ] All data preserved

### AC-9.2: Forward Compatibility

**Requirement**: R9.2

**Given** FQC file created with v1.5 (minor version higher)
**When** decompressed with v1.0
**Then**:
- [ ] Unknown fields are skipped
- [ ] Core data is readable
- [ ] Warning displayed about newer format
