# Code Style

> Formatting rules and code organization patterns.

---

## Formatting Rules

| Rule | Value |
|------|-------|
| Indentation | 4 spaces (no tabs) |
| Line limit | 100 columns |
| Braces | Attached (K&R style) |
| Max empty lines | 1 |

---

## Include Order

Group includes in this order, separated by blank lines:

1. Project headers (`"fqc/..."`)
2. Standard library headers
3. Third-party/system headers

Includes are automatically sorted/regrouped by clang-format.

```cpp
#include "fqc/algo/block_compressor.h"
#include "fqc/common/error.h"
#include "fqc/common/types.h"

#include <algorithm>
#include <memory>
#include <span>
#include <vector>

#include <gtest/gtest.h>
```

---

## Section Separators

Use this style for major section breaks within files:

```cpp
// ============================================================================
// Section Name
// ============================================================================

// --- Subsection Name --- //

// Single-line comment for minor breaks
```

---

## API Usage Patterns

- Mark non-trivial return values `[[nodiscard]]`
- Prefer `std::span`, `std::string_view`, `std::filesystem::path` for parameters
- Use `constexpr`, `inline constexpr`, scoped enums for constants
- Prefer smart pointers/containers over raw owning pointers
- Avoid `new`/`delete` in normal code

```cpp
[[nodiscard]] Result<CompressedBlockData> compress(std::span<const ReadRecord> reads);

void processFile(const std::filesystem::path& inputPath);

inline constexpr size_t kDefaultThreads = 4;
```

---

## Formatting Tool

- **Tool**: clang-format-21
- **Config**: `.clang-format` in repository root
- **Check**: `./scripts/lint.sh format-check`
- **Apply**: `./scripts/lint.sh format`

---

## Examples from Codebase

### Function Declaration

```cpp
// include/fqc/format/fqc_writer.h
class FQCWriter {
public:
    FQCWriter() = default;
    ~FQCWriter();

    FQCWriter(const FQCWriter&) = delete;
    FQCWriter& operator=(const FQCWriter&) = delete;
    FQCWriter(FQCWriter&&) noexcept;
    FQCWriter& operator=(FQCWriter&&) noexcept;

    [[nodiscard]] VoidResult open(const std::filesystem::path& path,
                                   const FQCHeader& header);
    [[nodiscard]] VoidResult writeBlock(const CompressedBlock& block);
    [[nodiscard]] VoidResult close();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
```

### Implementation File Structure

```cpp
// src/format/fqc_writer.cpp

#include "fqc/format/fqc_writer.h"

#include "fqc/common/error.h"
#include "fqc/common/logger.h"

#include <fstream>

#include <zstd.h>

namespace fqc::format {

// ============================================================================
// FQCWriter::Impl
// ============================================================================

class FQCWriter::Impl {
public:
    Impl() = default;

    VoidResult open(const std::filesystem::path& path, const FQCHeader& header) {
        // Implementation...
    }

    // ...

private:
    std::ofstream file_;
    // ...
};

// ============================================================================
// FQCWriter Public Interface
// ============================================================================

FQCWriter::FQCWriter() : impl_(std::make_unique<Impl>()) {}

FQCWriter::~FQCWriter() = default;

FQCWriter::FQCWriter(FQCWriter&&) noexcept = default;
FQCWriter& FQCWriter::operator=(FQCWriter&&) noexcept = default;

VoidResult FQCWriter::open(const std::filesystem::path& path, const FQCHeader& header) {
    return impl_->open(path, header);
}

} // namespace fqc::format
```
