# 0003 Logical-validation scan regressed Illumina compress to 45.84 MiB/s

- Status: closed
- Severity: medium
- Found in: v2 phase 3 (pre-75b7400)
- Related: src/format/v2_archive.cpp, src/commands/v2_archive_engine.cpp,
  performance/optimizations/0001-fuse-validation-scan.md

## Symptom

Adding strict logical-record validation at writer and reader boundaries dropped Illumina-like
compression from ~53 MiB/s to 45.84 MiB/s, below the 50 MiB/s SLA floor.

## Reproduction

`FQC_PERF_SIZES=64 FQC_PERF_REPEATS=3 FQC_PERF_DATA=random FQC_PERF_MEMORY_MIB=64 \
FQC_PERF_ENFORCE_SLA=1 tests/e2e/test_performance.sh` — fails the compress floor on the
first-validation implementation.

## Investigation

- Profiled the validation path: sequence and quality data were being scanned a second time purely
  for validation, duplicating work already done by the stream measurement and exception-decode
  loops.
- Confirmed validation itself was correct; only the placement was wrong.

## Root cause

Validation was implemented as an independent pass instead of being fused into the existing
measurement and decode loops, so each record was walked twice.

## Fix

Fuse logical-record validation into the existing stream-measurement and exception-decoding loops
so each record is walked once. Final three-run median recovered to 53.15 MiB/s compress /
182.40 MiB/s decompress with validation intact.

## Verification

`FQC_PERF_ENFORCE_SLA=1` passes; corruption and truncation tests in `v2_archive_test` still pass.

## Follow-up / prevention

The performance harness enforces the SLA, so a future validation change that reintroduces a
redundant scan will fail CI rather than silently regressing. Full optimisation record in
`performance/optimizations/0001-fuse-validation-scan.md`.
