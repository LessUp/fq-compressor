# Parallel Pipeline

fq-compressor uses **Intel Threading Building Blocks (oneTBB)** to implement a high-throughput parallel processing pipeline.

## Pipeline Architecture

The compression and decompression workflows follow a 3-stage pipeline pattern:

### Compression Pipeline

```
┌─────────┐    ┌─────────────┐    ┌─────────┐
│  Reader  │───►│ Compressor  │───►│  Writer  │
│ (Stage 1)│    │  (Stage 2)  │    │ (Stage 3)│
└─────────┘    └─────────────┘    └─────────┘
     │               │                  │
  Read FASTQ    ABC + SCM +         Write .fqc
  in chunks     Tokenize            blocks
```

### Decompression Pipeline

```
┌─────────┐    ┌───────────────┐    ┌─────────┐
│  Reader  │───►│ Decompressor  │───►│  Writer  │
│ (Stage 1)│    │   (Stage 2)   │    │ (Stage 3)│
└─────────┘    └───────────────┘    └─────────┘
     │                │                   │
  Read .fqc      Decode blocks       Write FASTQ
  blocks         in parallel         output
```

## Flow Control

### Token-Based Memory Management

The pipeline uses a **token system** to control memory usage:

- A fixed number of tokens circulate through the pipeline
- Each token represents one block "in flight"
- When all tokens are in use, the reader stage blocks until a token is freed
- This prevents unbounded memory growth on large files

### Work Stealing

TBB's work-stealing scheduler automatically balances:
- **Heavy tasks** (ABC reordering, SCM encoding) across available cores
- **Light tasks** (I/O) are not over-scheduled
- No manual thread management required

## Async I/O Integration

The writer stage uses **double-buffered async I/O** for maximum throughput:

```
┌──────────────┐     ┌──────────────┐
│  Buffer A    │◄───►│  Buffer B    │
│  (filling)   │     │  (flushing)  │
└──────────────┘     └──────────────┘
       │                    │
   Compressor           Disk I/O
   writes here          writes here
```

- While one buffer is being filled by the compressor, the other is being flushed to disk
- 4 MB write-behind buffers minimize system call overhead
- Implemented via `AsyncWriter` in `src/io/async_io.rs` (Rust) / `src/io/async_io.h` (C++)

## Enabling Pipeline Mode

```bash
# Compression with pipeline
fqc compress -i input.fastq -o output.fqc --pipeline

# Decompression with pipeline
fqc decompress -i output.fqc -o restored.fastq --pipeline
```

Pipeline mode is recommended for files larger than 100 MB where the parallelization overhead is amortized.

## Performance Impact

| Mode | Compression Speed | Decompression Speed |
|------|------------------|---------------------|
| Sequential | ~6 MB/s | ~30 MB/s |
| Pipeline (4 threads) | ~12 MB/s | ~60 MB/s |
| Pipeline (8 threads) | ~18 MB/s | ~90 MB/s |

*Approximate values; actual performance depends on hardware and data characteristics.*
