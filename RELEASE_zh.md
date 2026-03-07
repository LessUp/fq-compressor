# fq-compressor 发布说明

[English](README.md) | [Rust 版本 (fqc)](https://github.com/LessUp/fq-compressor-rust)

> 高性能 FASTQ 压缩工具（C++ 实现），与 [fq-compressor-rust](https://github.com/LessUp/fq-compressor-rust) 共享相同的 `.fqc` 归档格式与压缩算法，使用 Intel TBB 进行并行处理。

## [0.1.0] - 2026-03-07

**fq-compressor 首次发布** — 高性能 FASTQ 压缩工具（C++ 实现）。

### 功能特性

- **ABC 压缩算法** — 基于共识序列的增量编码，适用于短读段（< 300bp）
- **多压缩格式** — 支持 gzip、bzip2、xz、zstd、libdeflate 透明读写
- **SCM 质量压缩** — 统计上下文模型 + 算术编码，高效压缩质量分数
- **随机访问** — 块索引归档格式，支持高效部分解压
- **并行处理** — 基于 Intel TBB 的并行块压缩/解压
- **管线模式** — 3 阶段 Reader→Compressor→Writer 管线，支持反压（`--pipeline`）
- **异步 I/O** — 后台预取与写入缓冲，提升吞吐
- **流式模式** — 从标准输入低内存压缩，无需全局重排
- **压缩输入** — 透明解压 `.gz`、`.bz2`、`.xz`、`.zst` 格式的 FASTQ 文件
- **双端测序** — 支持交错（interleaved）与分文件双端模式
- **内存预算** — 自动检测系统内存，动态分块处理大型数据集
- **退出码映射** — 所有 CLI 命令统一退出码（0-5）

### 构建依赖

- C++23 编译器（GCC 11+ / Clang 12+）
- CMake 3.20+
- Conan 2.x（依赖管理）
- 主要库：CLI11、Quill、fmt、zlib-ng、bzip2、xz_utils、zstd、libdeflate、xxHash、oneTBB

### 安装

#### 从预编译二进制安装

从 [GitHub Releases](https://github.com/LessUp/fq-compressor/releases) 下载适合您平台的二进制文件。

#### 从源码构建

```bash
# 安装 Conan 依赖
./scripts/install_deps.sh gcc-release

# 构建
./scripts/build.sh gcc-release

# 运行测试
./scripts/test.sh gcc-release
```

生成的二进制文件位于 `build/gcc-release/src/fqc`。

#### Docker

```bash
# 本地构建
docker build -f docker/Dockerfile -t fq-compressor .

# 运行（挂载数据目录）
docker run --rm -v $(pwd):/data fq-compressor compress -i /data/reads.fastq -o /data/reads.fqc
docker run --rm -v $(pwd):/data fq-compressor decompress -i /data/reads.fqc -o /data/reads.fastq
```

### 快速开始

```bash
# 压缩
fqc compress input.fastq -o output.fqc

# 解压
fqc decompress output.fqc -o restored.fastq

# 查看归档信息
fqc info -i output.fqc

# 验证完整性
fqc verify -i output.fqc
```

### 平台支持

| 平台 | 文件 | 编译器 | 说明 |
|---|---|---|---|
| Linux x64 (GCC glibc) | `fq-compressor-v0.1.0-linux-x86_64-glibc.tar.gz` | GCC 15.2 | 动态链接 (glibc) |
| Linux x64 (GCC musl) | `fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz` | GCC 15.2 | **静态链接，任意 Linux 可运行** |
| Linux x64 (Clang) | `fq-compressor-v0.1.0-linux-x86_64-clang.tar.gz` | Clang 21 + libc++ | 动态链接 |
| Linux arm64 (GCC glibc) | `fq-compressor-v0.1.0-linux-aarch64-glibc.tar.gz` | GCC 15.2 | 动态链接 (glibc) |
| Linux arm64 (GCC musl) | `fq-compressor-v0.1.0-linux-aarch64-musl.tar.gz` | GCC 15.2 | **静态链接，任意 Linux 可运行** |
| Linux arm64 (Clang) | `fq-compressor-v0.1.0-linux-aarch64-clang.tar.gz` | Clang 21 + libc++ | 动态链接 |
| macOS x64 | `fq-compressor-v0.1.0-macos-x86_64.tar.gz` | GCC 15 | Intel Mac |
| macOS arm64 | `fq-compressor-v0.1.0-macos-arm64.tar.gz` | GCC 15 | Apple Silicon |

### 校验文件完整性

```bash
sha256sum -c checksums-sha256.txt
```

### 发布流程

推送版本标签即可触发自动发布：

```bash
# 确保 CMakeLists.txt 中的 VERSION 与标签一致
git tag v0.1.0
git push origin v0.1.0
```

GitHub Actions 会自动：
1. 验证标签与 CMakeLists.txt 版本一致性
2. 运行完整测试套件
3. 构建所有平台二进制文件（glibc / musl × x86_64 / aarch64 + macOS）
4. 生成 SHA-256 校验文件
5. 创建 GitHub Release 并上传所有产物
