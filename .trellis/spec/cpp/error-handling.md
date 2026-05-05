# Error Handling

> Error handling patterns: exceptions, Result types, and error codes.
>
> **Reference**: See `include/fqc/common/error.h` for full API definitions.

---

## Error Handling Strategy

The codebase uses a **layered approach**:

| Layer | Strategy | Rationale |
|-------|----------|-----------|
| Library code (algo, format, io, pipeline) | `Result<T>` / `VoidResult` | Testable, no exception overhead |
| CLI boundary (commands, main) | Convert to exit codes | User-facing error reporting |

**When to use each:**
- Use `Result<T>` for recoverable errors in library code
- Use `VoidResult` for operations that return nothing on success
- Throw `FQCException` (or derived) only at CLI boundary for fatal errors
- Never throw from library code - return `Result` instead

---

## Result Types

### VoidResult

For operations that can fail but return no value:

```cpp
// include/fqc/common/error.h
VoidResult open(const std::filesystem::path& path);

// Usage
auto result = writer.open(path);
if (!result) {
    return result.error();  // Propagate error
}
```

### Result<T>

For operations that return a value or fail:

```cpp
// include/fqc/common/error.h
Result<CompressedBlockData> compress(std::span<const ReadRecord> reads);

// Usage
auto result = compressor.compress(reads);
if (!result) {
    return result.error();
}
auto data = std::move(*result);
```

---

## Exception Types

**DO NOT throw `std::runtime_error`.** Use the project's exception hierarchy:

| Exception | Use Case |
|-----------|----------|
| `FQCException` | Base class for all project exceptions |
| `ArgumentError` | Invalid command-line arguments |
| `IOError` | File I/O failures |
| `FormatError` | Invalid file format |
| `ChecksumError` | Checksum verification failures |

```cpp
// include/fqc/common/error.h
class FQCException : public std::exception {
public:
    explicit FQCException(ErrorCode code, std::string message,
                          std::source_location loc = std::source_location::current());
    ErrorCode code() const noexcept;
    const char* what() const noexcept override;
};
```

---

## Error Codes

Exit codes for CLI commands:

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Usage/argument error |
| 2 | I/O error |
| 3 | Format/version error |
| 4 | Checksum verification failed |
| 5 | Unsupported algorithm/codec |

```cpp
// src/commands/compress_command.cpp
int toExitCode(ErrorCode code) {
    return static_cast<int>(code);
}
```

---

## Error Context

The `ErrorContext` struct provides additional location information for debugging:

```cpp
// See include/fqc/common/error.h for full definition
ErrorContext ctx;
ctx.withFile("data.fqc").withBlock(42).withRead(1001);
```

The `Error` class wraps an `ErrorCode` and message for use with `Result<T>`:

```cpp
// Create error results
auto err = makeError<T>(ErrorCode::kIOError, "Failed to open file");
```

---

## Examples from Codebase

### Library Code Pattern

```cpp
// src/format/fqc_reader.cpp
VoidResult FQCReader::open(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return makeVoidError(ErrorCode::kIOError,
            fmt::format("Failed to open file: {}", path.string()));
    }
    // ... continue processing
    return makeVoidSuccess();
}
```

### Command Pattern

```cpp
// src/commands/compress_command.cpp
int CompressCommand::execute() {
    auto result = pipeline_.run(inputPath_, outputPath_);
    if (!result) {
        try {
            throw FQCException(result.error().code, result.error().message);
        } catch (const FQCException& e) {
            FQC_LOG_ERROR("Compression failed: {}", e.what());
            return toExitCode(e.code());
        }
    }
    return 0;
}
```

### Testing Errors

```cpp
// tests/format/fqc_writer_test.cpp
TEST(FQCWriterTest, OpenInvalidPath) {
    FQCWriter writer;
    auto result = writer.open("/nonexistent/path/file.fqc", {});
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::kIOError);
}
```

---

## Common Mistakes

### Wrong: Using std::runtime_error

```cpp
// DON'T DO THIS
throw std::runtime_error("File not found");
```

### Right: Use project exception types

```cpp
// DO THIS INSTEAD
throw IOError(fmt::format("File not found: {}", path.string()));
```

### Wrong: Ignoring Result

```cpp
// DON'T DO THIS
writer.open(path);  // Return value ignored
```

### Right: Check and propagate

```cpp
// DO THIS INSTEAD
auto result = writer.open(path);
if (!result) {
    return result.error();
}
```
