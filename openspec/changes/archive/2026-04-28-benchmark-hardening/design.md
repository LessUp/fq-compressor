## Context

The repository has a benchmark framework, but its docs and public claims are ahead of its evidence. Current docs reference `fq-data/...` inputs that are not in the repository, silently skip unavailable tools, and publish static comparison tables that are not backed by tracked result artifacts.

## Goals / Non-Goals

**Goals:**
- Make benchmark inputs reproducible via a tracked dataset manifest
- Make tool availability explicit in benchmark reports
- Refresh benchmark docs from tracked evidence
- Answer whether `fq-compressor` is first-tier on the measured dimensions of the measured workload

**Non-Goals:**
- Do not optimize the compression algorithm in this change
- Do not promise full coverage of every specialized FASTQ compressor
- Do not keep unsupported historical benchmark numbers in public docs

## Decisions

### Use a tracked dataset manifest

Store benchmark dataset metadata in `benchmark/datasets.yaml` so the runner and docs share the same provenance source.

**Alternatives considered:**
- Hard-code dataset paths in scripts → Not reproducible, docs drift from code
- Store datasets in repo → Too large for Git, not scalable

### Record requested, available, and unavailable tools explicitly

The benchmark result JSON and markdown report must say which tools were requested, which were actually run, and which were unavailable or deferred.

**Alternatives considered:**
- Silent skip of unavailable tools → Misleading comparisons, false claims
- Fail entirely if any tool missing → Blocks partial but useful benchmarks

### Commit one tracked local-supported result set

Store a deterministic result artifact for the first refreshed benchmark at:
- `benchmark/results/err091571-local-supported.json`
- `benchmark/results/err091571-local-supported.md`

**Alternatives considered:**
- Generate on-demand from CI → Hard to review, no audit trail
- Store in external artifact store → Adds infrastructure complexity

### Phrase conclusions by dimension

The refreshed docs must separate:
- Compression ratio
- Compression speed
- Decompression speed
- Unproven specialized-peer comparisons

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Public dataset is small, conclusions remain workload-scoped | State scope explicitly in all benchmark docs |
| Some specialized peers may remain unavailable locally | Call out absence explicitly, don't hide it |
| First refreshed report may show `fq-compressor` is competitive but not first-tier on some dimensions | Report honestly; better evidence beats inflated claims |

## Migration Plan

1. Archive the completed roadmap change (`introduce-closeout-roadmap`)
2. Create `benchmark-hardening` change
3. Add performance/docs-site spec deltas
4. Add dataset manifest and benchmark-runner changes
5. Generate a tracked result artifact for the supported local toolset
6. Refresh benchmark docs and rerun repository validation
