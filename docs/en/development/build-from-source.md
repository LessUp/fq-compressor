# Build from Source

This guide explains how to build fq-compressor from source code.

## Prerequisites

### Required Tools

| Tool | Version | Purpose |
|------|---------|---------|
| **C++ Compiler** | GCC 15.2+ or Clang 21+ | C++23 support |
| **CMake** | 3.28+ | Build system |
| **Conan** | 2.0+ | Dependency management |
| **Ninja** | 1.10+ | Build generator (recommended) |

### Platform Support

- **Linux**: Fully supported (primary platform)
- **macOS**: Supported with Homebrew dependencies
- **Windows**: Not officially supported

## Quick Start

### 1. Install Dependencies

```bash
# Install Conan (if not already installed)
pip install conan

# Detect and create default profile
conan profile detect

# Install dependencies
./scripts/install_deps.sh gcc-release
```

### 2. Build

```bash
# Using GCC (recommended for production)
./scripts/build.sh gcc-release

# Or using Clang (recommended for development)
./scripts/build.sh clang-release
```

### 3. Run Tests

```bash
./scripts/test.sh gcc-release
```

## Build Presets

| Preset | Compiler | Build Type | Use Case |
|--------|----------|------------|----------|
| `gcc-release` | GCC | Release | Production builds |
| `gcc-debug` | GCC | Debug | Debugging |
| `clang-release` | Clang | Release | Production (Clang) |
| `clang-debug` | Clang | Debug | Development |
| `clang-asan` | Clang | Debug + ASan | Memory debugging |
| `clang-tsan` | Clang | Debug + TSan | Thread debugging |
| `coverage` | GCC | Debug | Code coverage |

## Manual Build

If you prefer manual control:

```bash
# Install dependencies
conan install . --build=missing -c tools.cmake.cmaketoolchain:system_name=Linux

# Configure
cmake --preset gcc-release

# Build
cmake --build --preset gcc-release -j$(nproc)
```

## Docker Build

For reproducible builds, use Docker:

```bash
# Build Docker image
docker build -f docker/Dockerfile -t fq-compressor .

# Run compression
docker run --rm -v $(pwd):/data fq-compressor compress -i /data/reads.fastq -o /data/reads.fqc
```

## Development Environment

### Using Dev Container

The project includes VS Code dev container configuration:

1. Open project in VS Code
2. Press `F1` and select "Dev Containers: Reopen in Container"
3. The container includes all necessary tools

### Using Docker Compose

```bash
# Start development environment
docker compose up dev -d

# Run commands
docker compose exec dev ./scripts/build.sh gcc-release
```

## Troubleshooting

### Common Issues

**Conan profile not found:**
```bash
conan profile detect
```

**Compiler not found:**
Ensure GCC 15+ or Clang 21+ is installed and in PATH.

**CMake version too old:**
Install CMake 3.28+ via pip or your package manager:
```bash
pip install cmake
```

### Getting Help

- Check [FAQ](../reference/faq.md) for common questions
- Open an issue on [GitHub](https://github.com/LessUp/fq-compressor/issues)
