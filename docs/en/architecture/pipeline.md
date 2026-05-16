---
title: Pipeline
description: Compression and decompression pipeline boundaries
---

# Pipeline

fq-compressor is organized around a bounded block pipeline: serial ingest, parallel block work, and serial archive output.
That split is visible in the public pipeline interfaces under `include/fqc/pipeline/` and in the command entry points under `src/commands/`.

## Compression path

The compression command wires together five responsibilities:

1. **Input opening and FASTQ parsing** via `include/fqc/io/fastq_parser.h` and `include/fqc/io/compressed_stream.h`.
2. **Global analysis** via `include/fqc/algo/global_analyzer.h`, which decides how reads should be grouped and whether reorder metadata is needed.
3. **Chunk marshaling** via `include/fqc/pipeline/pipeline.h`, which turns parsed records into per-stream views.
4. **Block compression** via `include/fqc/algo/block_compressor.h`, `include/fqc/algo/id_compressor.h`, and `include/fqc/algo/quality_compressor.h`.
5. **Archive writing** via `include/fqc/format/fqc_writer.h` and `include/fqc/pipeline/writer_node.h`.

In practical terms the flow is:

```mermaid
flowchart LR
    A[FASTQ or compressed FASTQ] --> B[Parser]
    B --> C[Global analyzer and reorder planning]
    C --> D[Read chunks]
    D --> E[Parallel block compression]
    E --> F[FQC writer]
    F --> G[.fqc archive]
```

The important boundary is the block.
Analysis may look across the full input, but payload encoding and archive storage stay block-local so compression work can scale across cores without giving up direct block lookup later.

## Decompression path

Decompression mirrors the same boundary in reverse:

```mermaid
flowchart LR
    A[.fqc archive] --> B[FQC reader]
    B --> C[Block lookup]
    C --> D[Parallel stream decode]
    D --> E[Optional original-order restore]
    E --> F[FASTQ writer]
```

The reader side is anchored in `include/fqc/format/fqc_reader.h`, `include/fqc/pipeline/fqc_reader_node.h`, and `include/fqc/pipeline/decompressor_node.h`.
`src/commands/decompress_command.cpp` decides whether the run is a full restore or a targeted extraction.

## Why the pipeline is split this way

### Serial ingress and egress

FASTQ parsing and final archive writes are naturally ordered operations.
Keeping those stages serial avoids cross-thread coordination around record framing, footer updates, and final output order.

### Parallel block work

Once a chunk has a stable archive-order read range, each block can be compressed or decompressed independently.
That is why `include/fqc/pipeline/pipeline.h` models `ReadChunk` and `CompressedBlock` as complete units of work.

### Explicit backpressure

The pipeline is designed to stop unlimited in-flight buffering.
`include/fqc/pipeline/pipeline.h` defines `kDefaultMaxInFlightBlocks`, while memory budgeting in `include/fqc/common/memory_budget.h` controls how much data the pipeline should hold at once.

## Relationship to the command layer

The CLI is thin.
`src/main.cpp` parses options, while `src/commands/compress_command.cpp` and `src/commands/decompress_command.cpp` translate CLI choices into pipeline configuration: threads, memory limit, order preservation, verification, and output mode.

That separation keeps most operational behavior in reusable library code instead of in CLI-specific branches.

## Code anchors

- Pipeline types and coordination: `include/fqc/pipeline/pipeline.h`, `src/pipeline/pipeline.cpp`
- Compression nodes: `include/fqc/pipeline/reader_node.h`, `include/fqc/pipeline/writer_node.h`
- Decompression nodes: `include/fqc/pipeline/fqc_reader_node.h`, `include/fqc/pipeline/decompressor_node.h`, `include/fqc/pipeline/fastq_writer_node.h`
- Command wiring: `src/commands/compress_command.cpp`, `src/commands/decompress_command.cpp`

## Continue with

- [FQC format and random access](/en/architecture/format-random-access) to see why block boundaries matter externally
- [Academy](/en/academy/) if your next question is operational
