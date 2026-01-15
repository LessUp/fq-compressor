# Engineering Setup Complete

Date: 2026-01-15

## Summary

Completed the engineering standards evaluation and setup for fq-compressor project based on fastq-tools' mature practices.

## Changes

### Documentation Created
- `docs/dev/coding-standards.md` - Comprehensive coding standards document
- `docs/dev/project-setup.md` - Project engineering specifications

### Configuration Files Updated
- `.clang-format` - Updated include categories to use `fqc/` prefix
- `.clang-tidy` - Enhanced with additional checks:
  - Added `clang-analyzer-*`, `cppcoreguidelines-*`, `misc-*`, `portability-*`
  - Added naming rules for enums, macros, and template parameters
  - Added `FQC_ASSERT` to assert macros
  - Added cognitive complexity threshold

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

## Files Modified
- `.clang-format`
- `.clang-tidy`
- `docs/dev/coding-standards.md`

## Files Created
- `scripts/lint.sh`
- `scripts/build.sh`
- `scripts/test.sh`
- `scripts/install_deps.sh`
