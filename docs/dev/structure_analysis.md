# Project Structure Analysis & Recommendations

**Date**: 2026-04-17  
**Status**: Review & Recommendations

---

## Executive Summary

The current project structure is **well-organized** and follows modern C++ best practices. The directory layout is clean, consistent, and aligns with the AGENTS.md specification. However, there are **several optimization opportunities** that can improve maintainability, clarity, and developer experience.

---

## Current Structure Strengths ✅

### 1. Clean Architecture Separation
- **Public API** (`include/fqc/`) - Well-organized headers
- **Implementation** (`src/`) - Mirrors include structure perfectly
- **Tests** (`tests/`) - Follows same pattern, easy to locate

### 2. Consistent Directory Naming
```
include/fqc/{algo,commands,common,format,io,pipeline}
src/{algo,commands,common,format,io,pipeline}
tests/{algo,commands,common,format,io,pipeline,e2e}
```
This mirroring makes it trivial to find related files.

### 3. Proper Spec-Driven Development
- `/specs/product/` - Product requirements
- `/specs/rfc/` - Technical design documents
- `/specs/testing/` - Test specifications
- Clear workflow in AGENTS.md

### 4. Build System Organization
- Top-level `CMakeLists.txt` for project configuration
- Per-directory `CMakeLists.txt` files (`src/`, `tests/`)
- CMake presets in `CMakePresets.json`
- Conan for dependency management

### 5. CI/CD & Automation
- GitHub workflows in `.github/workflows/`
- Helper scripts in `scripts/`
- Docker configurations in `docker/`

---

## Issues Found & Recommendations 🔍

### 🔴 **HIGH PRIORITY**

#### 1. **Missing `tests/algo/` Unit Tests Directory Structure**
**Issue**: `tests/algo/` only contains property-based tests, but no regular unit tests.

**Current State**:
```
tests/algo/
├── id_compressor_property_test.cpp
├── long_read_property_test.cpp
├── pe_property_test.cpp
├── quality_compressor_property_test.cpp
└── two_phase_compression_property_test.cpp
```

**Recommendation**: 
- ✅ **KEEP AS-IS** - This is actually correct since all algorithm tests are property-based
- No action needed

**Verdict**: ✅ **NO CHANGE REQUIRED**

---

#### 2. **E2E Tests Are Shell Scripts, Not Integrated with CTest**
**Issue**: `tests/e2e/` contains shell scripts (`test_cli.sh`, `test_performance.sh`) that are not registered as CTest targets.

**Current State**:
```
tests/e2e/
├── test_cli.sh
└── test_performance.sh
```

**Problems**:
- Not executed by `ctest` or `./scripts/test.sh`
- May become outdated without CI feedback
- No pass/fail tracking in CI

**Recommendation**: 
Create wrapper test targets in `tests/CMakeLists.txt`:

```cmake
# E2E Tests
add_test(NAME e2e_cli_test
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/test_cli.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/e2e
)
set_tests_properties(e2e_cli_test PROPERTIES
    LABELS "e2e"
    TIMEOUT 600
)

add_test(NAME e2e_performance_test
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/test_performance.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/e2e
)
set_tests_properties(e2e_performance_test PROPERTIES
    LABELS "e2e"
    TIMEOUT 900
)
```

**Verdict**: 🔧 **RECOMMENDED FIX**

---

#### 3. **Benchmark Directory Lacks CMake Integration**
**Issue**: `benchmark/` directory contains Python scripts and shell scripts but no CMakeLists.txt or build integration.

**Current State**:
```
benchmark/
├── benchmark.py
├── benchmark_parallel.sh
├── compiler_benchmark.py
├── run_benchmark.sh
├── tools.yaml
└── visualize_benchmark.py
```

**Problems**:
- Not built or validated by CI
- Dependencies not tracked
- No automated execution

**Recommendation**: 

**Option A: Keep as standalone scripts (current approach)**
- Add `benchmark/README.md` with clear usage instructions
- Add to CI as optional workflow (manual trigger only)

**Option B: Integrate with CMake (recommended for formal benchmarking)**
- Create `benchmark/CMakeLists.txt`
- Build benchmark executable linking against `fqc_core`
- Add CI workflow for periodic benchmarks

**Verdict**: 📋 **DECISION NEEDED** - Depends on whether benchmarks should be automated in CI

---

### 🟡 **MEDIUM PRIORITY**

#### 4. **Documentation Directory Structure is Overly Complex**
**Issue**: `docs/` has too many overlapping subdirectories with unclear purposes.

