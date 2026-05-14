# ABC pipeline

Assembly-based Compression (ABC) is the sequence path used for short-read data in fq-compressor.
It combines a global ordering pass with block-local sequence modeling so similar reads reach the same block and can share a compact representation.

## Role in fq-compressor

The pipeline is split into two phases.
Phase 1 scans reads, extracts canonical minimizers, decides whether short-read reordering should run, and computes block boundaries.
Phase 2 compresses each block independently: IDs, sequences, qualities, and auxiliary data are encoded as separate logical streams and written with a checksum.

## Core mechanism

1. **Global analysis:** reads are sampled for length class, then minimizers are extracted with canonical k-mers and grouped by hash.
2. **Reordering:** short reads can be greedily reordered so neighboring archive positions are more likely to share sequence context.
3. **Block compression:** each reordered block is compressed independently, preserving O(1) block lookup and bounded memory.
4. **Stream writing:** the writer stores block metadata, payload streams, and index entries so decompression can seek to a block without replaying the full file.

## Compression effect

ABC helps sequence compression because local redundancy becomes explicit before entropy coding.
Once similar reads are co-located, consensus/delta encoding removes most repeated bases and leaves smaller mismatch streams for later coding.
The block boundary also becomes the random-access boundary, so better compression does not require abandoning direct block retrieval.

## Current boundaries

In the current implementation, the full ABC path is aimed at short reads.
Medium and long reads still use the same block container, but sequence coding falls back to simpler Zstd-backed handling rather than short-read consensus assembly.
