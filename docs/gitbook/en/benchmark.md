# Benchmarks

fq-compressor includes a comprehensive benchmarking framework for evaluating compiler optimizations and multi-dimensional performance analysis.

## Latest Results

| Compiler | Compression | Decompression | Ratio | Bits/Base |
|----------|-------------|---------------|-------|-----------|
| GCC 13   | 11.30 MB/s  | 60.10 MB/s    | 3.97x | ~2.01     |
| Clang 16 | 11.90 MB/s  | 62.30 MB/s    | 3.97x | ~2.01     |

*Test environment: Intel Core i7-9700 @ 3.00GHz, 8 cores, 2.27M Illumina reads (511 MB uncompressed)*

## Key Metrics

| Metric | Description |
|--------|-------------|
| **Compression Speed (MB/s)** | Throughput during compression |
| **Decompression Speed (MB/s)** | Throughput during decompression |
| **Compression Ratio** | Original size / Compressed size |
| **Bits per Base** | Compressed bits per DNA base |
| **Peak Memory (MB)** | Maximum memory during operation |
| **Scalability** | Performance gains with multiple threads |
| **Parallel Efficiency** | Actual speedup / Ideal speedup |

## Running Benchmarks

### Compiler Comparison

```bash
# Build both GCC and Clang Release versions
./scripts/build.sh gcc-release 4
./scripts/build.sh clang-release 4

# Run compiler performance comparison with visualization
python3 benchmark/compiler_benchmark.py \
  --input data/reads.fastq.gz \
  --gcc-binary build/gcc-release/src/fqc \
  --clang-binary build/clang-release/src/fqc \
  -t 1 4 8 \
  -r 3 \
  --output-dir docs/benchmark \
  --visualize
```

### Standard Benchmark

```bash
# Run the benchmark suite
python3 benchmark/benchmark.py \
  --input data/reads.fastq.gz \
  --binary build/clang-release/src/fqc \
  -t 4 \
  -r 5

# Generate visualizations
python3 benchmark/visualize_benchmark.py \
  --input docs/benchmark/results/benchmark-results.json
```

## Generated Reports

The benchmarking framework automatically generates:

| Output | Description |
|--------|-------------|
| `benchmark-report.html` | Comprehensive HTML report with embedded visualizations |
| `benchmark-report.md` | Markdown format report |
| `benchmark-results.json` | Raw data in JSON format |
| `compression-speed.png` | Compression throughput comparison |
| `decompression-speed.png` | Decompression throughput comparison |
| `compression-ratio.png` | Compression efficiency comparison |
| `memory-usage.png` | Peak memory usage analysis |
| `scalability.png` | Parallel scalability metrics |

## Scalability

| Threads | Compression | Decompression | Efficiency |
|---------|-------------|---------------|------------|
| 1       | ~6 MB/s     | ~30 MB/s      | 100%       |
| 4       | ~12 MB/s    | ~60 MB/s      | ~50%       |
| 8       | ~18 MB/s    | ~90 MB/s      | ~38%       |

*Efficiency = (actual speedup / ideal speedup). Sub-linear scaling is expected due to I/O bottlenecks and memory bandwidth.*
