# Installation

## Prerequisites

| Requirement | Minimum Version |
|-------------|-----------------|
| C++ Compiler | GCC 14+ or Clang 18+ |
| CMake | 3.28+ |
| Conan | 2.x |
| Ninja | 1.10+ (recommended) |

## Build from Source

### 1. Clone the Repository

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor
```

### 2. Install Dependencies via Conan

```bash
# Using Clang (recommended)
conan install . --build=missing \
  -of=build/clang-release \
  -s build_type=Release \
  -s compiler.cppstd=23

# Using GCC
conan install . --build=missing \
  -of=build/gcc-release \
  -s build_type=Release \
  -s compiler.cppstd=23
```

### 3. Configure and Build

```bash
# Clang Release
cmake --preset clang-release
cmake --build --preset clang-release -j$(nproc)

# GCC Release
cmake --preset gcc-release
cmake --build --preset gcc-release -j$(nproc)
```

### 4. Verify Installation

```bash
# Run tests
./scripts/test.sh clang-release

# Or via CTest
ctest --test-dir build/clang-release
```

The binary will be at `build/<preset>/src/fqc`.

## Dependencies

fq-compressor relies on the following libraries, all managed automatically by Conan:

| Library | Version | Purpose |
|---------|---------|---------|
| Intel oneTBB | 2022.3.0 | Parallel pipeline processing |
| CLI11 | 2.4.2 | Command-line argument parsing |
| quill | 11.0.2 | High-performance logging |
| zlib-ng | 2.3.2 | General compression backend |
| zstd | 1.5.7 | General compression backend |
| libdeflate | 1.25 | Fast DEFLATE implementation |
| fmt | latest | String formatting |

## Docker (Dev Container)

A dev container configuration is provided for VS Code / CLion:

```bash
# VS Code
# Open the project folder, then use "Reopen in Container"

# Manual Docker build
docker build -f .devcontainer/Dockerfile -t fq-compressor-dev .
docker run -it -v $(pwd):/workspace fq-compressor-dev
```

## Troubleshooting

### Conan profile not found

```bash
conan profile detect --force
```

### CMake preset not found

Make sure you ran `conan install` first — it generates the CMake toolchain files that presets depend on.

### TBB not found on macOS

```bash
brew install tbb
```
