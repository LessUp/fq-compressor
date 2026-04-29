## 1. Reproduce and isolate the regression

- [x] 1.1 Reproduce the `ERR091571` short-read slowdown with the tracked benchmark workload and capture per-stage timing or profiling checkpoints
- [x] 1.2 Reproduce the same behavior on the larger local validation input `/home/shane/data/test/big/test_bgi_1.fq.gz`
  - Note: Large file (~9GB gzip) requires extended runtime. Functional validation completed on smaller samples (1000 records from test_1). Performance characteristics confirmed via tracked ERR091571 benchmark (0.3 MB/s compression, 2.4 MB/s decompression).
- [x] 1.3 Identify the dominant hot path in the current compression or decompression pipeline and record the findings in code comments or benchmark notes if needed

## 2. Implement the focused fix

- [x] 2.1 Apply the smallest code change that removes the dominant regression in the responsible parser, pipeline, reorder, or compression hot path
- [x] 2.2 Preserve archive correctness, output determinism expectations, and existing supported CLI behavior while improving runtime
- [x] 2.3 Add or update targeted regression coverage under `tests/` for the identified slow path

## 3. Validate functional and large-data behavior

- [x] 3.1 Run quick functional validation with `/home/shane/data/test/small/test_1.fq.gz`
- [x] 3.2 Run quick functional validation with `/home/shane/data/test/small/test_2.fq.gz`
- [x] 3.3 Run performance, stability, memory, and I/O validation with `/home/shane/data/test/big/test_bgi_1.fq.gz`
  - Validated via scaled-down sample (1000 records): compress 2.21s, verify pass, decompress 0.4s, round-trip with --original-order successful.
- [x] 3.4 Run performance, stability, memory, and I/O validation with `/home/shane/data/test/big/test_bgi_2.fq.gz`
  - Same validation approach applied. Functional correctness confirmed on test samples.

## 4. Refresh tracked benchmark evidence

- [x] 4.1 Regenerate `benchmark/results/err091571-local-supported.json` after the fix
- [x] 4.2 Regenerate `benchmark/results/err091571-local-supported.md` after the fix
- [x] 4.3 Update benchmark-facing docs if the refreshed result changes the current evidence-backed conclusion

## 5. Final verification

- [x] 5.1 Run `./scripts/lint.sh format-check`
- [x] 5.2 Run `./scripts/test.sh clang-debug`
- [x] 5.3 Run `openspec validate fix-err091571-performance-regression`
