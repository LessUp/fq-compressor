# Engineering Setup Complete

Date: 2026-01-15

## Summary

Completed the engineering standards evaluation and setup for fq-compressor project based on fastq-tools' mature practices.

## Changes

### Documentation Created
- `docs/dev/coding-standards.md` - Comprehensive coding standards document
- `docs/dev/project-setup.md` - Project engineering specifications

### Configuration Files Updated/Created
- `.editorconfig` - Enhanced with complete configuration:
  - Added global `charset = utf-8`
  - Added `max_line_length = 100` for C++ and Python
  - Added CMake, Python, Makefile, Docker specific rules
- `.clang-format` - Updated include categories to use `fqc/` prefix
- `.clang-tidy` - Significantly enhanced:
  - Added `clang-analyzer-*`, `cppcoreguidelines-*`, `misc-*`, `portability-*`
  - **Enabled** `readability-function-cognitive-complexity` (threshold: 25)
  - **Enabled** `readability-magic-numbers` and `cppcoreguidelines-avoid-magic-numbers`
    - Ignored common values: 0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024
    - Ignored powers of 2
  - Added naming rules for enums, macros, and template parameters
  - Added `FQC_ASSERT` to assert macros
- `.pre-commit-config.yaml` - **New file** with:
  - Pre-commit hooks for trailing whitespace, EOF, YAML/JSON validation
  - clang-format integration
  - cmake-format integration
  - shellcheck for shell scripts
  - black for Python files
  - commitizen for commit message validation
  - clang-tidy (manual stage)

### Scripts Created
- `scripts/lint.sh` - Code quality checking (format, format-check, lint)
- `scripts/build.sh` - Unified build script with preset support
- `scripts/test.sh` - Test runner with filter support
- `scripts/install_deps.sh` - Conan dependency installation

## Key Decisions

1. **Naming Convention**: Keep fastq-tools' mixed style (PascalCase classes, camelCase functions/variables, snake_case namespaces) - internally consistent and not a major issue for open-source acceptance

2. **Return Type Style**: Use regular return types by default; trailing return types only for complex/template returns

3. **Include Order**: Follow Google Style (corresponding header first, then C system, C++ stdlib, third-party, project headers)

4. **Git Commits**: Conventional Commits format in English

5. **Stricter Checks**: Enabled cognitive complexity and magic numbers checks with reasonable thresholds

## Files Modified
- `.clang-format`
- `.clang-tidy`
- `.editorconfig`
- `docs/dev/coding-standards.md`

## Files Created
- `.pre-commit-config.yaml`
- `scripts/lint.sh`
- `scripts/build.sh`
- `scripts/test.sh`
- `scripts/install_deps.sh`
