<!-- From: /home/shane/dev/fq-compressor/AGENTS.md -->
# AGENTS.md - fq-compressor

This is `fq-compressor`, a high-performance FASTQ compression tool written in C++23.
FQC v2 uses independent sequential frames, bounded-memory admission, packed DNA, Zstd,
and XXH64. It deliberately has no v1 compatibility or random-access path.

---

## Project Posture

`fq-compressor` is in **closeout mode**. Prioritize finishing, simplifying, and stabilizing the
current version over adding new product surface area.

- Treat `docs/archive/` as archived reference only.
- Do not treat OpenSpec as the current requirements or workflow source.
- Prefer deletion/consolidation over preserving duplicate docs and duplicate tooling.
- Keep CI and tool integrations minimal and high-signal.

## Development Workflow

Simple, direct workflow optimized for closeout mode:

1. **Understand** - Read relevant code and docs first
2. **Implement** - Write code directly, following existing patterns
3. **Verify** - Run `./scripts/test.sh clang-debug` and `./scripts/lint.sh format-check`
4. **Commit** - Push directly when checks pass; no PR required

For risky or cross-file changes, use `/review` before pushing.

## AI Tool Roles

| Tool | Best use in this repo |
|------|------------------------|
| Claude / Codex / Copilot | Cross-file refactors, tooling cleanup, documentation |
| Review model / subagents | Pre-merge diff review, doc drift review |

---

## Project Overview

- **Name**: fq-compressor
- **Version**: 0.3.0-rc1
- **Language**: C++23
- **License**: MIT (project code); vendored code keeps its own license
- **Homepage**: https://github.com/LessUp/fq-compressor
- **Documentation**: See `README.md` and `ARCHITECTURE.md` in the repository

### Key Features

- Lossless sequential FQC v2 frames with strict checksums and allocation bounds
- Measured x86_64 fallback throughput above 50 MiB/s compress and 100 MiB/s decompress
- Illumina, ONT, PacBio HiFi, and PacBio CLR profile metadata
- Transparent support for .gz inputs

## Project Structure

```text
include/fqc/           Public header files (API)
  commands/           V2 engine API
  common/             Errors, logging, record types
  format/             Sequential FQC v2 reader/writer
  io/                 FASTQ parsing and stream I/O

src/                   Implementation files
  commands/           V2 engine implementation
  common/             Common utilities
  format/             V2 format implementation
  io/                 I/O implementations
  main.cpp            CLI entry point

tests/                 Test files (GTest)
  commands/           Command tests
  common/             Common utility tests
  format/             Format tests
  io/                 I/O tests
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

troubleshooting/        Problem log: symptoms, investigation, root cause, fix
  INDEX.md            Status table for all issues
  TEMPLATE.md         Copy this when opening a new issue
  NNNN-*.md           One file per issue

performance/            Performance data and optimisation history
  INDEX.md            Current best results per profile x dataset
  baselines/          Immutable milestone snapshots
  optimizations/      One file per optimisation (before/after)
  runs/YYYY-MM-DD/    Raw harness output and pinned environment
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
| Compression | zlib-ng | 2.3.2 |
| Compression | zstd | 1.5.7 |
| Checksums | xxHash | 0.8.3 |
| Testing | GTest | 1.12.1 |

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
./scripts/run_tests.sh -p gcc-release -f '^v2_archive_engine_test$'

# Run a specific GTest case
build/clang-debug/tests/v2_archive_engine_test \
  --gtest_filter=V2ArchiveEngineTest.*
```

### Current Test Targets

**Unit Tests:**
- `build_smoke_test`
- `error_test`
- `v2_archive_test`
- `stream_factory_test`
- `fastq_parser_test`
- `compressed_stream_test`
- `v2_archive_engine_test`
- `v2_cli_smoke_test`

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
| Types/Classes/Structs | PascalCase | `ArchiveWriter` |
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
FASTQ Input → FastqParser → Profile Resolver → Bounded Frame Accumulator → FQC v2 Writer
```

### Decompression Pipeline

```
FQC v2 Input → Bounded Frame Reader → Canonical FASTQ Writer → FASTQ Output
```

### Key Algorithms

1. **Sequence packing** - 2-bit uppercase A/C/G/T plus exact delta-position exceptions.
2. **Stream coding** - independent varint-framed ID/comment, sequence, and quality streams using
   Zstd level 1.
3. **Integrity** - XXH64 header, logical frame, and rolling footer checksums.
4. **Memory admission** - exact raw stream measurement and conservative encode/decode peak checks.

### Module Organization

| Namespace | Purpose |
|-----------|---------|
| `fqc::commands::v2` | Profile resolution and bounded archive engine |
| `fqc::common` | Shared utilities (errors, logging, types) |
| `fqc::format::v2` | Sequential FQC v2 archive definition |
| `fqc::io` | I/O abstractions (FASTQ, compressed streams) |

## CLI Commands

### Main Commands

```bash
fqc compress -i <input> -o <output> [options]
fqc decompress -i <input> -o <output> [options]
fqc verify <file>
```

### Global Options
- `-q, --quiet`: Suppress non-error output
- `--memory-limit <MiB>`: Hard operation budget (default 16384, minimum 64)

## CI/CD and Deployment

### GitHub Actions Workflows

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `ci.yml` | PR/push to main | Format check, build & test with multiple presets |
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
3. Run: `./scripts/run_tests.sh -f '^my_test$'`

## Useful Resources

- **Documentation**: See `README.md` and `ARCHITECTURE.md` in this repository
- **CI Status**: See badges in README.md
- **Format and memory model**: See `ARCHITECTURE.md`

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
