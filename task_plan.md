# fq-compressor v2 implementation plan

## Goal

Replace the current split compression architecture with a streamable, bounded-memory v2
fallback path, prove its correctness/scaling/performance, and only then admit specialised codecs
that pass the agreed release gates.

## Decisions locked

- Lossless record fields and pairing; archive order may differ from input order.
- Profiles: Illumina, ONT, PacBio HiFi, PacBio CLR; reference-free only.
- Sequential v2 frames; no random access, reorder map, or v1 compatibility.
- stdin/stdout process integration; 16 GiB hard memory limit; no scratch files.
- Production gate on 8-core ARM64 and x86_64 reference machines: compress >= 50 MiB/s,
  decompress >= 100 MiB/s.
- A specialised codec ships only if it meets the SLA and improves median size by >= 5% over
  fallback without a per-dataset regression above 1%.

## Phases

| Phase | Status | Deliverable |
|---|---|---|
| 0. Baseline and repository grounding | complete | Clean baseline test/format results; current data-flow facts |
| 1. Throwaway fallback prototype | complete | Runnable packed-sequence/frame prototype and scaling evidence |
| 2. V2 format and fallback tracer bullet | complete | Production v2 reader/writer, compress/decompress/verify round trip |
| 3. Unified bounded execution graph | complete | Single engine, byte-budget admission, safe frame-local codec state |
| 4. Profiles and common codecs | complete | Profile detection/override and three-stream fallback codecs |
| 5. Specialised codec feasibility gates | complete | Documented ship/reject/do-not-ship decisions; no unmeasured codec admitted |
| 6. Cleanup and public surface | complete | Minimal CLI, removal of v1/random-access/duplicate modules, docs |
| 7. Full verification | complete | Tests, format check, sanitizers where available, benchmark report |

## Current focus

Implementation and local x86_64 verification are complete. ARM64 and representative biological
corpora remain release-environment gates rather than claims made by this workspace run.

## Errors encountered

| Error | Attempt | Resolution |
|---|---:|---|
| Baseline `format-check` fails in existing `compression_profile.cpp` and stream-factory files | 1 | Record as pre-existing clean-commit debt; do not attribute to v2 work. Format touched files and decide separately whether to include the small baseline cleanup. |
| `clang-format-21` is not installed | 1 | Use the repository script's supported fallback, `/usr/bin/clang-format`. |
| Release build could not update Conan's cache inside the sandbox | 1 | Re-ran the approved build with escalated cache access; build passed. |
| Prototype cleanup patch used stale progress-file context | 1 | Inspected the current tail and reapplied a smaller exact patch successfully. |
| Production-engine patch hit stale CMake context after applying earlier hunks | 1 | Inspected the partial result, confirmed engine/test registration was present, and isolated the missing stdout adapter. |
| `stream_factory.cpp` is owned by `nobody` and cannot be patched in place | 1 | Recreate it through `apply_patch` move operations in the writable source directory, preserving all existing content. |
| First v2 engine test run failed the mixed-symbol single-end case without showing its error | 1 | Add the structured error message to the assertion, rerun the focused case, then fix the underlying invariant. |
| Expanding `ParserOptions::validBases` did not change validation | 1 | Found that `validateSequence` ignored the option and called a hard-coded helper; route validation through the configured alphabet and update the helper consistently. |
| Minimal CLI build used exception-style `Error::what()` on value errors | 1 | Use the `Result` error API's `message()` accessor; preserve `what()` only for caught `FQCException`. |
| Benchmark-notes patch expected a gate sentence that was not in `findings.md` | 1 | Inspect the current file tails and append the measured production section at the actual end. |
| Byte-budget engine patch expected pre-format line wrapping | 1 | Inspect exact line numbers and apply the reserve/frame/reader changes in smaller patches. |
| Slimmed test matrix exposed two legacy CLI expectations in `build_smoke_test` | 1 | Retain the high-signal process tests but update them to v2 memory/quiet/verify semantics; remove `info`, threads, and progress assumptions. |
| Canonical-byte helper rename patch expected different indentation | 1 | Locate all four exact symbols with `rg` and apply the mechanical rename directly. |
| `./scripts/test.sh clang-asan` had no generated Conan toolchain for the new preset | 1 | Bootstrap the preset with `./scripts/build.sh clang-asan`, then rerun the test script. |
| Conan added multiple included presets named `conan-debug`, so ASan configuration was rejected | 1 | Delete the unused root `CMakeUserPresets.json` aggregator after every dependency install; project presets already point directly at each toolchain. |
| LeakSanitizer aborts under the managed environment's ptrace wrapper before test exit | 1 | Rerun the ASan+UBSan binary checks with `ASAN_OPTIONS=detect_leaks=0`; retain address/undefined checks and record leak detection as environment-unavailable. |
| Clang 21 is newer than Conan's declared compiler settings | 1 | Detect the installed compiler and clamp only Conan's compatibility setting to its highest known Clang value (20). |
| Clang 21 ASan/UBSan runtime lacks the vptr C++ symbols in this environment | 1 | Keep ASan and UBSan enabled but disable the unavailable vptr sub-check in the sanitizer preset. |
| zlib-ng default mode did not export `ZLIB::ZLIB` and allowed a system-zlib fallback | 1 | Set `zlib_compat=True`; fresh Release configuration now resolves Conan's target. |
| Installed CMake export interpreted `COMPONENT` as an include path | 1 | Correct the install/export argument placement and verify an external configure/build/run consumer. |
| Adding full logical validation regressed Illumina compression to 45.84 MiB/s | 1 | Fuse validation into stream measurement and exception decoding; final median is 53.15 MiB/s with validation intact. |
| Concurrent Debug/Release tests deleted a shared compressed-stream temp directory | 1 | Scope fixture directories by process ID; both presets now pass concurrently. |
