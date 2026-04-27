# fq-compressor - QWEN Context File

## Project Overview

**fq-compressor** is a high-performance FASTQ compression tool written in modern C++23. It leverages **Assembly-based Compression (ABC)** and **Statistical Context Mixing (SCM)** algorithms to achieve near-entropy compression ratios (3.97× on Illumina data) while maintaining **O(1) random access** to compressed archives.

### Key Features
- 🧬 **3.97× compression ratio** on Illumina sequencing data
- ⚡ **11.9 MB/s** compression, **62.3 MB/s** decompression (multithreaded)
- 🎯 **Random access** without full decompression
- 🚀 **Intel oneTBB** parallel pipeline for multicore scaling
- 📦 **Transparent support** for .gz, .bz2, .xz compressed inputs
- 🔧 Modern C++23 codebase with type safety and high performance

### Core Algorithms

1. **Assembly-based Compression (ABC)**: Treats reads as genome fragments
   - Minimizer bucketing by shared k-mer signatures
   - TSP reordering to maximize neighbor similarity
   - Consensus generation per bucket
   - Delta encoding from consensus sequences

2. **Statistical Context Mixing (SCM)**: For quality scores
   - High-order context modeling (previous QVs + current base + position)
   - Adaptive arithmetic coding
   - Near-entropy compression

## Technology Stack

### Languages & Standards
- **C++23** (strict standard compliance, no extensions)
- **CMake 3.28+** (build system)
- **Conan 2.x** (dependency management)

### Key Dependencies
| Library | Purpose | Version |
|---------|---------|---------|
| CLI11 | Command-line parsing | 2.4.2 |
| Quill + fmt | Asynchronous logging | 11.0.2 |
| Intel oneTBB | Parallel pipeline | 2022.3.0 |
| xxHash | Checksums | 0.8.3 |
| zlib-ng | gzip support | 2.3.2 |
| zstd | Fast compression | 1.5.7 |
| bzip2 | bzip2 support | 1.0.8 |
| xz_utils | LZMA support | 5.4.5 |
| libdeflate | Fast deflate | 1.25 |

### Testing Frameworks
- **Google Test** (gtest 1.15.0) - Unit testing
- **RapidCheck** (cci.20230815) - Property-based testing

### Tooling
- **Ninja** - Build generator
- **clang-format-21** - Code formatting
- **clang-tidy-21** - Static analysis
- **ccache** - Build caching

## Project Structure

```
fq-compressor/
├── include/fqc/           # Public API headers
│   ├── algo/              # Compression algorithms
│   ├── commands/          # CLI command APIs
│   ├── common/            # Errors, logging, types, memory
│   ├── format/            # FQC archive format
│   ├── io/                # FASTQ parsing, I/O
│   └── pipeline/          # TBB pipeline nodes
├── src/                   # Implementation files
│   ├── algo/              # Algorithm implementations
│   ├── commands/          # Command handlers
│   ├── common/            # Common utilities
│   ├── format/            # Format implementations
│   ├── io/                # I/O implementations
│   ├── pipeline/          # Pipeline implementations
│   ├── CMakeLists.txt     # Source build config
│   └── main.cpp           # CLI entry point
├── tests/                 # Test suite
│   ├── algo/              # Algorithm tests
│   ├── commands/          # Command tests
│   ├── common/            # Common tests
│   ├── e2e/               # End-to-end tests
│   ├── format/            # Format tests
│   ├── io/                # I/O tests
│   ├── pipeline/          # Pipeline tests
│   └── CMakeLists.txt     # Test build config
├── specs/                 # Technical specifications (SSOT)
│   ├── product/           # Product requirements
│   ├── rfc/               # Technical design documents
│   └── testing/           # Test specifications
├── docs/                  # Documentation
│   ├── website/           # Nextra documentation site
│   ├── archive/           # Historical documents
│   ├── dev/               # Developer guides
│   ├── en/                # English docs
│   └── zh/                # Chinese docs
├── scripts/               # Build/test/lint scripts
├── cmake/                 # CMake helpers
├── docker/                # Docker configurations
├── benchmark/             # Performance benchmarks
├── conan/                 # Conan profiles/lockfiles
└── vendor/                # Vendored dependencies
```

## Building and Running

### Prerequisites
- **Compiler**: GCC 14+ or Clang 18+
- **CMake**: 3.28+
- **Conan**: 2.x
- **Ninja**: (auto-installed by Conan)

