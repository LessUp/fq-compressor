// =============================================================================
// fq-compressor - Compressed Stream Unit Tests
// =============================================================================
// Unit tests for transparent compression/decompression stream support.
// Tests gzip, bzip2, xz, and uncompressed formats.
// =============================================================================

#include "fqc/io/compressed_stream.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <vector>

#include <gtest/gtest.h>

namespace fqc::io {
namespace {

// =============================================================================
// Test Fixtures
// =============================================================================

class CompressedStreamTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir_ = std::filesystem::temp_directory_path() / "fqc_compressed_stream_test";
        std::filesystem::create_directories(tempDir_);

        // Generate test data
        std::mt19937 rng(42);
        std::uniform_int_distribution<char> dist('A', 'Z');
        testData_.resize(4096);
        for (auto& c : testData_) {
            c = dist(rng);
        }
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir_);
    }

    std::filesystem::path tempDir_;
    std::string testData_;

    // Helper to write test data to a file
    void writeTestFile(const std::filesystem::path& path, const std::string& data) {
        std::ofstream file(path, std::ios::binary);
        file << data;
    }

    // Helper to read entire file content
    std::string readFile(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

// =============================================================================
// Compression Format Detection Tests
// =============================================================================

TEST_F(CompressedStreamTest, DetectNoneFormat) {
    // Plain text doesn't match any compression magic
    std::string plain = "Hello, World!";
    auto format = detectCompressionFormat(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(plain.data()), plain.size()));
    EXPECT_EQ(format, CompressionFormat::kNone);
}

TEST_F(CompressedStreamTest, DetectGzipMagic) {
    // gzip magic bytes: 0x1f 0x8b
    std::uint8_t gzipMagic[] = {0x1f, 0x8b, 0x08, 0x00};
    auto format = detectCompressionFormat(gzipMagic);
    EXPECT_EQ(format, CompressionFormat::kGzip);
}

TEST_F(CompressedStreamTest, DetectBzip2Magic) {
    // bzip2 magic: "BZ"
    std::uint8_t bzip2Magic[] = {0x42, 0x5a, 0x68, 0x39};  // "BZh9"
    auto format = detectCompressionFormat(bzip2Magic);
    EXPECT_EQ(format, CompressionFormat::kBzip2);
}

TEST_F(CompressedStreamTest, DetectXzMagic) {
    // xz magic: 0xFD "7zXZ"
    std::uint8_t xzMagic[] = {0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00};
    auto format = detectCompressionFormat(xzMagic);
    EXPECT_EQ(format, CompressionFormat::kXz);
}

TEST_F(CompressedStreamTest, DetectFromExtension) {
    EXPECT_EQ(detectCompressionFormatFromExtension("file.gz"), CompressionFormat::kGzip);
    EXPECT_EQ(detectCompressionFormatFromExtension("file.GZ"), CompressionFormat::kGzip);
    EXPECT_EQ(detectCompressionFormatFromExtension("file.bz2"), CompressionFormat::kBzip2);
    EXPECT_EQ(detectCompressionFormatFromExtension("file.BZ2"), CompressionFormat::kBzip2);
    EXPECT_EQ(detectCompressionFormatFromExtension("file.xz"), CompressionFormat::kXz);
    EXPECT_EQ(detectCompressionFormatFromExtension("file.XZ"), CompressionFormat::kXz);
    EXPECT_EQ(detectCompressionFormatFromExtension("file.fastq"), CompressionFormat::kNone);
    EXPECT_EQ(detectCompressionFormatFromExtension("file.txt"), CompressionFormat::kNone);
}

TEST_F(CompressedStreamTest, FormatExtension) {
    EXPECT_EQ(compressionFormatExtension(CompressionFormat::kGzip), ".gz");
    EXPECT_EQ(compressionFormatExtension(CompressionFormat::kBzip2), ".bz2");
    EXPECT_EQ(compressionFormatExtension(CompressionFormat::kXz), ".xz");
    EXPECT_EQ(compressionFormatExtension(CompressionFormat::kNone), "");
}

