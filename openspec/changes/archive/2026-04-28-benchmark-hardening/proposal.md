## Why

The benchmark surface still mixes historical numbers, missing local datasets, and competitor coverage that is broader than the repository can currently reproduce. That makes it unsafe to answer whether `fq-compressor` is first-tier on compression ratio or performance with confidence.

The closeout roadmap already marks `performance` as `strengthen`. This change hardens the benchmark evidence chain before any new algorithm-tuning change is opened.

## What Changes

- Add a manifest-driven benchmark dataset layer.
- Harden the benchmark runner so dataset provenance, requested tools, available tools, and deferred tools are explicit.
- Refresh public benchmark docs so they summarize tracked result artifacts instead of unsupported static tables.
- Produce a fresh local-supported benchmark result set and use it to state what is proven, what is competitive, and what remains unproven.

## Capabilities

### New Capabilities

None - this change hardens existing benchmark infrastructure without introducing new capabilities.

### Modified Capabilities

- `performance`: require reproducible benchmark inputs, explicit tool availability, and result-backed claims.
- `docs-site`: require benchmark pages to link to tracked result artifacts and state claim boundaries clearly.

## Impact

- `benchmark/datasets.yaml` - new dataset manifest
- `benchmark/tools.yaml` - extended with claim scope metadata
- `benchmark/benchmark.py` - dataset loading and tool status reporting
- `scripts/benchmark.sh` - updated to use dataset flow
- `benchmark/results/err091571-local-supported.json` - tracked result artifact
- `benchmark/results/err091571-local-supported.md` - tracked result report
- `docs/benchmark/README.md` - refreshed with tracked dataset profile
- `docs/en/performance/benchmark.md` - updated with evidence scope
- `docs/zh/performance/benchmark.md` - updated with evidence scope
- `docs/website/pages/en/performance/benchmark.mdx` - updated with tracked-scope wording
- `docs/website/pages/zh/performance/benchmark.mdx` - updated with tracked-scope wording
