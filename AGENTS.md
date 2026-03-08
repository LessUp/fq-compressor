# AGENTS.md — AI Agent Guidelines for fq-compressor

## Identity

This is **fq-compressor**, a high-performance FASTQ compressor in C++23. It uses Assembly-based Compression (ABC) for short reads and Zstd for medium/long reads, with Intel oneTBB parallel pipelines.

## Build & Verify

```bash
# Always run before committing
./scripts/build.sh gcc-release          # must compile cleanly
./scripts/test.sh gcc-release           # all tests pass
./scripts/lint.sh format-check          # clang-format check
./scripts/lint.sh lint clang-debug      # clang-tidy static analysis
```

## Project Structure

```
include/fqc/
├── algo/
│   ├── block_compressor.h    # ABC (consensus + delta) / Zstd codec
│   ├── global_analyzer.h     # Minimizer-based global read reordering
│   ├── id_compressor.h       # ID stream: delta + tokenization
│   ├── pe_optimizer.h        # Paired-end complementarity optimization
│   └── quality_compressor.h  # SCM order-1/2 arithmetic coding
├── commands/
│   ├── compress_command.h    # Compression command
│   ├── decompress_command.h  # Decompression command
│   ├── info_command.h        # Archive info display
│   └── verify_command.h      # Integrity verification
├── common/
│   ├── error.h               # ErrorCode (0-5), FQCException, Result<T>
│   ├── logger.h              # Quill async logger
│   ├── memory_budget.h       # System memory detection, chunking
│   └── types.h               # ReadRecord, QualityMode, IdMode, ReadLengthClass
├── format/
│   ├── fqc_format.h          # Magic bytes, GlobalHeader, BlockHeader, Footer
│   ├── fqc_reader.h          # Block-indexed archive reader
│   ├── fqc_writer.h          # Archive writer with finalize
│   └── reorder_map.h         # ZigZag delta + varint bidirectional map
├── io/
│   ├── async_io.h            # AsyncReader/AsyncWriter/BufferPool
│   ├── compressed_stream.h   # Transparent gz/bz2/xz/zst decompression
│   ├── fastq_parser.h        # FASTQ parser with validation, stats, PE
│   └── paired_end_parser.h   # Interleaved PE parser
└── pipeline/
    ├── pipeline.h            # TBB 8-node parallel pipeline
    └── pipeline_node.h       # Pipeline node base class

src/                          # 28 .cpp files (1:1 with headers + main.cpp)
├── algo/                     # 5 algorithm implementations
├── commands/                 # 4 CLI commands
├── common/                   # 3 infrastructure modules
├── format/                   # 3 format modules
├── io/                       # 4 I/O modules
├── pipeline/                 # 8 pipeline nodes
└── main.cpp                  # CLI entry point (CLI11)

tests/                        # 11 test files
├── algo/                     # 5 property tests (ID, quality, PE, long-read, two-phase)
├── common/                   # 1 unit test (memory budget)
├── format/                   # 2 tests (format property, writer)
├── io/                       # 1 property test (FASTQ parser)
├── pipeline/                 # 1 property test (pipeline)
└── e2e/                      # End-to-end tests
```

## Coding Rules

### Must Follow
- **C++23** — use `std::expected`, Concepts, Ranges; do not use deprecated features
- **GCC 15+ / Clang 21+** — minimum compiler requirement
- **clang-format clean** — run `./scripts/lint.sh format-check` before commit
- **clang-tidy clean** — run `./scripts/lint.sh lint clang-debug`
- **All tests pass** — run `./scripts/test.sh gcc-release`
- **Use FQCException** — never throw `std::runtime_error`; use `FQCException(ErrorCode::kXxx, msg)`
- **Use Quill logger** — `LOG_INFO`, `LOG_WARNING`, `LOG_DEBUG`; never `std::cout` for status
- **Conan for all deps** — never `apt-get install` library `-dev` packages in Dockerfiles

