# Distribution & Best Practices

## Why No Built-in `.fqc.gz` Output?

The `.fqc` format is **already highly compressed** using domain-specific algorithms. Adding an external compression layer provides minimal benefit while introducing significant drawbacks:

1. **Marginal Compression Gain** — External compression typically adds only 1-5% on already-compressed `.fqc` files.
2. **Breaks Random Access** — The `.fqc` format supports O(1) random access. External compression destroys this capability.
3. **Increased Latency** — Decompression requires two passes (external + FQC).
4. **Complexity** — Managing nested compression adds operational overhead.

## Recommended Workflows

### For Network Transfer

If you need to distribute `.fqc` files and want additional compression for transfer:

```bash
# Compress for distribution
xz -9 archive.fqc              # Creates archive.fqc.xz

# Before use, decompress the outer layer
xz -d archive.fqc.xz           # Restores archive.fqc

# Now use fqc normally with random access
fqc decompress -i archive.fqc --range 1000:2000 -o subset.fastq
```

### Scenario Guide

| Scenario | Recommendation |
|----------|----------------|
| **Active Analysis** | Use `.fqc` directly (random access enabled) |
| **Long-term Archive** | Use `.fqc` directly (already near-optimal compression) |
| **Network Transfer** | Optionally wrap with `xz -9` or `zstd -19`, unwrap before use |
| **Cloud Storage** | Use `.fqc` directly (cloud providers often compress at rest) |

## Compression Efficiency

The `.fqc` format achieves compression ratios of **0.4–0.6 bits/base**, which is already close to the theoretical entropy limit for genomic data. External compression cannot significantly improve upon this.

### Comparison with General-purpose Compressors

| Tool | Bits/Base | Random Access | Parallel |
|------|-----------|---------------|----------|
| **fqc** | 0.4–0.6 | ✅ Yes | ✅ Yes |
| gzip | 1.8–2.2 | ❌ No | ❌ No |
| pigz | 1.8–2.2 | ❌ No | ✅ Yes |
| xz | 1.2–1.6 | ❌ No | ❌ No |
| zstd | 1.4–1.8 | ❌ No | ✅ Yes |

## Integration Tips

### Bioinformatics Pipelines

```bash
# Compress after sequencing
fqc compress -i raw_reads.fastq -o reads.fqc -t 8

# Extract a subset for QC
fqc decompress -i reads.fqc --range 0:10000 -o qc_subset.fastq
fastqc qc_subset.fastq

# Full decompression for alignment
fqc decompress -i reads.fqc -o reads.fastq
bwa mem ref.fa reads.fastq | samtools sort -o aligned.bam
```

### Paired-End Workflow

```bash
# Compress PE data
fqc compress -i R1.fastq.gz -I R2.fastq.gz -o sample.fqc

# Decompress PE data
fqc decompress -i sample.fqc -o R1.fastq -O R2.fastq
```
