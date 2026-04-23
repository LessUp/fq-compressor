<!-- From: /home/shane/dev/fq-compressor/AGENTS.md -->
# AGENTS.md - fq-compressor

This is `fq-compressor`, a high-performance FASTQ compression tool written in C++23.
It combines Assembly-based Compression (ABC) and Statistical Context Mixing (SCM)
to achieve near-entropy compression ratios while maintaining O(1) random access.

---

## Project Posture

`fq-compressor` is in **closeout mode**. Prioritize finishing, simplifying, and stabilizing the
current version over adding new product surface area.

- Treat `openspec/specs/` as the only living requirements source.
- Treat `specs/` as archived reference only.
- Prefer deletion/consolidation over preserving duplicate docs and duplicate tooling.
- Keep GitHub Pages focused on project positioning, onboarding, and proof points rather than being
  a README mirror.
- Keep CI, hooks, MCP, and tool integrations minimal and high-signal.

## Required Development Loop

1. Run `./scripts/dev/preflight.sh` before substantial work.
2. Check `openspec list --json` and keep **one active change at a time**.
3. Use `./scripts/dev/create-worktree.sh <change-name>` only when you want extra isolation.
4. Implement against OpenSpec tasks, not free-form prompts.
5. Run targeted checks first, then repository gates:
   - `./scripts/lint.sh format-check`
   - `./scripts/test.sh clang-debug`
6. Use `/review` for risky or cross-file diffs when it adds signal.
7. Push directly once local checks are green; a PR is not required for the default solo workflow.

## AI Tool Roles

| Tool | Best use in this repo |
|------|------------------------|
| Claude / Codex / Copilot | Cross-file refactors, workflow/tooling cleanup, OpenSpec artifact authoring |
| GLM / lower-cost models | Small OpenSpec tasks with explicit acceptance criteria |
| Review model / subagents | Pre-merge diff review, workflow drift review, doc drift review |

## OpenSpec Workflow

