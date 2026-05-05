# Testing Guidelines

> Testing patterns with GTest and RapidCheck property-based testing.

---

## Test Structure

Tests are organized in `tests/` mirroring the source structure:

```
tests/
├── algo/               Algorithm tests
├── commands/           Command tests
├── common/             Common utility tests
├── format/             Format tests
├── io/                 I/O tests
├── pipeline/           Pipeline tests
└── e2e/                End-to-end tests
```

---

## Test Types

### Unit Tests (GTest)

Standard unit tests for specific functionality:

```cpp
// tests/common/error_test.cpp
#include <gtest/gtest.h>
#include "fqc/common/error.h"

TEST(ErrorTest, ErrorCodeValues) {
    EXPECT_EQ(static_cast<int>(ErrorCode::kSuccess), 0);
    EXPECT_EQ(static_cast<int>(ErrorCode::kIOError), 2);
}

TEST(ErrorTest, VoidResultSuccess) {
    VoidResult result;
    EXPECT_TRUE(result);
}

TEST(ErrorTest, VoidResultError) {
    VoidResult result = ErrorContext{ErrorCode::kIOError, "test error"};
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::kIOError);
}
```

### Property-Based Tests (RapidCheck)

For fuzzing and invariants:

```cpp
// tests/algo/quality_compressor_property_test.cpp
#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>
#include "fqc/algo/quality_compressor.h"

RC_GTEST_PROP(QualityCompressorPropertyTest, RoundTripPreservesData,
              (const std::vector<std::string>& qualities)) {
    QualityCompressor compressor;
    
    // Compress
    auto compressed = compressor.compress(qualities);
    RC_ASSERT(compressed);
    
    // Decompress
    auto decompressed = compressor.decompress(*compressed);
    RC_ASSERT(decompressed);
    
    // Verify round-trip
    RC_ASSERT(*decompressed == qualities);
}
```

---

## Running Tests

### Simple Test Runner

```bash
./scripts/test.sh [preset] [filter]
```

Use this for quick iteration during development.

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

Use this when you need fine-grained control over test selection.

> See [Error Handling](./error-handling.md) for testing error conditions.

### Running Specific Tests

```bash
# Run all tests with clang-debug
./scripts/test.sh clang-debug

# Run only unit tests
./scripts/run_tests.sh -p clang-debug -l unit

# Run specific test by name
./scripts/run_tests.sh -p clang-debug -f '^memory_budget_test$'

# Run a specific GTest case
build/clang-debug/tests/memory_budget_test \
  --gtest_filter=MemoryBudgetTest.DefaultConstruction
```

---

## Adding New Tests

1. Add test file in appropriate `tests/` subdirectory
2. Register in `tests/CMakeLists.txt`:

```cmake
fqc_add_test(my_test
    SOURCES my/my_test.cpp
)

# For property-based tests
fqc_add_test(my_property_test
    SOURCES my/my_property_test.cpp
    PROPERTY_BASED
)
```

3. Run: `./scripts/run_tests.sh -f '^my_test$'`

---

## Test Coverage

### Coverage Build

```bash
./scripts/build.sh coverage
./scripts/test.sh coverage
```

Coverage reports are generated in `build/coverage/`.

---

## Examples from Codebase

### Unit Test Pattern

```cpp
// tests/common/memory_budget_test.cpp
class MemoryBudgetTest : public ::testing::Test {
protected:
    void SetUp() override {
        budget_ = std::make_unique<MemoryBudget>(1024 * 1024);
    }
    std::unique_ptr<MemoryBudget> budget_;
};

TEST_F(MemoryBudgetTest, AllocateAndFree) {
    auto* ptr = budget_->allocate(1024);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(budget_->used(), 1024);
    budget_->free(ptr);
    EXPECT_EQ(budget_->used(), 0);
}
```

### Property Test Pattern

```cpp
// tests/algo/quality_compressor_property_test.cpp
RC_GTEST_PROP(QualityCompressorTest, RoundTrip, (const std::vector<std::string>& quals)) {
    QualityCompressor compressor;
    auto compressed = compressor.compress(quals);
    RC_ASSERT(compressed);
    auto decompressed = compressor.decompress(*compressed);
    RC_ASSERT(*decompressed == quals);
}
```

---

## Test Naming Conventions

| Pattern | Example |
|---------|---------|
| Test fixture class | `MemoryBudgetTest` |
| Simple test | `TEST(MemoryBudgetTest, DefaultConstruction)` |
| Fixture test | `TEST_F(MemoryBudgetTest, AllocateAndFree)` |
| Property test | `RC_GTEST_PROP(QualityCompressorPropertyTest, RoundTripPreservesData, (...))` |
