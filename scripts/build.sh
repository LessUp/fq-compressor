#!/usr/bin/env bash
# scripts/build.sh - 统一构建脚本

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# 默认值
PRESET="${1:-clang-debug}"
JOBS="${2:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

print_usage() {
    echo "Usage: $0 [preset] [jobs]"
    echo ""
    echo "Available presets:"
    echo "  gcc-debug        - GCC Debug build"
    echo "  gcc-release      - GCC Release build (production)"
    echo "  gcc-relwithdebinfo - GCC Release with debug info"
    echo "  clang-debug      - Clang Debug build (recommended for development)"
    echo "  clang-release    - Clang Release build"
    echo "  clang-asan       - Clang with AddressSanitizer"
    echo "  clang-tsan       - Clang with ThreadSanitizer"
    echo "  coverage         - GCC with coverage analysis"
    echo ""
    echo "Examples:"
    echo "  $0                    # Build with clang-debug preset"
    echo "  $0 gcc-release        # Build with gcc-release preset"
    echo "  $0 clang-asan 4       # Build with clang-asan using 4 jobs"
}

# 检查参数
if [[ "$PRESET" == "-h" || "$PRESET" == "--help" ]]; then
    print_usage
    exit 0
fi

# 验证 preset 是否存在
VALID_PRESETS="gcc-debug gcc-release gcc-relwithdebinfo clang-debug clang-release clang-asan clang-tsan coverage"
if ! echo "$VALID_PRESETS" | grep -qw "$PRESET"; then
    echo "Error: Unknown preset '$PRESET'"
    print_usage
    exit 1
fi

cd "$PROJECT_DIR"

echo "=== Building fq-compressor ==="
echo "Preset: $PRESET"
echo "Jobs: $JOBS"
echo ""

# 检查 Conan 依赖是否已安装
BUILD_DIR="build/$PRESET"
if [ ! -f "$BUILD_DIR/conan_toolchain.cmake" ]; then
    echo "Installing Conan dependencies..."
    ./scripts/install_deps.sh "$PRESET"
fi

# 配置
echo "Configuring..."
cmake --preset "$PRESET"

# 构建
echo "Building..."
cmake --build --preset "$PRESET" -j "$JOBS"

echo ""
echo "=== Build complete ==="
echo "Binary location: $BUILD_DIR"
