# fq-compressor tests

This directory contains unit tests and property-based tests.

## Testing frameworks

- **Google Test (GTest)** for unit tests
- **RapidCheck** for property-based tests

## Test file naming convention

- Unit tests: `*_test.cpp`
- Property tests: `*_property_test.cpp`

## Directory structure

- `algo/` — Algorithm tests (block compressor, ID compressor, quality compressor, etc.)
- `common/` — Common utility tests (memory budget, etc.)
- `e2e/` — End-to-end CLI tests (shell scripts)
- `format/` — FQC format tests (reader/writer round-trip)
- `io/` — I/O tests (FASTQ parser)
- `pipeline/` — Pipeline tests (compression/decompression round-trip)

## Running tests

```bash
# Run all tests with a preset
./scripts/test.sh clang-debug

# Run with filter
./scripts/test.sh clang-debug 'MemoryBudget*'

# Run via ctest directly
ctest --test-dir build/clang-debug --output-on-failure
```
