# System design

This section maps the conceptual story to the codebase and archive layout. Use it when you need to understand how fq-compressor is split across parsing, analysis, block compression, format materialization, and targeted retrieval.

<ArchitectureAtlas locale="en" />

## Layer map

| Layer | Responsibility | Key anchors |
| --- | --- | --- |
| Ingest | Open FASTQ and compressed FASTQ inputs, normalize stream handling | `include/fqc/io/fastq_parser.h`, `include/fqc/io/compressed_stream.h` |
| Analysis | Collect global statistics, reorder intent, and memory discipline | `include/fqc/algo/global_analyzer.h`, `include/fqc/common/memory_budget.h` |
| Compression | Encode sequence, IDs, and quality values on block-local units | `include/fqc/algo/block_compressor.h`, `include/fqc/algo/id_compressor.h`, `include/fqc/algo/quality_compressor.h` |
| Format | Write blocks, checksums, reorder metadata, and direct-lookup structures | `include/fqc/format/fqc_writer.h`, `include/fqc/format/fqc_header.h` |
| Retrieval | Verify, range decode, and optionally restore original order | `include/fqc/format/fqc_reader.h`, `include/fqc/pipeline/decompressor_node.h` |

## Non-negotiable invariants

- The block is the unit that makes throughput, checksum scope, and random access compatible.
- The archive format is part of the product contract. It is not just an opaque byte bucket behind the CLI.
- The command layer stays thin so compression and decompression behavior lives in reusable library code, not in CLI-only branches.

## Continue with

- [Pipeline](./pipeline) for concurrency and flow control
- [FQC format and random access](./format-random-access) for archive layout and lookup semantics
- [I/O and memory](./io-memory) for buffering, backpressure, and memory-budget behavior
