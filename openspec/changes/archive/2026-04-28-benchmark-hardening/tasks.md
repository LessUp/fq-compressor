## 1. Transition OpenSpec state

- [x] 1.1 Archive completed `introduce-closeout-roadmap` change
- [x] 1.2 Verify clean OpenSpec state (no active changes)

## 2. Add dataset manifest and tool availability

- [x] 2.1 Create `benchmark/datasets.yaml` with ERR091571 dataset entry
- [x] 2.2 Update `benchmark/tools.yaml` with `claim_scope` metadata
- [x] 2.3 Add `benchmark/data/` to `.gitignore`
- [x] 2.4 Extend `benchmark/benchmark.py` with dataset loading and tool status reporting
- [x] 2.5 Update `scripts/benchmark.sh` to use dataset flow

## 3. Generate tracked benchmark evidence

- [x] 3.1 Download and prepare ERR091571 dataset
- [x] 3.2 Build `clang-release` preset
- [x] 3.3 Run benchmark and generate `benchmark/results/err091571-local-supported.json`
- [x] 3.4 Run benchmark and generate `benchmark/results/err091571-local-supported.md`

## 4. Refresh benchmark documentation

- [x] 4.1 Update `docs/benchmark/README.md` with tracked dataset profile
- [x] 4.2 Update `docs/en/performance/benchmark.md` with evidence scope
- [x] 4.3 Update `docs/zh/performance/benchmark.md` with evidence scope
- [x] 4.4 Update `docs/website/pages/en/performance/benchmark.mdx` with tracked-scope wording
- [x] 4.5 Update `docs/website/pages/zh/performance/benchmark.mdx` with tracked-scope wording

## 5. Validate and finalize

- [x] 5.1 Run `openspec validate benchmark-hardening`
- [x] 5.2 Run `./scripts/lint.sh format-check`
- [x] 5.3 Run `./scripts/test.sh clang-debug`
- [x] 5.4 Smoke-test benchmark runner with dataset provenance
