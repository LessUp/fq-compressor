# 工具链稳定性与构建可复现性修复

**日期**: 2026-03-08
**类型**: build, chore
**影响范围**: docker, conan, cmake

## 变更内容

### 1. Clang 20 → Clang 22（LLVM 22.1.0 稳定版，2026-02-24 发布）

升级到 Clang 22（LLVM 22.1.0 最新稳定发布版），与 fastq-tools 项目工具链对齐。

**影响文件**:
- `docker/Dockerfile.dev`: llvm.sh 20 → 22，所有 `-20` 后缀 → `-22`
- `docker/Dockerfile.clang-release`: 同上
- `conan/profiles/clang`: `compiler.version=20` → `22`
- `conan/profiles/ci-clang`: 同上
- `.devcontainer/devcontainer.json`: 注释更新

### 2. Clang Release Dockerfile 基础镜像优化

`Dockerfile.clang-release` 基础镜像从 `gcc:15.2-bookworm` 改为 `debian:bookworm`。
Clang Release 构建不需要 GCC 编译器，使用 `debian:bookworm` 更加语义清晰，
并添加 `build-essential` 提供基础构建工具。

### 3. CMake 最低版本提升: 3.20 → 3.28

项目使用 C++23 特性，CMake 3.28 是支持 C++23 模块的最低推荐版本。
同步提升编译器最低版本要求：GCC 14+、Clang 18+。

**影响文件**:
- `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.28)`
- `CMakePresets.json`: `cmakeMinimumRequired.minor: 28`
- `CLAUDE.md`: 构建依赖说明更新

### 4. 移除冗余系统 dev 包

所有项目依赖（TBB、zstd、libdeflate、zlib-ng、bzip2、xz_utils 等）已通过
Conan 管理（见 `conanfile.py`）。Dockerfile 中 `apt-get install` 的 `-dev`
包与 Conan 构建的库冗余且可能冲突，已移除。

生产运行时镜像仅保留 `libtbb12`（onetbb/hwloc 为 shared）和 `libstdc++6`
（GCC 运行时必需）。

**影响文件**:
- `docker/Dockerfile.dev`: 移除 libdeflate-dev, zlib1g-dev 等 6 个 dev 包
- `docker/Dockerfile`: 构建阶段移除 6 个 dev 包，运行时精简为 libtbb12 + libstdc++6
