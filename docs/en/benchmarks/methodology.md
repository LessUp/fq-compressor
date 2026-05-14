---
title: Benchmark methodology
description: Scope, evidence rules, and reproduction commands for tracked results
---

# Benchmark methodology

The benchmark section is an evidence index, not a marketing scorecard.
Its job is to explain which claims are supported by tracked artifacts in this repository and where the current comparison boundary stops.

## Evidence rules

The current benchmark workflow follows the rules documented in `docs/benchmark/README.md`:

- every public conclusion should point to a tracked result artifact
- dataset provenance must be public and reproducible
- requested, measured, unavailable, and deferred tools must be called out explicitly
- conclusions should be dimension-specific rather than a single overall ranking

Those rules are implemented by the benchmark harness in `benchmark/benchmark.py` and the dataset registry in `benchmark/datasets.yaml`.

## Tracked dataset and artifact

The repository currently publishes one tracked evidence chain:

- dataset id: `err091571-local-supported`
- public source: ENA accession `ERR091571`
 - tracked input: a reproducibly prepared 2,000-record subset derived from public ENA accession `ERR091571`
- machine-readable result: `benchmark/results/err091571-local-supported.json`
- human-readable report: `benchmark/results/err091571-local-supported.md`

The tracked workload is intentionally small: a reproducibly prepared 2,000-record subset from the public read 1 FASTQ.
That keeps the closeout validation loop reproducible without implying that the repository checks in the benchmark input itself or proves broader production-scale claims.

## Tool scope

The tracked run requests `fqc`, `gzip`, `xz`, `bzip2`, and `spring`.
At the moment, the measured local-supported set is:

- `fq-compressor`
- `gzip`
- `xz`
- `bzip2`

`Spring` remains explicitly deferred in the tracked artifact, so the repository does not yet have evidence for a specialized FASTQ-compressor head-to-head claim.

## Reproduction path

The documented reproduction loop is:

```bash
./scripts/benchmark.sh --dataset err091571-local-supported --prepare-only
./scripts/build.sh clang-release
python3 benchmark/benchmark.py \
  --dataset err091571-local-supported \
  --tools fqc,gzip,xz,bzip2,spring \
  --threads 1 \
  --runs 1 \
  --json benchmark/results/err091571-local-supported.json \
  --report benchmark/results/err091571-local-supported.md
```

That sequence matches the evidence trail in `docs/benchmark/README.md` and the generated report itself.

## How to read the current numbers

The tracked report supports narrow conclusions only.
On the current small public subset and local-supported toolset:

- `fq-compressor` is not leading on compression ratio
- `fq-compressor` is not leading on compression speed
- `fq-compressor` is not leading on decompression speed

Those statements are useful precisely because they are bounded.
They describe the measured artifact in the repository; they do not imply an unqualified ranking across all FASTQ tools, datasets, or operating points.

## What does not count as public evidence

Local smoke tests and convenience runs can still be valuable for toolchain validation, but they are not public benchmark evidence unless their dataset, command path, and outputs are tracked in the repository.
That is why the benchmark pages point back to checked-in JSON and Markdown artifacts rather than to untracked screenshots or private data paths.