### Quick Build Commands

```bash
# Preferred release build
./scripts/build.sh gcc-release

# Development builds
./scripts/build.sh clang-debug
./scripts/build.sh clang-asan    # With AddressSanitizer
./scripts/build.sh clang-tsan    # With ThreadSanitizer
./scripts/build.sh coverage      # With coverage

# Direct CMake commands
cmake --preset gcc-release
cmake --build --preset gcc-release -j$(nproc)
```

### Build Presets

| Preset | Compiler | Mode | Sanitizers |
|--------|----------|------|------------|
| `gcc-debug` | GCC 15.2 | Debug | None |
| `gcc-release` | GCC 15.2 | Release | None |
| `clang-debug` | Clang 21 | Debug | None |
| `clang-asan` | Clang 21 | Debug | AddressSanitizer |
| `clang-tsan` | Clang 21 | Debug | ThreadSanitizer |
| `coverage` | GCC 15.2 | Debug | Coverage |

### Build Outputs
```
build/<preset>/src/fqc                          # Main binary
build/<preset>/tests/<test_target>              # Test executables
build/<preset>/compile_commands.json            # For IDE/tooling
```

### Running the Tool

```bash
# Compress FASTQ
fqc compress -i reads.fastq -o reads.fqc

# Verify archive
fqc verify reads.fqc

# Decompress
fqc decompress -i reads.fqc -o restored.fastq

# Random access (extract reads 1000-2000)
fqc decompress -i reads.fqc --range 1000:2000 -o subset.fastq

# Multi-threaded compression
fqc compress -i reads.fastq -o reads.fqc -t 8 -v

# Paired-end data
fqc compress -i reads_1.fastq -2 reads_2.fastq -o paired.fqc --paired

# Archive inspection
fqc info reads.fqc
```

## Testing

### Test Commands

```bash
# Run full test suite
./scripts/test.sh gcc-release
./scripts/test.sh clang-debug

# Advanced test runner
./scripts/run_tests.sh -p gcc-release
./scripts/run_tests.sh -p gcc-release -v        # Verbose
./scripts/run_tests.sh -p gcc-release -l unit   # Unit tests only
./scripts/run_tests.sh -p gcc-release -l property  # Property tests only

# Run specific test target
./scripts/run_tests.sh -p gcc-release -f '^memory_budget_test$'
ctest --test-dir build/gcc-release --output-on-failure -R '^memory_budget_test$'

# Build single test
cmake --build --preset gcc-release --target memory_budget_test

# Run single GTest case
build/gcc-release/tests/memory_budget_test \
  --gtest_filter=MemoryBudgetTest.DefaultConstruction
```

### Test Targets
```
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

## Code Quality & Linting

```bash
# Format code
./scripts/lint.sh format

# Check formatting
./scripts/lint.sh format-check

# Run linters
./scripts/lint.sh lint clang-debug

