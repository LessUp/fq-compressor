# =============================================================================
# src/CMakeLists.txt
# =============================================================================
# Source directory CMake configuration for fq-compressor
#
# This file defines the library targets and the main executable.
# =============================================================================

# =============================================================================
# Core Library (fqc_core)
# =============================================================================
# Contains the core compression/decompression logic, format handling, and
# algorithm implementations.

# Collect source files for the core library
set(FQC_CORE_SOURCES
    common/logger.cpp
)

# Create the core library
add_library(fqc_core STATIC ${FQC_CORE_SOURCES})

target_include_directories(fqc_core
    PUBLIC
        $<BUILD_INTERFACE:${FQC_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(fqc_core PUBLIC cxx_std_20)

# Link dependencies
target_link_libraries(fqc_core
    PUBLIC
        fmt::fmt
        quill::quill
        TBB::tbb
        ZLIB::ZLIB
        BZip2::BZip2
        LibLZMA::LibLZMA
        $<TARGET_NAME_IF_EXISTS:zstd::libzstd_shared>
        $<TARGET_NAME_IF_EXISTS:zstd::libzstd_static>
        libdeflate::libdeflate_shared
)

# =============================================================================
# CLI Library (fqc_cli)
# =============================================================================
# Contains command-line interface logic and command implementations.

add_library(fqc_cli INTERFACE)

target_include_directories(fqc_cli
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${FQC_INCLUDE_DIR}>
)

target_link_libraries(fqc_cli
    INTERFACE
        fqc_core
        CLI11::CLI11
)

# =============================================================================
# Main Executable (fqc)
# =============================================================================

add_executable(fqc main.cpp)

target_link_libraries(fqc
    PRIVATE
        fqc_cli
)

target_include_directories(fqc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${FQC_INCLUDE_DIR}
)

# =============================================================================
# Installation
# =============================================================================

# Install libraries
install(TARGETS fqc_core fqc_cli
    EXPORT FQCompressorTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT Development
)
