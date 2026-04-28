---
title: Performance Benchmarking
description: Tracked benchmark evidence and scope for fq-compressor
version: 0.2.0
last_updated: 2026-04-28
language: en
---

# Performance Benchmarking

> Benchmark claims in this repository are now limited to tracked result artifacts.

## Tracked Evidence

- JSON artifact: `benchmark/results/err091571-local-supported.json`
- Markdown artifact: `benchmark/results/err091571-local-supported.md`
- Dataset manifest: `benchmark/datasets.yaml`
- Runner entrypoint: `scripts/benchmark.sh`

## Current Verified Scope

The current tracked result set is intentionally narrow.

- Dataset: ENA `ERR091571`
- Workload: public Illumina paired-end human WGS run
- Measured input: the first 2,000 records streamed from read 1 into `benchmark/data/ERR091571/ERR091571_1.2000.fastq`
- Measured local tools: `fqc`, `gzip`, `xz`, `bzip2`
- Deferred specialized peer: `spring`

This means the current repository can only make claims about the supported local toolset on this measured workload slice.

## Current Findings

From `benchmark/results/err091571-local-supported.json`:

- Compression ratio: `fq-compressor` does not lead the measured set; `bzip2` is best on this artifact
- Compression speed: `fq-compressor` does not lead the measured set; `bzip2` is faster on this artifact
- Decompression speed: `fq-compressor` does not lead the measured set; `xz` is fastest on this artifact
- Specialized FASTQ-peer ranking: still unproven because `spring` is deferred, not measured

The evidence-backed answer today is therefore not "fq-compressor is first-tier." The honest answer is that it is not first-tier on the currently measured local-supported dimensions, and specialized-peer comparison remains incomplete.

## Why The Tracked Dataset Is Small

The benchmark framework now uses a public dataset manifest and stores provenance inside the result artifact.

The tracked artifact is pinned to a 2,000-record subset because:

- it is streamed directly from a public ENA source
- it completes inside the closeout validation loop
- the current `fq-compressor` build regresses sharply on the larger 20,000-record exploratory subset for this workload

That larger subset is useful for investigation, but it is not the pinned tracked evidence artifact.

## Reproducing The Tracked Result

```bash
./scripts/benchmark.sh \
  --dataset err091571-local-supported \
  --prepare \
  --build \
  --tools fqc,gzip,xz,bzip2,spring \
  --threads 1 \
  --runs 1
```

This updates:

- `benchmark/results/err091571-local-supported.json`
- `benchmark/results/err091571-local-supported.md`

## Scope Boundary

Do not read the current result set as a universal ranking.

- It covers one public workload slice
- It covers only the supported local toolset
- It explicitly excludes deferred specialized peers from verified comparison claims

## Related Notes

- For benchmark framework details, see `docs/benchmark/README.md`
- For the full tracked report, read `benchmark/results/err091571-local-supported.md`

---

**🌐 Language**: [English (current)](./benchmark.md)
