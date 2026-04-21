---
title: Quick Start
description: Get started with fq-compressor in 5 minutes - from first compression to advanced usage
version: 0.2.0
last_updated: 2026-04-22
language: en
---

# Quick Start

> 🚀 Compress your first FASTQ file in under 5 minutes

## Prerequisites

- [fq-compressor installed](./installation.md)
- A FASTQ file to compress (or use the example below)

## Step 1: Create a Test File

If you don't have a FASTQ file handy, create a small test file:

```bash
# Create a minimal FASTQ file
cat > sample.fastq << 'EOF'
@read_1
ACGTACGTACGTACGTACGTACGTACGTACGTACGT
+
IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
@read_2
TGCATGCATGCATGCATGCATGCATGCATGCATGCA
+
IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
@read_3
AAAATTTTCCCCGGGGAAAATTTTCCCCGGGGAAAA
+
IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
EOF
```

## Step 2: Compress

Compress your FASTQ file to `.fqc` format:

```bash
# Basic compression
fqc compress -i sample.fastq -o sample.fqc

# With progress output
fqc compress -i sample.fastq -o sample.fqc -v

# Output:
# [2026-04-16 10:30:15] INFO: Starting compression...
# [2026-04-16 10:30:15] INFO: Compression complete: 108 bytes -> 67 bytes (1.61x)
```

## Step 3: Verify

Verify the integrity of the compressed file:

```bash
fqc verify sample.fqc

# Output:
# [2026-04-16 10:30:20] INFO: Verification successful
# Archive: sample.fqc
#   Total blocks: 1
#   Total reads: 3
#   Checksums: PASSED
```

## Step 4: View Info

Get detailed information about the archive:

```bash
fqc info sample.fqc

# Output:
# Archive: sample.fqc
#   Version: 1
#   Blocks: 1
#   Total reads: 3
#   Total bases: 108
#   Compressed size: 67 bytes
#   Original size: 108 bytes
#   Compression ratio: 1.61x
```

For JSON output (useful for scripts):

```bash
fqc info sample.fqc --json
```

## Step 5: Decompress

Restore the original FASTQ file:

```bash
# Full decompression
fqc decompress -i sample.fqc -o restored.fastq

# Compare with original
diff sample.fastq restored.fastq
# (no output means files are identical)
```

## Advanced Quick Examples

### Paired-End Compression

```bash
# Separate files
fqc compress -i reads_R1.fastq -i2 reads_R2.fastq -o paired.fqc --paired

# One interleaved file
fqc compress -i interleaved.fastq -o paired.fqc --paired
```

### Parallel Compression (Faster)

```bash
# Use 8 threads
fqc compress -i large.fastq -o large.fqc --threads 8

# Limit memory usage
fqc compress -i large.fastq -o large.fqc --threads 8 --memory-limit 4096
```

### Random Access (Partial Decompression)

```bash
# Extract only reads 1000-2000
fqc decompress -i large.fqc --range 1000:2000 -o subset.fastq

# Extract first 1000 reads
fqc decompress -i large.fqc --range :1000 -o first_1000.fastq

# Extract from read 5000 to end
fqc decompress -i large.fqc --range 5000: -o from_5000.fastq
```

### Compressed Input Support

```bash
# Compress directly from .gz or .bz2 files
fqc compress -i input.fastq.gz -o output.fqc
fqc compress -i input.fastq.bz2 -o output.fqc
fqc compress -i input.fastq.xz -o output.fqc
```

## Workflow Summary

```bash
# Typical workflow
fqc compress -i input.fastq -o output.fqc          # 1. Compress
fqc verify output.fqc                                # 2. Verify
fqc info output.fqc                                  # 3. Inspect
fqc decompress -i output.fqc -o restored.fastq      # 4. Decompress
```

## Common Options Reference

| Option | Description | Example |
|--------|-------------|---------|
| `-i, --input` | Input FASTQ file | `-i reads.fastq` |
| `-o, --output` | Output file | `-o reads.fqc` |
| `-i2` | Second input for paired-end | `-i2 reads_2.fastq` |
| `--paired` | Enable paired-end mode | `--paired` |
| `-t, --threads` | Number of threads | `-t 8` |
| `--memory-limit` | Memory limit in MB | `--memory-limit 4096` |
| `--range` | Range for partial decompression | `--range 1000:2000` |
| `-v, --verbose` | Verbose output | `-v` |
| `-h, --help` | Show help | `-h` |

## Performance Tips

1. **Use multiple threads** for large files: `--threads 8`
2. **Set a memory limit** if needed: `--memory-limit 4096`
3. **Compress directly from .gz** to avoid intermediate files
4. **Use random access** instead of full decompression when you only need a subset

## Next Steps

- [CLI Usage Guide](./cli-usage.md) - Complete command reference
- [Core Algorithms](../core-concepts/algorithms.md) - Learn how compression works
- [Performance Benchmarks](../performance/benchmark.md) - See detailed performance metrics

---

**🌐 Language**: [English (current)](./quickstart.md) | [简体中文](../../zh/getting-started/quickstart.md)
