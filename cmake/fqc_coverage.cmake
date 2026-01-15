# =============================================================================
# cmake/fqc_coverage.cmake
# =============================================================================
# fq-compressor Code Coverage Support Module
# Provides code coverage integration for GCC and Clang compilers.
#
# Usage:
#   include(cmake/fqc_coverage.cmake)
#   fqc_add_coverage_flags(target_name)       # Per-target
#   fqc_add_coverage_flags_global()           # Global (all targets)
#
# Options:
#   ENABLE_COVERAGE - Enable code coverage instrumentation
#
# After building and running tests, generate coverage reports with:
#   - GCC: gcov + lcov/genhtml
#   - Clang: llvm-cov
# =============================================================================

# =============================================================================
# Coverage Option
# =============================================================================

option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

# =============================================================================
# Coverage Validation
# =============================================================================

if(ENABLE_COVERAGE)
    # Coverage works best with Debug builds
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING 
            "Code coverage is most accurate with Debug builds. "
            "Current build type: ${CMAKE_BUILD_TYPE}")
    endif()
    
    # Check for supported compilers
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR 
            "Code coverage requires GCC or Clang compiler. "
            "Current compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

# =============================================================================
# Coverage Flags
# =============================================================================

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC coverage flags
    set(FQC_COVERAGE_COMPILE_FLAGS 
        "--coverage"
        "-fprofile-arcs"
        "-ftest-coverage"
    )
    set(FQC_COVERAGE_LINK_FLAGS "--coverage")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang coverage flags (source-based coverage)
    set(FQC_COVERAGE_COMPILE_FLAGS 
        "-fprofile-instr-generate"
        "-fcoverage-mapping"
    )
    set(FQC_COVERAGE_LINK_FLAGS 
        "-fprofile-instr-generate"
        "-fcoverage-mapping"
    )
endif()

# =============================================================================
# Function: fqc_add_coverage_flags
# =============================================================================
# Add coverage flags to a specific target.
#
# Arguments:
#   target - The CMake target to add coverage flags to
#
# Example:
#   fqc_add_coverage_flags(my_library)
#

function(fqc_add_coverage_flags target)
    if(NOT ENABLE_COVERAGE)
        return()
    endif()
    
    if(NOT TARGET ${target})
        message(WARNING 
            "fqc_add_coverage_flags: Target '${target}' does not exist")
        return()
    endif()

    message(STATUS "Enabling code coverage for target: ${target}")
    target_compile_options(${target} PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:${FQC_COVERAGE_COMPILE_FLAGS}>
    )
    target_link_options(${target} PRIVATE ${FQC_COVERAGE_LINK_FLAGS})
endfunction()

# =============================================================================
# Function: fqc_add_coverage_flags_global
# =============================================================================
# Add coverage flags globally to all targets.
#
# Example:
#   fqc_add_coverage_flags_global()
#

function(fqc_add_coverage_flags_global)
    if(NOT ENABLE_COVERAGE)
        return()
    endif()

    message(STATUS "Enabling code coverage globally")
    add_compile_options(${FQC_COVERAGE_COMPILE_FLAGS})
    add_link_options(${FQC_COVERAGE_LINK_FLAGS})
endfunction()

# =============================================================================
# Status Output
# =============================================================================

if(ENABLE_COVERAGE)
    message(STATUS "")
    message(STATUS "=== Code Coverage Configuration ===")
    message(STATUS "  Coverage enabled: ON")
    message(STATUS "  Compiler:         ${CMAKE_CXX_COMPILER_ID}")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(STATUS "  Tool:             gcov/lcov")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(STATUS "  Tool:             llvm-cov")
    endif()
    message(STATUS "===================================")
    message(STATUS "")
endif()
