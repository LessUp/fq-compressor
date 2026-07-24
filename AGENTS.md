# AGENTS.md - fq-compressor

Personal practice project: high-performance C++23 concurrent pipeline for FASTQ compression.

## Build & Test

```bash
./scripts/build.sh clang-debug      # build
./scripts/test.sh clang-debug       # run all tests
./scripts/test.sh clang-asan        # sanitizer build + test
./scripts/lint.sh format-check      # check formatting
./scripts/lint.sh format            # auto-format
```

## Code Style

- C++23, clang + libc++, 4-space indent, 100 col limit
- Naming: PascalCase types, camelCase functions/variables, `trailing_` members, `kPascal` constants
- Error handling: `Result<T>` (alias for `std::expected<T, Error>`). No exceptions in library code.
- Logging: `FQC_LOG_INFO(...)` etc. from `fqc/log.h` (fmt-based, stderr)
- Includes: project headers first, then stdlib, then third-party

## Architecture

```
FASTQ input → FastqParser → [SPSC queue] → ArchiveWriter (encode + zstd + xxh64) → output
```

Key modules:
- `include/fqc/pipeline/` — SPSC queue + pipeline orchestration (the learning core)
- `src/format/archive.cpp` — binary format: varint, 2-bit DNA packing, zstd frames, XXH64
- `src/io/` — FASTQ parsing, gzip transparent input
- `src/commands/` — CLI orchestration (compress/decompress/verify)

## Dependencies (Conan)

cli11, fmt, zlib-ng, zstd, xxhash, gtest (test only)

## Git

- 提交信息使用中文
- 格式：`<类型>: <简述>`，如 `refactor: 移除 Quill 依赖`、`feat: 添加 SPSC 并发流水线`
