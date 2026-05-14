# SCM quality modeling

fq-compressor treats quality values as a separate signal stream and compresses them with Statistical Context Mixing (SCM).
The goal is to preserve the structure of Phred scores instead of forcing them through sequence-oriented logic.

## Role in fq-compressor

Quality strings are encoded independently from read IDs and bases.
For lossless mode, the compressor models each quality symbol with adaptive arithmetic coding; for lower-fidelity workflows it can first map scores into Illumina's 8-bin scheme.
This keeps sequence compression and quality compression tunable without changing the archive container.

## Core mechanism

1. **Symbol space:** qualities are converted from ASCII to Phred values over a 94-symbol alphabet.
2. **Context selection:** the active model is chosen from previous quality values and a position bin within the read; the default implementation uses order-2 context and 16 position bins.
3. **Adaptive coding:** arithmetic coding emits each symbol while updating the selected model online, so frequent local transitions become cheaper over time.
4. **Container stage:** the arithmetic-coded byte stream is wrapped in Zstd, which removes residual byte-level redundancy after context coding.

## Compression effect

SCM works because FASTQ qualities are not independent symbols.
Runs near the start or end of a read, and transitions after a high or low score, tend to repeat across many reads.
The context model turns those regularities into shorter codes while keeping decompression exact in lossless mode.

## Current boundaries

The current model uses previous qualities and read position, with optional Illumina 8-bin quantization when the selected mode allows it.
The header also leaves room for other quality modes, but the whitepaper should treat the adaptive SCM path and Illumina-style binning as the public, implemented behavior.