**Current State**:
```
docs/
├── archive/           # Historical docs?
├── benchmark/         # Benchmark docs
├── dev/               # Development docs
├── en/                # English user docs
├── research/          # Research papers?
├── specs/             # Technical specs (duplicates /specs?)
├── website/           # Nextra website
├── zh/                # Chinese user docs
├── README.md
└── WEBSITE_MIGRATION.md
```

**Problems**:
- `/docs/specs/` vs `/specs/` - **Confusing duplication**
- `/docs/archive/` - Should this be in git history instead?
- `/docs/research/` - Unclear purpose vs `/specs/rfc/`
- Too many language/audience splits

**Recommendation**:

**Proposed Structure**:
```
docs/
├── website/           # Documentation website (Nextra)
│   ├── pages/
│   ├── public/
│   └── ...
├── en/                # English documentation
│   └── ...
├── zh/                # Chinese documentation  
│   └── ...
├── dev/               # Developer documentation
│   └── contributing.md
└── README.md          # Documentation hub

specs/                 # Technical specifications (KEEP AT ROOT)
├── product/           # Product requirements
├── rfc/               # Technical design documents
└── testing/           # Test specifications
```

**Actions**:
1. **Move** `/docs/specs/` content to `/specs/` (merge or verify no duplication)
2. **Archive** `/docs/archive/` - Move to a `docs/legacy/` or remove entirely
3. **Clarify** `/docs/research/` - Move to `/specs/rfc/` if technical, or remove
4. **Document** the structure in `docs/README.md`

**Verdict**: 🔧 **RECOMMENDED REORGANIZATION**

---

#### 5. **`ref-projects/` Directory is Empty (Git-Ignored)**
**Issue**: `ref-projects/` exists but all contents are gitignored.

**Current State**:
```
ref-projects/
└── (8 git-ignored files)
```

**Problems**:
- Unclear purpose
- Clutters root directory
- If reference projects are large, they should be in a separate repo

**Recommendation**:

**Option A**: If these are test datasets:
- Move to `testdata/` or `datasets/` directory
- Use Git LFS if files are large
- Document purpose

**Option B**: If these are external reference implementations:
- Move to separate repository
- Add as git submodule if needed
- Or document why they're here

**Option C**: If not actively used:
- Remove from repository
- Document where to find them if needed

**Verdict**: ❓ **NEEDS CLARIFICATION**

---

#### 6. **`.arts/` Directory Purpose Unclear**
**Issue**: `.arts/` directory contains VS Code-like configuration files.

**Current State**:
```
.arts/
├── extensions.json
├── launch.json
├── settings.json
└── tasks.json
```

**Problems**:
- Non-standard directory name (typo? should be `.vscode/`?)
- Duplicates some `.vscode/` functionality
- Not documented

**Recommendation**:

**If this is for a specific IDE/tool**:
- Rename to standard name (e.g., `.vscode/`, `.idea/`)
- Add to `.gitignore` if user-specific
- Document purpose in `docs/dev/`

**If this is legacy/unused**:
- Remove from repository

**Verdict**: 🔧 **RENAME OR REMOVE**

---

#### 7. **`conan/` Directory Structure Underutilized**
**Issue**: `conan/` directory has `lockfiles/` and `profiles/` but no `config.yml` or main configuration.

**Current State**:
```
conan/
├── lockfiles/
│   └── .gitkeep
└── profiles/
```

**Recommendation**:

If Conan profiles and lockfiles are actively used:
- Add example profiles (`conan/profiles/gcc`, `conan/profiles/clang`)
- Document usage in `docs/dev/`
- Add lockfile management scripts

If not actively used:
- Simplify to just track in root `conanfile.py`
- Remove directory

**Verdict**: 📋 **DECISION NEEDED**

---

### 🟢 **LOW PRIORITY (Cosmetic)**

#### 8. **Root Directory Has Too Many Markdown Files**
**Current State**:
```
.
├── AGENTS.md
├── CHANGELOG.md
├── CHANGELOG.zh-CN.md
├── CLAUDE.md
├── README.md
├── README.zh-CN.md
├── RELEASE_NOTES.md
├── RELEASE_zh.md
└── help.md
```

**Recommendation**:

Consolidate:
- `CHANGELOG.md` + `CHANGELOG.zh-CN.md` → Keep as-is (standard practice)
- `RELEASE_NOTES.md` + `RELEASE_zh.md` → Merge into CHANGELOG or move to `docs/`
- `CLAUDE.md` → Merge into `AGENTS.md` (same purpose)
- `help.md` → Move to `docs/en/reference/` or `docs/zh/reference/`

**Proposed**:
```
.
├── AGENTS.md              (AI agent instructions)
├── CHANGELOG.md
├── CHANGELOG.zh-CN.md
├── README.md
├── README.zh-CN.md
└── CMakeLists.txt
```

