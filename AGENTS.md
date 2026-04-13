# AGENTS.md

## Scope

This repository is `fq-compressor`, a C++23 FASTQ compressor.
It uses CMake presets, Conan 2.x, Ninja, Google Test, optional RapidCheck, Quill logging, and Intel oneTBB.
Core binaries and libraries are built from `src/`; public headers live in `include/fqc/`.

No Cursor rules were found in `.cursor/rules/` or `.cursorrules`.
No Copilot rules were found in `.github/copilot-instructions.md`.

## High-Level Layout

```text
include/fqc/algo/        Compression algorithms
include/fqc/commands/    CLI command APIs
include/fqc/common/      Errors, logging, shared types, memory budget
include/fqc/format/      FQC archive format, reader, writer, reorder map
include/fqc/io/          FASTQ parsing and compressed stream I/O
include/fqc/pipeline/    TBB pipeline types and nodes
src/                     Implementations and main executable
tests/                   GTest and RapidCheck tests
scripts/                 Build, test, lint, dependency helpers
cmake/                   Sanitizer and coverage helpers
```

## Build Commands

Preferred preset for release verification:

```bash
./scripts/build.sh gcc-release
```

Common development builds:

```bash
./scripts/build.sh clang-debug
./scripts/build.sh clang-asan
./scripts/build.sh clang-tsan
./scripts/build.sh coverage
```

Direct CMake equivalents:

```bash
cmake --preset gcc-release
cmake --build --preset gcc-release -j$(nproc)
```

Important build outputs:

```text
build/<preset>/src/fqc
build/<preset>/tests/<test_target>
build/<preset>/compile_commands.json
```

`./scripts/build.sh` auto-runs Conan dependency installation if the toolchain file is missing.
Do not add system `-dev` packages to Dockerfiles for project libraries; use Conan.

## Test Commands

Run the full suite:

```bash
./scripts/test.sh gcc-release
./scripts/test.sh clang-debug
```

Advanced test runner entrypoint:

```bash
./scripts/run_tests.sh -p gcc-release
./scripts/run_tests.sh -p clang-asan -v
./scripts/run_tests.sh -p gcc-release -l unit
./scripts/run_tests.sh -p gcc-release -l property
```

Run a single CTest target by name:

```bash
./scripts/run_tests.sh -p gcc-release -f '^memory_budget_test$'
ctest --test-dir build/gcc-release --output-on-failure -R '^memory_budget_test$'
```

Build one test target only:

```bash
cmake --build --preset gcc-release --target memory_budget_test
```

Run one GTest case inside a test binary:

```bash
build/gcc-release/tests/memory_budget_test \
  --gtest_filter=MemoryBudgetTest.DefaultConstruction
```

Useful current test targets registered in `tests/CMakeLists.txt`:

```text
build_smoke_test
memory_budget_test
fqc_writer_test
original_order_command_test
fqc_format_property_test
fastq_parser_property_test
two_phase_compression_property_test
quality_compressor_property_test
id_compressor_property_test
long_read_property_test
pe_property_test
pipeline_property_test
```

Notes:

- `./scripts/test.sh <preset> <filter>` passes the filter to `ctest -R`.
- Property tests are only registered when RapidCheck is found by CMake.
- Test executables live under `build/<preset>/tests/` in the current layout.

## Lint / Format / Static Analysis

Run these before committing:

```bash
./scripts/lint.sh format-check
./scripts/lint.sh lint clang-debug
./scripts/test.sh gcc-release
./scripts/build.sh gcc-release
```

Format in place:

```bash
./scripts/lint.sh format
```

Combined checks:

```bash
./scripts/lint.sh all clang-debug
```

Tooling expectations:

- `clang-format-21` is preferred when available.
- `clang-tidy-21` is preferred when available.
- `clang-tidy` reads `build/<preset>/compile_commands.json`.

## Language And Toolchain Rules

