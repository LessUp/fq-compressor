# =============================================================================
# cmake/fqc_sanitizers.cmake
# =============================================================================
# fq-compressor Sanitizer Support Module
# Provides compiler sanitizer integration for memory, thread, and undefined
# behavior detection.
#
# Usage:
#   include(cmake/fqc_sanitizers.cmake)
#   fqc_add_sanitizer_flags(target_name)      # Per-target
#   fqc_add_sanitizer_flags_global()          # Global (all targets)
#
# Options:
#   ENABLE_ASAN  - Enable AddressSanitizer (memory errors)
#   ENABLE_TSAN  - Enable ThreadSanitizer (data races)
#   ENABLE_UBSAN - Enable UndefinedBehaviorSanitizer
#   ENABLE_MSAN  - Enable MemorySanitizer (Clang only, uninitialized memory)
#
# Note: ASan, TSan, and MSan are mutually exclusive. UBSan can be combined
# with ASan or TSan.
# =============================================================================

# =============================================================================
# Sanitizer Options
# =============================================================================

option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer for data race detection" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_MSAN "Enable MemorySanitizer (Clang only)" OFF)

# =============================================================================
# Sanitizer Validation
# =============================================================================

# Check for mutually exclusive sanitizers
set(_FQC_SANITIZER_COUNT 0)
if(ENABLE_ASAN)
    math(EXPR _FQC_SANITIZER_COUNT "${_FQC_SANITIZER_COUNT} + 1")
endif()
if(ENABLE_TSAN)
    math(EXPR _FQC_SANITIZER_COUNT "${_FQC_SANITIZER_COUNT} + 1")
endif()
if(ENABLE_MSAN)
    math(EXPR _FQC_SANITIZER_COUNT "${_FQC_SANITIZER_COUNT} + 1")
endif()

if(_FQC_SANITIZER_COUNT GREATER 1)
    message(FATAL_ERROR 
        "Only one of ENABLE_ASAN, ENABLE_TSAN, or ENABLE_MSAN can be enabled "
        "at a time. These sanitizers are mutually exclusive.")
endif()

