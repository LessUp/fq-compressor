# Data Integrity Specification

This document defines the data integrity capabilities of fq-compressor, including checksum verification, corruption detection, and recovery options.

## Requirements

### Requirement: Global checksum
The system SHALL compute and store a global checksum for each FQC file.

#### Scenario: Global checksum computation
- **WHEN** FQC file is created
- **THEN** xxhash64 checksum is computed over all uncompressed data
- **AND** checksum is stored in file footer

#### Scenario: Global checksum verification
- **WHEN** user runs `fqc verify input.fqc`
- **THEN** global checksum is validated
- **AND** exit code is 0 on success

### Requirement: Block-level checksums
The system SHALL compute per-block checksums for granular integrity verification.

#### Scenario: Block checksum computation
- **WHEN** each block is written
- **THEN** xxhash64 checksum is computed over uncompressed block data
- **AND** checksum is stored in block header

#### Scenario: Block checksum verification
- **WHEN** block is read
- **THEN** checksum is validated against stored value
- **AND** corrupted blocks are identified

### Requirement: Verify command
The system SHALL provide a standalone verification command.

#### Scenario: Verify valid file
- **WHEN** user runs `fqc verify valid.fqc`
- **THEN** global checksum is validated
- **AND** all block checksums are validated
- **AND** exit code is 0

#### Scenario: Detect corrupted file
- **WHEN** user runs `fqc verify corrupted.fqc`
- **THEN** corruption is detected
- **AND** error message identifies affected block
- **AND** exit code is 4 (CHECKSUM_ERROR)

#### Scenario: Verify with JSON output
- **WHEN** user runs `fqc verify --json input.fqc`
- **THEN** verification results are output in JSON format

### Requirement: Error isolation
The system SHALL isolate corrupted blocks without affecting valid blocks.

#### Scenario: Partial decompression
- **WHEN** decompressing file with some corrupted blocks
- **AND** user specifies `--skip-corrupted`
- **THEN** valid blocks are decompressed
- **AND** corrupted blocks are skipped with warning
- **AND** exit code is 0 (success with warnings)

#### Scenario: Fail-fast mode
- **WHEN** decompressing file with corrupted block
- **AND** user specifies `--fail-fast`
- **THEN** decompression stops at first error
- **AND** exit code is 4 (CHECKSUM_ERROR)

### Requirement: Recovery options
The system SHALL provide options for recovering data from partially corrupted files.

#### Scenario: Skip corrupted blocks
- **WHEN** user runs `fqc decompress --skip-corrupted damaged.fqc -o output.fq`
- **THEN** all readable blocks are extracted
- **AND** corrupted blocks are logged

#### Scenario: Extract specific range
- **WHEN** user runs `fqc decompress --range 1000:2000 damaged.fqc -o output.fq`
- **THEN** only specified range is extracted
- **AND** corruption in other blocks is bypassed

---

## Checksum Algorithm

### xxhash64

- Fast non-cryptographic hash
- 64-bit output
- Used for both global and block-level checksums

## Exit Codes

| Code | Name | Meaning |
|------|------|---------|
| 0 | SUCCESS | Operation completed |
| 4 | CHECKSUM_ERROR | Checksum mismatch |
