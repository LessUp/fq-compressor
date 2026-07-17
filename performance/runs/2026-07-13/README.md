# Runs — 2026-07-13

Raw benchmark output for the pre-release FQC v2 baseline. Drop `benchmarks.jsonl` (produced by
`tests/e2e/test_performance.sh`) into this directory after each run.

## Files

- `env.yaml` — pinned environment and `FQC_PERF_*` parameters for this date.
- `benchmarks.jsonl` — raw harness output (to be added when re-running).

## Reproducing

```bash
./scripts/build.sh clang-release
FQC_PERF_SIZES=64 FQC_PERF_REPEATS=3 FQC_PERF_DATA=random \
  FQC_PERF_MEMORY_MIB=64 FQC_PERF_ENFORCE_SLA=1 \
  FQC_PERF_KEEP_TEMP=1 bash tests/e2e/test_performance.sh
# then copy RESULTS_DIR/benchmarks.jsonl here
```
