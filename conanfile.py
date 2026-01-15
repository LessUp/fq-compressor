# conanfile.py
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
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        # CLI 解析
        self.requires("cli11/2.4.2")
        # 日志
        self.requires("quill/11.0.2")
        self.requires("fmt/12.1.0")
        # 压缩库
        self.requires("zlib-ng/2.3.2")
        self.requires("bzip2/1.0.8")
        self.requires("xz_utils/5.4.5")
        self.requires("zstd/1.5.7")
        self.requires("libdeflate/1.25", override=True)
        # 并行处理
        self.requires("onetbb/2022.3.0")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["fqcomp_core", "fqcomp_cli"]
        self.cpp_info.set_property("cmake_file_name", "FQCompressor")
        self.cpp_info.set_property("cmake_target_name", "FQCompressor::FQCompressor")