TEST_F(CompressedStreamTest, FormatName) {
    EXPECT_EQ(compressionFormatName(CompressionFormat::kGzip), "gzip");
    EXPECT_EQ(compressionFormatName(CompressionFormat::kBzip2), "bzip2");
    EXPECT_EQ(compressionFormatName(CompressionFormat::kXz), "xz");
    EXPECT_EQ(compressionFormatName(CompressionFormat::kNone), "none");
}

// =============================================================================
// Uncompressed File Tests
// =============================================================================

TEST_F(CompressedStreamTest, ReadUncompressedFile) {
    auto path = tempDir_ / "test.txt";
    writeTestFile(path, testData_);

    CompressedInputStream stream(path);
    EXPECT_EQ(stream.format(), CompressionFormat::kNone);
    EXPECT_FALSE(stream.isCompressed());

    std::stringstream buffer;
    buffer << stream.rdbuf();
    EXPECT_EQ(buffer.str(), testData_);
}

TEST_F(CompressedStreamTest, OpenCompressedFileUncompressed) {
    auto path = tempDir_ / "test.txt";
    writeTestFile(path, testData_);

    auto stream = openCompressedFile(path);
    std::stringstream buffer;
    buffer << stream->rdbuf();
    EXPECT_EQ(buffer.str(), testData_);
}

// =============================================================================
// Gzip Tests
// =============================================================================

TEST_F(CompressedStreamTest, ReadGzipFile) {
    auto compressedPath = tempDir_ / "test.gz";

    // Create gzip file using system command
    {
        auto plainPath = tempDir_ / "test_plain.txt";
        writeTestFile(plainPath, testData_);
        std::string cmd = "gzip -c " + plainPath.string() + " > " + compressedPath.string();
        ASSERT_EQ(system(cmd.c_str()), 0);
    }

    CompressedInputStream stream(compressedPath);
    EXPECT_EQ(stream.format(), CompressionFormat::kGzip);
    EXPECT_TRUE(stream.isCompressed());

    std::stringstream buffer;
    buffer << stream.rdbuf();
    EXPECT_EQ(buffer.str(), testData_);
}

TEST_F(CompressedStreamTest, OpenCompressedFileGzip) {
    auto compressedPath = tempDir_ / "test.gz";

    {
        auto plainPath = tempDir_ / "test_plain.txt";
        writeTestFile(plainPath, testData_);
        std::string cmd = "gzip -c " + plainPath.string() + " > " + compressedPath.string();
        ASSERT_EQ(system(cmd.c_str()), 0);
    }

    auto stream = openCompressedFile(compressedPath);
    std::stringstream buffer;
    buffer << stream->rdbuf();
    EXPECT_EQ(buffer.str(), testData_);
}

// =============================================================================
// Bzip2 Tests
// =============================================================================

TEST_F(CompressedStreamTest, ReadBzip2File) {
    auto compressedPath = tempDir_ / "test.bz2";

    // Create bzip2 file using system command
    {
        auto plainPath = tempDir_ / "test_plain.txt";
        writeTestFile(plainPath, testData_);
        std::string cmd = "bzip2 -c " + plainPath.string() + " > " + compressedPath.string();
        // Skip if bzip2 not available
        if (system("which bzip2 > /dev/null 2>&1") != 0) {
            GTEST_SKIP() << "bzip2 not available";
        }
        ASSERT_EQ(system(cmd.c_str()), 0);
    }

    CompressedInputStream stream(compressedPath);
    EXPECT_EQ(stream.format(), CompressionFormat::kBzip2);
    EXPECT_TRUE(stream.isCompressed());

    std::stringstream buffer;
    buffer << stream.rdbuf();
    EXPECT_EQ(buffer.str(), testData_);
}

// =============================================================================
// Xz Tests
// =============================================================================

