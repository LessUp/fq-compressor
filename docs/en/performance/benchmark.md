---
title: Performance Benchmarking
description: Comprehensive benchmarking guide for fq-compressor
version: 0.1.0
last_updated: 2026-04-16
language: en
---

# Performance Benchmarking

> Evaluate fq-compressor performance on your data

## Overview

fq-compressor includes comprehensive benchmarking tools to evaluate:

- **Compression Ratio**: How much smaller is the compressed data?
- **Speed**: Throughput for compression and decompression
- **Memory Usage**: Peak memory consumption
- **Scalability**: Performance scaling with thread count

## Benchmark Suite

### Quick Benchmark

```bash
# Simple single-run benchmark
./scripts/benchmark.sh --quick --input data.fastq
```

### Full Benchmark

```bash
# Comprehensive benchmark with multiple runs
./scripts/benchmark.sh --full --input data.fastq
```

## Metrics

### Compression Metrics

| Metric | Description | Target |
|--------|-------------|--------|
| **Compression Ratio** | Original / Compressed | 3.5-5x |
| **Bits per Base** | Compressed bits per nucleotide | 0.4-0.6 |
| **Speed** | MB/s of uncompressed data | >10 MB/s |
| **Memory** | Peak RAM usage | <2 GB (100MB file) |

### Decompression Metrics

| Metric | Description | Target |
|--------|-------------|--------|
| **Speed** | MB/s of compressed data | >50 MB/s |
| **Memory** | Peak RAM usage | <500 MB |

## GCC vs Clang Comparison

The benchmark framework can compare compiler builds:

```bash
# Build both versions
./scripts/build.sh gcc-release
./scripts/build.sh clang-release

# Run comparison
python3 benchmark/compiler_benchmark.py \
    --input test_data.fastq.gz \
    --gcc-binary build/gcc-release/src/fqc \
    --clang-binary build/clang-release/src/fqc \
    --threads 1 4 8 \
    --runs 3 \
    --visualize
```

### Expected Results

| Compiler | Compression | Decompression | Notes |
|----------|-------------|---------------|-------|
| GCC 15.2 | ~11.3 MB/s | ~60 MB/s | Stable |
| Clang 21 | ~11.9 MB/s | ~62 MB/s | +5% faster |

## Multi-Tool Comparison

Compare fq-compressor against other tools:

```bash
python3 benchmark/benchmark.py \
    --input data.fastq \
    --tools fqc,gzip,pigz,zstd \
    --threads 1,4,8 \
    --output-dir results/
```

### Typical Comparison

| Tool | Ratio | Compress | Decompress | Parallel |
|------|-------|----------|------------|----------|
| fq-compressor | 3.97x | 11.9 MB/s | 62.3 MB/s | ✓ |
| gzip | 2.1x | 45 MB/s | 180 MB/s | ✗ |
| pigz | 2.1x | 150 MB/s | 180 MB/s | ✓ |
| zstd | 2.5x | 220 MB/s | 500 MB/s | ✗ |

## Interpreting Results

### Speed vs Ratio Trade-off

fq-compressor prioritizes compression ratio over raw speed:

```
High Compression ──────────────────► Fast Decompression
     │                                       │
  fq-compressor                          gzip
  Spring                                 pigz
  fqzcomp5                               zstd
```

### Memory Scaling

Memory usage should scale roughly linearly with thread count:

```
Threads:  1    2    4    8    16
Memory:   500M 800M 1.2G 1.8G 2.5G (example)
```

Use `--memory-limit` to constrain usage.

## Continuous Benchmarking

### CI Integration

Add to GitHub Actions:

```yaml
- name: Benchmark
  run: |
    python3 benchmark/compiler_benchmark.py \
        --input benchmarks/test_data.fq.gz \
        --gcc-binary build/gcc-release/src/fqc \
        --clang-binary build/clang-release/src/fqc \
        --output-dir benchmark-results/
    
- name: Upload Results
  uses: actions/upload-artifact@v4
  with:
    name: benchmark-results
    path: benchmark-results/
```

### Performance Regression Detection

```bash
# Compare against baseline
python3 benchmark/compare_baseline.py \
    --baseline results/baseline.json \
    --current results/current.json \
    --threshold 5%  # Fail if >5% regression
```

## Benchmarking Best Practices

### Data Selection

- Use representative datasets from your domain
- Include both small (<100MB) and large (>1GB) files
- Test paired-end and single-end data

### Environment

- Close other CPU-intensive applications
- Ensure sufficient RAM (avoid swapping)
- Use warm cache (run once before timing)
- Run multiple iterations for accuracy

### Reporting

Include in reports:
- Hardware specs (CPU, RAM, storage)
- Software versions (OS, compiler, fqc)
- Dataset characteristics (size, read count, read length)
- Command-line options used

## Troubleshooting

### Variable Results

- Ensure CPU frequency scaling is disabled
- Check for thermal throttling
- Run with `taskset` to pin to specific cores

### Out of Memory

```bash
# Reduce parallelism or block size
fqc compress -i large.fastq -o out.fqc --threads 4 --memory-limit 4096
```

---

**🌐 Language**: [English (current)](./benchmark.md)
