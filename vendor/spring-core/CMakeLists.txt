# Spring Core Algorithms - Extracted for fq-compressor
# 
# IMPORTANT: This code is licensed under the "Non-exclusive Research Use License
# for SPRING Software". See LICENSE file for details.
# 
# Usage is restricted to non-commercial, research purposes only.

cmake_minimum_required(VERSION 3.20)

project(spring-core
    VERSION 1.0.0
    DESCRIPTION "Spring core algorithms for FASTQ compression (ABC)"
    LANGUAGES CXX
)

# =============================================================================
# Options
# =============================================================================

option(SPRING_CORE_USE_OPENMP "Enable OpenMP support" OFF)
option(SPRING_CORE_BUILD_TESTS "Build tests" OFF)

# =============================================================================
# Library Target
# =============================================================================

add_library(spring-core STATIC
    src/util.cpp
    src/encoder.cpp
)

add_library(spring::core ALIAS spring-core)

target_include_directories(spring-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(spring-core PUBLIC cxx_std_17)

# =============================================================================
# Compiler Options
# =============================================================================

target_compile_options(spring-core PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# =============================================================================
# OpenMP Support (Optional)
# =============================================================================

if(SPRING_CORE_USE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(spring-core PUBLIC OpenMP::OpenMP_CXX)
        target_compile_definitions(spring-core PUBLIC USE_OPENMP)
    else()
        message(WARNING "OpenMP not found, building without OpenMP support")
    endif()
endif()

# =============================================================================
# Platform-specific Settings
# =============================================================================

# 128-bit integer support for BooPHF
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC and Clang support __uint128_t natively
elseif(MSVC)
    # MSVC needs special handling for 128-bit integers
    message(WARNING "MSVC may have limited support for 128-bit integers in BooPHF")
endif()

# =============================================================================
# Installation
# =============================================================================

include(GNUInstallDirs)

install(TARGETS spring-core
    EXPORT spring-core-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES LICENSE README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

# =============================================================================
# Export Configuration
# =============================================================================

install(EXPORT spring-core-targets
    FILE spring-core-targets.cmake
    NAMESPACE spring::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/spring-core
)

# =============================================================================
# Package Configuration
# =============================================================================

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/spring-core-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/spring-core-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/spring-core
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/spring-core-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/spring-core-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/spring-core-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/spring-core
)