TEST_F(CompressedStreamTest, ReadXzFile) {
    auto compressedPath = tempDir_ / "test.xz";

    // Create xz file using system command
    {
        auto plainPath = tempDir_ / "test_plain.txt";
        writeTestFile(plainPath, testData_);
        std::string cmd = "xz -c " + plainPath.string() + " > " + compressedPath.string();
        ASSERT_EQ(system(cmd.c_str()), 0);
    }

    CompressedInputStream stream(compressedPath);
    EXPECT_EQ(stream.format(), CompressionFormat::kXz);
    EXPECT_TRUE(stream.isCompressed());

    std::stringstream buffer;
    buffer << stream.rdbuf();
    EXPECT_EQ(buffer.str(), testData_);
}

// =============================================================================
// GzipStreamBuf Tests
// =============================================================================

class GzipStreamBufTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir_ = std::filesystem::temp_directory_path() / "fqc_gzip_streambuf_test";
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir_);
    }

    std::filesystem::path tempDir_;
};

TEST_F(GzipStreamBufTest, DecompressGzipStream) {
    auto compressedPath = tempDir_ / "test.gz";
    std::string testData(1024, 'X');

    // Create gzip file
    {
        auto plainPath = tempDir_ / "test_plain.txt";
        std::ofstream file(plainPath);
        file << testData;
        std::string cmd = "gzip -c " + plainPath.string() + " > " + compressedPath.string();
        ASSERT_EQ(system(cmd.c_str()), 0);
    }

    std::ifstream compressedFile(compressedPath, std::ios::binary);
    ASSERT_TRUE(compressedFile.is_open());

    GzipStreamBuf streamBuf(compressedFile);
    std::istream stream(&streamBuf);

    std::stringstream buffer;
    buffer << stream.rdbuf();
    EXPECT_EQ(buffer.str(), testData);
}

TEST_F(GzipStreamBufTest, MoveSemantics) {
    auto compressedPath = tempDir_ / "test.gz";
    std::string testData(512, 'A');

    {
        auto plainPath = tempDir_ / "test_plain.txt";
        std::ofstream file(plainPath);
        file << testData;
        std::string cmd = "gzip -c " + plainPath.string() + " > " + compressedPath.string();
        ASSERT_EQ(system(cmd.c_str()), 0);
    }

    std::ifstream compressedFile(compressedPath, std::ios::binary);
    GzipStreamBuf streamBuf1(compressedFile);

    // Move construct
    GzipStreamBuf streamBuf2(std::move(streamBuf1));

    // Move assign
    GzipStreamBuf streamBuf3;
    streamBuf3 = std::move(streamBuf2);

    std::istream stream(&streamBuf3);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    EXPECT_EQ(buffer.str(), testData);
}

// =============================================================================
// XzStreamBuf Tests
// =============================================================================

class XzStreamBufTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir_ = std::filesystem::temp_directory_path() / "fqc_xz_streambuf_test";
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir_);
    }

    std::filesystem::path tempDir_;
};

TEST_F(XzStreamBufTest, DecompressXzStream) {
    auto compressedPath = tempDir_ / "test.xz";
    std::string testData(1024, 'Y');

    {
        auto plainPath = tempDir_ / "test_plain.txt";
        std::ofstream file(plainPath);
        file << testData;
        std::string cmd = "xz -c " + plainPath.string() + " > " + compressedPath.string();
        ASSERT_EQ(system(cmd.c_str()), 0);
    }

    std::ifstream compressedFile(compressedPath, std::ios::binary);
    ASSERT_TRUE(compressedFile.is_open());

    XzStreamBuf streamBuf(compressedFile);
    std::istream stream(&streamBuf);

    std::stringstream buffer;
    buffer << stream.rdbuf();
    EXPECT_EQ(buffer.str(), testData);
}

// =============================================================================
// Compression Support Query Tests
// =============================================================================

TEST_F(CompressedStreamTest, IsCompressionSupported) {
    EXPECT_TRUE(isCompressionSupported(CompressionFormat::kNone));
    EXPECT_TRUE(isCompressionSupported(CompressionFormat::kGzip));
    EXPECT_TRUE(isCompressionSupported(CompressionFormat::kBzip2));
    EXPECT_TRUE(isCompressionSupported(CompressionFormat::kXz));
    // Zstd is noted as not yet supported in header
    EXPECT_FALSE(isCompressionSupported(CompressionFormat::kZstd));
}

