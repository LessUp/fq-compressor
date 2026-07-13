# 0005 Concurrent preset runs collided on shared fixture temp dir

- Status: closed
- Severity: medium
- Found in: v2 phase 7 (pre-75b7400)
- Related: tests/io/compressed_stream_test.cpp, tests/CMakeLists.txt

## Symptom

Running `./scripts/test.sh clang-debug` and `./scripts/test.sh clang-release` concurrently caused
one preset's `compressed_stream_test` to delete a compressed fixture still in use by the other,
producing intermittent "file not found" failures.

## Reproduction

In two shells, simultaneously:

```bash
./scripts/test.sh clang-debug
./scripts/test.sh clang-release
```

## Investigation

- Failures were intermittent and pointed at the same temp path.
- Inspected the fixture setup: a hard-coded shared temp directory under `/tmp` was used by both
  presets.

## Root cause

Fixture directories were not scoped per process, so concurrent preset runs operated on the same
files.

## Fix

Scope fixture directories by process ID (PID) so each test run gets a process-unique temp tree.

## Verification

Concurrent `clang-debug` and `clang-release` test runs both pass 8/8 after the change.

## Follow-up / prevention

The pattern is now established in the test CMake setup: any new fixture that writes to temp must
use the process-unique helper rather than a shared path.