- Use C++23.
- Keep `CMAKE_CXX_EXTENSIONS` off; write portable standard C++.
- Minimum supported compilers in CMake are GCC 14+ and Clang 18+.
- Repository guidance prefers GCC 15.2 and Clang 21 when available.
- Use the preset system in `CMakePresets.json` instead of ad hoc build directories.

## Code Style

Formatting is enforced by `.clang-format`:

- 4-space indentation.
- 100-column line limit.
- Attached braces.
- No tabs.
- One blank line max between blocks.
- Includes are sorted and regrouped automatically.

Use the repository's file/header style:

- Files and directories: `snake_case`.
- Types, structs, classes, test fixtures: `PascalCase`.
- Functions, methods, local variables, parameters: `camelCase`.
- Private/protected members: `camelCase_`.
- Constants: `kCamelCase`.
- Namespaces: `lower_case`.
- Macros: `UPPER_CASE`.

Follow the include order implied by `.clang-format` and existing code:

1. Project headers, especially `"fqc/..."`
2. Standard library headers
3. Third-party/system headers

Keep include blocks sorted; do not hand-format around the formatter.

## API And Type Usage

- Prefer `std::expected`-style flows through the project's `Result<T>` and `VoidResult` aliases.
- Use `std::span`, `std::string_view`, `std::filesystem::path`, and fixed-width integer types where appropriate.
- Mark non-trivial return values `[[nodiscard]]` when they should not be ignored; this is common across public headers here.
- Prefer `constexpr`, `inline constexpr`, and scoped enums for constants and flags.
- Prefer smart pointers or containers for ownership; avoid raw owning pointers.
- Avoid `new` and `delete` in normal project code.

## Error Handling

- Do not throw `std::runtime_error`.
- Use `FQCException` or a project-specific derived error such as `ArgumentError`.
- Use `ErrorCode` values from `include/fqc/common/error.h`.
- Map failures to the structured error categories already defined there.
- For recoverable operations and library-style APIs, prefer `Result<T>` / `VoidResult` over throwing.
- Preserve detailed context when constructing errors; `ErrorContext` and `std::source_location` support already exist.

## Logging And Output

- Use Quill-based logging from `include/fqc/common/logger.h`.
- Prefer `FQC_LOG_TRACE`, `FQC_LOG_DEBUG`, `FQC_LOG_INFO`, `FQC_LOG_WARNING`, `FQC_LOG_ERROR`, and `FQC_LOG_CRITICAL`.
- Do not use `std::cout` for status/debug logging.
- Reserve stdout/stderr for real CLI output, stream data, or user-facing summaries only when that behavior is intentional.

## Testing Conventions

- Unit tests use Google Test.
- Property tests use RapidCheck through `RC_GTEST_PROP` when available.
- New tests must be added to `tests/CMakeLists.txt` via `fqc_add_test(...)`.
- Unit tests get the `unit` CTest label; property tests get the `property` label.
- Keep or improve coverage when changing compression, format, parser, or pipeline behavior.
- Do not delete or weaken tests to make a change pass.

## Repository-Specific Implementation Rules

- Public API headers live in `include/fqc/...`; implementation files should stay in the matching `src/...` area.
- Keep command implementations in `src/commands/` and register new CLI subcommands in `src/main.cpp`.
- Binary-format changes require explicit versioning updates; do not silently change archive layout.
- Compression, decompression, format, and reorder-map changes should be paired with tests.
- Dependency additions need justification and should go through Conan, not ad hoc package installation.

## Commenting And Structure

- Match the existing source style with major section separators like `// =============================================================================` when editing established files that already use them.
- Public headers commonly use concise Doxygen-style comments; keep additions consistent.
- Add comments only when they explain non-obvious behavior, invariants, or format details.
- Prefer small, direct changes over introducing unnecessary abstractions.

## Agent Checklist

Before finishing substantial code changes, run:

```bash
./scripts/build.sh gcc-release
./scripts/test.sh gcc-release
./scripts/lint.sh format-check
./scripts/lint.sh lint clang-debug
```

If you only touched a small area, at minimum run the most relevant single test target plus formatting/linting for the affected preset.
