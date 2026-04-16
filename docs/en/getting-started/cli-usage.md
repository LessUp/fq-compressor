---
title: CLI Usage
description: Complete command-line interface reference for fq-compressor
version: 0.1.0
last_updated: 2026-04-16
language: en
---

# CLI Usage Reference

> Complete reference for fq-compressor command-line interface

## Global Options

These options apply to all commands:

| Option | Short | Description | Default |
|--------|-------|-------------|---------|
| `--help` | `-h` | Show help message | - |
| `--version` | `-v` | Show version information | - |
| `--verbose` | `-v` | Enable verbose output | false |
| `--threads` | `-t` | Number of threads to use | Auto |
| `--memory-limit` | - | Memory limit in MB | Auto |
| `--no-progress` | - | Disable progress output | false |

---

## Commands

### `compress` - Compress FASTQ to FQC

Compress one or more FASTQ files into the `.fqc` format.

```bash
fqc compress [OPTIONS] -i <input> -o <output>
```

#### Options

| Option | Short | Description | Default |
|--------|-------|-------------|---------|
| `--input` | `-i` | Input FASTQ file (required) | - |
| `--input2` | `-i2` | Second input file for paired-end | - |
| `--output` | `-o` | Output FQC file (required) | - |
| `--paired` | - | Enable paired-end mode | false |
| `--mode` | `-m` | Compression mode (fast/balanced/best) | balanced |
| `--block-size` | - | Block size in bases | 10000000 |
| `--scan-all-lengths` | - | Scan all sequence lengths | false |
| `--pipeline` | - | Use pipeline mode | false |

#### Examples

```bash
# Basic compression
fqc compress -i reads.fastq -o reads.fqc

# Parallel compression with 8 threads
fqc compress -i reads.fastq -o reads.fqc -t 8

# Paired-end (separate files)
fqc compress -i reads_1.fastq -i2 reads_2.fastq -o paired.fqc --paired

# Paired-end (interleaved)
fqc compress -i interleaved.fastq -o paired.fqc --paired

# Best compression mode (slower)
fqc compress -i reads.fastq -o reads.fqc --mode best

# Fast compression mode
fqc compress -i reads.fastq -o reads.fqc --mode fast

# Compress from compressed input
fqc compress -i reads.fastq.gz -o reads.fqc
```

#### Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Invalid arguments |
| 3 | I/O error |
| 4 | Format error |
| 5 | Memory error |

---

### `decompress` - Decompress FQC to FASTQ

Decompress an `.fqc` archive back to FASTQ format.

```bash
fqc decompress [OPTIONS] -i <input> -o <output>
```

#### Options

| Option | Short | Description | Default |
|--------|-------|-------------|---------|
| `--input` | `-i` | Input FQC file (required) | - |
| `--output` | `-o` | Output FASTQ file (required) | - |
| `--range` | - | Range of reads to decompress | All |
| `--range-pairs` | - | Range in pair mode | All |
| `--original-order` | - | Restore original read order | false |
| `--header-only` | - | Extract headers only | false |
| `--sequence-only` | - | Extract sequences only | false |
| `--quality-only` | - | Extract quality scores only | false |

#### Examples

```bash
# Full decompression
fqc decompress -i reads.fqc -o reads.fastq

# Extract reads 1000-2000
fqc decompress -i reads.fqc --range 1000:2000 -o subset.fastq

# Extract first 1000 reads
fqc decompress -i reads.fqc --range :1000 -o first_1000.fastq

# Extract from read 5000 to end
fqc decompress -i reads.fqc --range 5000: -o from_5000.fastq

# Restore original order (if reordered during compression)
fqc decompress -i reads.fqc -o reads.fastq --original-order

# Extract only headers
fqc decompress -i reads.fqc --header-only -o headers.txt
```

#### Range Syntax

| Syntax | Meaning |
|--------|---------|
| `1000:2000` | Reads 1000 to 2000 (inclusive) |
| `:1000` | First 1000 reads (0-999) |
| `5000:` | From read 5000 to end |
| `1000` | Single read 1000 |

---

### `info` - Show Archive Information

Display detailed information about an FQC archive.

```bash
fqc info [OPTIONS] <archive>
```

#### Options