This project uses [OpenSpec](https://github.com/Fission-AI/OpenSpec) for spec-driven development.

### Starting New Work

```
/opsx:propose "<feature description>"
```

This creates a change folder at `openspec/changes/<name>/` with:
- `proposal.md` - What and why
- `specs/` - Requirements and scenarios
- `design.md` - Technical approach
- `tasks.md` - Implementation checklist

### Implementing

```
/opsx:apply <change-name>
```

Works through tasks one by one, marking them complete with `[x]`.

### Completing

```
/opsx:archive <change-name>
```

Moves completed change to archive, syncs delta specs into main specs.

### Exploring

```
/opsx:explore "<topic>"
```

Research and investigate before committing to a change.

### Branch and Review Discipline

- Direct push is the default solo-maintainer path.
- Optional local branches or worktrees are fine when they reduce risk, but they are not mandatory.
- Prefer `/review` before pushing when the diff is risky, architectural, or unusually broad.
- Use `/research` for bounded investigation before drafting a proposal.
- Use `/remote` only when the remote task maps cleanly to one OpenSpec change.

### Spec Structure

Living specifications are in `openspec/specs/`:

| Capability | Description |
|------------|-------------|
| `compression` | FASTQ compression, algorithms, long-read/PE support |
| `decompression` | Decompression, range extraction, output options |
| `random-access` | Block-based O(1) random access |
| `quality-handling` | Quality score modes (lossless/lossy/discard) |
| `performance` | Parallelism, memory management |
| `integrity` | Checksums, verification, recovery |
| `cli` | Command-line interface |
| `build-system` | CMake, Conan, CI/CD |
| `atomic-write` | Safe file writing |
| `compatibility` | Version compatibility |
| `file-format` | FQC binary format |

---

## Project Overview

- **Name**: fq-compressor
- **Version**: 0.2.0
- **Language**: C++23
- **License**: MIT (project code); vendored code keeps its own license
- **Homepage**: https://github.com/LessUp/fq-compressor
- **Documentation**: https://lessup.github.io/fq-compressor/

### Key Features

- 3.97× compression ratio on Illumina data
- 11.9 MB/s compression, 62.3 MB/s decompression (multithreaded)
- O(1) random access without full decompression
- Intel oneTBB parallel pipeline
- Transparent support for .gz, .bz2, .xz inputs

## Project Structure

```text
include/fqc/           Public header files (API)
  algo/               Compression algorithms (ABC, SCM)
  commands/           CLI command APIs
  common/             Errors, logging, types, memory budget
  format/             FQC archive format, reader/writer, reorder map
  io/                 FASTQ parsing, compressed stream I/O, async I/O
  pipeline/           TBB pipeline types and nodes

src/                   Implementation files
  algo/               Algorithm implementations
  commands/           Command implementations
  common/             Common utilities
  format/             Format implementations
  io/                 I/O implementations
  pipeline/           Pipeline node implementations
  main.cpp            CLI entry point

tests/                 Test files (GTest + optional RapidCheck)
  algo/               Algorithm tests
  commands/           Command tests
  common/             Common utility tests
  format/             Format tests
  io/                 I/O tests
  pipeline/           Pipeline tests
  e2e/                End-to-end tests

scripts/               Build, test, lint, dependency helpers
  build.sh            Main build script
  test.sh             Simple test runner (delegates to run_tests.sh)
  run_tests.sh        Advanced test runner with filtering/labels
  lint.sh             Format and static analysis
  install_deps.sh     Conan dependency installation

cmake/                 CMake modules
  fqc_coverage.cmake  Code coverage support
  fqc_sanitizers.cmake Sanitizer support (ASan, TSan, UBSan, MSan)

docs/                  Documentation
  en/                 English documentation
  zh/                 Chinese documentation
  website/            Next.js + Nextra documentation site

vendor/                Third-party vendored code
  spring-core/        Spring ABC algorithm (research license)

ref-projects/          Reference implementations for comparison
  HARC/, NanoSpring/, Spring/, fqzcomp5/, minicom/, etc.
```

## Technology Stack

### Build System
- **CMake**: 3.28+ with presets (see `CMakePresets.json`)
- **Ninja**: Build generator
- **Conan**: 2.x for dependency management
- **Compiler**: GCC 14+ or Clang 18+ (C++23 required)

### Dependencies (via Conan)
| Purpose | Library | Version |
|---------|---------|---------|
| CLI parsing | CLI11 | 2.4.2 |
| Logging | Quill | 11.0.2 |
| Formatting | fmt | 12.1.0 |
| Compression | zlib-ng, bzip2, xz_utils | various |
| Compression | zstd, libdeflate | 1.5.7, 1.25 |
| Checksums | xxHash | 0.8.3 |
| Parallelism | Intel oneTBB | 2022.3.0 |
| Testing | GTest | 1.12.1 |
| Property Testing | RapidCheck | optional (currently disabled) |

### Additional Tools
- **clang-format-21**: Code formatting (100 column limit, 4-space indent)
- **clang-tidy-21**: Static analysis
- **ccache**: Compilation caching

## Build Commands

### Prerequisites
```bash
# Install Conan 2.x
pip install conan

# Create default profile
conan profile detect --force
```

### Building

**Main build script (recommended):**
```bash
./scripts/build.sh <preset> [jobs]
```

**Direct CMake:**
```bash
cmake --preset <preset>
cmake --build --preset <preset> -j$(nproc)
```

### Available Build Presets

| Preset | Compiler | Mode | Notes |
|--------|----------|------|-------|
| `gcc-debug` | GCC | Debug | Development |
| `gcc-release` | GCC | Release | Production build |
| `gcc-relwithdebinfo` | GCC | RelWithDebInfo | Release + debug info |
| `clang-debug` | Clang | Debug | Recommended for dev |
| `clang-release` | Clang | Release | Optimized build |
| `clang-asan` | Clang | Debug | AddressSanitizer |
| `clang-tsan` | Clang | Debug | ThreadSanitizer |
| `coverage` | GCC | Debug | Code coverage |

### Build Outputs

```text
build/<preset>/
  src/fqc                    Main executable
  src/libfqc_core.a          Core library
  src/libfqc_cli.a           CLI library
  tests/<test_name>          Test executables
  compile_commands.json      For clang-tidy
```

## Test Commands

### Simple Test Runner
```bash
./scripts/test.sh [preset] [filter]
```

### Advanced Test Runner
```bash
./scripts/run_tests.sh -p <preset> [options]

Options:
  -p, --preset <name>     CMake preset (default: clang-debug)
  -v, --verbose           Verbose output
  -f, --filter <pattern>  Filter tests by regex (ctest -R)
  -l, --label <label>     Run only tests with label (unit|property)
  -b, --build-only        Build only, don't run
```

### Running Specific Tests

```bash
# Run all tests with gcc-release
./scripts/test.sh gcc-release

# Run only unit tests
./scripts/run_tests.sh -p gcc-release -l unit

# Run specific test by name
./scripts/run_tests.sh -p gcc-release -f '^memory_budget_test$'

# Run a specific GTest case
build/clang-debug/tests/memory_budget_test \
  --gtest_filter=MemoryBudgetTest.DefaultConstruction
```

### Current Test Targets

**Unit Tests:**
- `build_smoke_test`
- `memory_budget_test`
- `error_test`
- `fqc_writer_test`
- `async_io_test`
- `compressed_stream_test`
- `original_order_command_test`

**Property Tests (RapidCheck):**
- `fqc_format_property_test`
- `fastq_parser_property_test`
- `two_phase_compression_property_test`
- `quality_compressor_property_test`
- `id_compressor_property_test`
- `long_read_property_test`
- `pe_property_test`
- `pipeline_property_test`

## Lint / Format Commands

```bash
# Format all source files in place
./scripts/lint.sh format

# Check formatting (CI mode)
./scripts/lint.sh format-check

# Run clang-tidy static analysis
./scripts/lint.sh lint clang-debug

# Run all checks (format + lint)
./scripts/lint.sh all clang-debug
```

**Notes:**
- Requires `compile_commands.json` from a successful build for linting
- Prefers `clang-format-21` and `clang-tidy-21` when available

## Code Style Guidelines

### Naming Conventions

| Entity | Convention | Example |
|--------|------------|---------|
| Files/Directories | snake_case | `block_compressor.cpp` |
| Types/Classes/Structs | PascalCase | `BlockCompressor` |
| Functions/Methods | camelCase | `compressBlock()` |
| Variables/Parameters | camelCase | `inputPath` |
| Private/Protected members | camelCase + `_` | `buffer_` |
| Constants | k + PascalCase | `kMaxBlockSize` |
| Namespaces | lower_case | `fqc::algo` |
| Macros | UPPER_CASE | `FQC_ASSERT` |
| Enums | PascalCase values | `ErrorCode::kSuccess` |

### Code Formatting

- **Indentation**: 4 spaces (no tabs)
- **Line limit**: 100 columns
- **Braces**: Attached (K&R style)
- **Max empty lines**: 1

### Include Order

1. Project headers (`"fqc/..."`)
2. Standard library headers
3. Third-party/system headers

Includes are automatically sorted/regrouped by clang-format.

### Error Handling

- **DO NOT** throw `std::runtime_error`
- Use `FQCException` or derived exceptions (`ArgumentError`, `IOError`, etc.)
- Use `ErrorCode` values from `include/fqc/common/error.h`
- Prefer `Result<T>` / `VoidResult` over throwing for library APIs
- Preserve detailed context using `ErrorContext` and `std::source_location`

### API Usage Patterns

- Mark non-trivial return values `[[nodiscard]]`
- Prefer `std::span`, `std::string_view`, `std::filesystem::path`
- Use `constexpr`, `inline constexpr`, scoped enums for constants
- Prefer smart pointers/containers over raw owning pointers
- Avoid `new`/`delete` in normal code

### Logging

Use Quill-based logging from `include/fqc/common/logger.h`:

```cpp
FQC_LOG_TRACE("...");
FQC_LOG_DEBUG("...");
FQC_LOG_INFO("...");
FQC_LOG_WARNING("...");
FQC_LOG_ERROR("...");
FQC_LOG_CRITICAL("...");
```

Do NOT use `std::cout` for status/debug logging.

## Architecture Overview

### Compression Pipeline

```
FASTQ Input → FastqParser → GlobalAnalyzer → BlockCompressor → FQC Writer
                                              (TBB parallel pipeline)
```

### Decompression Pipeline

```
FQC Input → FQC Reader → Decompressor → FastqWriter → FASTQ Output
              (TBB parallel pipeline)
```

### Key Algorithms

1. **Assembly-based Compression (ABC)**
   - Minimizer Bucketing - groups reads by shared k-mer signatures
   - TSP Reordering - maximizes neighbor similarity
   - Consensus Generation - builds local consensus per bucket
   - Delta Encoding - stores only edits from consensus

2. **Statistical Context Mixing (SCM)**
   - Context: Previous QVs + current base + position
   - Prediction: High-order context modeling
   - Coding: Adaptive arithmetic coding

### Module Organization

| Namespace | Purpose |
|-----------|---------|
| `fqc::algo` | Compression algorithms (ABC core) |
| `fqc::commands` | CLI command implementations |
| `fqc::common` | Shared utilities (errors, logging, types) |
| `fqc::format` | FQC archive format definition |
| `fqc::io` | I/O abstractions (FASTQ, compressed streams) |
| `fqc::pipeline` | TBB-based parallel processing pipelines |

## CLI Commands

### Main Commands

```bash
fqc compress -i <input> -o <output> [options]
fqc decompress -i <input> -o <output> [options]
fqc info <file>
fqc verify <file>
```

### Global Options
- `-t, --threads <n>`: Thread count (0 = auto)
- `-v, --verbose`: Increase verbosity
- `-q, --quiet`: Suppress non-error output
- `--memory-limit <MB>`: Memory limit
- `--no-progress`: Disable progress display
- `--json`: JSON output for info/verify

## CI/CD and Deployment

### GitHub Actions Workflows

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `ci.yml` | PR/push to main | Build & test with multiple presets |
| `quality.yml` | PR to main | Format and static analysis |
| `pages.yml` | push to main | Deploy documentation |
| `release.yml` | Tag push | Build and publish release binaries |

### Docker Support

```bash
# Build development image
docker build -f docker/Dockerfile.dev -t fq-compressor:dev .

# Build release images
docker build -f docker/Dockerfile.musl-release -t fq-compressor:musl .
docker build -f docker/Dockerfile.glibc-release -t fq-compressor:glibc .
```

### Release Process

1. Update version in `CMakeLists.txt`
2. Update `CHANGELOG.md`
3. Create git tag: `git tag -a v0.x.x -m "Release v0.x.x"`
4. Push tag: `git push origin v0.x.x`
5. GitHub Actions builds and publishes release binaries

## Security Considerations

- Sanitizer builds (`clang-asan`, `clang-tsan`) detect memory and thread issues
- Checksum verification (xxh64) ensures data integrity
- Input validation at all entry points
- No use of unsafe C functions
- Fuzzing support via property-based tests

## Development Checklist

Before submitting changes:

```bash
# 1. Build with preset of choice
./scripts/build.sh gcc-release

# 2. Run tests
./scripts/test.sh gcc-release

# 3. Check formatting
./scripts/lint.sh format-check

# 4. Run static analysis
./scripts/lint.sh lint clang-debug
```

## Common Patterns

### Adding a New Command

1. Add header in `include/fqc/commands/`
2. Add implementation in `src/commands/`
3. Register in `src/main.cpp`
4. Add tests in `tests/commands/`

### Adding a New Test

1. Add test file in appropriate `tests/` subdirectory
2. Register in `tests/CMakeLists.txt`:
   ```cmake
   fqc_add_test(my_test
       SOURCES my/my_test.cpp
   )
   ```
   For property tests, add `PROPERTY_BASED` flag.
3. Run: `./scripts/run_tests.sh -f '^my_test$'`

## Useful Resources

- **Documentation**: https://lessup.github.io/fq-compressor/
- **CI Status**: See badges in README.md
- **Reference**: Spring paper (Chandak et al., 2019) for ABC algorithm
- **Spring Core**: Located in `vendor/spring-core/` (research license)

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Usage/argument error |
| 2 | I/O error |
| 3 | Format/version error |
| 4 | Checksum verification failed |
| 5 | Unsupported algorithm/codec |

## Notes for AI Agents

- Always use the build scripts (`./scripts/build.sh`, `./scripts/test.sh`) rather than raw cmake when possible
- Follow existing file header style with section separators (`// ==========`)
- Match existing code style exactly (formatting is enforced)
- Preserve or improve test coverage for any changes
- Binary format changes require explicit versioning updates
- Do not add system packages to Dockerfiles; use Conan for dependencies
- The project uses C++23 features extensively; verify compiler compatibility
