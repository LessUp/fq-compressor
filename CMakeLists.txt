# =============================================================================
# fq-compressor - High Performance FASTQ Compressor
# =============================================================================
# CMake build configuration for fq-compressor project
# 
# Requirements: CMake 3.20+, C++20 compiler (GCC 11+ or Clang 12+)
# Dependencies: CLI11, Quill, xxHash, TBB, GTest, RapidCheck (via Conan 2.x)
#
# Build presets are defined in CMakePresets.json for consistent builds.
# =============================================================================

cmake_minimum_required(VERSION 3.20)

project(FQCompressor
    LANGUAGES CXX
    VERSION 0.1.0
    DESCRIPTION "High-performance FASTQ compressor with random access support"
    HOMEPAGE_URL "https://github.com/LessUp/fq-compressor"
)

# =============================================================================
# Project Configuration
# =============================================================================

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS 
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# =============================================================================
# C++ Standard Configuration
# =============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Compiler Version Check
# =============================================================================
# Enforce minimum compiler versions required for C++20 support.

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "11.0")
        message(FATAL_ERROR 
            "GCC version ${CMAKE_CXX_COMPILER_VERSION} is too old. "
            "fq-compressor requires GCC 11.0 or newer for C++20 support.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.0")
        message(FATAL_ERROR 
            "Clang version ${CMAKE_CXX_COMPILER_VERSION} is too old. "
            "fq-compressor requires Clang 12.0 or newer for C++20 support.")
    endif()
else()
    message(WARNING 
        "Compiler ${CMAKE_CXX_COMPILER_ID} is not officially supported. "
        "Your build may fail due to lack of C++20 support.")
endif()

# =============================================================================
# Compiler Warning Flags
# =============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wcast-qual
        -Wformat=2
        -Wundef
        -Werror=return-type
        -Wno-unused-parameter  # Allow unused parameters in interface implementations
    )
    
    # Additional warnings for GCC
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wnull-dereference
        )
    endif()
    
    # Additional warnings for Clang
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(
            -Wshadow-all
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
        )
    endif()
    
    # Build type specific flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O2 -g)
    endif()
endif()

# =============================================================================
# Sanitizer Support
# =============================================================================

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fqc_sanitizers.cmake)
fqc_add_sanitizer_flags_global()

# =============================================================================
# Code Coverage Support
# =============================================================================

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fqc_coverage.cmake)
if(ENABLE_COVERAGE)
    fqc_add_coverage_flags_global()
endif()

# =============================================================================
# Conan Integration
# =============================================================================
# Dependencies are managed via Conan 2.x. Run `conan install . --build=missing`
# before configuring CMake. The conan_toolchain.cmake will be generated in the
# build directory.

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(EXISTS "${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
        include("${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
    else()
        message(STATUS 
            "No Conan toolchain detected. "
            "Run 'conan install . --build=missing' to install dependencies.")
    endif()
endif()

# =============================================================================
# Find Dependencies (handled by Conan)
# =============================================================================

# CLI parsing library
find_package(CLI11 CONFIG REQUIRED)
message(STATUS "Found CLI11: ${CLI11_VERSION}")

# Logging library
find_package(quill CONFIG REQUIRED)
message(STATUS "Found Quill: ${quill_VERSION}")

# Formatting library
find_package(fmt CONFIG REQUIRED)
message(STATUS "Found fmt: ${fmt_VERSION}")

# Compression libraries
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(libdeflate CONFIG REQUIRED)
add_compile_definitions(USE_LIBDEFLATE)
message(STATUS "Found libdeflate: ${libdeflate_VERSION}")

# Parallel processing
find_package(TBB CONFIG REQUIRED)
message(STATUS "Found TBB: ${TBB_VERSION}")

# Handle fmt header-only vs compiled library
if(TARGET fmt::fmt-header-only AND NOT TARGET fmt::fmt)
    add_library(fmt::fmt ALIAS fmt::fmt-header-only)
endif()

# =============================================================================
# Project Include Directories
# =============================================================================

# Define the public include directory for the project
set(FQC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# =============================================================================
# Add Subdirectories
# =============================================================================

# Source files (libraries and main executable)
add_subdirectory(src)

# Vendor dependencies (Spring core, etc.)
# add_subdirectory(vendor)  # Uncomment when vendor dependencies are added

# =============================================================================
# Testing Configuration
# =============================================================================

include(CTest)
option(BUILD_TESTING "Build tests" ON)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    # RapidCheck for property-based testing (optional)
    find_package(rapidcheck CONFIG QUIET)
    if(rapidcheck_FOUND)
        message(STATUS "Found RapidCheck: Property-based testing enabled")
    else()
        message(STATUS "RapidCheck not found: Property-based testing disabled")
    endif()
    
    add_subdirectory(tests)
endif()

# =============================================================================
# Installation Configuration
# =============================================================================

# Install the main executable
install(TARGETS fqc
    EXPORT FQCompressorTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime
)

# Install public headers
install(DIRECTORY ${FQC_INCLUDE_DIR}/fqc/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fqc
    COMPONENT Development
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# =============================================================================
# Package Configuration
# =============================================================================

set(CMAKE_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/FQCompressor)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FQCompressorConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/FQCompressorConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/FQCompressorConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/FQCompressorConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/FQCompressorConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
)

install(EXPORT FQCompressorTargets
    FILE FQCompressorTargets.cmake
    NAMESPACE FQCompressor::
    DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
    COMPONENT Development
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "=== fq-compressor Configuration Summary ===")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build testing:  ${BUILD_TESTING}")
message(STATUS "============================================")
message(STATUS "")
