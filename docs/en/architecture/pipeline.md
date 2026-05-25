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

<div class="wp-flow">
  <article class="wp-flow__step">
    <p class="wp-kicker">01</p>
    <h3>FASTQ input</h3>
    <p>FASTQ or compressed FASTQ streams enter through the parser stack.</p>
  </article>
  <article class="wp-flow__step">
    <p class="wp-kicker">02</p>
    <h3>Parser</h3>
    <p>Record framing and stream normalization turn raw input into archive-ready records.</p>
  </article>
  <article class="wp-flow__step">
    <p class="wp-kicker">03</p>
    <h3>Global analyzer</h3>
    <p>Statistics, reorder intent, and memory discipline are established before block work begins.</p>
  </article>
  <article class="wp-flow__step">
    <p class="wp-kicker">04</p>
    <h3>Read chunks</h3>
    <p>Chunk marshaling turns parsed data into stable archive-order units of work.</p>
  </article>
  <article class="wp-flow__step">
    <p class="wp-kicker">05</p>
    <h3>Parallel block compression</h3>
    <p>Sequence, ID, and quality streams are encoded on block-local boundaries.</p>
  </article>
  <article class="wp-flow__step">
    <p class="wp-kicker">06</p>
    <h3>FQC archive</h3>
    <p>The writer emits blocks, checksums, metadata, and the lookup structures used later.</p>
  </article>
</div>

The important boundary is the block.
Analysis may look across the full input, but payload encoding and archive storage stay block-local so compression work can scale across cores without giving up direct block lookup later.

## Decompression path

Decompression mirrors the same boundary in reverse:

<div class="wp-flow">
  <article class="wp-flow__step">
    <p class="wp-kicker">01</p>
    <h3>.fqc archive</h3>
    <p>Reader entry starts from archive metadata, index tables, and block boundaries.</p>
  </article>
  <article class="wp-flow__step">
    <p class="wp-kicker">02</p>
    <h3>FQC reader</h3>
    <p>Header and block metadata are loaded so the command can select a full restore or targeted extraction.</p>
  </article>
  <article class="wp-flow__step">
    <p class="wp-kicker">03</p>
    <h3>Block lookup</h3>
    <p>Index-driven lookup resolves the exact block set that must be decoded.</p>
  </article>
  <article class="wp-flow__step">
    <p class="wp-kicker">04</p>
    <h3>Parallel stream decode</h3>
    <p>Payload streams are decompressed independently on the same block-local contract.</p>
  </article>
  <article class="wp-flow__step">
    <p class="wp-kicker">05</p>
    <h3>Optional reorder restore</h3>
    <p>Original-order recovery is applied only when the command asks for it.</p>
  </article>
  <article class="wp-flow__step">
    <p class="wp-kicker">06</p>
    <h3>FASTQ writer</h3>
    <p>The decoded records leave the archive in the requested output mode.</p>
  </article>
</div>

The reader side is anchored in `include/fqc/format/fqc_reader.h`, `include/fqc/pipeline/fqc_reader_node.h`, and `include/fqc/pipeline/decompressor_node.h`.
`src/commands/decompress_command.cpp` decides whether the run is a full restore or a targeted extraction.
`include/fqc/pipeline/pipeline_node.h` and `include/fqc/pipeline/pipeline_nodes.h` remain as
implementation-oriented compatibility includes; new integration code should prefer
`include/fqc/pipeline/pipeline.h`.

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
- Legacy compatibility includes (not primary integration points): `include/fqc/pipeline/pipeline_node.h`, `include/fqc/pipeline/pipeline_nodes.h`
- Command wiring: `src/commands/compress_command.cpp`, `src/commands/decompress_command.cpp`

## Continue with

- [FQC format and random access](/en/architecture/format-random-access) to see why block boundaries matter externally
- [Academy](/en/academy/) if your next question is operational