**Verdict**: 🎨 **OPTIONAL CLEANUP**

---

#### 9. **`vendor/` Directory Has `.gitkeep` But Contains `spring-core/`**
**Issue**: `vendor/.gitkeep` suggests the directory should be empty, but `vendor/spring-core/` exists.

**Recommendation**:
- Remove `.gitkeep` if directory is actively used
- Document vendor dependencies in `docs/dev/`
- Consider if `spring-core` should be a Conan package or git submodule

**Verdict**: 🎨 **MINOR CLEANUP**

---

#### 10. **Missing `testdata/` or `fixtures/` Directory**
**Issue**: No dedicated directory for test data files (FASTQ samples, etc.)

**Recommendation**:

Create `testdata/` directory:
```
testdata/
├── small.fastq
├── medium.fastq
├── paired/
│   ├── sample_R1.fastq
│   └── sample_R2.fastq
└── expected/
    └── (expected output files for verification)
```

- Use Git LFS for files > 1MB
- Add to CI test workflows
- Document in `tests/README.md`

**Verdict**: 🔧 **RECOMMENDED ADDITION**

---

## Summary of Recommended Actions

### 🔴 **HIGH PRIORITY (Do Now)**
1. ✅ Integrate E2E tests with CTest
2. 📋 Decide on benchmark CI strategy

### 🟡 **MEDIUM PRIORITY (Do Soon)**
3. 🔧 Reorganize `docs/` directory structure
4. ❓ Clarify `ref-projects/` purpose
5. 🔧 Fix `.arts/` directory naming
6. 📋 Decide on `conan/` directory usage

### 🟢 **LOW PRIORITY (Optional)**
7. 🎨 Consolidate root markdown files
8. 🎨 Clean up `vendor/` directory
9. 🔧 Add `testdata/` directory

---

## Recommended Final Structure

```
fq-compressor/
├── .github/               # CI/CD workflows
├── benchmark/             # Performance benchmarks
├── build/                 # Build artifacts (gitignored)
├── cmake/                 # CMake helper modules
├── conan/                 # Conan profiles & lockfiles
├── docker/                # Docker configurations
├── docs/                  # User & developer documentation
│   ├── dev/               # Developer guides
│   ├── en/                # English documentation
│   ├── zh/                # Chinese documentation
│   └── website/           # Nextra documentation site
├── include/fqc/           # Public headers
│   ├── algo/              # Compression algorithms
│   ├── commands/          # CLI command APIs
│   ├── common/            # Errors, logging, types, memory
│   ├── format/            # FQC archive format
│   ├── io/                # FASTQ parsing, compressed I/O
│   └── pipeline/          # TBB pipeline nodes
├── scripts/               # Build, test, lint scripts
├── specs/                 # Technical specifications (SSOT)
│   ├── product/           # Product requirements
│   ├── rfc/               # Technical design documents
│   └── testing/           # Test specifications
├── src/                   # Source implementations
│   ├── algo/
│   ├── commands/
│   ├── common/
│   ├── format/
│   ├── io/
│   ├── pipeline/
│   ├── CMakeLists.txt
│   └── main.cpp
├── testdata/              # Test data files (NEW)
├── tests/                 # Test suite
│   ├── algo/
│   ├── commands/
│   ├── common/
│   ├── e2e/
│   ├── format/
│   ├── io/
│   ├── pipeline/
│   ├── CMakeLists.txt
│   └── README.md
├── vendor/                # Vendored dependencies
│   └── spring-core/
├── AGENTS.md              # AI agent workflow guide
├── CHANGELOG.md           # Version history
├── CHANGELOG.zh-CN.md     # Chinese changelog
├── CMakeLists.txt         # Root CMake configuration
├── CMakePresets.json      # Build presets
├── conanfile.py           # Conan dependencies
├── README.md              # Project overview
└── README.zh-CN.md        # Chinese README
```

---

## Next Steps

1. **Review this document** and provide feedback on each recommendation
2. **Prioritize** which changes to implement
3. **Create tracking issues** for approved changes
4. **Implement changes** in small, focused PRs

---

## Questions for Project Maintainers

1. What is the purpose of `ref-projects/`? Should it be tracked or removed?
2. Should benchmarks be automated in CI or remain manual?
3. Is `.arts/` a typo for `.vscode/` or does it serve a different purpose?
4. Should `docs/specs/` content be merged with `/specs/`?
5. Do you want a `testdata/` directory with sample FASTQ files?

---

**Prepared by**: AI Code Review  
**Review Type**: Project Structure Analysis  
**Status**: Awaiting Feedback