### Style
- 4-space indentation, max line width 100 (`.clang-format`)
- Naming: PascalCase (types), camelCase (functions/variables), kConstant (enum values)
- Member variables: `camelCase_` (trailing underscore)
- Include order: project headers → standard library → system headers
- Section separators: `// ====...====` block comments for major sections
- Commit messages: Conventional Commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`)

### Avoid
- Changing the FQC binary format without bumping version
- Deleting or weakening tests
- Raw pointers for ownership — use `std::unique_ptr` / `std::shared_ptr`
- `new` / `delete` — use smart pointers or containers
- Adding Conan dependencies without justification

## Key Types

| Type | Location | Purpose |
|------|----------|---------|
| `ReadRecord` | `common/types.h` | Single FASTQ record (id, sequence, quality) |
| `ErrorCode` | `common/error.h` | Exit codes 0-5 (kSuccess..kUnsupportedCodec) |
| `FQCException` | `common/error.h` | Structured exception with ErrorCode + message |
| `Result<T>` | `common/error.h` | `std::expected<T, FQCException>` alias |
| `GlobalHeader` | `format/fqc_format.h` | Archive header (flags, read count, filename) |
| `BlockHeader` | `format/fqc_format.h` | Per-block header (codec, counts, sizes) |
| `ReadLengthClass` | `common/types.h` | Short / Medium / Long classification |

## Common Tasks

### Add a CLI command
1. Create `include/fqc/commands/<cmd>_command.h` + `src/commands/<cmd>_command.cpp`
2. Register in `main.cpp` CLI11 subcommand setup
3. Add test in `tests/`

### Add an error code
1. Add variant to `ErrorCode` in `include/fqc/common/error.h`
2. Update `errorCodeToString()` mapping
3. Use via `throw FQCException(ErrorCode::kNewCode, "message")`

### Add a test
1. Create test file in appropriate `tests/` subdirectory
2. Use Google Test (`TEST`, `TEST_F`) or RapidCheck (`rc::check`)
3. Register in `tests/CMakeLists.txt`

## Dependencies (Conan)

| Library | Version | Purpose |
|---------|---------|---------|
| Intel oneTBB | 2022.3.0 | 8-node parallel pipeline |
| CLI11 | 2.4.2 | CLI argument parsing |
| Quill | 11.0.2 | Async logging |
| fmt | 12.1.0 | String formatting |
| zstd | 1.5.7 | Zstd compression |
| zlib-ng | 2.3.2 | Gzip compression |
| libdeflate | 1.25 | Fast gzip compression |
| bzip2 | 1.0.8 | Bzip2 compression |
| xz_utils | 5.4.5 | XZ/LZMA compression |
| xxHash | 0.8.3 | Non-cryptographic checksums |
| Google Test | 1.15.0 | Unit testing |

## 工具链版本规范

### 编译器版本

| 组件 | 统一版本 | 最低兼容 | CI 兼容性检查 |
|------|---------|---------|----------|
| **GCC** | 15.2 | 11.0 | 14.x（allow-failure） |
| **Clang/LLVM** | 21 | 12.0 | 19（allow-failure） |
| **C++ 标准** | C++23 | C++23 | — |

### 构建工具版本

| 工具 | 锁定版本 | 最低要求 |
|------|---------|----------|
| **CMake** | 4.0.2（Docker 内） | 3.28 |
| **Conan** | 2.24.0 | 2.0 |
| **Ninja** | 系统最新 | 1.10 |

### Docker 工具链选型

| 组件 | 选型 | 理由 |
|------|------|------|
| **基础镜像** | `gcc:15.2-bookworm` (Debian 12) | Docker 官方 GCC 镜像，自带 GCC 15.2，无需额外编译 |
| **Clang** | Clang 21 (via `apt.llvm.org`) | LLVM 21 qualification branch，C++23 支持完善 |
| **运行时镜像** | `debian:bookworm-slim` | 与构建镜像同系，共享基础层，体积最小 |
| **不选 Ubuntu 24.04** | — | 无官方 `gcc:` 镜像，需自行编译 GCC 15；glibc 2.39 二进制兼容性较窄 |

## CI/CD

- **ci.yml** — push/PR: Docker build toolchain, multi-preset build & test (gcc-release, clang-debug, clang-release)
- **quality.yml** — format check (clang-format) + static analysis (clang-tidy)
- **release.yml** — `v*` tag: validate version, build 6 targets (glibc/musl/clang × x86_64/aarch64), macOS, GitHub Release with checksums

## Do NOT

- Throw `std::runtime_error` — use `FQCException` with proper `ErrorCode`
- Use `std::cout` for status output — use Quill logger
- Install system `-dev` packages in Docker — use Conan
- Add `unsafe` raw memory operations without justification
- Delete or weaken existing tests
- Change the FQC binary format without updating version numbers
