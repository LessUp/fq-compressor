---
title: Installation
description: Installation guide for fq-compressor - multiple installation methods for different platforms
version: 0.1.0
last_updated: 2026-04-16
language: en
---

# Installation

> 📦 Multiple installation methods for different platforms and use cases

## System Requirements

### Minimum Requirements
- **Operating System**: Linux (glibc 2.31+) or macOS 11+
- **CPU**: x86_64 or ARM64 architecture
- **Memory**: 2 GB RAM (4 GB recommended for large datasets)
- **Disk Space**: 100 MB for binary installation

### Build Requirements (Source Compilation)
- **C++ Compiler**: GCC 14+ or Clang 18+
- **CMake**: 3.28 or higher
- **Conan**: 2.x package manager
- **Python**: 3.8+ (for benchmark scripts)

## Installation Methods

### Method 1: Pre-built Binaries (Recommended)

Pre-built binaries are available for Linux and macOS on x86_64 and ARM64 architectures.

#### Linux (x86_64, musl - Static)

```bash
# Download and extract
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-x86_64-musl.tar.gz

# Install to /usr/local/bin
sudo mv fq-compressor-v0.1.0-linux-x86_64-musl/fqc /usr/local/bin/
sudo chmod +x /usr/local/bin/fqc

# Verify installation
fqc --version
```

#### Linux (x86_64, glibc - Dynamic)

```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-x86_64-glibc.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-x86_64-glibc.tar.gz
sudo mv fq-compressor-v0.1.0-linux-x86_64-glibc/fqc /usr/local/bin/
```

#### Linux (ARM64, musl - Static)

```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-linux-aarch64-musl.tar.gz
tar -xzf fq-compressor-v0.1.0-linux-aarch64-musl.tar.gz
sudo mv fq-compressor-v0.1.0-linux-aarch64-musl/fqc /usr/local/bin/
```

#### macOS (Intel)

```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-macos-x86_64.tar.gz
tar -xzf fq-compressor-v0.1.0-macos-x86_64.tar.gz
sudo mv fq-compressor-v0.1.0-macos-x86_64/fqc /usr/local/bin/
```

#### macOS (Apple Silicon)

```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.1.0/fq-compressor-v0.1.0-macos-arm64.tar.gz
tar -xzf fq-compressor-v0.1.0-macos-arm64.tar.gz
sudo mv fq-compressor-v0.1.0-macos-arm64/fqc /usr/local/bin/
```

### Method 2: Build from Source

Building from source gives you the latest features and optimizations for your specific platform.

#### Prerequisites

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake python3 python3-pip

# Install Conan
pip3 install conan

# macOS (with Homebrew)
brew install cmake python conan
```

#### Build Steps

```bash
# Clone the repository
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor

# Install dependencies with Conan
conan install . --build=missing -of=build/clang-release \
    -s build_type=Release \
    -s compiler.cppstd=23

# Configure with CMake
cmake --preset clang-release

# Build
cmake --build --preset clang-release -j$(nproc)

# The binary will be at:
# build/clang-release/src/fqc
```

#### GCC Build Alternative

```bash
# For GCC build
conan install . --build=missing -of=build/gcc-release \
    -s build_type=Release \
    -s compiler=gcc \
    -s compiler.cppstd=23

cmake --preset gcc-release
cmake --build --preset gcc-release -j$(nproc)
```

### Method 3: Docker

Docker is ideal for isolated environments and CI/CD pipelines.

```bash
# Build the Docker image
docker build -f docker/Dockerfile -t fq-compressor:latest .

# Compress a file
docker run --rm \
    -v $(pwd):/data \
    fq-compressor:latest \
    compress -i /data/reads.fastq -o /data/reads.fqc

# Decompress a file
docker run --rm \
    -v $(pwd):/data \
    fq-compressor:latest \
    decompress -i /data/reads.fqc -o /data/reads.fastq
```

### Method 4: DevContainer (VS Code)

For development and contribution:

1. Open the project in VS Code
2. Press `F1` and select "Dev Containers: Reopen in Container"
3. The environment comes pre-configured with all build tools

## Available Builds

| Platform | Architecture | Link Type | Use Case |
|----------|--------------|-----------|----------|
| Linux | x86_64 | musl (static) | Universal compatibility, no dependencies |
| Linux | x86_64 | glibc (dynamic) | Slightly smaller binary |
| Linux | aarch64 | musl (static) | ARM servers (AWS Graviton, etc.) |
| macOS | x86_64 | dynamic | Intel Macs |
| macOS | arm64 | dynamic | Apple Silicon (M1/M2/M3) |

## Which Build Should I Use?

- **General Use**: `linux-x86_64-musl` (static, works everywhere)
- **ARM Servers**: `linux-aarch64-musl` (AWS Graviton, etc.)
- **macOS Intel**: `macos-x86_64`
- **macOS Apple Silicon**: `macos-arm64`
- **Development**: Build from source with Clang for best performance

## Verification

After installation, verify that `fqc` is properly installed:

```bash
# Check version
fqc --version

# View help
fqc --help

# Test with a small file
echo -e "@test\nACGT\n+\n1234" | fqc compress -o test.fqc
fqc info test.fqc
```

## Troubleshooting

### "Permission denied" Error

```bash
# Make the binary executable
chmod +x /usr/local/bin/fqc
```

### "Command not found" Error

```bash
# Ensure /usr/local/bin is in your PATH
echo $PATH | grep /usr/local/bin

# If not, add it (for bash)
echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Library Not Found (glibc builds)

If using glibc builds, ensure your system has a compatible glibc version:

```bash
# Check glibc version
ldd --version

# If version < 2.31, use the musl (static) build instead
```

## Next Steps

- [Quick Start Guide](./quickstart.md) - Compress your first file
- [CLI Usage Guide](./cli-usage.md) - Learn all commands and options

---

**🌐 Language**: [English (current)](./installation.md) | [简体中文](../../zh/getting-started/installation.md)
