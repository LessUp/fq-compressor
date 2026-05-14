---
title: I/O and memory
description: Input handling, buffering, and memory-budget boundaries
---

# I/O and memory

fq-compressor treats I/O and memory control as first-class architecture concerns.
The relevant implementation lives under `include/fqc/io/`, `include/fqc/common/memory_budget.h`, and the pipeline coordination code in `include/fqc/pipeline/`.

## Input side

The input path is designed for streaming rather than preload.
`include/fqc/io/compressed_stream.h` detects plain FASTQ, gzip, bzip2, and xz sources and exposes them through a uniform input stream.
`include/fqc/io/fastq_parser.h` then reads standard four-line FASTQ records into bounded chunks.

That design does two things:

- it keeps compressed input support out of the compression algorithms themselves
- it lets the parser stay record-aware while the transport layer stays byte-oriented

## Output side

Archive output is also buffered and staged.
`include/fqc/format/fqc_writer.h` appends blocks, records their offsets, then finalizes the index and footer once all payload sizes are known.
Because offsets are only authoritative at finalize time, the writer owns archive layout and checksum bookkeeping rather than the worker threads.

## Async I/O support

`include/fqc/io/async_io.h` defines buffer-pool-based async readers and writers.
The model is explicit: reusable buffers, bounded prefetch depth, and write-behind instead of unlimited queue growth.
Even where the current command path stays conservative, the abstractions make overlap possible without changing parser or codec interfaces.

## Memory budgeting model

`include/fqc/common/memory_budget.h` documents the memory model directly:

- phase 1 global analysis is estimated at about 24 bytes per read
- phase 2 block work is estimated at about 50 bytes per read per active block
- the default budget is split across analysis reserve, block buffers, and worker stack allowance

The `MemoryEstimator` and `ChunkPlanner` exist to decide whether a run fits in one pass or needs divide-and-conquer chunking.
That is important for large datasets: the tool would rather split work explicitly than let in-flight buffers grow until the process is unstable.

## Backpressure and bounded concurrency

The pipeline boundary enforces memory discipline in two places:

1. `include/fqc/pipeline/pipeline.h` limits in-flight blocks.
2. `include/fqc/io/async_io.h` limits buffer-pool depth.

Together they cap how much unread input, uncompressed chunk state, and compressed output can exist at once.
This is why the project can talk about parallel throughput and still keep predictable memory ceilings.

## Operational implication

The `--memory-limit` CLI option is not a cosmetic knob.
It feeds the budget model that decides chunk sizing, reserve allocation, and whether the run should fall back to chunked execution.
For operators, that means memory policy is part of normal compression behavior, not an afterthought.

## Code anchors

- Transparent input streams: `include/fqc/io/compressed_stream.h`, `src/io/compressed_stream.cpp`
- FASTQ parsing: `include/fqc/io/fastq_parser.h`, `src/io/fastq_parser.cpp`
- Async buffer pools: `include/fqc/io/async_io.h`, `src/io/async_io.cpp`
- Budgeting and chunking: `include/fqc/common/memory_budget.h`, `src/common/memory_budget.cpp`
- Pipeline backpressure: `include/fqc/pipeline/pipeline.h`, `src/pipeline/pipeline.cpp`
