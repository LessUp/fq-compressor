#!/bin/bash
# =============================================================================
# fq-compressor - Test Execution Script
# =============================================================================
# This script provides convenient test execution for the fq-compressor project.
#
# Usage:
#   ./scripts/run_tests.sh [OPTIONS]
#
# Options:
#   -c, --compiler <gcc|clang>   Compiler to use (default: gcc)
#   -t, --type <Debug|Release>   Build type (default: Debug)
#   -v, --verbose                Verbose test output
#   -f, --filter <pattern>       Run only tests matching pattern
#   -h, --help                   Show this help message
# =============================================================================

set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
COMPILER="gcc"
BUILD_TYPE="Debug"
VERBOSE=""
FILTER=""
WORKSPACE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$@${NC}"
}

# Print usage
print_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Options:
  -c, --compiler <gcc|clang>   Compiler to use (default: gcc)
  -t, --type <Debug|Release>   Build type (default: Debug)
  -v, --verbose                Verbose test output
  -f, --filter <pattern>       Run only tests matching pattern (gtest format)
  -h, --help                   Show this help message

Examples:
  # Run all tests with GCC Debug build
  $0

  # Run all tests with Clang Release build
  $0 -c clang -t Release

  # Run verbose tests with filter
  $0 -v -f "MemoryBudgetTest.*"

  # Run specific test case
  $0 -f "MemoryBudgetTest.DefaultConstruction"
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--compiler)
            COMPILER="$2"
            shift 2
            ;;
        -t|--type)
            BUILD_TYPE="$2"
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

# Validate compiler
if [[ "$COMPILER" != "gcc" && "$COMPILER" != "clang" ]]; then
    print_msg "$RED" "Error: Invalid compiler '$COMPILER'. Must be 'gcc' or 'clang'."
    exit 1
fi

# Validate build type
if [[ "$BUILD_TYPE" != "Debug" && "$BUILD_TYPE" != "Release" ]]; then
    print_msg "$RED" "Error: Invalid build type '$BUILD_TYPE'. Must be 'Debug' or 'Release'."
    exit 1
fi

# Determine build directory
if [[ "$COMPILER" == "gcc" ]]; then
    if [[ "$BUILD_TYPE" == "Debug" ]]; then
        BUILD_DIR="$WORKSPACE_DIR/build/Debug"
    else
        BUILD_DIR="$WORKSPACE_DIR/build/gcc-release"
    fi
else
    BUILD_DIR="$WORKSPACE_DIR/build/clang-release"
fi

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    print_msg "$RED" "Error: Build directory not found: $BUILD_DIR"
    print_msg "$YELLOW" "Please run './scripts/build.sh $COMPILER $BUILD_TYPE' first."
    exit 1
fi

# Navigate to build directory
cd "$BUILD_DIR"

print_msg "$BLUE" "================================================"
print_msg "$BLUE" "fq-compressor Test Execution"
print_msg "$BLUE" "================================================"
print_msg "$GREEN" "Compiler:    $COMPILER"
print_msg "$GREEN" "Build Type:  $BUILD_TYPE"
print_msg "$GREEN" "Build Dir:   $BUILD_DIR"
if [[ -n "$FILTER" ]]; then
    print_msg "$GREEN" "Test Filter: $FILTER"
fi
print_msg "$BLUE" "================================================"
echo

# Check if testing is enabled
if ! cmake -L . 2>/dev/null | grep -q "BUILD_TESTING:BOOL=ON"; then
    print_msg "$YELLOW" "Warning: Testing is disabled. Enabling now..."
    cmake -DBUILD_TESTING=ON . > /dev/null 2>&1
fi

# Build test targets
print_msg "$BLUE" "Building test targets..."
if [[ "$COMPILER" == "gcc" ]]; then
    make placeholder_test memory_budget_test -j$(nproc) 2>&1 | tail -5
else
    ninja placeholder_test memory_budget_test 2>&1 | tail -5
fi

if [[ $? -ne 0 ]]; then
    print_msg "$RED" "Build failed!"
    exit 1
fi

print_msg "$GREEN" "Build successful!"
echo

# Run tests
if [[ -n "$FILTER" ]]; then
    # Run with gtest filter directly
    print_msg "$BLUE" "Running filtered tests: $FILTER"
    echo

    ./tests/placeholder_test --gtest_filter="$FILTER" 2>&1 || true
    ./tests/memory_budget_test --gtest_filter="$FILTER" 2>&1 || true
else
    # Run all tests with ctest
    print_msg "$BLUE" "Running all tests..."
    echo

    if [[ -n "$VERBOSE" ]]; then
        ctest --verbose --output-on-failure
    else
        ctest --output-on-failure
    fi
fi

# Capture test result
TEST_RESULT=$?

echo
print_msg "$BLUE" "================================================"

if [[ $TEST_RESULT -eq 0 ]]; then
    print_msg "$GREEN" "✓ All tests passed!"
else
    print_msg "$RED" "✗ Some tests failed!"
fi

print_msg "$BLUE" "================================================"

exit $TEST_RESULT
