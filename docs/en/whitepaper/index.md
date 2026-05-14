# Whitepaper

The whitepaper section gathers the public-facing technical narrative for fq-compressor.
It explains the main compression ideas at a system level without turning the site into an implementation notebook.

## Whitepaper topics

- [ABC pipeline](./abc-pipeline) explains how global analysis, block formation, and stream writing turn short-read redundancy into a seekable archive.
- [SCM quality modeling](./scm-quality) explains why quality values are handled as their own statistical signal with adaptive arithmetic coding.
- [Read reordering](./reordering) explains how minimizer-guided ordering improves locality while remaining fully reversible.
- [Consensus and delta representation](./consensus-delta) explains how local contigs store one consensus plus sparse per-read edits.

## Reading outcome

After these pages, a reader should understand the four main technical claims behind fq-compressor: why short-read ordering exists, how sequences are reduced to consensus-plus-delta form, how quality values are modeled separately, and where the current implementation intentionally stops short of broader claims.

## Continue with

- [Architecture](/en/architecture/) for how the system is organized
- [Benchmarks](/en/benchmarks/) for measured support behind public claims
