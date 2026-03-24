#!/usr/bin/env bash
# =============================================================================
# fq-compressor - Test Execution Script
# =============================================================================
# Builds and runs tests using the CMake preset system.
#
# Usage:
#   ./scripts/run_tests.sh [OPTIONS]
#
# Options:
#   -p, --preset <name>     CMake preset (default: clang-debug)
#   -v, --verbose           Verbose test output
#   -f, --filter <pattern>  Run only tests matching pattern (ctest -R regex)
#   -l, --label <label>     Run only tests with label (unit|property)
#   -b, --build-only        Build test targets without running
#   -h, --help              Show this help message
# =============================================================================

set -euo pipefail

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
PRESET="clang-debug"
VERBOSE=""
FILTER=""
LABEL=""
BUILD_ONLY=false
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
# shellcheck source=./common.sh
source "$SCRIPT_DIR/common.sh"

# Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$*${NC}"
}

# Print usage
print_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Options:
  -p, --preset <name>     CMake preset (default: clang-debug)
  -v, --verbose           Verbose test output
  -f, --filter <pattern>  Run only tests matching pattern (ctest -R regex)
  -l, --label <label>     Run only tests with label (unit|property)
  -b, --build-only        Build test targets without running
  -h, --help              Show this help message

Available presets:
  gcc-debug          GCC Debug build
  gcc-release        GCC Release build
  clang-debug        Clang Debug build (default)
  clang-release      Clang Release build
  clang-asan         Clang with AddressSanitizer
  clang-tsan         Clang with ThreadSanitizer
  coverage           GCC with coverage analysis

Examples:
  $0                              # Run all tests with clang-debug
  $0 -p gcc-release               # Run all tests with gcc-release
  $0 -v -f 'MemoryBudget'        # Run matching tests verbosely
  $0 -l property                   # Run only property-based tests
  $0 -b                            # Build test targets only
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--preset)
            PRESET="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE="--verbose"
            shift
            ;;
        -f|--filter)
            FILTER="$2"
            shift 2
            ;;
        -l|--label)
            LABEL="$2"
            shift 2
            ;;
        -b|--build-only)
            BUILD_ONLY=true
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            print_msg "$RED" "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

BUILD_DIR="${PROJECT_DIR}/build/${PRESET}"

cd "$PROJECT_DIR"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    print_msg "$BLUE" "Configuring preset $PRESET..."
    cmake --preset "$PRESET"
fi

CTEST_DIR=$(resolve_cmake_build_dir "$PROJECT_DIR" "$PRESET" CTestTestfile.cmake)
if [[ ! -f "$CTEST_DIR/CTestTestfile.cmake" ]]; then
    print_msg "$RED" "Error: CTest metadata not found in $CTEST_DIR"
    print_msg "$YELLOW" "Please run './scripts/build.sh $PRESET' first."
    exit 1
fi

BUILD_DIR=$(resolve_cmake_build_dir "$PROJECT_DIR" "$PRESET" CMakeCache.txt)
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    print_msg "$RED" "Error: CMake cache not found in $BUILD_DIR"
    print_msg "$YELLOW" "Please run './scripts/build.sh $PRESET' first."
    exit 1
fi

if [[ "$CTEST_DIR" != "$BUILD_DIR" ]]; then
    print_msg "$YELLOW" "Using legacy test directory layout: $CTEST_DIR"
fi

if [[ "$BUILD_DIR" != "${PROJECT_DIR}/build/${PRESET}" ]]; then
    print_msg "$YELLOW" "Using legacy build directory layout: $BUILD_DIR"
fi

if [[ ! -d "$BUILD_DIR" ]]; then
    print_msg "$RED" "Error: Build directory not found: $BUILD_DIR"
    exit 1
fi

print_msg "$BLUE" "================================================"
print_msg "$BLUE" "fq-compressor Test Execution"
print_msg "$BLUE" "================================================"
print_msg "$GREEN" "Preset:    $PRESET"
print_msg "$GREEN" "Build Dir: $BUILD_DIR"
print_msg "$GREEN" "CTest Dir: $CTEST_DIR"
[[ -n "$FILTER" ]] && print_msg "$GREEN" "Filter:    $FILTER"
[[ -n "$LABEL" ]]  && print_msg "$GREEN" "Label:     $LABEL"
print_msg "$BLUE" "================================================"
echo

# Build test targets
print_msg "$BLUE" "Building test targets..."
cmake --build --preset "$PRESET" 2>&1 | tail -10
BUILD_EXIT=${PIPESTATUS[0]}

if [[ $BUILD_EXIT -ne 0 ]]; then
    print_msg "$RED" "Build failed!"
    exit 1
fi

print_msg "$GREEN" "Build successful!"
echo

if [[ "$BUILD_ONLY" == true ]]; then
    print_msg "$GREEN" "Build-only mode: skipping test execution."
    exit 0
fi

# Assemble ctest arguments
CTEST_ARGS=(--test-dir "$CTEST_DIR" --output-on-failure)
[[ -n "$VERBOSE" ]] && CTEST_ARGS+=("$VERBOSE")
[[ -n "$FILTER" ]]  && CTEST_ARGS+=(-R "$FILTER")
[[ -n "$LABEL" ]]   && CTEST_ARGS+=(-L "$LABEL")

# Run tests
print_msg "$BLUE" "Running tests..."
echo

ctest "${CTEST_ARGS[@]}"
TEST_RESULT=$?

echo
print_msg "$BLUE" "================================================"

if [[ $TEST_RESULT -eq 0 ]]; then
    print_msg "$GREEN" "All tests passed!"
else
    print_msg "$RED" "Some tests failed!"
fi

print_msg "$BLUE" "================================================"

exit $TEST_RESULT
