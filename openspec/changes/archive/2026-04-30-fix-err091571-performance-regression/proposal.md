## Why

The newly tracked `ERR091571` benchmark artifact exposed a severe `fq-compressor` performance regression on this workload: even the current 2,000-record pinned subset is far slower than the local-supported comparison set, and the 20,000-record exploratory subset degrades sharply enough to fall out of the closeout validation loop.

This should be handled as a focused optimization and stability change now that benchmark claims are evidence-backed. The project needs a reproducible path to improve compression, decompression, and scaling behavior on the measured Illumina workload before any broader performance positioning can change.

## What Changes

- Identify and fix the dominant runtime regression in the `fq-compressor` compression path for the `ERR091571` short-read workload.
- Add targeted regression coverage so the slow path is detectable with repository-controlled datasets.
- Extend benchmark and validation flow so small local data can be used for quick functional checks, while large local data is used for performance, stability, memory, and I/O verification.
- Refresh the tracked benchmark artifact after the fix and document what improved and what remains outside the verified comparison scope.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `performance`: tighten requirements around regression-sensitive benchmark workloads, measured validation tiers, and evidence-backed performance improvements.
- `build-system`: extend the documented validation loop so targeted performance regression checks can accompany the default format/test gates when performance-sensitive changes are implemented.

## Impact

- `src/` compression pipeline, parser, and/or algorithm hot paths on the measured short-read workload
- `tests/` targeted regression coverage for the identified slowdown
- `benchmark/` validation and tracked result artifacts
- `docs/benchmark/` and performance-facing docs if the refreshed evidence changes the current conclusion
- local validation inputs:
  - quick functional validation: `/home/shane/data/test/small/test_1.fq.gz`, `/home/shane/data/test/small/test_2.fq.gz`
  - large performance/stability validation: `/home/shane/data/test/big/test_bgi_1.fq.gz`, `/home/shane/data/test/big/test_bgi_2.fq.gz`
