#!/usr/bin/env bash
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

# 根据 preset 确定编译器，使用数组避免引号/转义问题
CONAN_EXTRA_ARGS=()
case $PRESET in
    gcc-*|coverage)
        CONAN_EXTRA_ARGS+=(-s compiler.cppstd=gnu23)
        ;;
    clang-*)
        # Clang 使用 libc++（与 CMakePresets.json 中 -stdlib=libc++ 保持一致）
        # 检测系统 Clang 版本，生成临时 profile 确保：
        #   1. compiler settings 正确（clang + libc++）
        #   2. [buildenv] 设置 CC/CXX 环境变量，Conan 构建依赖时使用 Clang
        #   3. compiler_executables 在 conan_toolchain.cmake 中设置 CMAKE_CXX_COMPILER
        DETECTED_CLANG_VER=$(clang --version 2>/dev/null | head -1 | sed 's/[^0-9]*\([0-9]*\).*/\1/')
        if [[ -z "$DETECTED_CLANG_VER" ]]; then
            echo "Error: Could not detect Clang version"
            exit 1
        fi
        CONAN_SETTINGS="$(conan config home)/settings.yml"
        MAX_CONAN_CLANG_VER=$(
            awk '
                /^    clang:$/ { in_clang = 1; next }
                in_clang && /^        version:/ { in_version = 1 }
                in_version { print; if (/\]/) exit }
            ' "$CONAN_SETTINGS" \
                | grep -oE '"[0-9]+([.][0-9]+)?"' \
                | tr -d '"' \
                | sort -V \
                | tail -n 1
        )
        if [[ -z "$MAX_CONAN_CLANG_VER" ]]; then
            echo "Error: Could not determine Conan's maximum supported Clang version"
            exit 1
        fi
        CONAN_CLANG_VER="$DETECTED_CLANG_VER"
        if ((DETECTED_CLANG_VER > ${MAX_CONAN_CLANG_VER%%.*})); then
            CONAN_CLANG_VER="$MAX_CONAN_CLANG_VER"
            printf 'Warning: Conan does not model Clang %s; using its latest known ABI setting (%s)\n' \
                "$DETECTED_CLANG_VER" "$CONAN_CLANG_VER"
        fi
        export CC=clang
        export CXX=clang++
        # 生成临时 profile，包含正确的 Clang 版本和编译器路径
        TMPPROFILE=$(mktemp)
        trap 'rm -f "$TMPPROFILE"' EXIT
        cat > "$TMPPROFILE" <<EOF
[settings]
os=Linux
arch=x86_64
compiler=clang
compiler.version=${CONAN_CLANG_VER}
compiler.libcxx=libc++
compiler.cppstd=23

[buildenv]
CC=clang
CXX=clang++

[conf]
tools.build:compiler_executables={"c": "clang", "cpp": "clang++"}
tools.build:skip_test=True
EOF
        printf 'Generated temporary Clang profile (compiler=%s, Conan setting=%s):\n' \
            "$DETECTED_CLANG_VER" "$CONAN_CLANG_VER"
        cat "$TMPPROFILE"
        echo ""
        CONAN_EXTRA_ARGS+=(--profile:host "$TMPPROFILE" --profile:build "$TMPPROFILE")
        ;;
esac

BUILD_DIR="build/$PRESET"

echo "=== Installing Conan dependencies ==="
echo "Preset: $PRESET"
echo "Build type: $BUILD_TYPE"
echo "Output: $BUILD_DIR"
echo ""

# 设置 CMake 策略版本最低值，以兼容旧版 CMake 配置的依赖包（如 rapidcheck）
export CMAKE_POLICY_VERSION_MINIMUM=3.5

# 安装依赖
conan install . \
    --output-folder="$BUILD_DIR" \
    --build=missing \
    -s build_type="$BUILD_TYPE" \
    "${CONAN_EXTRA_ARGS[@]}"

# Project presets reference each generated toolchain directly. Conan's root user-preset aggregator
# assigns the same `conan-debug` name to debug, ASan, and TSan outputs, which makes CMake reject the
# repository as soon as a second Debug-family preset is installed.
rm -f "$PROJECT_DIR/CMakeUserPresets.json"

echo ""
echo "=== Dependencies installed ==="
echo "Toolchain file: $BUILD_DIR/build/$BUILD_TYPE/generators/conan_toolchain.cmake"