| Option | Short | Description | Default |
|--------|-------|-------------|---------|
| `--json` | - | Output in JSON format | false |
| `--detailed` | `-d` | Show detailed block info | false |

#### Examples

```bash
# Basic info
fqc info archive.fqc

# JSON output for scripting
fqc info archive.fqc --json

# Detailed block information
fqc info archive.fqc --detailed
```

#### Sample Output

```
Archive: archive.fqc
  Version: 1
  Format flags: 0x0003
  Compression mode: balanced
  
  Summary:
    Total blocks: 12
    Total reads: 2270000
    Total bases: 227000000
    Paired-end: Yes
    Reordered: Yes
    
  Size:
    Compressed: 128.5 MB
    Original: 511.0 MB
    Ratio: 3.97x
    Bits/base: 4.53
    
  Blocks:
    Block 0: 189000 reads, 10.7 MB
    Block 1: 189000 reads, 10.5 MB
    ...
```

---

### `verify` - Verify Archive Integrity

Verify the integrity of an FQC archive.

```bash
fqc verify [OPTIONS] <archive>
```

#### Options

| Option | Short | Description | Default |
|--------|-------|-------------|---------|
| `--verbose` | `-v` | Verbose output | false |

#### Examples

```bash
# Verify archive
fqc verify archive.fqc

# Verbose verification
fqc verify archive.fqc -v
```

#### Sample Output

```
Verifying archive.fqc...

Structure check: PASSED
Block count: 12
Block checksums: 12/12 PASSED
Reorder map: PRESENT, VALID
Metadata: VALID

Archive integrity: VERIFIED ✓
```

---

### `split-pe` - Split Paired-End Archive

Split a paired-end FQC archive into two separate FASTQ files.

```bash
fqc split-pe [OPTIONS] -i <input> -1 <output1> -2 <output2>
```

#### Options

| Option | Short | Description | Default |
|--------|-------|-------------|---------|
| `--input` | `-i` | Input paired-end FQC (required) | - |
| `--output1` | `-1` | Output file for first pair (required) | - |
| `--output2` | `-2` | Output file for second pair (required) | - |
| `--range` | - | Range of pairs to extract | All |
| `--original-order` | - | Restore original order | false |

#### Examples

```bash
# Split paired-end archive
fqc split-pe -i paired.fqc -1 reads_1.fastq -2 reads_2.fastq

# Split with range
fqc split-pe -i paired.fqc -1 r1.fastq -2 r2.fastq --range 1000:2000
```

---

## Compression Modes

| Mode | Description | Speed | Ratio |
|------|-------------|-------|-------|
| `fast` | Fastest compression | ~15 MB/s | ~3.5x |
| `balanced` | Balanced performance | ~11 MB/s | ~4.0x |
| `best` | Best compression | ~6 MB/s | ~4.5x |

---

## Environment Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `FQC_THREADS` | Default thread count | `FQC_THREADS=8` |
| `FQC_MEMORY_LIMIT` | Default memory limit (MB) | `FQC_MEMORY_LIMIT=4096` |
| `FQC_LOG_LEVEL` | Log level (trace/debug/info/warn/error) | `FQC_LOG_LEVEL=debug` |

---

## Common Workflows

### Standard Compression Workflow

```bash
# 1. Compress
fqc compress -i input.fastq -o output.fqc -t 8

# 2. Verify
fqc verify output.fqc

# 3. Check info
fqc info output.fqc

# 4. Decompress when needed
fqc decompress -i output.fqc -o restored.fastq
```

### Paired-End Workflow

```bash
# Compress paired-end data
fqc compress -i reads_1.fastq -i2 reads_2.fastq -o paired.fqc --paired -t 8

# Split back to separate files
fqc split-pe -i paired.fqc -1 out_1.fastq -2 out_2.fastq
```

### Random Access Workflow

```bash
# Compress once
fqc compress -i large.fastq -o large.fqc

# Extract subsets as needed
fqc decompress -i large.fqc --range 0:10000 -o chunk_0.fastq
fqc decompress -i large.fqc --range 10000:20000 -o chunk_1.fastq
```

---

**🌐 Language**: [English (current)](./cli-usage.md) | [简体中文](../../zh/getting-started/cli-usage.md)
