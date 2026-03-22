# FAQ

## General

### What is fq-compressor?

fq-compressor is a high-performance FASTQ compression tool that uses domain-specific algorithms (Assembly-based Compression for sequences, Statistical Context Mixing for quality scores) to achieve near-theoretical-limit compression ratios while supporting random access and parallel processing.

### How does it compare to gzip/zstd?

General-purpose compressors treat FASTQ data as arbitrary text. fq-compressor exploits the biological structure of sequencing data to achieve 3-5x better compression ratios. See the [Benchmarks](benchmark.md) page for detailed comparisons.

### Is it lossless?

Yes. fq-compressor performs fully lossless compression by default. Every base, quality score, and identifier is preserved exactly.

---

## Compatibility

### What FASTQ formats are supported?

- Standard 4-line FASTQ (`.fastq`, `.fq`)
- Gzip-compressed FASTQ (`.fastq.gz`, `.fq.gz`)
- Bzip2-compressed FASTQ (`.fastq.bz2`, `.fq.bz2`)
- XZ-compressed FASTQ (`.fastq.xz`, `.fq.xz`)
- Single-end (SE) and paired-end (PE) data
- Illumina, BGI, and other common identifier formats

> Zstandard-compressed FASTQ input (`.zst`) is not supported yet.

### What read lengths are supported?

fq-compressor handles all read lengths, but uses different strategies:

| Read Length | Strategy |
|-------------|----------|
| Short (≤ 511 bp) | Full ABC compression with reordering |
| Medium (512 bp – 10 KB) | Zstd compression, no reordering |
| Long (> 10 KB) | Zstd compression, no reordering |

### Can I use `.fqc` files with other tools?

No — `.fqc` is a custom format. You need to decompress with `fqc decompress` before using standard bioinformatics tools. The random access feature makes it easy to extract subsets without full decompression.

---

## Performance

### How many threads should I use?

For best results, use the number of physical CPU cores. The `--pipeline` flag is recommended for files larger than 100 MB.

```bash
fqc compress -i reads.fastq -o reads.fqc -t 8 --pipeline
```

### How much memory does it need?

Memory usage depends on the block size and number of in-flight blocks. The default memory limit is 8 GB, adjustable via `--memory-limit`:

```bash
fqc compress -i reads.fastq -o reads.fqc --memory-limit 4096  # 4 GB
```

### Why is compression slower than decompression?

Compression involves computationally expensive operations (minimizer extraction, read reordering, consensus generation) that don't have equivalents during decompression. This asymmetry is by design — compress once, decompress many times.

---

## Troubleshooting

### "Format error" when decompressing

Ensure the file was created by fq-compressor and hasn't been corrupted:

```bash
fqc verify archive.fqc
```

### Out of memory during compression

Reduce the memory limit to force smaller block sizes:

```bash
fqc compress -i reads.fastq -o reads.fqc --memory-limit 2048
```

### Build fails with "TBB not found"

Make sure Conan installed dependencies correctly:

```bash
conan install . --build=missing -of=build
```

---

## Academic

### How should I cite fq-compressor?

If you use fq-compressor in your research, please cite the GitHub repository:

```
LessUp. fq-compressor: High-performance FASTQ compression with random access.
https://github.com/LessUp/fq-compressor
```

### What algorithms does it build on?

- **Spring** (Chandak et al., 2019) — Assembly-based sequence compression
- **Mincom** (Liu et al.) — Minimizer bucketing
- **fqzcomp5** (Bonfield) — Quality score context mixing
- **HARC** (Chandak et al., 2018) — Haplotype-aware reordering
