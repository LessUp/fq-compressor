# Read reordering

Reordering is the bridge between global similarity and block-local compression.
Instead of preserving the original FASTQ order inside the archive, fq-compressor can place similar short reads near each other and record a reversible mapping.

## Role in fq-compressor

The global analyzer extracts minimizers from every read, sorts bucket entries by minimizer hash, and uses those buckets to guide archive order.
The result is a forward map from original read ID to archive ID and a reverse map for reconstruction.
Later stages use archive order for compression while reporting or restoring original order through the stored maps.

## Core mechanism

1. **Bucket building:** each read contributes canonical minimizers that act as coarse similarity keys.
2. **Neighbor search:** starting from the current archive tail, the analyzer looks for unused reads that share minimizer buckets and chooses a close candidate with a greedy approximate Hamiltonian-path strategy.
3. **Block formation:** once reads are reordered, contiguous archive ranges become compression blocks.
4. **Map serialization:** forward and reverse maps are stored with delta encoding plus varints, so reversibility costs much less than a raw integer array.

## Compression effect

Reordering raises the density of local matches before consensus construction begins.
That improves contig formation, reduces mismatch payloads, and makes block-local codecs behave more like they are seeing one coherent region instead of many unrelated reads.
The explicit reorder map preserves exact reversibility, so compression gains do not require sacrificing original output order.

## Current boundaries

Reordering is currently a short-read optimization.
The implementation prefers a practical greedy search over an expensive exact path solver, which keeps memory bounded and fits the project's block-oriented random-access design.
