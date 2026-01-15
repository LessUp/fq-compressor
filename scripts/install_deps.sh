#!/bin/bash
# scripts/install_deps.sh - Conan 依赖安装脚本

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# 默认值
PRESET="${1:-clang-debug}"

print_usage() {
    echo "Usage: $0 [preset]"
    echo ""
    echo "Available presets:"
    echo "  gcc-debug        - Install for GCC Debug build"
    echo "  gcc-release      - Install for GCC Release build"
    echo "  gcc-relwithdebinfo - Install for GCC RelWithDebInfo build"
    echo "  clang-debug      - Install for Clang Debug build"
    echo "  clang-release    - Install for Clang Release build"
    echo "  clang-asan       - Install for Clang ASan build"
    echo "  clang-tsan       - Install for Clang TSan build"
    echo "  coverage         - Install for coverage build"
    echo ""
    echo "Examples:"
    echo "  $0                    # Install for clang-debug"
    echo "  $0 gcc-release        # Install for gcc-release"
}

# 检查参数
if [[ "$PRESET" == "-h" || "$PRESET" == "--help" ]]; then
    print_usage
    exit 0
fi

cd "$PROJECT_DIR"

# 检测 Conan
if ! command -v conan &> /dev/null; then
    echo "Error: Conan not found. Install with: pip install conan"
    exit 1
fi

# 确保 Conan profile 存在
if ! conan profile show &> /dev/null; then
    echo "Creating default Conan profile..."
    conan profile detect --force
fi

# 根据 preset 确定 build_type 和编译器
case $PRESET in
    gcc-debug|clang-debug|clang-asan|clang-tsan|coverage)
        BUILD_TYPE="Debug"
        ;;
    gcc-release|clang-release)
        BUILD_TYPE="Release"
        ;;
    gcc-relwithdebinfo)
        BUILD_TYPE="RelWithDebInfo"
        ;;
    *)
        echo "Error: Unknown preset '$PRESET'"
        print_usage
        exit 1
        ;;
esac

# 根据 preset 确定编译器
case $PRESET in
    gcc-*)
        COMPILER_SETTINGS=""
        ;;
    clang-*)
        # Clang 使用 libc++
        COMPILER_SETTINGS="-s compiler.libcxx=libc++"
        ;;
    coverage)
        COMPILER_SETTINGS=""
        ;;
esac

BUILD_DIR="build/$PRESET"

echo "=== Installing Conan dependencies ==="
echo "Preset: $PRESET"
echo "Build type: $BUILD_TYPE"
echo "Output: $BUILD_DIR"
echo ""

# 安装依赖
conan install . \
    --output-folder="$BUILD_DIR" \
    --build=missing \
    -s build_type="$BUILD_TYPE" \
    -s compiler.cppstd=20 \
    $COMPILER_SETTINGS

echo ""
echo "=== Dependencies installed ==="
echo "Toolchain file: $BUILD_DIR/conan_toolchain.cmake"
