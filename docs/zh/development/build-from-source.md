# 从源码构建

本指南说明如何从源码构建 fq-compressor。

## 前置要求

### 必需工具

| 工具 | 版本 | 用途 |
|------|---------|---------|
| **C++ 编译器** | GCC 15.2+ 或 Clang 21+ | C++23 支持 |
| **CMake** | 3.28+ | 构建系统 |
| **Conan** | 2.0+ | 依赖管理 |
| **Ninja** | 1.10+ | 构建生成器（推荐） |

### 平台支持

- **Linux**: 完全支持（主要平台）
- **macOS**: 支持（需 Homebrew 依赖）
- **Windows**: 不官方支持

## 快速开始

### 1. 安装依赖

```bash
# 安装 Conan（如未安装）
pip install conan

# 检测并创建默认配置
conan profile detect

# 安装依赖
./scripts/install_deps.sh gcc-release
```

### 2. 构建

```bash
# 使用 GCC（生产环境推荐）
./scripts/build.sh gcc-release

# 或使用 Clang（开发推荐）
./scripts/build.sh clang-release
```

### 3. 运行测试

```bash
./scripts/test.sh gcc-release
```

## 构建预设

| 预设 | 编译器 | 构建类型 | 用途 |
|--------|----------|------------|----------|
| `gcc-release` | GCC | Release | 生产构建 |
| `gcc-debug` | GCC | Debug | 调试 |
| `clang-release` | Clang | Release | 生产构建（Clang） |
| `clang-debug` | Clang | Debug | 开发 |
| `clang-asan` | Clang | Debug + ASan | 内存调试 |
| `clang-tsan` | Clang | Debug + TSan | 线程调试 |
| `coverage` | GCC | Debug | 代码覆盖率 |

## 手动构建

如果您喜欢手动控制：

```bash
# 安装依赖
conan install . --build=missing -c tools.cmake.cmaketoolchain:system_name=Linux

# 配置
cmake --preset gcc-release

# 构建
cmake --build --preset gcc-release -j$(nproc)
```

## Docker 构建

使用 Docker 实现可复现构建：

```bash
# 构建 Docker 镜像
docker build -f docker/Dockerfile -t fq-compressor .

# 运行压缩
docker run --rm -v $(pwd):/data fq-compressor compress -i /data/reads.fastq -o /data/reads.fqc
```

## 开发环境

### 使用 Dev Container

项目包含 VS Code dev container 配置：

1. 在 VS Code 中打开项目
2. 按 `F1` 并选择 "Dev Containers: Reopen in Container"
3. 容器包含所有必要工具

### 使用 Docker Compose

```bash
# 启动开发环境
docker compose up dev -d

# 运行命令
docker compose exec dev ./scripts/build.sh gcc-release
```

## 故障排除

### 常见问题

**Conan 配置未找到：**
```bash
conan profile detect
```

**编译器未找到：**
确保已安装 GCC 15+ 或 Clang 21+ 并在 PATH 中。

**CMake 版本过低：**
通过 pip 或包管理器安装 CMake 3.28+：
```bash
pip install cmake
```

### 获取帮助

- 查看 [FAQ](../reference/faq.md) 获取常见问题解答
- 在 [GitHub](https://github.com/LessUp/fq-compressor/issues) 提交问题
