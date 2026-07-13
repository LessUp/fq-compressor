# 0007 Conan toolchain drift (zlib-ng fallback, Clang 21 compat, CMake export)

- Status: closed
- Severity: medium
- Found in: v2 phases 6-7 (pre-75b7400)
- Related: conanfile.py, cmake/FQCompressorConfig.cmake.in, CMakePresets.json

## Symptom

Three related dependency/toolchain issues blocked a clean release build and external consumer use:

1. zlib-ng default mode did not export `ZLIB::ZLIB` and allowed a silent fallback to system zlib.
2. Clang 21 is newer than Conan's declared compiler settings, so ASan configuration was rejected.
3. The installed CMake export interpreted `COMPONENT` as an include path, breaking external
   consumers.

## Reproduction

1. Fresh `clang-release` configure with zlib-ng in default mode — resolves system zlib instead.
2. `./scripts/test.sh clang-asan` with Conan's stock Clang compatibility setting — rejected.
3. External `find_package(FQCompressor)` + `target_link_libraries(... FQCompressor::fqc_core)` —
   include-path error.

## Investigation

- zlib-ng: confirmed the alias target was missing in compat mode; the system zlib silently
  satisfied the dependency.
- Clang 21: Conan's profile knew Clang up to 20; the compiler.version check rejected 21.
- CMake export: the `COMPONENT` argument was placed where CMake interpreted it as a path fragment.

## Root cause

- zlib-ng needs `zlib_compat=True` to expose the `ZLIB::ZLIB` target.
- Conan's compatibility table lags the installed compiler; the setting must be clamped to the
  highest known value rather than left to auto-detect.
- The install/export argument placement was wrong for the `COMPONENT` keyword.

## Fix

- Set `zlib_compat=True` in `conanfile.py`; fresh configurations now resolve Conan's zlib-ng
  target.
- Detect the installed compiler and clamp only Conan's compatibility setting to its highest known
  Clang value (20); delete the unused root `CMakeUserPresets.json` aggregator after each dependency
  install so multiple `conan-debug` presets do not collide.
- Correct the install/export argument placement and verify an external configure/build/run
  consumer.

## Verification

- Fresh `clang-release` and `clang-asan` configure/build pass.
- An external consumer project configures, builds, and runs against the installed package.

## Follow-up / prevention

The `install_deps.sh` flow now produces a single toolchain per preset; the external consumer check
is part of the release checklist in `README.md`.
