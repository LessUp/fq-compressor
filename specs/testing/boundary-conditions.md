# Boundary Conditions Specification

> **Version**: 1.0  
> **Last Updated**: 2026-01-27  
> **Status**: Adopted

---

## Overview

This document specifies how fq-compressor handles edge cases and boundary conditions.

---

## Input Boundary Conditions

### Empty Files

| Input | Behavior |
|-------|----------|
| Empty FASTQ (0 bytes) | Produce valid empty FQC with `total_reads=0`, `num_blocks=0` |
| FASTQ with only whitespace | Same as empty |
| Empty FQC | Decompress to empty FASTQ |

### Single Read

| Input | Behavior |
|-------|----------|
| 1 read (4 lines) | Produce single-block FQC, skip reordering |

### Read Length Boundaries

| Condition | Classification | Behavior |
|-----------|---------------|----------|
| max ≤ 511, median < 1KB | SHORT | ABC + reordering |
| max = 511 | SHORT | Boundary: last valid SHORT |
| max = 512 | MEDIUM | Switch to Zstd |
| median = 1KB | MEDIUM | Switch to Zstd |
| max ≥ 10KB | LONG | Conservative block sizing |
| max ≥ 100KB | Ultra-long | Strict block bases limit |

### Sequence Characters

| Character | Valid? | Behavior |
|-----------|--------|----------|
| A, C, G, T | ✅ | Normal encoding |
| N | ✅ | Noise encoding |
| a, c, g, t (lowercase) | ❌ | FormatError |
| Other (X, etc.) | ❌ | FormatError |

### Quality Characters

| Character | Valid? | Behavior |
|-----------|--------|----------|
| `!` (Phred 0) | ✅ | Minimum quality |
| `J` (Phred 41) | ✅ | Maximum Illumina |
| ASCII < 33 | ❌ | FormatError |
| ASCII > 126 | ❌ | FormatError |

### ID Format

| Condition | Behavior |
|-----------|----------|
| Empty ID (`@`) | FormatError |
| Contains newline | FormatError |
| Contains non-printable | FormatError |
| Length > 1KB | Accepted (warning in log) |

---

## File Format Boundaries

### Corrupted FQC

| Condition | Error Code | Message |
|-----------|------------|---------|
| Invalid magic header | 3 (FORMAT_ERROR) | "Invalid .fqc magic header" |
| Checksum mismatch | 4 (CHECKSUM_ERROR) | "Checksum mismatch: expected X, got Y" |
| Truncated file | 2 (IO_ERROR) | "Unexpected EOF: incomplete .fqc file" |
| Unknown codec family | 5 (UNSUPPORTED_CODEC) | "Unsupported codec family: 0x??" |

### Version Compatibility

| Scenario | Behavior |
|----------|----------|
| Major version > current | FORMAT_ERROR |
| Minor version > current | Accept with warning |
| Minor version < current | Full compatibility |

---

## Memory Boundaries

### Insufficient Memory

| Condition | Error Code | Message |
|-----------|------------|---------|
| `--memory-limit` too small | 5 (MEMORY_ERROR) | "Insufficient memory: need X MB, have Y MB" |
| System OOM | 5 | Catch `std::bad_alloc`, cleanup, exit |

### Block Size Limits

| Setting | Behavior |
|---------|----------|
| `--block-reads < 100` | Warning: "Very small block may hurt compression" |
| `--block-reads > 1M` | Warning: "Very large block may cause memory issues" |

---

## Thread Boundaries

| Condition | Behavior |
|-----------|----------|
| `--threads 0` | ArgumentError |
| `--threads < 0` | ArgumentError |
| `--threads > CPU cores` | Warning, proceed |

---

## stdin/stdout Boundaries

### stdin Detection

| Input | Behavior |
|-------|----------|
| `-i -` (stdin) | Auto-enable streaming mode, warn about no reordering |
| Pipe input | Same as `-i -` |

### stdout Binary

| Output | Behavior |
|--------|----------|
| `-o -` to terminal | Warning: "Binary to terminal" |
| `-o -` to pipe | Proceed |

---

## Error Message Format

```
ERROR: [Brief description]
Location: [file:line] (optional)
Reason: [Detailed explanation]
Suggestion: [Fix recommendation] (optional)
```

### Example

```
ERROR: Invalid .fqc magic header
Location: /path/to/file.fqc:1
Reason: Expected '\x89FQC\r\n\x1a\n', got '\x1f\x8b\x08...' (looks like gzip)
Suggestion: This file appears to be gzip-compressed, use gunzip first
```

---

## Exit Codes

| Code | Name | Trigger |
|------|------|---------|
| 0 | SUCCESS | Normal completion |
| 1 | ARGUMENT_ERROR | Invalid CLI arguments |
| 2 | IO_ERROR | File not found, disk full, permission denied |
| 3 | FORMAT_ERROR | Invalid FASTQ/FQC format, version mismatch |
| 4 | CHECKSUM_ERROR | Checksum validation failure |
| 5 | UNSUPPORTED_CODEC | Unknown codec family |
| 5 | MEMORY_ERROR | Memory allocation failure |

---

## Implementation Priority

### P0 - MVP Required

- ✅ Empty file handling
- ✅ Invalid magic detection
- ✅ Basic argument validation

### P1 - Production Ready

- ✅ Checksum validation
- ✅ Version compatibility
- ✅ Memory limit enforcement
- ✅ stdin/stdout handling

### P2 - Enhanced

- ⏸️ `--skip-corrupted` mode
- ⏸️ PE pair validation
- ⏸️ Long ID warning
- ⏸️ Disk space pre-check
