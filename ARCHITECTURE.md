# Architecture

System design overview for fq-compressor. Maps the conceptual story to the codebase.

## Layer Map

| Layer | Responsibility | Key Headers |
|-------|---------------|-------------|
| Ingest | Open FASTQ and compressed FASTQ inputs, normalize stream handling | `include/fqc/io/fastq_parser.h`, `include/fqc/io/compressed_stream.h` |
| Analysis | Collect global statistics, reorder intent, memory discipline | `include/fqc/algo/global_analyzer.h` |
| Compression | Encode sequence, IDs, and quality values on block-local units | `include/fqc/algo/block_compressor.h`, `include/fqc/algo/id_compressor.h`, `include/fqc/algo/quality_compressor.h` |
| Format | Write blocks, checksums, reorder metadata, and direct-lookup structures | `include/fqc/format/fqc_writer.h`, `include/fqc/format/fqc_format.h` |
| Retrieval | Verify, range decode, and optionally restore original order | `include/fqc/format/fqc_reader.h`, `include/fqc/pipeline/decompressor_node.h` |

## Invariants

- The block is the unit that makes throughput, checksum scope, and random access compatible.
- The archive format is part of the product contract. It is not just an opaque byte bucket behind the CLI.
- The command layer stays thin so compression and decompression behavior lives in reusable library code.

## Compression Pipeline

```
FASTQ Input -> FastqParser -> GlobalAnalyzer -> BlockCompressor -> FQC Writer
                                              (TBB parallel pipeline)
```

1. **Input opening and FASTQ parsing** via `include/fqc/io/fastq_parser.h` and `include/fqc/io/compressed_stream.h`.
2. **Global analysis** via `include/fqc/algo/global_analyzer.h`, which decides how reads should be grouped and whether reorder metadata is needed.
3. **Chunk marshaling** via `include/fqc/pipeline/pipeline.h`, which turns parsed records into per-stream views.
4. **Block compression** via `include/fqc/algo/block_compressor.h`, `include/fqc/algo/id_compressor.h`, and `include/fqc/algo/quality_compressor.h`.
5. **Archive writing** via `include/fqc/format/fqc_writer.h` and `include/fqc/pipeline/writer_node.h`.

The important boundary is the block. Analysis may look across the full input, but payload encoding and archive storage stay block-local so compression work can scale across cores without giving up direct block lookup later.

## Decompression Pipeline

```
FQC Input -> FQC Reader -> Decompressor -> FastqWriter -> FASTQ Output
              (TBB parallel pipeline)
```

1. Reader entry starts from archive metadata, index tables, and block boundaries.
2. Header and block metadata are loaded so the command can select a full restore or targeted extraction.
3. Index-driven lookup resolves the exact block set that must be decoded.
4. Payload streams are decompressed independently on the same block-local contract.
5. Optional reorder restore is applied only when the command asks for it.

The reader side is anchored in `include/fqc/format/fqc_reader.h`, `include/fqc/pipeline/fqc_reader_node.h`, and `include/fqc/pipeline/decompressor_node.h`.

## FQC Format and Random Access

The `.fqc` container is a block archive, not a raw byte stream wrapper. Its layout is defined in `include/fqc/format/fqc_format.h`.

### File Layout

```text
magic + version
-> global header
-> block payloads
-> optional reorder map
-> block index
-> file footer
```

The payload region stays append-only during compression, and the block index plus footer are written only after the writer knows every block offset and size.

### Per-Block Storage

A block groups a bounded run of reads and keeps its logical streams separate:

- identifiers
- sequences
- qualities
- auxiliary data (e.g. variable read lengths)

The split keeps codec choices local to each stream and lets readers skip data they do not need.

### What "Random Access" Means Here

Random access is block-addressable lookup. The reader opens the archive, reads the footer and index once, then uses index entries to seek directly to the block that owns a target read range. It does not need to replay every earlier block.

The cost is effectively constant with respect to archive length once the index is loaded: metadata read, one seek, then decode only the needed block payloads.

### Reorder Map

When reads are reordered for better compression, a forward/reverse map is stored in the archive. Original-order recovery is applied only when the command asks for it (`--original-order`).

## Pipeline Design

### Serial Ingress and Egress

FASTQ parsing and final archive writes are naturally ordered operations. Keeping those stages serial avoids cross-thread coordination around record framing, footer updates, and final output order.

### Parallel Block Work

Once a chunk has a stable archive-order read range, each block can be compressed or decompressed independently. `include/fqc/pipeline/pipeline.h` models `ReadChunk` and `CompressedBlock` as complete units of work.

### Explicit Backpressure

The pipeline is designed to stop unlimited in-flight buffering. `include/fqc/pipeline/pipeline.h` defines `kDefaultMaxInFlightBlocks`, while memory budgeting controls how much data the pipeline should hold at once.

## Key Algorithms

1. **Assembly-based Compression (ABC)**
   - Minimizer Bucketing - groups reads by shared k-mer signatures
   - TSP Reordering - maximizes neighbor similarity
   - Consensus Generation - builds local consensus per bucket
   - Delta Encoding - stores only edits from consensus

2. **Statistical Context Mixing (SCM)**
   - Context: Previous QVs + current base + position
   - Prediction: High-order context modeling
   - Coding: Adaptive arithmetic coding

## Module Organization

| Namespace | Purpose |
|-----------|---------|
| `fqc::algo` | Compression algorithms (ABC core) |
| `fqc::commands` | CLI command implementations |
| `fqc::common` | Shared utilities (errors, logging, types) |
| `fqc::format` | FQC archive format definition |
| `fqc::io` | I/O abstractions (FASTQ, compressed streams) |
| `fqc::pipeline` | TBB-based parallel processing pipelines |

## Code Anchors

- Pipeline types and coordination: `include/fqc/pipeline/pipeline.h`, `src/pipeline/pipeline.cpp`
- Compression nodes: `include/fqc/pipeline/reader_node.h`, `include/fqc/pipeline/writer_node.h`
- Decompression nodes: `include/fqc/pipeline/fqc_reader_node.h`, `include/fqc/pipeline/decompressor_node.h`, `include/fqc/pipeline/fastq_writer_node.h`
- Command wiring: `src/commands/compress_command.cpp`, `src/commands/decompress_command.cpp`
