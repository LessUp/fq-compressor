// =============================================================================
// fq-compressor - Thread Behavior Tests
// =============================================================================
// Tests that verify thread count doesn't change CLI semantics.
// All options should behave identically regardless of threads=1 or threads>1.
//
// Requirements: 1.6
// =============================================================================

#include "fqc/commands/compress_command.h"
#include "fqc/commands/decompress_command.h"
#include "fqc/common/types.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace fqc;
using namespace fqc::commands;

namespace {

// Helper to create a temporary FASTQ file with test data
std::filesystem::path createTempFastq(const std::vector<std::string>& records) {
    static int counter = 0;
    auto path = std::filesystem::temp_directory_path() /
        ("thread_test_" + std::to_string(counter++) + ".fastq");

    std::ofstream file(path);
    for (const auto& rec : records) {
        file << rec;
    }
    return path;
}

// Helper to read FASTQ file content
std::string readFastqFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

// Test data generator
std::vector<std::string> generateTestReads(int count) {
    std::vector<std::string> reads;
    reads.reserve(count);

    for (int i = 0; i < count; ++i) {
        std::string id = "@read_" + std::to_string(i + 1);
        std::string seq(count * 10 % 100 + 50, 'A');
        std::string qual(seq.length(), 'I');

        reads.push_back(id + "\n" + seq + "\n+\n" + qual + "\n");
    }

    return reads;
}

class ThreadBehaviorTest : public ::testing::Test {
protected:
    void SetUp() override {
        testReads_ = generateTestReads(50);
        inputPath_ = createTempFastq(testReads_);
        archiveSingle_ = std::filesystem::temp_directory_path() / "test_single.fqc";
        archiveMulti_ = std::filesystem::temp_directory_path() / "test_multi.fqc";
        outputSingle_ = std::filesystem::temp_directory_path() / "output_single.fastq";
        outputMulti_ = std::filesystem::temp_directory_path() / "output_multi.fastq";
    }

    void TearDown() override {
        std::filesystem::remove(inputPath_);
        std::filesystem::remove(archiveSingle_);
        std::filesystem::remove(archiveMulti_);
        std::filesystem::remove(outputSingle_);
        std::filesystem::remove(outputMulti_);
    }

    std::vector<std::string> testReads_;
    std::filesystem::path inputPath_;
    std::filesystem::path archiveSingle_;
    std::filesystem::path archiveMulti_;
    std::filesystem::path outputSingle_;
    std::filesystem::path outputMulti_;
};

}  // namespace

// =============================================================================
// Compression Tests: threads=1 vs threads>1 should produce identical archives
// =============================================================================

TEST_F(ThreadBehaviorTest, CompressionProducesIdenticalArchives) {
    // Compress with single thread
    CompressOptions opts1;
    opts1.inputPath = inputPath_;
    opts1.outputPath = archiveSingle_;
    opts1.threads = 1;
    opts1.showProgress = false;

    CompressCommand cmd1(std::move(opts1));
    EXPECT_EQ(cmd1.execute(), 0);

    // Compress with multiple threads
    CompressOptions opts4;
    opts4.inputPath = inputPath_;
    opts4.outputPath = archiveMulti_;
    opts4.threads = 4;
    opts4.showProgress = false;

    CompressCommand cmd4(std::move(opts4));
    EXPECT_EQ(cmd4.execute(), 0);

    // Decompress both and compare
    DecompressOptions decompOpts1;
    decompOpts1.inputPath = archiveSingle_;
    decompOpts1.outputPath = outputSingle_;
    decompOpts1.threads = 1;
    decompOpts1.showProgress = false;

    DecompressCommand decomp1(std::move(decompOpts1));
    EXPECT_EQ(decomp1.execute(), 0);

    DecompressOptions decompOpts4;
    decompOpts4.inputPath = archiveMulti_;
    decompOpts4.outputPath = outputMulti_;
    decompOpts4.threads = 1;
    decompOpts4.showProgress = false;

    DecompressCommand decomp4(std::move(decompOpts4));
    EXPECT_EQ(decomp4.execute(), 0);

    // Outputs should be identical
    auto content1 = readFastqFile(outputSingle_);
    auto content4 = readFastqFile(outputMulti_);
    EXPECT_EQ(content1, content4) << "Single-thread and multi-thread compression should produce "
                                     "identical decompression results";
}

// =============================================================================
// Decompression Tests: threads=1 vs threads>1 should produce identical output
// =============================================================================

TEST_F(ThreadBehaviorTest, DecompressionProducesIdenticalOutput) {
    // First, create a common archive
    CompressOptions compOpts;
    compOpts.inputPath = inputPath_;
    compOpts.outputPath = archiveSingle_;
    compOpts.threads = 4;  // Use multi-thread for archive creation
    compOpts.showProgress = false;

    CompressCommand compCmd(std::move(compOpts));
    EXPECT_EQ(compCmd.execute(), 0);

    // Decompress with single thread
    DecompressOptions decompOpts1;
    decompOpts1.inputPath = archiveSingle_;
    decompOpts1.outputPath = outputSingle_;
    decompOpts1.threads = 1;
    decompOpts1.showProgress = false;

    DecompressCommand decomp1(std::move(decompOpts1));
    EXPECT_EQ(decomp1.execute(), 0);

    // Decompress with multiple threads
    DecompressOptions decompOpts4;
    decompOpts4.inputPath = archiveSingle_;
    decompOpts4.outputPath = outputMulti_;
    decompOpts4.threads = 4;
    decompOpts4.showProgress = false;

    DecompressCommand decomp4(std::move(decompOpts4));
    EXPECT_EQ(decomp4.execute(), 0);

    // Outputs should be identical
    auto content1 = readFastqFile(outputSingle_);
    auto content4 = readFastqFile(outputMulti_);
    EXPECT_EQ(content1, content4) << "Single-thread and multi-thread decompression should produce "
                                     "identical output";
}

