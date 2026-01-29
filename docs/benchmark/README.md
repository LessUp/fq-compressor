# fq-compressor Benchmark Results

This directory contains benchmark results comparing GCC and Clang builds of fq-compressor.

## Directory Structure

```
benchmark/
├── README.md                    # This file
└── results/                     # Benchmark results
    ├── results_TIMESTAMP.json   # Raw JSON data
    ├── report_TIMESTAMP.md      # Markdown report
    └── charts_TIMESTAMP.html    # Interactive visualizations
```

## Running Benchmarks

### Quick Benchmark (1 iteration)
```bash
./scripts/benchmark.sh --quick
```

### Full Benchmark (5 iterations)
```bash
./scripts/benchmark.sh --full
```

### Test Specific Compiler
```bash
./scripts/benchmark.sh --gcc-only
./scripts/benchmark.sh --clang-only
```

## Benchmark Metrics

### Compression Performance
- **Throughput**: MB/s of input data processed
- **Compression Ratio**: Original size / Compressed size
- **Execution Time**: Total time for compression

### Decompression Performance
- **Throughput**: MB/s of output data produced
- **Execution Time**: Total time for decompression

## Test Data

Benchmarks use real Illumina sequencing data:
- `E150035817_L01_1201_1.sub10.fq.gz` (157 MB)
- `E150035817_L01_1201_2.sub10.fq.gz` (165 MB)

Total: ~2.27M reads, ~322 MB compressed

## Interpreting Results

### Throughput
Higher is better. Indicates how fast the compressor can process data.

### Compression Ratio
Higher is better. Indicates how much space is saved.

### Execution Time
Lower is better. Indicates total time to complete operation.

## Historical Results

See the `results/` directory for historical benchmark data.

Latest results are automatically linked in the main README.md.
