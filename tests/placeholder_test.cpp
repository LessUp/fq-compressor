// =============================================================================
// Build and CLI smoke tests for the test harness
// =============================================================================

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string quote(std::string_view value) {
    return "'" + std::string(value) + "'";
}

std::filesystem::path locateCliBinary() {
    const auto cwd = std::filesystem::current_path();
    const std::vector<std::filesystem::path> candidates = {
        cwd / "src" / "fqc",
        cwd.parent_path() / "src" / "fqc",
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return cwd / "src" / "fqc";
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

struct TempDir {
    std::filesystem::path path;

    TempDir()
        : path(std::filesystem::temp_directory_path() /
               ("fqc_build_smoke_" + std::to_string(std::random_device{}()))) {
        std::filesystem::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

}  // namespace

TEST(BuildSmokeTest, HarnessLoads) {
    EXPECT_TRUE(true);
}

TEST(BuildSmokeTest, CppStandard) {
// Verify C++23 is enabled
#if __cplusplus >= 202302L
    EXPECT_TRUE(true);
#elif __cplusplus >= 202002L
    // C++20 is acceptable (some compilers report C++20 for C++23 mode)
    EXPECT_TRUE(true);
#else
    FAIL() << "C++23 or later is not enabled";
#endif
}

TEST(BuildSmokeTest, AllowsGlobalOptionsAfterSubcommand) {
    TempDir tempDir;
    const auto binary = locateCliBinary();
    const auto inputPath = tempDir.path / "reads.fastq";
    const auto outputPath = tempDir.path / "reads.fqc";

    {
        std::ofstream out(inputPath);
        ASSERT_TRUE(out.is_open());
        out << "@r1\nACGT\n+\nIIII\n";
    }

    const std::string command = quote(binary.string()) + " compress -i " + quote(inputPath.string()) +
        " -o " + quote(outputPath.string()) + " -t 1 --no-progress >/dev/null 2>&1";

    EXPECT_EQ(std::system(command.c_str()), 0);
    EXPECT_TRUE(std::filesystem::exists(outputPath));
}

TEST(BuildSmokeTest, SupportsPositionalInfoAndVerify) {
    TempDir tempDir;
    const auto binary = locateCliBinary();
    const auto inputPath = tempDir.path / "reads.fastq";
    const auto archivePath = tempDir.path / "reads.fqc";
    const auto infoPath = tempDir.path / "info.json";
    const auto verifyPath = tempDir.path / "verify.txt";

    {
        std::ofstream out(inputPath);
        ASSERT_TRUE(out.is_open());
        out << "@r1\nACGT\n+\nIIII\n";
    }

    const std::string compressCommand = quote(binary.string()) + " -t 1 --no-progress compress -i " +
        quote(inputPath.string()) + " -o " + quote(archivePath.string()) + " >/dev/null 2>&1";
    ASSERT_EQ(std::system(compressCommand.c_str()), 0);

    const std::string infoCommand =
        quote(binary.string()) + " info --json " + quote(archivePath.string()) + " >" +
        quote(infoPath.string()) + " 2>/dev/null";
    EXPECT_EQ(std::system(infoCommand.c_str()), 0);

    const std::string verifyCommand =
        quote(binary.string()) + " verify " + quote(archivePath.string()) + " >" +
        quote(verifyPath.string()) + " 2>/dev/null";
    EXPECT_EQ(std::system(verifyCommand.c_str()), 0);
}

TEST(BuildSmokeTest, ReportsProjectVersion) {
    TempDir tempDir;
    const auto binary = locateCliBinary();
    const auto versionPath = tempDir.path / "version.txt";

    const std::string command = quote(binary.string()) + " --version >" + quote(versionPath.string()) +
        " 2>/dev/null";

    ASSERT_EQ(std::system(command.c_str()), 0);
    EXPECT_EQ(readFile(versionPath), "0.2.0\n");
}
