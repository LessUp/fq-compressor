# fq-compressor v2 progress

## 2026-07-13

- User approved destructive implementation of the revised v2 plan.
- Loaded `planning-with-files` and `prototype` skill instructions.
- Created persistent plan, findings, and progress files.
- Phase 0 started; repository was clean at commit `80047d1`.
- Baseline build and all 16 CTest targets passed.
- Baseline format check failed on pre-existing files before any production source changes.
- Phase 0 completed. Phase 1 started with the performance-oriented fallback prototype; the
  interactive TUI prototype skill branch was rejected as a poor fit for codec throughput evidence.
- Added the throwaway `fqc_v2_fallback_prototype` target and one-command runner. It serializes all
  record fields into ID/sequence/quality streams, packs DNA to 2-bit plus exact exceptions, applies
  Zstd level 1, decodes, and verifies the full record vector.
- Release prototype passed full round trip at 64 MiB: short 61.1/187.3 MiB/s and long
  69.1/171.3 MiB/s (compress/decompress), with max RSS below 260 MiB.
- 256 MiB runs also passed without throughput degradation: short 80.8/228.9 MiB/s and long
  110.2/369.1 MiB/s. Phase 1 is complete and the fallback wire shape is accepted for production
  integration.
- Phase 2 started: sequential v2 frame reader/writer and corruption tests.
- Added the production `format::v2::ArchiveWriter/ArchiveReader` seam with strict profile IDs,
  little-endian headers, independent compressed frames, exact exceptional-base preservation,
  per-frame/global checksums, bounded decode sizes, and an EOS footer.
- Added focused v2 tests for all-field round trip, paired invariants, multiple/empty frames,
  corruption, truncation, legacy rejection, and strict profile parsing.
- `v2_archive_test` passes. The prototype answer is captured in `findings.md`; its executable,
  runner, and CMake target were deleted after the validated packing logic was absorbed.
- Added the production v2 archive engine for single/paired FASTQ input, bounded sequential frame
  accumulation, profile detection/override, decompression, and full checksum verification.
- Added stdout output support to `FileStreamFactory`; paired inputs are interleaved atomically in
  archive/decompressed order.
- Fixed the parser's hard-coded A/C/G/T/N validation path so configured full IUPAC symbols are
  accepted and losslessly preserved.
- Focused parser and v2 engine tests pass. Phase 2 is complete; Phase 3 has started.
- Replaced the legacy CLI with the deliberately small v2 surface: `compress`, `decompress`, and
  `verify`, plus profile override, memory/frame limits, stdin/stdout, mate input, and overwrite.
- Updated and registered the CLI process smoke test; stdin, file output, stdout, IUPAC symbols,
  verification, and canonical round trip pass.
- Reworked the performance harness for v2 profiles and removed stale thread/original-order flags.
- Full release CLI benchmarks on the 8-core x86_64 host pass the production floor at 65.4--86.0
  MiB/s compression and 177.5--290.9 MiB/s decompression over 65--262 MiB fixtures.
- Randomised full-path runs also pass: 52.1--65.2 MiB/s compression and 163.6--252.1 MiB/s
  decompression. Under a 64 MiB configured budget, peak RSS is 32--36 MiB for compression.
- Added exact raw-stream measurement, conservative encode/decode aggregate memory preflight, and
  direct two-pass exception emission; raised the supported minimum budget to 64 MiB.
- Kept the measured sequential engine and frame-level concurrency seam; removed oneTBB because the
  production path already clears the local floor without an in-flight ordering/memory problem.
- Deleted the v1 format/index/reorder path, ABC/SCM codecs, old command layer, async/paired parser
  islands, duplicate pipelines, old tests, stale tracked v1 results, and duplicate smoke scripts.
- Rewrote English/Chinese README, architecture, changelog, AGENTS guidance, Docker runtime deps,
  benchmark adapter, and codec admission ledger for v2.
- Added staged atomic file publication and a regression proving failed paired compression leaves no
  partial archive. Gzip stdin is now streamed rather than fully buffered.
- Added strict logical-record validation at writer and reader boundaries, then fused it into the
  existing measurement/decode loops to avoid duplicate sequence and quality scans.
- Added concatenated-gzip support and fixed buffered-output loss in `GzipStreamBuf` move assignment;
  static analysis identified the latter defect.
