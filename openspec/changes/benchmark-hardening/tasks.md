## 1. Transition OpenSpec state

- [ ] 1.1 Archive completed `introduce-closeout-roadmap` change
- [ ] 1.2 Verify clean OpenSpec state (no active changes)

## 2. Add dataset manifest and tool availability

- [ ] 2.1 Create `benchmark/datasets.yaml` with ERR091571 dataset entry
- [ ] 2.2 Update `benchmark/tools.yaml` with `claim_scope` metadata
- [ ] 2.3 Add `benchmark/data/` to `.gitignore`
- [ ] 2.4 Extend `benchmark/benchmark.py` with dataset loading and tool status reporting
- [ ] 2.5 Update `scripts/benchmark.sh` to use dataset flow

## 3. Generate tracked benchmark evidence

- [ ] 3.1 Download and prepare ERR091571 dataset
- [ ] 3.2 Build `clang-release` preset
- [ ] 3.3 Run benchmark and generate `benchmark/results/err091571-local-supported.json`
- [ ] 3.4 Run benchmark and generate `benchmark/results/err091571-local-supported.md`

## 4. Refresh benchmark documentation

- [ ] 4.1 Update `docs/benchmark/README.md` with tracked dataset profile
- [ ] 4.2 Update `docs/en/performance/benchmark.md` with evidence scope
- [ ] 4.3 Update `docs/zh/performance/benchmark.md` with evidence scope
- [ ] 4.4 Update `docs/website/pages/en/performance/benchmark.mdx` with tracked-scope wording
- [ ] 4.5 Update `docs/website/pages/zh/performance/benchmark.mdx` with tracked-scope wording

## 5. Validate and finalize

- [ ] 5.1 Run `openspec validate benchmark-hardening`
- [ ] 5.2 Run `./scripts/lint.sh format-check`
- [ ] 5.3 Run `./scripts/test.sh clang-debug`
- [ ] 5.4 Smoke-test benchmark runner with dataset provenance
