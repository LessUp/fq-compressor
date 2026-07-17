# Performance index — current best results

Numbers are median-of-3 unless noted. All runs use the full release CLI path
(`tests/e2e/test_performance.sh`), randomised synthetic data, and a 64 MiB memory budget.

## Environment

| Item | Value |
|---|---|
| Host | 8-core/16-thread AMD Ryzen 7 5800H, x86_64 |
| OS | Linux 6.18.33.2-microsoft-standard-WSL2 |
| Compiler | Clang 21 |
| Preset | clang-release |
| Repeats | 3 |
| Memory budget | 64 MiB |
| Data mode | random (`FQC_PERF_DATA=random`) |

> **Caveat**: WSL2 wall-clock noise is high. Re-running on the same host can produce compress
> figures below the 50 MiB/s floor (observed 47.8-48.4 MiB/s on Illumina). Treat the numbers below
> as best-case medians, not as a guarantee. Non-WSL2 qualification was explicitly waived for
> v0.3.0-rc1, whose supported platform scope is x86_64 only.

## Current results (2026-07-13)

| Dataset | Input | Compress MiB/s | Decompress MiB/s | Compress RSS | Decompress RSS | Ratio |
|---|---:|---:|---:|---:|---:|---:|
| Illumina-like 150 bp | 65.49 MiB | 53.15 | 182.40 | 31.4 MiB | 12.1 MiB | 2.9487x |
| ONT-like 20 kbp | 63.97 MiB | 55.66 | 215.22 | 25.5 MiB | 12.9 MiB | 2.8401x |

## Scaling (structured data, larger inputs)

| Dataset | Input | Compress MiB/s | Decompress MiB/s |
|---|---:|---:|---:|
| Illumina 150 bp | 65.49 MiB | 65.40 | 177.53 |
| Illumina 150 bp | 262.29 MiB | 76.72 | 256.76 |
| ONT-like 20 kbp | 256.02 MiB | 86.02 | 290.91 |

Throughput does not degrade with size; no super-linear scaling signal observed up to 256 MiB.

## Codec-core prototype ceiling (excludes parsing/I/O/checksums)

| Profile shape | Ratio | Compress MiB/s | Decompress MiB/s | Max RSS |
|---|---:|---:|---:|---:|
| 150 bp short | 2.19x | 61.1 | 187.3 | 258 MiB |
| 20 kbp long | 2.04x | 69.1 | 171.3 | 216 MiB |

These are upper-bound evidence, not production SLA claims.

## Open verification items

- Real biological corpora (Illumina, ONT, HiFi, CLR): compression-ratio claims gated by
  `benchmark_v2/CODEC_GATES.md`.
- LeakSanitizer and ASan vptr: unavailable under managed ptrace env; see
  `troubleshooting/0006-sanitizer-env-limitations.md`.
- Non-WSL2 throughput stability remains a follow-up measurement, not a v0.3.0-rc1 release gate.
- ARM64 is outside the current platform support and release-artifact scope.
