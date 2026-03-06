# Design Overview

## Main Goals

fq-compressor aims to balance:

- compression ratio
- throughput
- random access
- maintainability
- modern engineering discipline

## Compression Strategy

### Sequence

For short reads, the project follows an **Assembly-based Compression (ABC)** direction inspired by Spring and Mincom.

### Quality

Quality values use an **SCM-style** statistical modeling approach inspired by fqzcomp5.

### Identifier

FASTQ identifiers are handled through tokenization and delta-oriented compression.

## Why a block-based archive

The `.fqc` format is designed around independent blocks to enable:

- random access
- block-level verification
- parallel decompression
- fault isolation
