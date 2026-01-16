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
    common/error.cpp
    common/logger.cpp
    common/memory_budget.cpp
    format/fqc_reader.cpp
    format/fqc_writer.cpp
    format/reorder_map.cpp
    io/fastq_parser.cpp
    io/compressed_stream.cpp
    io/async_io.cpp
    algo/global_analyzer.cpp
    algo/block_compressor.cpp
    algo/quality_compressor.cpp
    algo/id_compressor.cpp
    pipeline/pipeline.cpp
    pipeline/pipeline_node.cpp
)

# Command sources
set(FQC_COMMAND_SOURCES
    commands/compress_command.cpp
    commands/decompress_command.cpp
    commands/info_command.cpp
    commands/verify_command.cpp
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
        xxHash::xxhash
)

# =============================================================================
# CLI Library (fqc_cli)
# =============================================================================
# Contains command-line interface logic and command implementations.

add_library(fqc_cli STATIC ${FQC_COMMAND_SOURCES})

target_include_directories(fqc_cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${FQC_INCLUDE_DIR}>
)

target_link_libraries(fqc_cli
    PUBLIC
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