TEST_F(CompressedStreamTest, SupportedFormats) {
    auto formats = supportedCompressionFormats();
    EXPECT_FALSE(formats.empty());
    EXPECT_TRUE(std::find(formats.begin(), formats.end(), CompressionFormat::kGzip) !=
                formats.end());
    EXPECT_TRUE(std::find(formats.begin(), formats.end(), CompressionFormat::kBzip2) !=
                formats.end());
    EXPECT_TRUE(std::find(formats.begin(), formats.end(), CompressionFormat::kXz) != formats.end());
}

// =============================================================================
// Edge Cases and Error Handling
// =============================================================================

TEST_F(CompressedStreamTest, EmptyFile) {
    auto path = tempDir_ / "empty.txt";
    writeTestFile(path, "");

    CompressedInputStream stream(path);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    EXPECT_TRUE(buffer.str().empty());
}

TEST_F(CompressedStreamTest, EmptyGzipFile) {
    auto compressedPath = tempDir_ / "empty.gz";

    {
        auto plainPath = tempDir_ / "empty.txt";
        writeTestFile(plainPath, "");
        std::string cmd = "gzip -c " + plainPath.string() + " > " + compressedPath.string();
        ASSERT_EQ(system(cmd.c_str()), 0);
    }

    CompressedInputStream stream(compressedPath);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    EXPECT_TRUE(buffer.str().empty());
}

TEST_F(CompressedStreamTest, LargeFile) {
    // Test with larger data to exercise buffer handling
    std::string largeData(1024 * 1024, 'L');  // 1MB
    auto compressedPath = tempDir_ / "large.gz";

    {
        auto plainPath = tempDir_ / "large.txt";
        writeTestFile(plainPath, largeData);
        std::string cmd = "gzip -c " + plainPath.string() + " > " + compressedPath.string();
        ASSERT_EQ(system(cmd.c_str()), 0);
    }

    CompressedInputStream stream(compressedPath);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    EXPECT_EQ(buffer.str(), largeData);
}

TEST_F(CompressedStreamTest, NonexistentFile) {
    EXPECT_THROW(CompressedInputStream stream(tempDir_ / "nonexistent.txt"), IOError);
}

TEST_F(CompressedStreamTest, InvalidGzipData) {
    auto path = tempDir_ / "invalid.gz";
    std::ofstream file(path, std::ios::binary);
    // Write invalid gzip data (looks like gzip but isn't)
    file << "\x1f\x8b\x08\x00";
    file << "this is not valid gzip compressed data";

    CompressedInputStream stream(path);
    std::stringstream buffer;
    // Reading should either fail or throw
    EXPECT_TRUE(stream.bad() || !stream.good());
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST_F(CompressedStreamTest, RoundTripAllFormats) {
    std::vector<std::pair<std::string, std::string>> formats = {
        {"gzip", ".gz"}, {"xz", ".xz"},
        // bzip2 is optional in CI
    };

    for (const auto& [formatName, ext] : formats) {
        auto compressedPath = tempDir_ / ("test_" + formatName + ext);

        {
            auto plainPath = tempDir_ / "test_plain.txt";
            writeTestFile(plainPath, testData_);
            std::string cmd =
                formatName + " -c " + plainPath.string() + " > " + compressedPath.string();
            if (system(("which " + formatName + " > /dev/null 2>&1").c_str()) != 0) {
                continue;  // Skip if compressor not available
            }
            ASSERT_EQ(system(cmd.c_str()), 0);
        }

        auto stream = openCompressedFile(compressedPath);
        std::stringstream buffer;
        buffer << stream->rdbuf();
        EXPECT_EQ(buffer.str(), testData_) << "Failed for format: " << formatName;
    }
}

}  // namespace
}  // namespace fqc::io
