# CLI Specification

This document defines the command-line interface capabilities of fq-compressor.

## Requirements

### Requirement: Subcommand structure
The system SHALL use a subcommand-based CLI structure.

#### Scenario: Main help
- **WHEN** user runs `fqc --help`
- **THEN** all subcommands are listed
- **AND** usage examples are provided
- **AND** exit code is 0

#### Scenario: Subcommand help
- **WHEN** user runs `fqc compress --help`
- **THEN** all compression options are documented
- **AND** exit code is 0

### Requirement: Compress command
The system SHALL provide a compress subcommand.

#### Scenario: Basic compression
- **WHEN** user runs `fqc compress -i input.fq -o output.fqc`
- **THEN** FASTQ file is compressed to FQC format

#### Scenario: Compress with options
- **WHEN** user runs `fqc compress -i input.fq -o output.fqc -t 8 --memory-limit 4096`
- **THEN** compression uses 8 threads and 4GB memory limit

### Requirement: Decompress command
The system SHALL provide a decompress subcommand.

#### Scenario: Basic decompression
- **WHEN** user runs `fqc decompress -i input.fqc -o output.fq`
- **THEN** FQC file is decompressed to FASTQ format

### Requirement: Info command
The system SHALL provide an info subcommand for archive inspection.

#### Scenario: Display archive info
- **WHEN** user runs `fqc info archive.fqc`
- **THEN** metadata is displayed including:
  - Total read count
  - Compression mode
  - Codec information
  - Block count

#### Scenario: JSON output
- **WHEN** user runs `fqc info --json archive.fqc`
- **THEN** info is output in JSON format

### Requirement: Verify command
The system SHALL provide a verify subcommand for integrity checking.

#### Scenario: Verify archive
- **WHEN** user runs `fqc verify archive.fqc`
- **THEN** checksums are validated
- **AND** result is displayed

### Requirement: Global options
The system SHALL provide global options applicable to all commands.

#### Scenario: Thread count
- **WHEN** user specifies `-t 8` or `--threads 8`
- **THEN** 8 threads are used

#### Scenario: Verbosity
- **WHEN** user specifies `-v` or `--verbose`
- **THEN** additional output is displayed

#### Scenario: Quiet mode
- **WHEN** user specifies `-q` or `--quiet`
- **THEN** only errors are displayed

#### Scenario: Memory limit
- **WHEN** user specifies `--memory-limit 2048`
- **THEN** memory is limited to 2GB

#### Scenario: No progress
- **WHEN** user specifies `--no-progress`
- **THEN** progress bar is disabled

### Requirement: Error messages
The system SHALL provide clear error messages for invalid input.

#### Scenario: Invalid arguments
- **WHEN** user provides invalid arguments
- **THEN** clear error message is displayed
- **AND** exit code is 1
- **AND** help suggestion is provided

#### Scenario: File not found
- **WHEN** input file does not exist
- **THEN** error message indicates file not found
- **AND** exit code is 2 (IO_ERROR)

---

## CLI Commands

```
fqc <subcommand> [options]

Subcommands:
  compress    Compress FASTQ to FQC
  decompress  Decompress FQC to FASTQ
  info        Display archive metadata
  verify      Verify archive integrity
```

## Exit Codes

| Code | Name | Meaning |
|------|------|---------|
| 0 | SUCCESS | Operation completed |
| 1 | ARGUMENT_ERROR | Invalid CLI arguments |
| 2 | IO_ERROR | File I/O failure |
| 3 | FORMAT_ERROR | Invalid file format |
| 4 | CHECKSUM_ERROR | Checksum mismatch |
| 5 | UNSUPPORTED_CODEC | Unknown codec family |
