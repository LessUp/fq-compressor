---
title: Contributing
description: How to contribute to fq-compressor
version: 0.1.0
last_updated: 2026-04-16
language: en
---

# Contributing to fq-compressor

Thank you for your interest in contributing to fq-compressor! This document provides guidelines for contributing to the project.

## Code of Conduct

- Be respectful and constructive in all interactions
- Focus on technical merit and project improvement
- Help create a welcoming environment for newcomers

## Getting Started

### Development Environment Setup

1. **Fork and clone** the repository
2. **Set up build environment**:
   ```bash
   conan install . --build=missing -of=build/clang-debug
   cmake --preset clang-debug
   ```
3. **Build and test**:
   ```bash
   cmake --build --preset clang-debug -j$(nproc)
   ./scripts/test.sh clang-debug
   ```

### DevContainer (Recommended)

For a consistent development environment:

1. Open project in VS Code
2. Run `Dev Containers: Reopen in Container`
3. All tools are pre-installed

## Development Workflow

### Branch Naming

- `feature/description` - New features
- `fix/description` - Bug fixes
- `docs/description` - Documentation updates
- `refactor/description` - Code refactoring

### Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

Types:
- `feat` - New feature
- `fix` - Bug fix
- `docs` - Documentation only
- `style` - Formatting changes
- `refactor` - Code restructuring
- `perf` - Performance improvements
- `test` - Test updates
- `chore` - Build/tooling changes

Examples:
```
feat(compressor): add quality score binning option
fix(parser): handle empty FASTQ files
docs(readme): update installation instructions
```

### Code Style

The project uses automated formatting and linting:

```bash
# Format code
./scripts/lint.sh format

# Run linters
./scripts/lint.sh lint clang-debug

# Check everything
./scripts/lint.sh all clang-debug
```

Style guidelines:
- 4-space indentation
- 100-column line limit
- Snake_case files, PascalCase types, camelCase functions
- `[[nodiscard]]` for important return values
- No raw owning pointers

## Submitting Changes

### Pull Request Process

1. **Create a branch** from `main`
2. **Make your changes** with clear commits
3. **Ensure tests pass**:
   ```bash
   ./scripts/test.sh clang-release
   ```
4. **Update documentation** if needed
5. **Submit PR** with clear description

### PR Checklist

- [ ] Code compiles without warnings
- [ ] All tests pass
- [ ] New tests added for new functionality
- [ ] Documentation updated
- [ ] Changelog updated (if applicable)
- [ ] Code formatted with clang-format
- [ ] No clang-tidy warnings

### Review Process

1. Maintainers will review within 48 hours
2. Address review feedback promptly
3. Squash commits if requested
4. Once approved, maintainers will merge

## Testing Guidelines

### Unit Tests

Use Google Test:

```cpp
#include <gtest/gtest.h>

TEST(CompressionTest, BasicCompression) {
    // Arrange
    Compressor comp;
    
    // Act
    auto result = comp.compress(data);
    
    // Assert
    EXPECT_TRUE(result.has_value());
    EXPECT_LT(result->size(), data.size());
}
```

### Property Tests

Use RapidCheck when available:

```cpp
RC_GTEST_PROP(CompressorTest, RoundTrip, ()) {
    auto data = *rc::gen::container<std::vector<uint8_t>>(
        rc::gen::inRange(0, 255)
    );
    
    auto compressed = compress(data);
    auto restored = decompress(compressed);
    
    RC_ASSERT(restored == data);
}
```

### Adding Tests

Add tests to appropriate file in `tests/`:

```cmake
# In tests/CMakeLists.txt
fqc_add_test(my_feature_test src/my_feature_test.cpp)
```

## Performance Considerations

When modifying compression code:

1. **Benchmark before and after**:
   ```bash
   python3 benchmark/compiler_benchmark.py \
       --input test_data.fq \
       --gcc-binary build/gcc-release/src/fqc \
       --visualize
   ```

2. **Memory profiling**:
   ```bash
   valgrind --tool=massif ./fqc compress -i large.fastq -o out.fqc
   ```

3. **Target metrics**:
   - Compression ratio: maintain or improve
   - Speed: minimal regression acceptable
   - Memory: stay within budget system limits

## Documentation

### Code Documentation

Use Doxygen-style comments for public APIs:

```cpp
/**
 * Compress a FASTQ file to FQC format.
 * 
 * @param input Path to input FASTQ file
 * @param output Path to output FQC file
 * @param options Compression configuration
 * @return Result indicating success or error details
 * 
 * @throws FQCException on unrecoverable errors
 */
Result<void> compressFile(const std::filesystem::path& input,
                          const std::filesystem::path& output,
                          const CompressOptions& options);
```

### User Documentation

Update relevant documentation:
- CLI changes → `docs/en/getting-started/cli-usage.md`
- Algorithm changes → `docs/en/core-concepts/algorithms.md`
- Format changes → `docs/en/core-concepts/fqc-format.md`
- Breaking changes → `CHANGELOG.md`

## Areas for Contribution

### Good First Issues

- Documentation improvements
- Additional test coverage
- Error message improvements
- Log message clarifications

### Advanced Contributions

- Algorithm optimizations
- New compression modes
- Platform support expansion
- Tooling improvements

## Questions?

- Open a [discussion](https://github.com/LessUp/fq-compressor/discussions) for questions
- Join our community chat (coming soon)

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

Thank you for contributing to fq-compressor!
