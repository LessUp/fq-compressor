# 0001 Legacy split architecture debt (v1/ABC/SCM/reorder)

- Status: closed
- Severity: high
- Found in: pre-v2 baseline (commit 80047d1)
- Related: —

## Symptom

The pre-v2 codebase carried a split compression architecture with multiple redundant paths and
unbounded memory behaviour:

- Pipeline execution normally bypassed global reordering while the short-read ABC implementation
  assumed similar reads were locally adjacent.
- Medium and long reads were length-prefixed ASCII followed by Zstd; there was no overlap codec.
- Quality Order-2 allocated a dense context table per worker and performed a full alphabet
  cumulative update per symbol.
- Production backpressure limited token count, not actual retained bytes; configured memory limits
  were not admission controls.
- Compression and decompression had separate single-thread and pipeline implementations.
- Tracked evidence was Illumina-only and did not establish third-generation suitability.

## Reproduction

Read the pre-v2 tree at commit `80047d1`: `include/fqc/algo/`, `include/fqc/commands/`, the
duplicate pipeline sources, and the 16-target CTest split.

## Investigation

- Confirmed the architecture could not be repaired incrementally because admission control,
  ordering, and the codec model were entangled across modules.
- A throwaway fallback prototype (`fqc_v2_fallback_prototype`) was built first to prove a
  bounded-memory sequential path could clear the 50/100 MiB/s floor before committing to a
  production rewrite.
- Prototype results (release, 64 MiB synthetic): short 61.1/187.3 MiB/s, long 69.1/171.3 MiB/s,
  max RSS below 260 MiB. 256 MiB runs showed no super-linear scaling.

## Root cause

The v1 design coupled ordering, codec state, and memory accounting at the wrong layer, so the
production memory limit was advisory rather than enforced and the codec assumptions did not match
the actual data flow.

## Fix

Commit `75b7400` replaced the legacy compressor with bounded FQC v2:

- Sequential independent frames, packed DNA + exact exceptions, three-stream Zstd, XXH64 checksums.
- Exact raw-stream measurement and conservative encode/decode peak-memory preflight.
- Single compress/decompress/verify engine; deleted v1, ABC/SCM, random access, ReorderMap, TBB,
  async I/O, and duplicate pipelines (27,835 lines removed).

## Verification

- 8/8 CTest targets pass under clang-debug, clang-release, clang-asan (ASan+UBSan), clang-tsan.
- `./scripts/lint.sh format-check` passes.
- Randomised 64 MiB full-path runs clear the x86_64 50/100 floor (see
  `performance/baselines/2026-07-13-pre-release-v2.md`).

## Follow-up / prevention

ARM64 and representative biological corpora remain release-environment gates; see
`benchmark_v2/CODEC_GATES.md` for the codec admission ledger that prevents unmeasured codecs from
shipping.
