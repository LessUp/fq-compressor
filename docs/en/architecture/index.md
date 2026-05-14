# Architecture

The architecture section explains how fq-compressor is assembled from parsing, analysis, compression, storage, and access components.
It focuses on boundaries and data flow so readers can understand the system without reading every source file.

## Section purpose

- Describe the major subsystems and why they are separated
- Show where compression, indexing, and random access responsibilities live
- Connect public concepts to implementation areas in the repository

## What this section should answer

- How FASTQ input moves through the pipeline into an FQC archive
- Which components own format concerns, algorithms, and command orchestration
- Where to look in the codebase for deeper detail

## Continue with

- [Pipeline](/en/architecture/pipeline) for compression and decompression flow boundaries
- [FQC format and random access](/en/architecture/format-random-access) for archive layout and block lookup
- [I/O and memory](/en/architecture/io-memory) for buffering and memory-budget behavior
- [Guides](/en/guides/) for operator-facing workflows
- [Resources](/en/resources/) for source, specs, and supporting material
