---
title: FQC format and random access
description: How the archive layout supports direct block lookup
---

# FQC format and random access

The `.fqc` container is a block archive, not a raw byte stream wrapper.
Its layout is defined in `include/fqc/format/fqc_format.h` and implemented by `include/fqc/format/fqc_writer.h` and `include/fqc/format/fqc_reader.h`.

## File layout

At a high level the file is written in this order:

```text
magic + version
-> global header
-> block payloads
-> optional reorder map
-> block index
-> file footer
```

That order matters.
The payload region stays append-only during compression, and the block index plus footer are written only after the writer knows every block offset and size.

## What the format stores per block

A block groups a bounded run of reads and keeps its logical streams separate:

- identifiers
- sequences
- qualities
- auxiliary data such as variable read lengths

`include/fqc/format/fqc_reader.h` exposes this as `BlockData`, while `include/fqc/format/fqc_writer.h` exposes `BlockPayload`.
The split keeps codec choices local to each stream and lets readers skip data they do not need.

## Global metadata and flags

`include/fqc/format/fqc_format.h` also carries the archive-wide flags:

- paired-end vs single-end layout
- quality and ID handling modes
- preserve-order vs reordered archive layout
- reorder-map presence
- streaming mode

Those flags tell the reader whether original order can be reconstructed and which decoding path is valid.

## What “random access” means here

In this project, random access is block-addressable lookup.
The reader opens the archive, reads the footer and index once, then uses index entries to seek directly to the block that owns a target read range.
It does not need to replay every earlier block.

That is why `FQCReader::open()` loads metadata first and why `FQCReader::readBlock()` accepts a block id directly.
The cost is effectively constant with respect to archive length once the index is loaded: metadata read, one seek, then decode only the needed block payloads.

## Role of the reorder map

Reordering improves compression locality, but it would destroy original read order unless the mapping were persisted.
`include/fqc/format/reorder_map.h` stores both directions:

- original id -> archive id
- archive id -> original id

The map is delta- and varint-encoded so it remains small enough to keep with the archive.
When decompression wants original order, the reverse map tells the writer how to restore it.
When targeted lookup starts from original read coordinates, the forward map tells the reader which archive positions own those reads.

## Why the format stays block-based

The project wants three things at once: domain-specific compression, bounded memory, and direct retrieval.
A single monolithic entropy stream would improve some ratios but would make direct seeking expensive.
A block container is the compromise: each block is self-contained enough to decode independently, while the footer and index make the archive globally navigable.

## Code anchors

- Format definitions: `include/fqc/format/fqc_format.h`
- Writer path: `include/fqc/format/fqc_writer.h`, `src/format/fqc_writer.cpp`
- Reader path: `include/fqc/format/fqc_reader.h`, `src/format/fqc_reader.cpp`
- Reorder metadata: `include/fqc/format/reorder_map.h`, `src/format/reorder_map.cpp`
