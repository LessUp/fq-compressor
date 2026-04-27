---
title: Frequently Asked Questions
description: Common questions and answers about fq-compressor
version: 0.2.0
last_updated: 2026-04-22
language: en
---

# Frequently Asked Questions

## General

### What is fq-compressor?

fq-compressor is a high-performance FASTQ compression tool designed for genomic sequencing data. It uses domain-specific algorithms (ABC for sequences, SCM for quality scores) to achieve compression ratios 2-3x better than general-purpose compressors like gzip.

### How does it compare to other FASTQ compressors?

| Tool | Ratio | Speed | Random Access | Parallel |
|------|-------|-------|---------------|----------|
| fq-compressor | 3.5-4x | Medium | ✓ | ✓ |
| Spring | 4-5x | Slow | ✗ | ✓ |
| fqzcomp5 | 3-4x | Medium | ✗ | ✗ |
| gzip | 2-3x | Fast | ✗ | ✗ |

### Is it lossless?

Yes, by default fq-compressor is completely lossless. Optional lossy quality compression modes are available.

## Installation

### What platforms are supported?

- Linux x86_64 (glibc 2.31+, musl)
- Linux ARM64/AArch64
- macOS x86_64 (Intel)
- macOS ARM64 (Apple Silicon)

### Do I need to build from source?

No. Pre-built binaries are available for all supported platforms. Build from source only if you need:
- Latest development version
- Custom optimizations
- Debug symbols

### What are the runtime dependencies?

Static builds (musl) have **no runtime dependencies**. Dynamic builds (glibc) require:
- glibc 2.31+ (Linux)
- Standard C++ library

## Usage

### Can I compress already-compressed files?

Yes, fq-compressor can read .gz, .bz2, and .xz files directly:

```bash
fqc compress -i reads.fastq.gz -o reads.fqc
```

### How do I use paired-end data?

Two modes supported:

```bash
# Separate files
fqc compress -i reads_1.fastq -2 reads_2.fastq -o paired.fqc --paired

# Interleaved file
fqc compress -i interleaved.fastq -o paired.fqc --paired
```

### What compression mode should I use?

| Mode | Use Case |
|------|----------|
| `fast` | Quick compression, slightly lower ratio |
| `balanced` | Default, good trade-off |
| `best` | Maximum compression, slower |

## Performance

### How much memory does it need?

Memory scales with:
- Dataset size
- Thread count
- Block size

Rule of thumb: ~2-4GB for typical datasets with 8 threads.

Use `--memory-limit` to constrain RAM usage.

### Can I use all CPU cores?

Yes, fq-compressor scales well with thread count:

```bash
fqc compress -i data.fastq -o out.fqc --threads $(nproc)
```

### Why is decompression faster than compression?

Compression involves:
- Minimizer calculation
- Reordering (TSP)
- Consensus generation
- Delta encoding

Decompression reverses these but with simpler operations.

## Features

### What is "random access"?

The ability to extract a subset of reads without decompressing the entire file:

```bash
# Extract only reads 1000-2000
fqc decompress -i archive.fqc --range 1000:2000 -o subset.fastq
```

This is O(1) - constant time regardless of archive size.

### Can I restore the original read order?

Yes, if the archive was created with reordering enabled:

```bash
fqc decompress -i archive.fqc -o restored.fastq --original-order
```

### Is the format stable?

The FQC format has version markers for forward compatibility. Archives created with v0.1.0 will be readable by future versions.

## Troubleshooting

### "Command not found" after installation

```bash
# Ensure fqc is in PATH
echo $PATH | grep /usr/local/bin

# Or use full path
/usr/local/bin/fqc --version
```

### "Permission denied"

```bash
chmod +x /usr/local/bin/fqc
```

### Compression is slow

- Ensure you're using release build (not debug)
- Use multiple threads: `--threads 8`
- Consider `fast` mode for speed-critical applications

### Out of memory errors

```bash
# Limit memory usage
fqc compress -i large.fastq -o out.fqc --memory-limit 2048

# Reduce threads
fqc compress -i large.fastq -o out.fqc --threads 4
```

## Contributing

### How can I contribute?

See our [Contributing Guide](../development/contributing.md).

### Where can I report bugs?

[GitHub Issues](https://github.com/LessUp/fq-compressor/issues)

### Is there a development roadmap?

Check [GitHub Discussions](https://github.com/LessUp/fq-compressor/discussions) for planned features.

## Licensing

### Can I use fq-compressor commercially?

Yes. The project code is MIT licensed.

Note: `vendor/spring-core/` contains third-party code under a separate research license.

### What is the patent situation?

fq-compressor does not implement any patented algorithms. The ABC and SCM algorithms are published academic research.

---

**Don't see your question?** [Open an issue](https://github.com/LessUp/fq-compressor/issues/new) or [start a discussion](https://github.com/LessUp/fq-compressor/discussions/new).
