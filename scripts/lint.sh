#!/usr/bin/env bash
# scripts/lint.sh - 代码质量检查脚本

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

ACTION="${1:-lint}"
PRESET="${2:-clang-debug}"

# 查找所有源文件（含测试，用于格式检查）
find_all_sources() {
    find "$PROJECT_DIR/src" "$PROJECT_DIR/include" "$PROJECT_DIR/tests" \
        \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
        -not -path "*/build/*" 2>/dev/null || true
}

# 查找生产源文件（不含测试，用于 clang-tidy 静态分析）
find_lint_sources() {
    find "$PROJECT_DIR/src" "$PROJECT_DIR/include" \
        \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
        -not -path "*/build/*" 2>/dev/null || true
}

# 检测 clang-format 版本（优先项目标准版本）
detect_clang_format() {
    if command -v clang-format-21 &> /dev/null; then
        echo "clang-format-21"
    elif command -v clang-format &> /dev/null; then
        echo "clang-format"
    else
        echo "Error: clang-format not found" >&2
        exit 1
    fi
}

# 检测 clang-tidy 版本（优先项目标准版本）
detect_clang_tidy() {
    if command -v clang-tidy-21 &> /dev/null; then
        echo "clang-tidy-21"
    elif command -v clang-tidy &> /dev/null; then
        echo "clang-tidy"
    else
        echo "Error: clang-tidy not found" >&2
        exit 1
    fi
}

CLANG_FORMAT=$(detect_clang_format)
CLANG_TIDY=$(detect_clang_tidy)

case $ACTION in
    format)
        echo "Formatting code with $CLANG_FORMAT..."
        mapfile -t sources < <(find_all_sources)
        if [ ${#sources[@]} -gt 0 ]; then
            "$CLANG_FORMAT" -i "${sources[@]}"
            echo "Done. Formatted ${#sources[@]} files."
        else
            echo "No source files found."
        fi
        ;;
    format-check)
        echo "Checking format with $CLANG_FORMAT..."
        mapfile -t sources < <(find_all_sources)
        if [ ${#sources[@]} -gt 0 ]; then
            "$CLANG_FORMAT" --dry-run --Werror "${sources[@]}"
            echo "Format check passed."
        else
            echo "No source files found."
        fi
        ;;
    lint)
        echo "Running $CLANG_TIDY..."
        BUILD_DIR="$PROJECT_DIR/build/$PRESET"
        if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
            echo "Error: compile_commands.json not found in $BUILD_DIR"
            echo "Please run './scripts/build.sh $PRESET' first."
            exit 1
        fi
        mapfile -t sources < <(find_lint_sources)
        if [ ${#sources[@]} -gt 0 ]; then
            "$CLANG_TIDY" -p "$BUILD_DIR" "${sources[@]}"
            echo "Lint check passed."
        else
            echo "No source files found."
        fi
        ;;
    all)
        "$0" format-check "$PRESET"
        "$0" lint "$PRESET"
        ;;
    *)
        echo "Usage: $0 {format|format-check|lint|all} [preset]"
        echo ""
        echo "Commands:"
        echo "  format       - Format all source files in place"
        echo "  format-check - Check formatting without modifying files"
        echo "  lint         - Run clang-tidy static analysis"
        echo "  all          - Run format-check and lint"
        echo ""
        echo "Preset:"
        echo "  Optional for lint/all, defaults to clang-debug"
        exit 1
        ;;
esac