// =============================================================================
// Range Extraction Tests: --range should work identically
// =============================================================================

TEST_F(ThreadBehaviorTest, RangeExtractionWorksIdentically) {
    // Create archive
    CompressOptions compOpts;
    compOpts.inputPath = inputPath_;
    compOpts.outputPath = archiveSingle_;
    compOpts.threads = 4;
    compOpts.showProgress = false;

    CompressCommand compCmd(std::move(compOpts));
    EXPECT_EQ(compCmd.execute(), 0);

    // Extract range with single thread
    DecompressOptions opts1;
    opts1.inputPath = archiveSingle_;
    opts1.outputPath = outputSingle_;
    opts1.threads = 1;
    opts1.range = ReadRange{10, 20};  // Extract reads 10-20
    opts1.showProgress = false;

    DecompressCommand cmd1(std::move(opts1));
    EXPECT_EQ(cmd1.execute(), 0);

    // Extract same range with multiple threads
    DecompressOptions opts4;
    opts4.inputPath = archiveSingle_;
    opts4.outputPath = outputMulti_;
    opts4.threads = 4;
    opts4.range = ReadRange{10, 20};
    opts4.showProgress = false;

    DecompressCommand cmd4(std::move(opts4));
    EXPECT_EQ(cmd4.execute(), 0);

    // Outputs should be identical
    auto content1 = readFastqFile(outputSingle_);
    auto content4 = readFastqFile(outputMulti_);
    EXPECT_EQ(content1, content4)
        << "Range extraction should be identical regardless of thread count";

    // Verify correct reads extracted
    EXPECT_NE(content1.find("read_10"), std::string::npos);
    EXPECT_NE(content1.find("read_20"), std::string::npos);
    EXPECT_EQ(content1.find("read_9"), std::string::npos);
    EXPECT_EQ(content1.find("read_21"), std::string::npos);
}

// =============================================================================
// Header-Only Tests: --header-only should work identically
// =============================================================================

TEST_F(ThreadBehaviorTest, HeaderOnlyWorksIdentically) {
    // Create archive
    CompressOptions compOpts;
    compOpts.inputPath = inputPath_;
    compOpts.outputPath = archiveSingle_;
    compOpts.threads = 4;
    compOpts.showProgress = false;

    CompressCommand compCmd(std::move(compOpts));
    EXPECT_EQ(compCmd.execute(), 0);

    // Extract headers only with single thread
    DecompressOptions opts1;
    opts1.inputPath = archiveSingle_;
    opts1.outputPath = outputSingle_;
    opts1.threads = 1;
    opts1.headerOnly = true;
    opts1.showProgress = false;

    DecompressCommand cmd1(std::move(opts1));
    EXPECT_EQ(cmd1.execute(), 0);

    // Extract headers only with multiple threads
    DecompressOptions opts4;
    opts4.inputPath = archiveSingle_;
    opts4.outputPath = outputMulti_;
    opts4.threads = 4;
    opts4.headerOnly = true;
    opts4.showProgress = false;

    DecompressCommand cmd4(std::move(opts4));
    EXPECT_EQ(cmd4.execute(), 0);

    // Outputs should be identical
    auto content1 = readFastqFile(outputSingle_);
    auto content4 = readFastqFile(outputMulti_);
    EXPECT_EQ(content1, content4) << "Header-only extraction should be identical";

    // Verify only headers extracted (no sequence/quality)
    EXPECT_NE(content1.find("@read_1"), std::string::npos);
    EXPECT_EQ(content1.find("+\n"), std::string::npos);  // No + lines
}

// =============================================================================
// Statistics Tests: thread count should not affect stats
// =============================================================================

TEST_F(ThreadBehaviorTest, StatisticsAreConsistent) {
    // Create archive
    CompressOptions compOpts;
    compOpts.inputPath = inputPath_;
    compOpts.outputPath = archiveSingle_;
    compOpts.threads = 4;
    compOpts.showProgress = false;

    CompressCommand compCmd(std::move(compOpts));
    ASSERT_EQ(compCmd.execute(), 0);

    // Decompress with single thread
    DecompressOptions opts1;
    opts1.inputPath = archiveSingle_;
    opts1.outputPath = outputSingle_;
    opts1.threads = 1;
    opts1.showProgress = false;

    DecompressCommand cmd1(std::move(opts1));
    ASSERT_EQ(cmd1.execute(), 0);

    // Decompress with multiple threads
    DecompressOptions opts4;
    opts4.inputPath = archiveSingle_;
    opts4.outputPath = outputMulti_;
    opts4.threads = 4;
    opts4.showProgress = false;

    DecompressCommand cmd4(std::move(opts4));
    ASSERT_EQ(cmd4.execute(), 0);

    // Stats should be identical
    const auto& stats1 = cmd1.stats();
    const auto& stats4 = cmd4.stats();

    EXPECT_EQ(stats1.totalReads, stats4.totalReads);
    EXPECT_EQ(stats1.totalBases, stats4.totalBases);
    EXPECT_EQ(stats1.blocksProcessed, stats4.blocksProcessed);
    EXPECT_EQ(stats1.corruptedBlocks, stats4.corruptedBlocks);
}
