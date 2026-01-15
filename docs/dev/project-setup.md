# fq-compressor 项目工程化规范

## 概述

本文档定义了 fq-compressor 项目的工程化规范，包括构建系统、依赖管理、容器化开发、CI/CD 配置等。

## 技术栈

| 类别 | 技术选型 | 版本要求 | 说明 |
|------|----------|----------|------|
| **语言** | C++ | C++20 | 使用 Concepts, Ranges, std::format |
| **构建系统** | CMake | 3.25+ | Modern CMake 最佳实践 |
| **依赖管理** | Conan | 2.x | 自动化第三方库管理 |
| **编译器** | Clang | 19+ | 开发首选（更好的诊断） |
| **编译器** | GCC | 14+ | 生产编译备选 |
| **格式化** | clang-format | 19+ | 代码格式化 |
| **静态分析** | clang-tidy | 19+ | 静态代码检查 |
| **测试框架** | Google Test | 1.14+ | 单元测试 |
| **属性测试** | RapidCheck | 最新 | 属性测试 |
| **并发框架** | Intel oneTBB | 2021.10+ | 并行流水线 |
| **CLI 库** | CLI11 | 2.4+ | 命令行解析 |
| **日志库** | Quill | 4.x | 异步日志 |
| **压缩库** | zstd, zlib, liblzma | 最新稳定版 | 压缩支持 |

## 开发环境

### 推荐开发环境（Dev Container）

项目提供 Dev Container 配置，确保开发环境一致性：

```json
// .devcontainer/devcontainer.json
{
  "name": "fq-compressor-dev",
  "image": "mcr.microsoft.com/devcontainers/cpp:ubuntu-24.04",
  "features": {
    "ghcr.io/devcontainers/features/common-utils:2": {},
    "ghcr.io/devcontainers/features/git:1": {}
  },
  "customizations": {
    "vscode": {
      "extensions": [
        "llvm-vs-code-extensions.vscode-clangd",
        "ms-vscode.cmake-tools",
        "xaver.clang-format"
      ]
    }
  },
  "postCreateCommand": "./scripts/setup-dev.sh"
}
```

### 本地开发环境设置

```bash
# Ubuntu 24.04+
sudo apt update
sudo apt install -y \
    build-essential \
    clang-19 \
    clang-format-19 \
    clang-tidy-19 \
    cmake \
    ninja-build \
    python3-pip \
    git

# 安装 Conan
pip3 install conan

# 配置 Conan profile
conan profile detect --force

# 克隆项目
git clone https://github.com/your-org/fq-compressor.git
cd fq-compressor

# 安装依赖
./scripts/install_deps.sh

# 构建
./scripts/build.sh
```

## CMake 配置

### 项目结构

```cmake
# CMakeLists.txt (根目录)
cmake_minimum_required(VERSION 3.25)
project(fq-compressor
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "High-performance FASTQ compressor"
)

# C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 导出 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 选项
option(FQC_BUILD_TESTS "Build tests" ON)
option(FQC_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(FQC_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(FQC_ENABLE_USAN "Enable UndefinedBehaviorSanitizer" OFF)
option(FQC_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# 编译警告
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wconversion
    -Wsign-conversion
    -Wcast-qual
    -Wformat=2
    -Wundef
    -Wshadow
    -Wcast-align
    -Wunused
    -Wnull-dereference
    -Wdouble-promotion
)

# Clang 特定警告
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wdocumentation
        -Wthread-safety
    )
endif()

# Sanitizers
if(FQC_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(FQC_ENABLE_USAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

if(FQC_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

# 子目录
add_subdirectory(src)

if(FQC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### 库目标拆分

```cmake
# src/CMakeLists.txt

