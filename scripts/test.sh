#!/bin/bash
# scripts/test.sh - 测试运行脚本

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# 默认值
PRESET="${1:-clang-debug}"
FILTER="${2:-}"

print_usage() {
    echo "Usage: $0 [preset] [filter]"
    echo ""
    echo "Available presets:"
    echo "  gcc-debug        - Run tests with GCC Debug build"
    echo "  gcc-release      - Run tests with GCC Release build"
    echo "  clang-debug      - Run tests with Clang Debug build"
    echo "  clang-asan       - Run tests with AddressSanitizer"
    echo "  clang-tsan       - Run tests with ThreadSanitizer"
    echo "  coverage         - Run tests with coverage analysis"
    echo ""
    echo "Examples:"
    echo "  $0                           # Run all tests with clang-debug"
    echo "  $0 gcc-release               # Run all tests with gcc-release"
    echo "  $0 clang-debug 'BlockEncoder*' # Run tests matching filter"
}

# 检查参数
if [[ "$PRESET" == "-h" || "$PRESET" == "--help" ]]; then
    print_usage
    exit 0
fi

cd "$PROJECT_DIR"

echo "=== Running tests ==="
echo "Preset: $PRESET"
if [ -n "$FILTER" ]; then
    echo "Filter: $FILTER"
fi
echo ""

# 检查构建是否存在
BUILD_DIR="build/$PRESET"
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    echo "Please run './scripts/build.sh $PRESET' first."
    exit 1
fi

# 运行测试
if [ -n "$FILTER" ]; then
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R "$FILTER"
else
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

echo ""
echo "=== Tests complete ==="
