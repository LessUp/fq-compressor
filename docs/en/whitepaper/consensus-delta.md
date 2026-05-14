# Consensus and delta representation

After reordering has made similar reads local, fq-compressor compresses short-read sequences by turning each local group into a consensus plus per-read differences.
This is the point where repetitive biological signal becomes a compact archive structure.

## Role in fq-compressor

Inside each block, the sequence compressor builds contigs from nearby reads.
Each contig stores one consensus sequence and a list of delta-coded reads that refer back to that consensus.
The representation is self-contained at block scope, so decompression does not depend on neighboring blocks.

## Core mechanism

1. **Contig seeding:** an unassigned read starts a new contig and initializes the consensus.
2. **Alignment:** nearby reads are tested against the evolving consensus with bounded shift and reverse-complement support.
3. **Consensus update:** accepted reads update per-position base counts, and the majority base becomes the refreshed consensus symbol.
4. **Delta emission:** each read stores only alignment offset, orientation flag, length, mismatch positions, and mismatch symbols; substitution symbols can be remapped relative to the consensus base before final Zstd compression.

## Compression effect

A consensus absorbs the bases shared by many reads, so the archive spends space on disagreements rather than on every base copy.
When reads are well clustered, mismatch positions are sparse and offsets stay small, which makes the serialized delta stream much smaller than raw sequence text.
Because the consensus and deltas are stored together, the representation remains fully lossless.

## Current boundaries

The current contig builder scans forward within a limited local window and relies on the earlier reorder step to keep related reads close.
That keeps compression practical and block-local, but it also means the representation is strongest when the dataset has short-read redundancy that can be exposed by local ordering.
