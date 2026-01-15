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

# Note: Source files will be added as they are implemented
# For now, create an INTERFACE library to allow the project to configure

add_library(fqc_core INTERFACE)

target_include_directories(fqc_core
    INTERFACE
        $<BUILD_INTERFACE:${FQC_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(fqc_core INTERFACE cxx_std_20)

# Link dependencies
target_link_libraries(fqc_core
    INTERFACE
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

# Create a placeholder main.cpp if it doesn't exist
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp" 
"// =============================================================================
// fq-compressor - High Performance FASTQ Compressor
// =============================================================================
// Main entry point for the fqc command-line tool.
// =============================================================================

#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << \"fq-compressor v${PROJECT_VERSION}\" << std::endl;
    std::cout << \"High-performance FASTQ compressor with random access support\" << std::endl;
    std::cout << std::endl;
    std::cout << \"Usage: fqc <command> [options]\" << std::endl;
    std::cout << \"Commands: compress, decompress, info, verify\" << std::endl;
    std::cout << std::endl;
    std::cout << \"Run 'fqc --help' for more information.\" << std::endl;
    return 0;
}
")
endif()

add_executable(fqc main.cpp)

target_link_libraries(fqc
    PRIVATE
        fqc_cli
        fmt::fmt
        TBB::tbb
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
