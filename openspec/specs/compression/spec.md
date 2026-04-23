# Compression Specification

This document defines the compression capabilities of fq-compressor, including algorithm selection, stream handling, and support for various read types.

## Requirements

### Requirement: Input format support
The system SHALL accept FASTQ files in various formats for compression.

#### Scenario: Compress uncompressed FASTQ
- **WHEN** user runs `fqc compress -i input.fq -o output.fqc`
- **THEN** output file is created with size smaller than input

#### Scenario: Compress gzip input
- **WHEN** user runs `fqc compress -i input.fq.gz -o output.fqc`
- **THEN** file is decompressed transparently and compressed to FQC

#### Scenario: Compress bzip2 input
- **WHEN** user runs `fqc compress -i input.fq.bz2 -o output.fqc`
- **THEN** file is decompressed transparently and compressed to FQC

#### Scenario: Compress xz input
- **WHEN** user runs `fqc compress -i input.fq.xz -o output.fqc`
- **THEN** file is decompressed transparently and compressed to FQC

### Requirement: Stream separation
The system SHALL independently compress ID, Sequence, and Quality streams using optimal algorithms.

#### Scenario: Independent stream processing
- **WHEN** compression completes
- **THEN** each stream uses its optimal algorithm (ABC/SCM/Delta)

#### Scenario: Sequence stream uses ABC for short reads
- **WHEN** compressing short reads (max ≤ 511bp, median < 1KB)
- **THEN** sequence stream uses ABC_V1 codec
- **AND** global reordering is enabled

#### Scenario: Sequence stream uses Zstd for long reads
- **WHEN** compressing long reads (max > 511bp or median ≥ 1KB)
- **THEN** sequence stream uses ZSTD_PLAIN codec
- **AND** global reordering is disabled

### Requirement: Compression modes
The system SHALL support multiple compression modes for different use cases.

#### Scenario: Reorder mode (default)
- **WHEN** user runs `fqc compress -i input.fq -o output.fqc` without mode flags
- **THEN** reads are reordered for maximum compression
- **AND** reorder map is stored for original order restoration

#### Scenario: Order-preserving mode
- **WHEN** user runs `fqc compress --preserve-order -i input.fq -o output.fqc`
- **THEN** reads maintain original order
- **AND** no reorder map is stored

#### Scenario: Streaming mode
- **WHEN** user pipes data via stdin: `cat input.fq | fqc compress -i - -o output.fqc`
- **THEN** warning is displayed about streaming mode limitations
- **AND** compression completes without global reordering

### Requirement: Read length classification
The system SHALL automatically classify reads and select appropriate compression strategies.

#### Scenario: Short read classification
- **WHEN** input has max ≤ 511bp AND median < 1KB
- **THEN** reads are classified as SHORT
- **AND** ABC algorithm is used
- **AND** block size is 100,000 reads

#### Scenario: Medium read classification
- **WHEN** input has max > 511bp OR median ≥ 1KB
- **THEN** reads are classified as MEDIUM
- **AND** Zstd algorithm is used
- **AND** block size is 50,000 reads

#### Scenario: Long read classification
- **WHEN** input has max ≥ 10KB
- **THEN** reads are classified as LONG
- **AND** Zstd algorithm is used
- **AND** block size is 10,000 reads
- **AND** block bases limit is 200MB

#### Scenario: Ultra-long read classification
- **WHEN** input has max ≥ 100KB
- **THEN** reads are classified as ULTRA_LONG
- **AND** Zstd algorithm is used
- **AND** block bases limit is 50MB

### Requirement: Paired-end input support
The system SHALL support paired-end FASTQ files with proper validation.

#### Scenario: Dual file input
- **WHEN** user runs `fqc compress -1 R1.fq -2 R2.fq -o paired.fqc`
- **THEN** both files are processed together
- **AND** paired flag is set in archive header
- **AND** R1 and R2 read counts match

#### Scenario: Interleaved input
- **WHEN** user runs `fqc compress --interleaved -i reads.fq -o paired.fqc`
- **THEN** file is parsed as interleaved R1/R2 pairs
- **AND** paired flag is set in archive header

