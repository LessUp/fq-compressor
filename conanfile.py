# conanfile.py
# =============================================================================
# fq-compressor - Conan 2.x Package Configuration
# =============================================================================
# Dependency management for fq-compressor project.
#
# Usage:
#   conan install . --build=missing --output-folder=build
#   cmake --preset conan-release  # or use CMakePresets.json
#
# Dependencies are organized by purpose:
#   - CLI parsing: CLI11
#   - Logging: Quill (with fmt)
#   - Compression: zlib-ng, bzip2, xz_utils, zstd, libdeflate
#   - Checksums: xxHash
#   - Parallel processing: oneTBB
#   - Testing: GTest, RapidCheck
# =============================================================================

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class FQCompressorConan(ConanFile):
    name = "fqcompressor"
    version = "0.1.0"
    license = "MIT"
    author = "LessUp <jiashuai.mail@gmail.com>"
    url = "https://github.com/LessUp/fq-compressor"
    description = "High-performance FASTQ compressor with random access support"
    topics = ("bioinformatics", "fastq", "compression", "genomics", "archival")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*", "tests/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        # =========================================================================
        # CLI Parsing (Requirement 6.1)
        # =========================================================================
        # CLI11: Modern, header-only command line parser
        self.requires("cli11/2.4.2")

        # =========================================================================
        # Logging (Requirement 4.2)
        # =========================================================================
        # Quill: Low-latency asynchronous logging library
        self.requires("quill/11.0.2")
        # fmt: Modern formatting library (required by Quill)
        self.requires("fmt/12.1.0")

        # =========================================================================
        # Compression Libraries (Requirement 1.1.1)
        # =========================================================================
        # zlib-ng: High-performance zlib replacement for gzip support
        self.requires("zlib-ng/2.3.2")
        # bzip2: bzip2 compression support
        self.requires("bzip2/1.0.8")
        # xz_utils (liblzma): xz/LZMA compression support
        self.requires("xz_utils/5.4.5")
        # zstd: Fast compression algorithm (for Medium/Long reads)
        self.requires("zstd/1.5.7")
        # libdeflate: Fast gzip/deflate compression
        self.requires("libdeflate/1.25")

        # =========================================================================
        # Checksums (Requirement 5.1, 5.2)
        # =========================================================================
        # xxHash: Extremely fast non-cryptographic hash algorithm
        self.requires("xxhash/0.8.3")

        # =========================================================================
        # Parallel Processing (Requirement 4.1)
        # =========================================================================
        # oneTBB: Intel Threading Building Blocks for parallel pipelines
        self.requires("onetbb/2022.3.0")

    def build_requirements(self):
        # =========================================================================
        # Testing Frameworks
        # =========================================================================
        # GTest: Google Test framework for unit testing
        self.test_requires("gtest/1.15.0")
        # RapidCheck: Property-based testing framework
        # NOTE: Temporarily disabled due to CMake version compatibility issue
        # self.test_requires("rapidcheck/cci.20230815")

    def generate(self):
        """Generate CMake toolchain and dependency files."""
        from conan.tools.cmake import CMakeDeps

        # Generate CMake toolchain file (conan_toolchain.cmake)
        tc = CMakeToolchain(self)
        # Enable position-independent code for shared library compatibility
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.get_safe("fPIC", True)
        # Fix CMake version compatibility for older packages like rapidcheck
        tc.variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"
        tc.generate()

        # Generate CMake find_package config files for all dependencies
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        """Build the project using CMake."""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        """Package the built artifacts."""
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        """Define package information for consumers."""
        self.cpp_info.libs = ["fqcomp_core", "fqcomp_cli"]
        self.cpp_info.set_property("cmake_file_name", "FQCompressor")
        self.cpp_info.set_property("cmake_target_name", "FQCompressor::FQCompressor")
