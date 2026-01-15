# =============================================================================
# tests/CMakeLists.txt
# =============================================================================
# Test directory CMake configuration for fq-compressor
#
# This file configures the test targets using Google Test and optionally
# RapidCheck for property-based testing.
# =============================================================================

# Enable testing
enable_testing()

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
    set(options PROPERTY_BASED)
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
    
    # Add RapidCheck for property-based tests
    if(ARG_PROPERTY_BASED AND rapidcheck_FOUND)
        target_link_libraries(${test_name}
            PRIVATE
                rapidcheck
        )
        target_compile_definitions(${test_name}
            PRIVATE
                FQC_HAS_RAPIDCHECK=1
        )
    endif()
    
    # Register with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    # Set test properties
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 300
        LABELS "unit"
    )
    
    if(ARG_PROPERTY_BASED)
        set_tests_properties(${test_name} PROPERTIES
            LABELS "property"
        )
    endif()
endfunction()

# =============================================================================
# Unit Tests
# =============================================================================

# Placeholder test to verify the build system works
# Real tests will be added as modules are implemented

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/placeholder_test.cpp")
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/placeholder_test.cpp"
"// =============================================================================
// Placeholder test to verify the build system
// =============================================================================

#include <gtest/gtest.h>

TEST(BuildSystemTest, Placeholder) {
    // This test verifies that the build system is correctly configured
    EXPECT_TRUE(true);
}

TEST(BuildSystemTest, CppStandard) {
    // Verify C++20 is enabled
    #if __cplusplus >= 202002L
        EXPECT_TRUE(true);
    #else
        FAIL() << \"C++20 is not enabled\";
    #endif
}
")
endif()

fqc_add_test(placeholder_test
    SOURCES placeholder_test.cpp
)

# =============================================================================
# Property-Based Tests (RapidCheck)
# =============================================================================

if(rapidcheck_FOUND)
    message(STATUS "Property-based testing enabled with RapidCheck")
    
    # FQC Format Property Tests
    # **Property 1: FQC 格式往返一致性**
    # Validates: Requirements 2.1, 5.1, 5.2
    fqc_add_test(fqc_format_property_test
        SOURCES format/fqc_format_property_test.cpp
        PROPERTY_BASED
    )
    
    # FASTQ Parser Property Tests
    # **Property 2: FASTQ 解析往返一致性**
    # Validates: Requirements 1.1.1
    fqc_add_test(fastq_parser_property_test
        SOURCES io/fastq_parser_property_test.cpp
        PROPERTY_BASED
    )
endif()

# =============================================================================
# Test Summary
# =============================================================================

message(STATUS "")
message(STATUS "=== Test Configuration ===")
message(STATUS "  Unit tests:     ON")
if(rapidcheck_FOUND)
    message(STATUS "  Property tests: ON (RapidCheck)")
else()
    message(STATUS "  Property tests: OFF (RapidCheck not found)")
endif()
message(STATUS "==========================")
message(STATUS "")
