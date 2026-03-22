# Quick Start

This guide walks you through compressing and decompressing your first FASTQ file.

## Compress a FASTQ File

```bash
fqc compress -i input.fastq -o output.fqc
```

This reads `input.fastq`, applies the full compression pipeline (ABC for sequences, SCM for quality scores, tokenization for identifiers), and writes the compressed archive to `output.fqc`.

### Common Options

```bash
# Specify thread count
fqc compress -i input.fastq -o output.fqc -t 8

# Set compression level (1-9, default: 5)
fqc compress -i input.fastq -o output.fqc -l 9

# Set memory limit (in MB)
fqc compress -i input.fastq -o output.fqc --memory-limit 4096

# Compress gzipped FASTQ directly
fqc compress -i input.fastq.gz -o output.fqc

# bzip2 and xz inputs are also supported
fqc compress -i input.fastq.bz2 -o output.fqc
fqc compress -i input.fastq.xz -o output.fqc
```

> Note: Zstandard-compressed FASTQ input (`.zst`) is not supported yet.

## Decompress an Archive

```bash
fqc decompress -i output.fqc -o restored.fastq
```

### Random Access Decompression

One of fq-compressor's key features is O(1) random access:

```bash
# Extract reads 1000 to 2000
fqc decompress -i output.fqc --range 1000:2000 -o subset.fastq
```

## Inspect an Archive

```bash
fqc info output.fqc
```

This displays metadata about the compressed archive:
- Number of reads
- Original and compressed size
- Compression ratio and bits per base
- Block count and index information
- Read length statistics

## Verify Integrity

```bash
fqc verify output.fqc
```

Performs a full integrity check of the archive, verifying:
- File header and footer checksums
- Block-level CRC validation
- Index consistency

## Paired-End (PE) Data

```bash
# Compress paired-end files
fqc compress -i read1.fastq -I read2.fastq -o paired.fqc

# Decompress paired-end
fqc decompress -i paired.fqc -o read1_out.fastq -O read2_out.fastq
```

## Pipeline Mode

For maximum throughput on large files, use the pipeline mode:

```bash
fqc compress -i input.fastq -o output.fqc --pipeline
fqc decompress -i output.fqc -o restored.fastq --pipeline
```

This enables the full 3-stage parallel pipeline (read → compress → write) using Intel oneTBB.
