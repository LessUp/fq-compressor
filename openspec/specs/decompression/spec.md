# Decompression Specification

This document defines the decompression capabilities of fq-compressor, including full file decompression, range extraction, and output options.

## Requirements

### Requirement: Full file decompression
The system SHALL decompress FQC files back to valid FASTQ format.

#### Scenario: Basic decompression
- **WHEN** user runs `fqc decompress -i input.fqc -o output.fq`
- **THEN** output FASTQ file is created
- **AND** all reads are restored

#### Scenario: Round-trip integrity
- **WHEN** FASTQ file is compressed and then decompressed in lossless mode
- **THEN** output matches original exactly

### Requirement: Range extraction
The system SHALL support extracting specific read ranges without full decompression.

#### Scenario: Range extraction
- **WHEN** user runs `fqc decompress -i input.fqc --range 50000:60000 -o subset.fq`
- **THEN** only reads 50000-60000 are extracted
- **AND** output has exactly 10,001 reads
- **AND** decompression is faster than full file

#### Scenario: 1-based indexing
- **WHEN** user specifies `--range 1:100`
- **THEN** first read (index 1) is included
- **AND** 100th read is included
- **AND** total 100 reads are extracted

#### Scenario: Invalid range error
- **WHEN** user specifies invalid range (e.g., `--range 0:100` or `--range 100:50`)
- **THEN** error message is displayed
- **AND** exit code is 1 (ARGUMENT_ERROR)

### Requirement: Header-only extraction
The system SHALL support extracting only read headers without sequence/quality data.

#### Scenario: Header-only extraction
- **WHEN** user runs `fqc decompress -i input.fqc --header-only -o headers.txt`
- **THEN** only read IDs are extracted
- **AND** no sequence/quality decoding is performed
- **AND** operation is faster than full decompression

### Requirement: Stream selection
The system SHALL allow selective extraction of specific streams.

#### Scenario: Extract all streams
- **WHEN** user runs `fqc decompress -i input.fqc --streams all -o output.fq`
- **THEN** ID, sequence, and quality streams are extracted

#### Scenario: Extract sequence only
- **WHEN** user runs `fqc decompress -i input.fqc --streams seq -o output.txt`
- **THEN** only sequence data is extracted

### Requirement: Paired-end decompression
The system SHALL support paired-end specific decompression options.

#### Scenario: Interleaved output (default)
- **WHEN** user decompresses paired-end FQC
- **THEN** output is interleaved FASTQ (R1, R2, R1, R2, ...)

#### Scenario: Split output
- **WHEN** user runs `fqc decompress -i paired.fqc --split-pe -o out`
- **THEN** `out_R1.fastq` is created
- **AND** `out_R2.fastq` is created
- **AND** both files have matching pair counts

#### Scenario: Pair-based range
- **WHEN** user runs `fqc decompress -i paired.fqc --range-pairs 1:50 -o out.fq`
- **THEN** first 50 pairs (100 reads) are extracted
- **AND** extraction is independent of storage layout

### Requirement: ID reconstruction
The system SHALL reconstruct read IDs when original IDs were discarded.

#### Scenario: SE ID reconstruction
- **WHEN** decompressing SE file compressed with `--id-mode discard`
- **THEN** IDs are generated as `@1`, `@2`, `@3`, ...

#### Scenario: PE interleaved ID reconstruction
- **WHEN** decompressing PE interleaved file compressed with `--id-mode discard`
- **THEN** IDs are generated as `@1/1`, `@1/2`, `@2/1`, `@2/2`, ...

#### Scenario: Custom ID prefix
- **WHEN** user runs `fqc decompress -i input.fqc --id-prefix "SAMPLE_A_" -o out.fq`
- **THEN** reconstructed IDs use the prefix: `@SAMPLE_A_1`, `@SAMPLE_A_2`, ...

### Requirement: Quality reconstruction
The system SHALL handle quality scores according to compression mode.

#### Scenario: Lossless quality restoration
- **WHEN** decompressing file compressed in lossless quality mode
- **THEN** all quality scores match original exactly

#### Scenario: Quality discard placeholder
- **WHEN** decompressing file compressed with `--quality-mode discard`
- **THEN** quality lines are `!` repeated (Phred 0)
- **AND** sequence and ID are preserved

### Requirement: Output to stdout
The system SHALL support outputting decompressed data to stdout.

#### Scenario: Stdout output
- **WHEN** user runs `fqc decompress -i input.fqc -o -`
- **THEN** FASTQ data is written to stdout

### Requirement: Progress reporting
The system SHALL provide progress feedback during decompression.

#### Scenario: Progress display
- **WHEN** decompressing large file
- **THEN** progress percentage is displayed
- **AND** current read count is shown

#### Scenario: Quiet mode
- **WHEN** user runs `fqc decompress -q -i input.fqc -o output.fq`
- **THEN** no progress output is displayed
- **AND** only errors are shown

---

## Performance Targets

| Metric | Target | Stretch Goal |
|--------|--------|--------------|
| Decompression speed | 100-200 MB/s/thread | 200-400 |
| Memory peak | < 4 GB | < 2 GB |
