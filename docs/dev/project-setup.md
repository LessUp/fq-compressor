# fq-compressor 项目工程化规范

## 概述

本文档定义了 fq-compressor 项目的工程化规范，包括构建系统、依赖管理、容器化开发、CI/CD 配置等。

## 技术栈

| 类别 | 技术选型 | 版本要求 | 说明 |
|------|----------|----------|------|
| **语言** | C++ | C++23 | 使用 Concepts, Ranges, std::expected |
| **构建系统** | CMake | 3.20+ | Modern CMake 最佳实践 |
| **依赖管理** | Conan | 2.x | 自动化第三方库管理 |
| **编译器** | Clang | 21+ | 开发首选（更好的诊断） |
| **编译器** | GCC | 15+ | 生产编译首选 |
| **格式化** | clang-format | 21+ | 代码格式化 |
| **静态分析** | clang-tidy | 21+ | 静态代码检查 |
| **测试框架** | Google Test | 1.15+ | 单元测试 |
| **属性测试** | RapidCheck | 最新 | 属性测试 |
| **并发框架** | Intel oneTBB | 2022.3+ | 并行流水线 |
| **CLI 库** | CLI11 | 2.4+ | 命令行解析 |
| **日志库** | Quill | 11.x | 异步日志 |
| **格式化库** | fmt | 12.x | 格式化输出 |
| **校验库** | xxHash | 0.8+ | 非加密哈希 |
| **压缩库** | zstd, zlib-ng, liblzma, bzip2, libdeflate | 最新稳定版 | 压缩支持 |

## Docker 工具链选型

项目所有构建环境（开发、CI、发布）均基于 **Debian Bookworm** 系列镜像：

| 组件 | 选型 | 理由 |
|------|------|------|
| **构建基础镜像** | `gcc:15.2-bookworm` | Docker 官方 GCC 镜像，自带 GCC 15.2，无需额外编译安装 |
| **Clang 工具链** | Clang 21 (via `apt.llvm.org`) | LLVM 21 qualification branch，C++23 支持完善 |
| **Clang Release 镜像** | `debian:bookworm` | Clang 构建不需要 GCC，语义更清晰 |
| **运行时镜像** | `debian:bookworm-slim` | 与构建镜像同系，共享基础层，体积最小 |
| **不选 Ubuntu 24.04** | — | 无官方 `gcc:` 镜像，需自行编译 GCC 15；glibc 2.39 二进制兼容性较窄 |

**选型理由详述**：

1. **官方镜像生态**：Docker Hub `gcc:` 官方镜像全部基于 Debian，使用 `gcc:15.2-bookworm` 可直接获得最新 GCC，无需维护自定义编译流程
2. **glibc 兼容性**：Bookworm 的 glibc 2.36 比 Ubuntu 24.04 的 2.39 更低，编译出的动态链接二进制文件可在更多 Linux 发行版上运行
3. **镜像层共享**：所有 Dockerfile 统一基于 Bookworm 系列，`docker pull` 时共享基础层，减少总拉取量
4. **编译器独立性**：GCC/Clang 版本由 Docker 镜像和 LLVM apt 源决定，与底层发行版自带版本无关

## 开发环境

### 推荐开发环境（Dev Container）

项目提供 Dev Container 配置，确保开发环境一致性：

```json
// .devcontainer/devcontainer.json
{
  "name": "fq-compressor-dev",
  "dockerComposeFile": "../docker/docker-compose.yml",
  "service": "dev",
  "workspaceFolder": "/workspace",
  "customizations": {
    "vscode": {
      "extensions": [
        "llvm-vs-code-extensions.vscode-clangd",
        "ms-vscode.cmake-tools",
        "xaver.clang-format"
      ]
    }
  }
}
```

### 本地开发环境设置

```bash
# Ubuntu 24.04+
sudo apt update
sudo apt install -y \
    build-essential \
    clang-21 \
    clang-format-21 \
    clang-tidy-21 \
    cmake \
    ninja-build \
    python3-pip \
    git

# 安装 Conan
pip3 install conan

# 配置 Conan profile
conan profile detect --force

# 克隆项目
git clone https://github.com/LessUp/fq-compressor.git
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
cmake_minimum_required(VERSION 3.20)
project(fq-compressor
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "High-performance FASTQ compressor"
)

# C++23 标准
set(CMAKE_CXX_STANDARD 23)
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

项目 CI 使用 Docker 化构建环境（`docker/Dockerfile.dev`），通过 CMake Presets 系统保证构建一致性。

CI 包含以下 Workflows：

| Workflow | 文件 | 功能 |
|----------|------|------|
| **CI** | `.github/workflows/ci.yml` | 多 preset 构建与测试（gcc-release, clang-debug, clang-release） |
| **Quality** | `.github/workflows/quality.yml` | 格式检查（clang-format）+ 静态分析（clang-tidy） |
| **Docs Pages** | `.github/workflows/docs-pages.yml` | GitBook 文档构建与 GitHub Pages 部署 |
| **Docs Quality** | `.github/workflows/docs-quality.yml` | 文档 lint 检查 |

CI 流程简述：

```bash
# 1. 构建 Docker 工具链镜像（GCC 15 + Clang 21）
docker build -f docker/Dockerfile.dev --target base -t fqcompressor-ci .

# 2. 在容器内运行构建和测试
docker run --rm -v .:/workspace fqcompressor-ci ./scripts/build.sh gcc-release
docker run --rm -v .:/workspace fqcompressor-ci ./scripts/test.sh gcc-release
```

## 脚本工具

项目提供统一的构建/测试/检查脚本，均基于 CMake Preset 系统：

```bash
# 构建（自动安装 Conan 依赖 + CMake 配置 + 编译）
./scripts/build.sh <preset>          # e.g., gcc-release, clang-debug

# 测试
./scripts/test.sh <preset>           # 运行指定 preset 的测试

# 代码质量
./scripts/lint.sh format-check       # clang-format 格式检查
./scripts/lint.sh lint clang-debug   # clang-tidy 静态分析

# Conan 依赖安装
./scripts/install_deps.sh <preset>   # 安装指定 preset 的依赖
```

可用 presets: `gcc-debug`, `gcc-release`, `gcc-relwithdebinfo`, `clang-debug`, `clang-release`, `clang-asan`, `clang-tsan`, `coverage`

## 总结

本项目工程化规范确保：

1. **一致性**：通过工具自动强制执行代码风格
2. **可维护性**：清晰的目录结构和模块划分
3. **可移植性**：容器化开发环境，跨平台构建
4. **质量保证**：静态分析、单元测试、属性测试
5. **自动化**：CI/CD 流水线自动检查和构建