#### Scenario: Paired-end count mismatch error
- **WHEN** user provides R1 and R2 files with different read counts
- **THEN** error message is displayed with both counts
- **AND** exit code is 3 (FORMAT_ERROR)

### Requirement: Paired-end storage layout
The system SHALL support different storage layouts for paired-end data.

#### Scenario: Interleaved layout (default)
- **WHEN** user compresses paired-end data without layout flag
- **THEN** R1 and R2 of same pair are stored adjacently
- **AND** R1/R2 similarity is exploited for compression

#### Scenario: Consecutive layout
- **WHEN** user runs `fqc compress --pe-layout consecutive -1 R1.fq -2 R2.fq -o out.fqc`
- **THEN** all R1 reads are stored first, then all R2 reads
- **AND** single-end extraction is contiguous

### Requirement: R1/R2 complementary optimization
The system SHALL detect and exploit reverse complement relationship between paired reads.

#### Scenario: Complementary pair detection
- **WHEN** R2 is reverse complement of R1
- **THEN** only differences are stored for R2
- **AND** compression ratio is improved

#### Scenario: Independent pair handling
- **WHEN** R2 is NOT reverse complement of R1
- **THEN** both reads are stored independently
- **AND** no information is lost

### Requirement: Long read CLI options
The system SHALL provide CLI options for controlling long read compression.

#### Scenario: Force long read mode
- **WHEN** user runs `fqc compress --long-read-mode long -i reads.fq -o out.fqc`
- **THEN** long read strategy is used regardless of detection

#### Scenario: Custom block size
- **WHEN** user runs `fqc compress --block-reads 5000 -i reads.fq -o out.fqc`
- **THEN** specified block size overrides default

#### Scenario: Custom block bases limit
- **WHEN** user runs `fqc compress --max-block-bases 100M -i long_reads.fq -o out.fqc`
- **THEN** block bases limit is set to 100MB

### Requirement: ABC algorithm pipeline
The system SHALL implement Assembly-based Compression for short read sequences.

#### Scenario: Minimizer extraction
- **WHEN** ABC compression runs
- **THEN** (w,k)-minimizers are extracted from each read (k=10, w=10)

#### Scenario: Bucketing
- **WHEN** ABC compression runs
- **THEN** reads are grouped by shared minimizers
- **AND** similar reads end up in same bucket

#### Scenario: TSP reordering
- **WHEN** ABC compression runs
- **THEN** reads are ordered to maximize neighbor similarity

#### Scenario: Consensus generation
- **WHEN** ABC compression runs
- **THEN** local consensus is built per bucket

#### Scenario: Delta encoding
- **WHEN** ABC compression runs
- **THEN** only edits from consensus are stored

### Requirement: Zstd fallback for long reads
The system SHALL use Zstd compression for reads exceeding ABC limits.

#### Scenario: Zstd default level
- **WHEN** Zstd compression is used
- **THEN** default compression level is 5

#### Scenario: No reordering for Zstd
- **WHEN** Zstd compression is used
- **THEN** read reordering is disabled

---

## Algorithm Reference

### Codec Families

| Family | Name | Use |
|--------|------|-----|
| 0x0 | RAW | No compression |
| 0x1 | ABC_V1 | Spring ABC (Sequence) |
| 0x2 | SCM_V1 | Context Mixing (Quality) |
| 0x3 | DELTA_LZMA | IDs |
| 0x4 | DELTA_ZSTD | IDs |
| 0x7 | ZSTD_PLAIN | Fallback (Sequence) |

### Read Length Classification

| Class | Criteria | Block Size | Algorithm |
|-------|----------|------------|-----------|
| SHORT | max ≤ 511 AND median < 1KB | 100K | ABC + reorder |
| MEDIUM | max > 511 OR median ≥ 1KB | 50K | Zstd |
| LONG | max ≥ 10KB | 10K | Zstd |
| ULTRA_LONG | max ≥ 100KB | ≤10K | Zstd |

### Performance Targets

| Metric | Target | Stretch Goal |
|--------|--------|--------------|
| Compression ratio | 0.4-0.6 bits/base | 0.3-0.4 |
| Compression speed | 20-50 MB/s/thread | 50-100 |
| Memory peak | < 4 GB | < 2 GB |
