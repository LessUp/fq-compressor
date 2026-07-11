# =============================================================================
# tests/CMakeLists.txt
# =============================================================================
# Test directory CMake configuration for fq-compressor
#
# This file configures the test targets using Google Test and optionally
# RapidCheck for property-based testing.
# =============================================================================

# =============================================================================
# Test Configuration
# =============================================================================

# Common test include directories
set(FQC_TEST_INCLUDE_DIRS
    ${FQC_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# =============================================================================
# Helper Function: Add Test Executable
# =============================================================================

function(fqc_add_test test_name)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES LIBRARIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${test_name} ${ARG_SOURCES})

    target_include_directories(${test_name}
        PRIVATE
            ${FQC_TEST_INCLUDE_DIRS}
    )

    target_link_libraries(${test_name}
        PRIVATE
            fqc_core
            GTest::gtest
            GTest::gtest_main
            ${ARG_LIBRARIES}
    )

    # Workaround for GTest built with libstdc++ when using libc++
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_FLAGS MATCHES "-stdlib=libc\\+\\+")
        target_link_libraries(${test_name}
            PRIVATE
                stdc++
        )
    endif()

    # Register with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})

    # Set test properties
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 300
        LABELS "unit"
    )
endfunction()

# =============================================================================
# Unit Tests
# =============================================================================

# Build-system smoke test
fqc_add_test(build_smoke_test
    SOURCES placeholder_test.cpp
)

# Error Handling Tests
fqc_add_test(error_test
    SOURCES common/error_test.cpp
)

# FQCWriter Unit Tests
# Regression: reorder map size fields written correctly (forwardMapSize/reverseMapSize)
# Validates: Requirements 2.1, 5.1
fqc_add_test(fqc_writer_test
    SOURCES format/fqc_writer_test.cpp
)

# Async I/O Tests
fqc_add_test(async_io_test
    SOURCES io/async_io_test.cpp
)

# Stream factory tests
fqc_add_test(stream_factory_test
    SOURCES io/stream_factory_test.cpp
)

# FASTQ parser tests
fqc_add_test(fastq_parser_test
    SOURCES io/fastq_parser_test.cpp
)

# Chunk marshaling tests
fqc_add_test(chunk_marshal_test
    SOURCES pipeline/chunk_marshal_test.cpp
)

# Backpressure controller regression tests
fqc_add_test(backpressure_controller_test
    SOURCES pipeline/backpressure_controller_test.cpp
)

# Compressed Stream Tests
fqc_add_test(compressed_stream_test
    SOURCES io/compressed_stream_test.cpp
)

# Block compressor regression tests
fqc_add_test(block_compressor_regression_test
    SOURCES algo/block_compressor_regression_test.cpp
)

# Algorithm regression tests
fqc_add_test(algo_regression_test
    SOURCES algo/algo_regression_test.cpp
)

# Original-order command tests
fqc_add_test(original_order_command_test
    SOURCES commands/original_order_command_test.cpp
    LIBRARIES fqc_cli
)

# Archive regression tests
fqc_add_test(archive_regression_test
    SOURCES commands/archive_regression_test.cpp
    LIBRARIES fqc_cli
)

# Compression profile tests
fqc_add_test(compression_profile_test
    SOURCES commands/compression_profile_test.cpp
    LIBRARIES fqc_cli
)

# Compression request tests
fqc_add_test(compression_request_test
    SOURCES commands/compression_request_test.cpp
    LIBRARIES fqc_cli
)

# Compression engine tests
fqc_add_test(compression_engine_test
    SOURCES commands/compression_engine_test.cpp
    LIBRARIES fqc_cli
)

# =============================================================================
# Test Summary
# =============================================================================

message(STATUS "")
message(STATUS "=== Test Configuration ===")
message(STATUS "  Unit tests:     ON")
message(STATUS "==========================")
message(STATUS "")