# MSan requires Clang
if(ENABLE_MSAN AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR 
        "MemorySanitizer (MSAN) requires Clang compiler. "
        "Current compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# =============================================================================
# Sanitizer Flags
# =============================================================================

# Common flags for better error reporting
set(FQC_SANITIZER_COMMON_FLAGS 
    "-fno-omit-frame-pointer"
    "-fno-optimize-sibling-calls"
)

# AddressSanitizer flags
set(FQC_ASAN_COMPILE_FLAGS 
    "-fsanitize=address"
    ${FQC_SANITIZER_COMMON_FLAGS}
)
set(FQC_ASAN_LINK_FLAGS "-fsanitize=address")

# ThreadSanitizer flags
set(FQC_TSAN_COMPILE_FLAGS 
    "-fsanitize=thread"
    ${FQC_SANITIZER_COMMON_FLAGS}
)
set(FQC_TSAN_LINK_FLAGS "-fsanitize=thread")

# UndefinedBehaviorSanitizer flags
set(FQC_UBSAN_COMPILE_FLAGS 
    "-fsanitize=undefined"
    "-fno-sanitize-recover=all"
    ${FQC_SANITIZER_COMMON_FLAGS}
)
set(FQC_UBSAN_LINK_FLAGS "-fsanitize=undefined")

# MemorySanitizer flags (Clang only)
set(FQC_MSAN_COMPILE_FLAGS 
    "-fsanitize=memory"
    "-fsanitize-memory-track-origins=2"
    ${FQC_SANITIZER_COMMON_FLAGS}
)
set(FQC_MSAN_LINK_FLAGS "-fsanitize=memory")

# =============================================================================
# Function: fqc_add_sanitizer_flags
# =============================================================================
# Add sanitizer flags to a specific target.
#
# Arguments:
#   target - The CMake target to add sanitizer flags to
#
# Example:
#   fqc_add_sanitizer_flags(my_library)
#

function(fqc_add_sanitizer_flags target)
    if(NOT TARGET ${target})
        message(WARNING 
            "fqc_add_sanitizer_flags: Target '${target}' does not exist")
        return()
    endif()

    # AddressSanitizer
    if(ENABLE_ASAN)
        message(STATUS "Enabling AddressSanitizer for target: ${target}")
        target_compile_options(${target} PRIVATE 
            $<$<COMPILE_LANGUAGE:CXX>:${FQC_ASAN_COMPILE_FLAGS}>
        )
        target_link_options(${target} PRIVATE ${FQC_ASAN_LINK_FLAGS})
    endif()

    # ThreadSanitizer
    if(ENABLE_TSAN)
        message(STATUS "Enabling ThreadSanitizer for target: ${target}")
        target_compile_options(${target} PRIVATE 
            $<$<COMPILE_LANGUAGE:CXX>:${FQC_TSAN_COMPILE_FLAGS}>
        )
        target_link_options(${target} PRIVATE ${FQC_TSAN_LINK_FLAGS})
    endif()

    # UndefinedBehaviorSanitizer (can be combined with ASan or TSan)
    if(ENABLE_UBSAN)
        message(STATUS "Enabling UndefinedBehaviorSanitizer for target: ${target}")
        target_compile_options(${target} PRIVATE 
            $<$<COMPILE_LANGUAGE:CXX>:${FQC_UBSAN_COMPILE_FLAGS}>
        )
        target_link_options(${target} PRIVATE ${FQC_UBSAN_LINK_FLAGS})
    endif()

    # MemorySanitizer (Clang only)
    if(ENABLE_MSAN)
        message(STATUS "Enabling MemorySanitizer for target: ${target}")
        target_compile_options(${target} PRIVATE 
            $<$<COMPILE_LANGUAGE:CXX>:${FQC_MSAN_COMPILE_FLAGS}>
        )
        target_link_options(${target} PRIVATE ${FQC_MSAN_LINK_FLAGS})
    endif()
endfunction()

# =============================================================================
# Function: fqc_add_sanitizer_flags_global
# =============================================================================
# Add sanitizer flags globally to all targets.
#
# Example:
#   fqc_add_sanitizer_flags_global()
#

function(fqc_add_sanitizer_flags_global)
    # AddressSanitizer
    if(ENABLE_ASAN)
        message(STATUS "Enabling AddressSanitizer globally")
        add_compile_options(${FQC_ASAN_COMPILE_FLAGS})
        add_link_options(${FQC_ASAN_LINK_FLAGS})
    endif()

    # ThreadSanitizer
    if(ENABLE_TSAN)
        message(STATUS "Enabling ThreadSanitizer globally")
        add_compile_options(${FQC_TSAN_COMPILE_FLAGS})
        add_link_options(${FQC_TSAN_LINK_FLAGS})
    endif()

    # UndefinedBehaviorSanitizer
    if(ENABLE_UBSAN)
        message(STATUS "Enabling UndefinedBehaviorSanitizer globally")
        add_compile_options(${FQC_UBSAN_COMPILE_FLAGS})
        add_link_options(${FQC_UBSAN_LINK_FLAGS})
    endif()

    # MemorySanitizer
    if(ENABLE_MSAN)
        message(STATUS "Enabling MemorySanitizer globally")
        add_compile_options(${FQC_MSAN_COMPILE_FLAGS})
        add_link_options(${FQC_MSAN_LINK_FLAGS})
    endif()
endfunction()

# =============================================================================
# Status Output
# =============================================================================

if(ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN OR ENABLE_MSAN)
    message(STATUS "")
    message(STATUS "=== Sanitizer Configuration ===")
    if(ENABLE_ASAN)
        message(STATUS "  AddressSanitizer:           ON")
    endif()
    if(ENABLE_TSAN)
        message(STATUS "  ThreadSanitizer:            ON")
    endif()
    if(ENABLE_UBSAN)
        message(STATUS "  UndefinedBehaviorSanitizer: ON")
    endif()
    if(ENABLE_MSAN)
        message(STATUS "  MemorySanitizer:            ON")
    endif()
    message(STATUS "===============================")
    message(STATUS "")
endif()
