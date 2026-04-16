---
title: 安装指南
description: fq-compressor 安装指南——多种安装方式适用于不同平台和场景
version: 0.1.0
last_updated: 2026-04-16
language: zh
---

# 安装指南

> 📦 多种安装方式适用于不同平台和场景

## 系统要求

### 最低要求
- **操作系统**: Linux（glibc 2.31+）或 macOS 11+
- **CPU**: x86_64 或 ARM64 架构
- **内存**: 2 GB RAM（处理大型数据集建议 4 GB）
- **磁盘空间**: 100 MB（二进制安装）

### 源码编译要求
- **C++ 编译器**: GCC 14+ 或 Clang 18+
- **CMake**: 3.28 或更高版本
- **Conan**: 2.x 包管理器
- **Python**: 3.8+（用于基准测试脚本）

## 安装方式

### 方式 1：预编译二进制文件（推荐）

为 Linux 和 macOS 的 x86_64 和 ARM64 架构提供预编译二进制文件。

#### Linux（x86_64，musl - 静态链接）

```bash
# 下载并解压
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz

# 安装到 /usr/local/bin
sudo mv fq-compressor-v0.1.0-linux-x86_64-musl/fqc /usr/local/bin/
sudo chmod +x /usr/local/bin/fqc

# 验证安装
fqc --version
```

#### Linux（x86_64，glibc - 动态链接）

```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-x86_64-glibc.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-x86_64-glibc.tar.gz
sudo mv fq-compressor-v0.1.0-linux-x86_64-glibc/fqc /usr/local/bin/
```

#### Linux（ARM64，musl - 静态链接）

```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-aarch64-musl.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-aarch64-musl.tar.gz
sudo mv fq-compressor-v0.1.0-linux-aarch64-musl/fqc /usr/local/bin/
```

#### macOS（Intel）

```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-macos-x86_64.tar.gz
tar -xzf fq-compressor-v0.1.0-macos-x86_64.tar.gz
sudo mv fq-compressor-v0.1.0-macos-x86_64/fqc /usr/local/bin/
```

#### macOS（Apple Silicon）

```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-macos-arm64.tar.gz
tar -xzf fq-compressor-v0.1.0-macos-arm64.tar.gz
sudo mv fq-compressor-v0.1.0-macos-arm64/fqc /usr/local/bin/
```

### 方式 2：从源码构建

从源码构建可获得最新功能和针对您特定平台的优化。

#### 前置条件

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake python3 python3-pip

# 安装 Conan
pip3 install conan

# macOS（使用 Homebrew）
brew install cmake python conan
```

#### 构建步骤

```bash
# 克隆仓库
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor

# 使用 Conan 安装依赖
conan install . --build=missing -of=build/clang-release \
    -s build_type=Release \
    -s compiler.cppstd=23

# 使用 CMake 配置
cmake --preset clang-release

# 构建
cmake --build --preset clang-release -j$(nproc)

# 二进制文件位置：
# build/clang-release/src/fqc
```

#### GCC 构建替代方案

```bash
# GCC 构建
conan install . --build=missing -of=build/gcc-release \
    -s build_type=Release \
    -s compiler=gcc \
    -s compiler.cppstd=23

cmake --preset gcc-release
cmake --build --preset gcc-release -j$(nproc)
```

### 方式 3：Docker

Docker 适用于隔离环境和 CI/CD 流水线。

```bash
# 构建 Docker 镜像
docker build -f docker/Dockerfile -t fq-compressor:latest .

# 压缩文件
docker run --rm \
    -v $(pwd):/data \
    fq-compressor:latest \
    compress -i /data/reads.fastq -o /data/reads.fqc

# 解压文件
docker run --rm \
    -v $(pwd):/data \
    fq-compressor:latest \
    decompress -i /data/reads.fqc -o /data/reads.fastq
```

### 方式 4：DevContainer（VS Code）

用于开发和贡献：

1. 在 VS Code 中打开项目
2. 按 `F1` 并选择 "Dev Containers: Reopen in Container"
3. 环境已预配置所有构建工具

## 可用构建版本

| 平台 | 架构 | 链接方式 | 适用场景 |
|------|------|----------|----------|
| Linux | x86_64 | musl（静态） | 通用兼容，无依赖 |
| Linux | x86_64 | glibc（动态） | 体积稍小 |
| Linux | aarch64 | musl（静态） | ARM 服务器（AWS Graviton 等） |
| macOS | x86_64 | 动态 | Intel Mac |
| macOS | arm64 | 动态 | Apple Silicon（M1/M2/M3） |

## 我应该使用哪个版本？

- **一般用途**: `linux-x86_64-musl`（静态链接，随处运行）
- **ARM 服务器**: `linux-aarch64-musl`（AWS Graviton 等）
- **macOS Intel**: `macos-x86_64`
- **macOS Apple Silicon**: `macos-arm64`
- **开发**: 从源码构建，使用 Clang 获得最佳性能

## 验证安装

安装后，验证 `fqc` 是否正确安装：

```bash
# 检查版本
fqc --version

# 查看帮助
fqc --help

# 测试小文件
echo -e "@test\nACGT\n+\n1234" | fqc compress -o test.fqc
fqc info test.fqc
```

## 故障排除

### "Permission denied" 错误

```bash
# 使二进制文件可执行
chmod +x /usr/local/bin/fqc
```

### "Command not found" 错误

```bash
# 确保 /usr/local/bin 在 PATH 中
echo $PATH | grep /usr/local/bin

# 如果不在，添加它（bash）
echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 找不到库（glibc 构建）

如果使用 glibc 构建，请确保系统有兼容的 glibc 版本：

```bash
# 检查 glibc 版本
ldd --version

# 如果版本 < 2.31，改用 musl（静态）构建
```

## 下一步

- [快速入门指南](./quickstart.md) - 压缩您的第一个文件
- [CLI 使用指南](./cli-usage.md) - 了解所有命令和选项

---

**🌐 语言**: [English](../../en/getting-started/installation.md) | [简体中文（当前）](./installation.md)