# 通用模块
add_library(fqc_common STATIC
    common/logger.cpp
    common/error.cpp
)
target_include_directories(fqc_common PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(fqc_common PUBLIC quill::quill)

# 格式模块
add_library(fqc_format STATIC
    format/fqc_writer.cpp
    format/fqc_reader.cpp
    format/reorder_map.cpp
)
target_link_libraries(fqc_format PUBLIC fqc_common xxHash::xxhash)

# 算法模块
add_library(fqc_algo STATIC
    algo/sequence_compressor.cpp
    algo/quality_compressor.cpp
    algo/id_compressor.cpp
)
target_link_libraries(fqc_algo PUBLIC fqc_common zstd::libzstd_static)

# IO 模块
add_library(fqc_io STATIC
    io/fastq_parser.cpp
    io/compressed_stream.cpp
)
target_link_libraries(fqc_io PUBLIC fqc_common ZLIB::ZLIB)

# 流水线模块
add_library(fqc_pipeline STATIC
    pipeline/compression_engine.cpp
    pipeline/decompression_engine.cpp
)
target_link_libraries(fqc_pipeline PUBLIC
    fqc_format
    fqc_algo
    fqc_io
    TBB::tbb
)

# CLI 可执行文件
add_executable(fqc
    main.cpp
    commands/compress_command.cpp
    commands/decompress_command.cpp
    commands/info_command.cpp
    commands/verify_command.cpp
)
target_link_libraries(fqc PRIVATE fqc_pipeline CLI11::CLI11)
```

## Conan 依赖管理

### conanfile.py

```python
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout

class FQCompressorConan(ConanFile):
    name = "fq-compressor"
    version = "0.1.0"
    
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    
    requires = [
        "cli11/2.4.2",
        "quill/4.3.0",
        "onetbb/2021.12.0",
        "xxhash/0.8.2",
        "zstd/1.5.6",
        "zlib/1.3.1",
        "xz_utils/5.4.5",
        "bzip2/1.0.8",
    ]
    
    test_requires = [
        "gtest/1.14.0",
        "rapidcheck/cci.20230815",
    ]
    
    def layout(self):
        cmake_layout(self)
    
    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    
    def package(self):
        cmake = CMake(self)
        cmake.install()
```

### 依赖安装脚本

```bash
#!/bin/bash
# scripts/install_deps.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# 检测 Conan
if ! command -v conan &> /dev/null; then
    echo "Error: Conan not found. Install with: pip install conan"
    exit 1
fi

# 安装依赖
conan install . \
    --output-folder=build \
    --build=missing \
    -s build_type=Release \
    -s compiler.cppstd=20

echo "Dependencies installed successfully!"
```

## 静态分析配置

### .clang-tidy

```yaml
---
Checks: >
  -*,
  bugprone-*,
  clang-analyzer-*,
  cppcoreguidelines-*,
  misc-*,
  modernize-*,
  performance-*,
  portability-*,
  readability-*,
  -bugprone-easily-swappable-parameters,
  -cppcoreguidelines-avoid-magic-numbers,
  -cppcoreguidelines-pro-bounds-array-to-pointer-decay,
  -cppcoreguidelines-pro-bounds-constant-array-index,
  -cppcoreguidelines-pro-bounds-pointer-arithmetic,
  -misc-non-private-member-variables-in-classes,
  -modernize-use-trailing-return-type,
  -readability-identifier-length,
  -readability-magic-numbers,

WarningsAsErrors: ''

HeaderFilterRegex: '^(.*[/\\])?(src|include|tests)/.*\.(h|hpp)$'

CheckOptions:
  # 命名约定
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.StructCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: camelBack
  - key: readability-identifier-naming.MethodCase
    value: camelBack
  - key: readability-identifier-naming.VariableCase
    value: camelBack
  - key: readability-identifier-naming.ParameterCase
    value: camelBack
  - key: readability-identifier-naming.PrivateMemberCase
    value: camelBack
  - key: readability-identifier-naming.PrivateMemberSuffix
    value: '_'
  - key: readability-identifier-naming.ProtectedMemberCase
    value: camelBack
  - key: readability-identifier-naming.ProtectedMemberSuffix
    value: '_'
  - key: readability-identifier-naming.ConstantCase
    value: CamelCase
  - key: readability-identifier-naming.ConstantPrefix
    value: 'k'
  - key: readability-identifier-naming.NamespaceCase
    value: lower_case
  - key: readability-identifier-naming.EnumCase
    value: CamelCase
  - key: readability-identifier-naming.EnumConstantCase
    value: CamelCase
  - key: readability-identifier-naming.MacroDefinitionCase
    value: UPPER_CASE
  - key: readability-identifier-naming.TemplateParameterCase
    value: CamelCase
  
  # 其他选项
  - key: bugprone-assert-side-effect.AssertMacros
    value: 'assert,FQC_ASSERT'
  - key: bugprone-unused-return-value.AllowCastToVoid
    value: true
  - key: misc-non-private-member-variables-in-classes.IgnoreClassesWithAllMemberVariablesBeingPublic
    value: true
  - key: performance-move-const-arg.CheckTriviallyCopyableMove
    value: false
  - key: readability-function-cognitive-complexity.Threshold
    value: 25
...
```

### .clang-format

```yaml
---
Language: Cpp
BasedOnStyle: LLVM

# 缩进
IndentWidth: 4
TabWidth: 4
UseTab: Never
IndentCaseLabels: true
IndentPPDirectives: None
NamespaceIndentation: None

# 行宽
ColumnLimit: 100

# 大括号
BreakBeforeBraces: Attach
BraceWrapping:
  AfterCaseLabel: false
  AfterClass: false
  AfterControlStatement: Never
  AfterEnum: false
  AfterFunction: false
  AfterNamespace: false
  AfterStruct: false
  AfterUnion: false
  AfterExternBlock: false
  BeforeCatch: false
  BeforeElse: false
  BeforeLambdaBody: false
  BeforeWhile: false
  SplitEmptyFunction: true
  SplitEmptyRecord: true
  SplitEmptyNamespace: true

# 对齐
AlignAfterOpenBracket: Align
AlignConsecutiveAssignments: None
AlignConsecutiveDeclarations: None
AlignOperands: DontAlign
AlignTrailingComments: true

# 换行
AllowAllArgumentsOnNextLine: true
AllowAllParametersOfDeclarationOnNextLine: true
AllowShortBlocksOnASingleLine: Never
AllowShortCaseLabelsOnASingleLine: false
AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: Never
AllowShortLambdasOnASingleLine: Inline
AllowShortLoopsOnASingleLine: false
AlwaysBreakAfterReturnType: None
AlwaysBreakBeforeMultilineStrings: true
AlwaysBreakTemplateDeclarations: Yes
BinPackArguments: false
BinPackParameters: false
BreakBeforeBinaryOperators: None
BreakBeforeTernaryOperators: true
BreakConstructorInitializers: BeforeColon
BreakInheritanceList: BeforeColon
BreakStringLiterals: true

# 空格
SpaceAfterCStyleCast: false
SpaceAfterLogicalNot: false
SpaceAfterTemplateKeyword: true
SpaceBeforeAssignmentOperators: true
SpaceBeforeCpp11BracedList: false
SpaceBeforeCtorInitializerColon: true
SpaceBeforeInheritanceColon: true
SpaceBeforeParens: ControlStatements
SpaceBeforeRangeBasedForLoopColon: true
SpaceInEmptyBlock: false
SpaceInEmptyParentheses: false
SpacesBeforeTrailingComments: 2
SpacesInAngles: false
SpacesInCStyleCastParentheses: false
SpacesInContainerLiterals: true
SpacesInParentheses: false
SpacesInSquareBrackets: false

# 指针对齐
DerivePointerAlignment: false
PointerAlignment: Left

# Include 排序
IncludeBlocks: Regroup
IncludeCategories:
  # 对应的头文件
  - Regex: '^"fqc/.*\.h"$'
    Priority: 1
    SortPriority: 1
  # C 系统头文件
  - Regex: '^<c(assert|ctype|errno|fenv|float|inttypes|limits|locale|math|setjmp|signal|stdarg|stddef|stdint|stdio|stdlib|string|time|uchar|wchar|wctype)>$'
    Priority: 2
    SortPriority: 2
  # C++ 标准库
  - Regex: '^<[a-z_]+>$'
    Priority: 3
    SortPriority: 3
  # 第三方库
  - Regex: '^<.*>$'
    Priority: 4
    SortPriority: 4
  # 本项目头文件
  - Regex: '^".*"$'
    Priority: 5
    SortPriority: 5
SortIncludes: true
SortUsingDeclarations: true

# 其他
CompactNamespaces: false
ConstructorInitializerAllOnOneLineOrOnePerLine: true
ConstructorInitializerIndentWidth: 4
ContinuationIndentWidth: 4
Cpp11BracedListStyle: true
EmptyLineBeforeAccessModifier: LogicalBlock
FixNamespaceComments: true
MaxEmptyLinesToKeep: 2
ReflowComments: true
Standard: Latest

# 文件结尾
InsertNewlineAtEOF: true
KeepEmptyLinesAtEOF: true
...
```

### .editorconfig

```ini
# .editorconfig
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true

[*.{h,hpp,c,cpp,cc,cxx}]
indent_style = space
indent_size = 4

[*.{cmake,txt}]
indent_style = space
indent_size = 2

[*.{json,yaml,yml}]
indent_style = space
indent_size = 2

[*.md]
trim_trailing_whitespace = false

[Makefile]
indent_style = tab
```

## Git 配置

### .gitignore

```gitignore
# Build directories
build/
build-*/
cmake-build-*/
_output/

# IDE
.idea/
.vscode/
*.swp
*.swo
*~

# Conan
CMakeUserPresets.json

# Compiled files
*.o
*.obj
*.a
*.lib
*.so
*.dylib

# Generated files
compile_commands.json

# Test artifacts
Testing/
*.gcov
*.gcda
*.gcno

# Package artifacts
*.tar.gz
*.zip
```

### .gitmessage.txt

```
# <type>(<scope>): <subject>
#
# Type:
#   feat     : New feature
#   fix      : Bug fix
#   docs     : Documentation only
#   style    : Code style (formatting, no logic change)
#   refactor : Code refactoring
#   perf     : Performance improvement
#   test     : Adding or updating tests
#   build    : Build system or dependencies
#   ci       : CI/CD configuration
#   chore    : Other maintenance
#   revert   : Revert previous commit
#
# Scope (optional): format, algo, io, pipeline, cli, docs
#
# Subject: Imperative mood, no period at end
#
# Body (optional): Explain what and why
#
# Footer (optional):
#   Breaking Change: ...
#   Closes #123
```

### Pre-commit Hooks

```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/pre-commit/pre-commit-hooks
    rev: v4.5.0
    hooks:
      - id: trailing-whitespace
      - id: end-of-file-fixer
      - id: check-yaml
      - id: check-added-large-files
        args: ['--maxkb=500']

  - repo: local
    hooks:
      - id: clang-format
        name: clang-format
        entry: clang-format-19 -i
        language: system
        types: [c++]
        
      - id: clang-tidy
        name: clang-tidy
        entry: ./scripts/run-clang-tidy.sh
        language: script
        types: [c++]
        stages: [manual]
```

## CI/CD 配置

### GitHub Actions

```yaml
# .github/workflows/ci.yml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-24.04]
        compiler: [clang-19, gcc-14]
        build_type: [Debug, Release]
        
    runs-on: ${{ matrix.os }}
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y ninja-build python3-pip
          pip3 install conan
          
      - name: Setup compiler
        run: |
          if [[ "${{ matrix.compiler }}" == "clang-19" ]]; then
            sudo apt-get install -y clang-19
            echo "CC=clang-19" >> $GITHUB_ENV
            echo "CXX=clang++-19" >> $GITHUB_ENV
          else
            sudo apt-get install -y gcc-14 g++-14
            echo "CC=gcc-14" >> $GITHUB_ENV
            echo "CXX=g++-14" >> $GITHUB_ENV
          fi
          
      - name: Install Conan dependencies
        run: |
          conan profile detect --force
          conan install . --output-folder=build --build=missing \
            -s build_type=${{ matrix.build_type }} \
            -s compiler.cppstd=20
            
      - name: Configure
        run: |
          cmake -B build -G Ninja \
            -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
            -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
            
      - name: Build
        run: cmake --build build
        
      - name: Test
        run: ctest --test-dir build --output-on-failure

  lint:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      
      - name: Install tools
        run: |
          sudo apt-get update
          sudo apt-get install -y clang-format-19 clang-tidy-19
          
      - name: Check format
        run: ./scripts/lint.sh format-check
        
      - name: Run clang-tidy
        run: ./scripts/lint.sh lint
```

## 脚本工具

### scripts/build.sh

```bash
#!/bin/bash
# scripts/build.sh - 统一构建脚本

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# 默认值
COMPILER="${1:-clang}"
BUILD_TYPE="${2:-Release}"
ENABLE_ASAN=false
ENABLE_USAN=false
ENABLE_TSAN=false

# 解析参数
shift 2 || true
while [[ $# -gt 0 ]]; do
    case $1 in
        --asan) ENABLE_ASAN=true ;;
        --usan) ENABLE_USAN=true ;;
        --tsan) ENABLE_TSAN=true ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

