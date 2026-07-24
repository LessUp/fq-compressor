#!/usr/bin/env bash
# Usage: ./scripts/test.sh [preset] [filter]
#        ./scripts/test.sh -p clang-asan -f 'Archive*' -v

set -euo pipefail

PRESET="clang-debug"
FILTER=""
LABEL=""
VERBOSE=""
BUILD_ONLY=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--preset) PRESET="$2"; shift 2 ;;
        -f|--filter) FILTER="$2"; shift 2 ;;
        -l|--label) LABEL="$2"; shift 2 ;;
        -v|--verbose) VERBOSE="--verbose"; shift ;;
        -b|--build-only) BUILD_ONLY=true; shift ;;
        -*) echo "Unknown option: $1" >&2; exit 1 ;;
        *) if [[ -z "$FILTER" && "$PRESET" == "clang-debug" ]]; then PRESET="$1"; else FILTER="$1"; fi; shift ;;
    esac
done

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build/$PRESET"

cd "$PROJECT_DIR"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake --preset "$PRESET"
fi

echo "=== Building ($PRESET) ==="
cmake --build --preset "$PRESET" 2>&1 | tail -5

if [[ "$BUILD_ONLY" == true ]]; then
    echo "Build-only mode: done."
    exit 0
fi

CTEST_ARGS=(--test-dir "$BUILD_DIR" --output-on-failure)
[[ -n "$VERBOSE" ]] && CTEST_ARGS+=("$VERBOSE")
[[ -n "$FILTER" ]] && CTEST_ARGS+=(-R "$FILTER")
[[ -n "$LABEL" ]] && CTEST_ARGS+=(-L "$LABEL")

echo ""
echo "=== Running tests ($PRESET) ==="
ctest "${CTEST_ARGS[@]}"
