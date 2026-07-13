# 0006 LeakSanitizer and ASan vptr unavailable under managed ptrace env

- Status: wontfix
- Severity: low
- Found in: v2 phase 7 (pre-75b7400)
- Related: cmake/fqc_sanitizers.cmake, scripts/build.sh

## Symptom

- `LeakSanitizer` aborted under the managed environment's ptrace wrapper before test exit, so
  `./scripts/test.sh clang-asan` could not complete with leak detection enabled.
- Clang 21's ASan/UBSan runtime in this environment lacked the `vptr` C++ symbols, so the vptr
  sub-check could not be linked.

## Reproduction

`./scripts/test.sh clang-asan` with default `ASAN_OPTIONS` in the managed (ptrace-restricted)
environment.

## Investigation

- Confirmed LeakSanitizer specifically requires ptrace to inspect the process at exit, which the
  managed wrapper disallows.
- Confirmed the missing vptr symbols are an environment runtime packaging issue, not a code defect.

## Root cause

Environment restrictions, not application bugs. LeakSanitizer needs ptrace; the vptr check needs
runtime symbols absent from the installed Clang 21 package.

## Fix

- Run ASan+UBSan with `ASAN_OPTIONS=detect_leaks=0` so address and undefined-behaviour checks still
  execute; leak detection is recorded as environment-unavailable.
- Disable the unavailable `vptr` sub-check in the sanitizer preset while keeping ASan and UBSan
  enabled.

## Verification

`ASAN_OPTIONS=detect_leaks=0 ./scripts/test.sh clang-asan` passes 8/8; `./scripts/test.sh
clang-tsan` passes 8/8.

## Follow-up / prevention

Leak detection and the vptr check remain release-environment verification items: they must be run
on an unrestricted machine before tagging a release. Documented in `README.md` and
`performance/baselines/2026-07-13-v0.2.0.md`.
