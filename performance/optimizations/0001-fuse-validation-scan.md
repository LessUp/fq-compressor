# 0001 Fuse logical-validation scan into measurement/decode loops

- Date: 2026-07-13
- Related issue: troubleshooting/0003-validation-scan-regression.md
- Baseline: performance/baselines/2026-07-13-v0.2.0.md (post-fix numbers)
- Target: compress >= 50 MiB/s on Illumina-like 150 bp at 64 MiB budget, without weakening
  logical-record validation.

## Context

Strict logical-record validation was added at writer and reader boundaries to guarantee exact
round-trip integrity. The first implementation scanned sequence and quality data a second time,
purely for validation, duplicating work already done by the stream-measurement and
exception-decode loops.

## Before

| Dataset | Compress MiB/s | Decompress MiB/s |
|---|---:|---:|
| Illumina-like 150 bp (64 MiB, random) | 45.84 | — |

Compress fell below the 50 MiB/s SLA floor.

## Change

Fuse validation into the existing stream-measurement and exception-decoding loops so each record
is walked once rather than twice. No validation rule was removed or relaxed.

Files: `src/format/v2_archive.cpp`, `src/commands/v2_archive_engine.cpp`.

## After

| Dataset | Compress MiB/s | Decompress MiB/s | Compress RSS | Ratio |
|---|---:|---:|---:|---:|
| Illumina-like 150 bp (64 MiB, random) | 53.15 | 182.40 | 31.4 MiB | 2.9487x |
| ONT-like 20 kbp (64 MiB, random) | 55.66 | 215.22 | 25.5 MiB | 2.8401x |

Three-run medians; `FQC_PERF_ENFORCE_SLA=1` passes.

## Verification

- `v2_archive_test` corruption and truncation cases still pass.
- `FQC_PERF_ENFORCE_SLA=1` passes on both Illumina-like and ONT-like shapes.
- No validation rule was weakened; the change is purely a loop-fusion refactor.

## Lessons

- Validation placed as an independent pass is a performance trap; fuse it into the loop that
  already touches the data.
- The SLA-enforcing harness caught this regression immediately, which is why
  `FQC_PERF_ENFORCE_SLA=1` is part of the release checklist.
