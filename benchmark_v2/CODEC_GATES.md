# Codec admission gates

FQC v2 has one production fallback. A profile-specific codec is admitted only when all of the
following hold on the maintained real-data corpus:

1. compression is at least 50 MiB/s and decompression at least 100 MiB/s on the 8-core ARM64 and
   x86_64 release machines;
2. median archive size improves by at least 5% relative to fallback;
3. no individual dataset is more than 1% larger than fallback;
4. exact record round trip, deterministic output, and the configured memory cap pass;
5. measured time and memory scale linearly through the largest workload.

## Current decisions

| Candidate | Evidence | Decision |
|---|---|---|
| Packed A/C/G/T + exact exceptions + Zstd fallback | Randomised short/long release runs pass exact round trip, x86_64 throughput, scaling, and 64 MiB memory runs | Ship |
| Legacy Illumina ABC + dense SCM | Historical tracked path was about 0.1 MiB/s compress and 2 MiB/s decompress; dense quality state was about 107 MiB per worker | Reject |
| Sparse/static rANS quality model | No adequate real Illumina/HiFi corpus result proving the 5% aggregate size gate | Do not ship |
| HiFi Hamming/reference-chain codec | No maintained representative HiFi corpus or measured size/throughput result | Do not ship |
| ONT homopolymer/overlap codec | No maintained representative ONT corpus; bounded bucket/window complexity is not yet measured | Do not ship |
| CLR overlap codec | No maintained representative CLR corpus; error-rate/band-width memory cost is not yet measured | Do not ship |

“Do not ship” is the gate working, not an implicit fallback to an unmeasured implementation. All
four profile IDs therefore use the same production fallback today.

## Reproduction

Synthetic throughput and memory stress:

```bash
FQC_BIN=build/clang-release/src/fqc \
FQC_PERF_SIZES="64 256" \
FQC_PERF_DATA=random \
FQC_PERF_MEMORY_MIB=64 \
FQC_PERF_ENFORCE_SLA=1 \
bash tests/e2e/test_performance.sh
```

The harness defaults to three repetitions, reports median wall-clock throughput and maximum RSS,
and rejects any workload below the floor when `FQC_PERF_ENFORCE_SLA=1`.

Dataset-driven peer/fallback comparisons use `./scripts/benchmark.sh`. Results should be
committed only when the dataset provenance and release-machine metadata are recorded; legacy v1
results were deleted because they describe a removed codec and CLI.