# All checks
./scripts/lint.sh all clang-debug
```

### Pre-commit Checklist
```bash
./scripts/build.sh gcc-release
./scripts/test.sh gcc-release
./scripts/lint.sh format-check
./scripts/lint.sh lint clang-debug
```

## Development Conventions

### Code Style
- **Formatting**: Enforced by `.clang-format`
  - 4-space indentation
  - 100-column line limit
  - Attached braces
  - No tabs
  - Auto-sorted includes

### Naming Conventions
| Element | Convention | Example |
|---------|-----------|---------|
| Files/directories | `snake_case` | `block_compressor.cpp` |
| Types/classes/structs | `PascalCase` | `BlockCompressor` |
| Functions/methods | `camelCase` | `compressBlock()` |
| Local variables | `camelCase` | `inputData` |
| Private members | `camelCase_` | `bufferSize_` |
| Constants | `kCamelCase` | `kMaxBufferSize` |
| Namespaces | `lower_case` | `fqc::algo` |
| Macros | `UPPER_CASE` | `FQC_LOG_INFO` |

### Include Order
1. Project headers (`"fqc/..."`)
2. Standard library headers
3. Third-party/system headers

### API Patterns
- Use `Result<T>` and `VoidResult` (based on `std::expected`)
- Prefer `std::span`, `std::string_view`, `std::filesystem::path`
- Mark return values `[[nodiscard]]` when they shouldn't be ignored
- Use `constexpr`, `inline constexpr`, scoped enums for constants
- Prefer smart pointers/containers; avoid raw `new`/`delete`

### Error Handling
- Use `FQCException` or derived errors (not `std::runtime_error`)
- Use `ErrorCode` from `include/fqc/common/error.h`
- Prefer `Result<T>` / `VoidResult` over throwing for library APIs
- Preserve context with `ErrorContext` and `std::source_location`

### Logging
- Use Quill-based logging (`include/fqc/common/logger.h`)
- Macros: `FQC_LOG_TRACE`, `FQC_LOG_DEBUG`, `FQC_LOG_INFO`, `FQC_LOG_WARNING`, `FQC_LOG_ERROR`, `FQC_LOG_CRITICAL`
- **Do not** use `std::cout` for status/debug logging

### Testing Practices
- Unit tests use Google Test
- Property tests use RapidCheck (`RC_GTEST_PROP`)
- Add tests to `tests/CMakeLists.txt` via `fqc_add_test(...)`
- Unit tests get `unit` label; property tests get `property` label
- **Never** delete or weaken tests to make a change pass

## CI/CD Workflows

### GitHub Actions
| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `ci.yml` | PR, push to master | Build & test matrix |
| `quality.yml` | PR, push to master | Format + static analysis |
| `docs-quality.yml` | PR, push to master | Documentation checks |
| `pages.yml` | Push to master | Deploy docs to GitHub Pages |
| `release.yml` | Tag push (v*) | Build & publish releases |

### Release Process
1. Update version in `CMakeLists.txt`
2. Tag with `v<version>` (e.g., `v0.2.0`)
3. Push tag to trigger release workflow
4. Workflow validates version, builds all platforms, creates GitHub release

## Docker Usage

```bash
# Build image
docker build -f docker/Dockerfile -t fq-compressor .

# Compress data
docker run --rm -v $(pwd):/data \
  fq-compressor compress -i /data/reads.fastq -o /data/reads.fqc

# Decompress data
docker run --rm -v $(pwd):/data \
  fq-compressor decompress -i /data/reads.fqc -o /data/restored.fastq
```

## Key Files Reference

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Root build configuration |
| `CMakePresets.json` | Build preset definitions (374 lines) |
| `conanfile.py` | Dependency management |
| `AGENTS.md` | AI agent instructions & coding standards |
| `specs/` | Technical specifications (single source of truth) |
| `.clang-format` | Code formatting rules |
| `.clang-tidy` | Static analysis configuration |
| `scripts/build.sh` | Build automation |
| `scripts/test.sh` | Test automation |
| `scripts/lint.sh` | Linting & formatting |

## Common Tasks

### Adding a New CLI Command
1. Create header in `include/fqc/commands/new_command.h`
2. Create implementation in `src/commands/new_command.cpp`
3. Register in `src/main.cpp`
4. Add tests in `tests/commands/`
5. Register test in `tests/CMakeLists.txt`

### Adding a New Algorithm
1. Create header in `include/fqc/algo/new_algo.h`
2. Create implementation in `src/algo/new_algo.cpp`
3. Add tests in `tests/algo/`
4. Register test in `tests/CMakeLists.txt`

### Modifying FQC Format
1. Update format spec in `include/fqc/format/`
2. Update implementation in `src/format/`
3. **Must** update versioning
4. **Must** add/update tests
5. **Must not** break backward compatibility without version bump

## Useful Links

- **Repository**: https://github.com/LessUp/fq-compressor
- **Documentation**: https://lessup.github.io/fq-compressor/
- **Releases**: https://github.com/LessUp/fq-compressor/releases
- **Rust Implementation**: https://github.com/LessUp/fq-compressor-rust
- **Issues**: https://github.com/LessUp/fq-compressor/issues
- **Discussions**: https://github.com/LessUp/fq-compressor/discussions

## License

- **Project Code**: MIT License
- **vendor/spring-core/**: Spring's original research license (not MIT)

## Current Version

**v0.2.0** (as defined in `CMakeLists.txt`)

### Roadmap
- [x] v0.1.0 - Initial release with ABC + SCM
- [x] v0.2.0 - Paired-end optimization, long-read support
- [ ] Streaming compression mode
- [ ] Additional lossy quality modes
- [ ] AWS S3/GCS native integration
- [ ] Python bindings
