# Performance

The performance section is the evidence contract for the portal. Its job is not to make fq-compressor look larger than life. Its job is to separate what the repository can prove from what remains design intent.

<EvidenceGrid locale="en" />

## Current public boundary

- The tracked public evidence is currently centered on ENA accession `ERR091571` and a reproducible 2,000-record smoke-scale subset.
- The site can discuss compression density, compression throughput, decompression throughput, and archive semantics, but not claim an already-proven specialized-peer leadership position.
- Spring remains deferred in the local evidence chain, so readers should treat the current benchmark story as bounded rather than exhaustive.
- Fresh local benchmark evidence should be generated with `./scripts/benchmark_v2.sh`, which emits both machine-readable JSON and Markdown conclusions.
- Current ratio and throughput standing should be read from those generated reports instead of from hard-coded repository copy.

## Claim matrix

| Claim | Public strength | Why |
| --- | --- | --- |
| fq-compressor has a documented short-read compression result | Strong | Supported by tracked benchmark artifacts in `benchmark/results/` |
| fq-compressor is already best-in-class among specialized peers | Not public | Comparative proof is incomplete while Spring stays deferred |
| O(1) random access is part of the system contract | Strong | Backed by format design, architecture docs, and repository code anchors |
| Throughput must be read together with archive semantics | Strong | The portal documents write path, read path, and retrieval cost as one story |

## Evidence anchors worth keeping open

- [Methodology](./methodology)
- `./scripts/benchmark_v2.sh run --json <path> --report <path>`
- [`docs/benchmark/README.md`](https://github.com/LessUp/fq-compressor/blob/master/docs/benchmark/README.md)
- [`benchmark/results/err091571-local-supported.json`](https://github.com/LessUp/fq-compressor/blob/master/benchmark/results/err091571-local-supported.json)
- [`benchmark/results/err091571-local-supported.md`](https://github.com/LessUp/fq-compressor/blob/master/benchmark/results/err091571-local-supported.md)

## Continue with

- [Algorithms](/en/whitepaper/) for the technical thesis behind the metrics
- [References](/en/research/) for papers, comparators, and project posture
