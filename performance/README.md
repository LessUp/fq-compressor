# Performance log

Measured throughput, memory, and compression ratio for `fq-compressor`, plus the optimisation
history that produced the current numbers. Raw run data lives under `runs/`, immutable baseline
snapshots under `baselines/`, and per-optimisation records under `optimizations/`.

## Layout

```text
performance/
  README.md         # this file: methodology and how to add a run
  INDEX.md          # current best results, one row per profile x dataset
  baselines/        # immutable milestone snapshots (one file per release/milestone)
  optimizations/    # one file per optimisation: before/after, what changed, why
  runs/             # raw run output, grouped by date
    YYYY-MM-DD/
      env.yaml      # host, OS, compiler, preset, FQC_PERF_* parameters
      *.jsonl       # raw benchmark output from tests/e2e/test_performance.sh
```

## Methodology

- **Harness**: `tests/e2e/test_performance.sh` (release binary, full CLI path including parsing,
  frame construction, checksums, filesystem I/O, exact round-trip).
- **Statistics**: median of `FQC_PERF_REPEATS` runs (default 3); max RSS across repeats.
- **SLA floor**: compress >= 50 MiB/s, decompress >= 100 MiB/s on the reference machine. A number
  is only claimable when `FQC_PERF_ENFORCE_SLA=1` passes.
- **Environment pinning**: record host CPU, OS, compiler, preset, and all `FQC_PERF_*` parameters
  in `runs/<date>/env.yaml`. Numbers without a pinned environment are not comparable.
- **Datasets**: synthetic randomised (`FQC_PERF_DATA=random`) for throughput/scaling; real
  biological corpora for compression-ratio claims (gated by `benchmark_v2/CODEC_GATES.md`).

## Reproducing a run

```bash
./scripts/build.sh clang-release
FQC_PERF_SIZES=64 FQC_PERF_REPEATS=3 FQC_PERF_DATA=random \
  FQC_PERF_MEMORY_MIB=64 FQC_PERF_ENFORCE_SLA=1 \
  FQC_PERF_KEEP_TEMP=1 bash tests/e2e/test_performance.sh
```

Copy the printed results and `RESULTS_DIR/benchmarks.jsonl` into
`performance/runs/$(date -I)/`.

## Adding a baseline

When tagging a release or reaching a milestone, copy the current `INDEX.md` table plus environment
into `baselines/YYYY-MM-DD-vX.Y.Z.md`. Baselines are immutable; later optimisations reference them
by filename.

## Adding an optimisation record

```bash
cp performance/optimizations/0001-fuse-validation-scan.md \
   performance/optimizations/$(printf '%04d' $((10#$(ls performance/optimizations/ | \
     grep -oE '^[0-9]{4}' | sort -n | tail -1) + 1)))-<slug>.md
```

Link the related `troubleshooting/NNNN-*.md` if the optimisation was driven by a defect, and
update `INDEX.md` with the new numbers.
