# Directory Structure Optimization Summary

**Date**: 2026-04-17  
**Status**: Completed

---

## Changes Made

### ✅ Completed Optimizations

#### 1. Removed Duplicate `.arts/` Directory
- **Problem**: `.arts/` contained VS Code configuration files (duplicate of `.vscode/`)
- **Action**: Deleted `.arts/` directory
- **Result**: Cleaner root directory, removed confusion

#### 2. Consolidated `docs/` Directory Structure
- **Problem**: `docs/specs/` and `docs/research/` duplicated content in root `/specs/`
- **Actions**:
  - Moved `docs/specs/*` → `docs/archive/` (historical specs superseded by `/specs/`)
  - Moved `docs/research/*` → `docs/archive/` (research docs now in `/specs/rfc/`)
  - Updated `docs/archive/README.md` to clarify it's for historical documents
- **Result**: Clear separation between active specs (`/specs/`) and historical docs (`docs/archive/`)

#### 3. Organized `ref-projects/` Directory
- **Problem**: Directory was gitignored but undocumented
- **Actions**:
  - Created `ref-projects/README.md` explaining purpose and usage
  - Updated `.gitignore` to allow tracking the README
- **Result**: Clear documentation for reference projects directory

#### 4. Consolidated Root Markdown Files
- **Problem**: Too many markdown files at root with overlapping content
- **Actions**:
  - Deleted `CLAUDE.md` (content already in `AGENTS.md`)
  - Moved `help.md` → `docs/dev/docker-help.md`
  - Moved `RELEASE_NOTES.md` → `docs/archive/RELEASE_NOTES.md`
  - Moved `RELEASE_zh.md` → `docs/archive/RELEASE_zh.md`
- **Result**: Cleaner root directory, better organization

---

## Final Directory Structure

```
fq-compressor/
├── .github/                    # CI/CD workflows and CODEOWNERS
├── .vscode/                    # VS Code IDE configuration
├── benchmark/                  # Performance benchmark scripts
├── build/                      # Build artifacts (gitignored)
├── cmake/                      # CMake helper modules
├── conan/                      # Conan profiles & lockfiles
├── docker/                     # Docker configurations
├── docs/                       # Documentation
│   ├── archive/                # Historical documents
│   ├── benchmark/              # Benchmark documentation
│   ├── dev/                    # Developer guides & tools
│   ├── en/                     # English documentation
│   ├── website/                # Nextra documentation site
│   ├── zh/                     # Chinese documentation
│   └── README.md               # Documentation hub
├── include/fqc/                # Public headers
│   ├── algo/                   # Compression algorithms
│   ├── commands/               # CLI command APIs
│   ├── common/                 # Errors, logging, types, memory
│   ├── format/                 # FQC archive format
│   ├── io/                     # FASTQ parsing, compressed I/O
│   └── pipeline/               # TBB pipeline nodes
├── ref-projects/               # Reference project sources (gitignored)
├── scripts/                    # Build, test, lint scripts
├── specs/                      # Technical specifications (SSOT)
│   ├── product/                # Product requirements
│   ├── rfc/                    # Technical design documents
│   └── testing/                # Test specifications
├── src/                        # Source implementations
│   ├── algo/
│   ├── commands/
│   ├── common/
│   ├── format/
│   ├── io/
│   ├── pipeline/
│   ├── CMakeLists.txt
│   └── main.cpp
├── tests/                      # Test suite
│   ├── algo/
│   ├── commands/
│   ├── common/
│   ├── e2e/
│   ├── format/
│   ├── io/
│   ├── pipeline/
│   ├── CMakeLists.txt
│   └── README.md
├── vendor/                     # Vendored dependencies (spring-core)
├── AGENTS.md                   # AI agent workflow guide
├── CHANGELOG.md                # Version history
├── CHANGELOG.zh-CN.md          # Chinese changelog
├── CMakeLists.txt              # Root CMake configuration
├── CMakePresets.json           # Build presets
├── conanfile.py                # Conan dependencies
├── README.md                   # Project overview
└── README.zh-CN.md             # Chinese README
```

---

## Root Directory Files (Before & After)

### Before
```
.clang-format
.clang-tidy
.dockerignore
.editorconfig
.gitattributes
.gitignore
.gitmessage.txt
.pre-commit-config.yaml
AGENTS.md
CHANGELOG.md
CHANGELOG.zh-CN.md
CLAUDE.md              ❌ Removed (duplicate of AGENTS.md)
CMakeLists.txt
CMakePresets.json
commitlint.config.js
conanfile.py
Doxyfile
help.md                ❌ Moved to docs/dev/
LICENSE
package.json
package-lock.json
README.md
README.zh-CN.md
RELEASE_NOTES.md       ❌ Moved to docs/archive/
RELEASE_zh.md          ❌ Moved to docs/archive/
```

### After
```
.clang-format
.clang-tidy
.dockerignore
.editorconfig
.gitattributes
.gitignore
.gitmessage.txt
.pre-commit-config.yaml
AGENTS.md
CHANGELOG.md
CHANGELOG.zh-CN.md
CMakeLists.txt
CMakePresets.json
commitlint.config.js
conanfile.py
Doxyfile
LICENSE
package.json
package-lock.json
README.md
README.zh-CN.md
```

**Reduction**: 4 fewer files at root level

---

## Benefits

1. **Clearer Organization**: Active specs at root (`/specs/`), historical docs in `docs/archive/`
2. **Reduced Clutter**: Removed duplicate configuration files and consolidated markdown files
3. **Better Documentation**: All docs properly categorized in `docs/` subdirectories
4. **Consistent Structure**: Directory names clearly indicate purpose (`.vscode/` not `.arts/`)
5. **Easier Navigation**: Developers can find files more quickly with logical organization

---

## Recommendations for Future

1. **Add `testdata/` Directory**: For sample FASTQ files used in tests
2. **Integrate E2E Tests**: Add shell scripts to CTest for automated testing
3. **Benchmark CI**: Consider adding benchmark workflow to CI (manual trigger)
4. **Document Structure**: Update `docs/dev/` with this optimization rationale

---

## Validation

All changes preserve existing functionality:
- ✅ Build system unchanged
- ✅ CI/CD workflows unchanged
- ✅ Test structure unchanged
- ✅ Public API headers unchanged
- ✅ Source code organization unchanged

Only documentation and configuration files were reorganized.