# 设置编译器
case $COMPILER in
    clang)
        export CC=clang-19
        export CXX=clang++-19
        ;;
    gcc)
        export CC=gcc-14
        export CXX=g++-14
        ;;
    *)
        echo "Unknown compiler: $COMPILER"
        exit 1
        ;;
esac

BUILD_DIR="$PROJECT_DIR/build-${COMPILER}-${BUILD_TYPE,,}"

echo "=== Building fq-compressor ==="
echo "Compiler: $COMPILER"
echo "Build type: $BUILD_TYPE"
echo "Build dir: $BUILD_DIR"

# 配置
cmake -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/build/conan_toolchain.cmake" \
    -DFQC_ENABLE_ASAN="$ENABLE_ASAN" \
    -DFQC_ENABLE_USAN="$ENABLE_USAN" \
    -DFQC_ENABLE_TSAN="$ENABLE_TSAN"

# 构建
cmake --build "$BUILD_DIR" -j "$(nproc)"

echo "=== Build complete ==="
```

### scripts/lint.sh

```bash
#!/bin/bash
# scripts/lint.sh - 代码质量检查脚本

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

ACTION="${1:-lint}"

find_sources() {
    find "$PROJECT_DIR/src" "$PROJECT_DIR/include" "$PROJECT_DIR/tests" \
        -name "*.cpp" -o -name "*.h" -o -name "*.hpp" 2>/dev/null || true
}

case $ACTION in
    format)
        echo "Formatting code..."
        find_sources | xargs clang-format-19 -i
        echo "Done."
        ;;
    format-check)
        echo "Checking format..."
        find_sources | xargs clang-format-19 --dry-run --Werror
        echo "Format check passed."
        ;;
    lint)
        echo "Running clang-tidy..."
        find_sources | xargs clang-tidy-19 -p "$PROJECT_DIR/build"
        echo "Lint check passed."
        ;;
    *)
        echo "Usage: $0 {format|format-check|lint}"
        exit 1
        ;;
esac
```

## 总结

本项目工程化规范确保：

1. **一致性**：通过工具自动强制执行代码风格
2. **可维护性**：清晰的目录结构和模块划分
3. **可移植性**：容器化开发环境，跨平台构建
4. **质量保证**：静态分析、单元测试、属性测试
5. **自动化**：CI/CD 流水线自动检查和构建
