# Fix CI Format Check Failure

## Context

The CI format check is failing because of a clang-format version mismatch:
- **Local environment**: clang-format **18.1.3**
- **CI environment**: clang-format **21** (from Docker image)

The same code formats differently between these versions. Files that pass format-check locally fail in CI.

## Affected Files

Based on the CI error log:
1. `src/io/stream_factory.cpp` - Lines 55-56, 108-109, 133-134, 197-198
2. `include/fqc/io/stream_factory.h` - Lines 105-106, 285-286
3. `tests/io/compressed_stream_test.cpp` - Line 451

## Root Cause

The formatting rules for function declarations with trailing return types differ between clang-format versions. The `BinPackParameters: false` and `AllowAllParametersOfDeclarationOnNextLine: true` settings behave differently.

## Solution

Reformat all affected files using clang-format-21 to match CI expectations. Since clang-format-21 is not installed locally, we have two options:

### Option A: Build CI Docker image locally (preferred)
1. Build the CI Docker image: `docker build -f docker/Dockerfile.dev --target base -t fqcompressor-ci:format .`
2. Run format via Docker: `docker run --rm -v "${PWD}:/workspace" -w /workspace fqcompressor-ci:format ./scripts/lint.sh format`
3. Verify: `docker run --rm -v "${PWD}:/workspace" -w /workspace fqcompressor-ci:format ./scripts/lint.sh format-check`

### Option B: Install clang-format-21 locally
1. Install clang-format-21 via apt or LLVM script
2. Run: `./scripts/lint.sh format`
3. Verify: `./scripts/lint.sh format-check`

## Recommended Approach: Option A

Using the same Docker image as CI ensures consistency and avoids version drift.

## Verification

After reformatting:
1. Run format-check with clang-format-21: `./scripts/lint.sh format-check` (must use clang-format-21)
2. Verify the changes look correct (read modified files)
3. Commit and push to trigger CI

## Critical Files

- `src/io/stream_factory.cpp`
- `include/fqc/io/stream_factory.h`
- `tests/io/compressed_stream_test.cpp`
