# Naming Conventions

> Naming rules for files, types, functions, and variables.

---

## File Names

| Entity | Convention | Example |
|--------|------------|---------|
| Files/Directories | snake_case | `block_compressor.cpp`, `fastq_parser.h` |

## Type Names

| Entity | Convention | Example |
|--------|------------|---------|
| Classes/Structs | PascalCase | `BlockCompressor`, `ReadRecord` |
| Enums | PascalCase values | `ErrorCode::kSuccess` |
| Type aliases | PascalCase | `Result<T>`, `VoidResult` |

## Function/Method Names

| Entity | Convention | Example |
|--------|------------|---------|
| Functions/Methods | camelCase | `compressBlock()`, `readChunk()` |
| Getters | camelCase | `size()`, `empty()` |
| Setters | set + PascalCase | `setBlockSize()` |

## Variable Names

| Entity | Convention | Example |
|--------|------------|---------|
| Local variables | camelCase | `inputPath`, `blockSize` |
| Parameters | camelCase | `config`, `numThreads` |
| Private/Protected members | camelCase + underscore suffix | `buffer_`, `impl_` |
| Constants | k + PascalCase | `kMaxBlockSize`, `kDefaultThreads` |

## Namespace Names

| Entity | Convention | Example |
|--------|------------|---------|
| Namespaces | lower_case | `fqc::algo`, `fqc::io`, `fqc::pipeline` |

## Preprocessor

| Entity | Convention | Example |
|--------|------------|---------|
| Macros | UPPER_CASE | `FQC_ASSERT`, `FQC_LOG_DEBUG` |

---

## Examples from Codebase

### Class Declaration

```cpp
// include/fqc/algo/block_compressor.h
class BlockCompressor {
public:
    explicit BlockCompressor(BlockCompressorConfig config);
    ~BlockCompressor();

    Result<CompressedBlockData> compress(std::span<const ReadRecord> reads);
    Result<DecompressedBlockData> decompress(std::span<const std::byte> data);

private:
    std::unique_ptr<BlockCompressorImpl> impl_;
};
```

### Enum Definition

```cpp
// include/fqc/common/error.h
enum class ErrorCode : int {
    kSuccess = 0,
    kInvalidArgument = 1,
    kIOError = 2,
    kFormatError = 3,
    kChecksumError = 4,
    kUnsupportedCodec = 5
};
```

### Constant Definition

```cpp
// include/fqc/common/types.h
inline constexpr size_t kDefaultBlockSize = 100'000;
inline constexpr size_t kMaxBlockSize = 1'000'000;
```
