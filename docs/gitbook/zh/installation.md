# 安装指南

## 前置依赖

| 依赖 | 最低版本 |
|------|---------|
| C++ 编译器 | GCC 15+ 或 Clang 21+ |
| CMake | 3.20+ |
| Conan | 2.x |
| Ninja | 1.10+（推荐） |

## 从源码构建

### 1. 克隆仓库

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor
```

### 2. 通过 Conan 安装依赖

```bash
# 使用 Clang（推荐）
conan install . --build=missing \
  -of=build/clang-release \
  -s build_type=Release \
  -s compiler.cppstd=20

# 使用 GCC
conan install . --build=missing \
  -of=build/gcc-release \
  -s build_type=Release \
  -s compiler.cppstd=20
```

### 3. 配置和构建

```bash
# Clang Release
cmake --preset clang-release
cmake --build --preset clang-release -j$(nproc)

# GCC Release
cmake --preset gcc-release
cmake --build --preset gcc-release -j$(nproc)
```

### 4. 验证安装

```bash
# 运行测试
./scripts/test.sh clang-release

# 或通过 CTest
ctest --test-dir build/clang-release
```

编译产物位于 `build/<preset>/src/fqc`。

## 第三方依赖

所有依赖由 Conan 自动管理：

| 库 | 版本 | 用途 |
|---|------|------|
| Intel oneTBB | 2022.3.0 | 并行流水线处理 |
| CLI11 | 2.4.2 | 命令行参数解析 |
| quill | 11.0.2 | 高性能日志 |
| zlib-ng | 2.3.2 | 通用压缩后端 |
| zstd | 1.5.7 | 通用压缩后端 |
| libdeflate | 1.25 | 快速 DEFLATE 实现 |
| fmt | latest | 字符串格式化 |

## Docker（开发容器）

项目提供了 VS Code / CLion 的 Dev Container 配置：

```bash
# VS Code
# 打开项目文件夹，然后使用 "Reopen in Container"

# 手动 Docker 构建
docker build -f .devcontainer/Dockerfile -t fq-compressor-dev .
docker run -it -v $(pwd):/workspace fq-compressor-dev
```

## 常见问题

### Conan profile 未找到

```bash
conan profile detect --force
```

### CMake preset 未找到

请确保已先运行 `conan install` — 它会生成 CMake 工具链文件，preset 依赖于这些文件。

### macOS 上 TBB 未找到

```bash
brew install tbb
```