- Removed stale TBB/LZMA/BZip2 package metadata, made zlib-ng compatibility explicit, fixed the
  installed CMake export, and passed an external consumer configure/build/run check.
- Final 64 MiB random-data, three-run medians passed the SLA: Illumina 53.15/182.40 MiB/s and ONT
  55.66/215.22 MiB/s, with 31.4/25.5 MiB maximum process RSS respectively.
- Full Debug, Release, ASan+UBSan, and TSan matrices pass 8/8 targets. LeakSanitizer alone remains
  unavailable under the managed ptrace environment; ASan was run with leak detection disabled.

## Test log

| Command | Result |
|---|---|
| `./scripts/lint.sh format-check` | FAIL: pre-existing clang-format violations in profile/stream factory files |
| `./scripts/test.sh clang-debug` | PASS: 16/16 tests |
| `clang-format-21 -i tools/v2_fallback_prototype.cpp` | FAIL: binary absent; retry with repository-supported `clang-format` |
| `./scripts/build.sh clang-release` | PASS after approved Conan cache access |
| `fqc_v2_fallback_prototype short 64` | PASS: 2.19x, 61.1/187.3 MiB/s, exact round trip |
| `fqc_v2_fallback_prototype long 64` | PASS: 2.04x, 69.1/171.3 MiB/s, exact round trip |
| `fqc_v2_fallback_prototype short 256` | PASS: 2.19x, 80.8/228.9 MiB/s, exact round trip |
| `fqc_v2_fallback_prototype long 256` | PASS: 2.04x, 110.2/369.1 MiB/s, exact round trip |
| `./scripts/run_tests.sh -p clang-debug -f '^v2_archive_test$'` | PASS |
| `./scripts/run_tests.sh -p clang-debug -f '^(fastq_parser_test|v2_archive_engine_test)$'` | PASS: 2/2 targets |
| `./scripts/run_tests.sh -p clang-debug -f '^v2_cli_smoke_test$'` | PASS |
| `./scripts/build.sh clang-release` | PASS with production v2 CLI |
| `FQC_PERF_SIZES='64 256' ... tests/e2e/test_performance.sh` | PASS: exact round trip and x86_64 50/100 floor |
| random data, 64 MiB budget, 16/64 MiB inputs | PASS: 50/100 floor, exact round trip, RSS recorded |
| `bash tests/e2e/benchmark_v2_smoke_test.sh` | PASS after v2 adapter/interleaved validation update |
| `./scripts/test.sh clang-debug` after destructive deletion | PASS: 8/8 targets |
| `./scripts/lint.sh format-check` | PASS after deleting legacy profile debt and formatting stream factory |
| `FQC_PERF_SIZES=64 FQC_PERF_REPEATS=3 ... FQC_PERF_ENFORCE_SLA=1` | PASS: Illumina 53.15/182.40; ONT 55.66/215.22 MiB/s |
| concurrent `./scripts/test.sh clang-debug` and `clang-release` | PASS: 8/8 each after process-unique fixture paths |
| `ASAN_OPTIONS=detect_leaks=0 ./scripts/test.sh clang-asan` | PASS: 8/8 ASan+UBSan targets |
| `./scripts/test.sh clang-tsan` | PASS: 8/8 TSan targets |
| targeted analyzer/bugprone/CERT/performance/portability clang-tidy on v2 format, engine, gzip I/O | PASS: no core diagnostics; three non-blocking shared error/logger micro-optimisation warnings |

## Key implementation files

- `task_plan.md`
- `findings.md`
- `progress.md`
- `src/CMakeLists.txt`
- `include/fqc/format/v2_archive.h`
- `src/format/v2_archive.cpp`
- `tests/format/v2_archive_test.cpp`
- `tests/CMakeLists.txt`
- `include/fqc/commands/v2_archive_engine.h`
- `src/commands/v2_archive_engine.cpp`
- `tests/commands/v2_archive_engine_test.cpp`
- `include/fqc/io/fastq_parser.h`
- `src/io/fastq_parser.cpp`
- `src/io/stream_factory.cpp`
- `tests/io/fastq_parser_test.cpp`
- `src/main.cpp`
- `tests/e2e/cli_smoke_test.sh`
- `tests/e2e/test_performance.sh`
