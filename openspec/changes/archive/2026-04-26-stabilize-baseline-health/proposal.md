## Why

The repository cannot safely absorb larger cleanup work while its default formatting and unit-test
baseline is unstable. A green, repeatable baseline is required before broader closeout refactors can
be reviewed with confidence.

## What Changes

- Re-establish the default repository health gate around `format-check` and `clang-debug` unit tests.
- Tighten regression coverage for async I/O and compressed stream edge cases.
- Make the baseline explicit so future cleanup changes start from the same validation contract.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `build-system`: define the default repository health gate and regression expectations used before
  merge.

## Impact

- `include/fqc/common/logger.h`
- `src/io/async_io.cpp`
- `src/io/compressed_stream.cpp`
- `tests/io/async_io_test.cpp`
- `tests/io/compressed_stream_test.cpp`
- Validation scripts and CI expectations tied to formatting and `clang-debug` unit tests
