// =============================================================================
// fq-compressor - Compression Request Tests
// =============================================================================

#include "fqc/commands/compression_request.h"

#include "fqc/commands/compress_command.h"

#include <filesystem>

#include <gtest/gtest.h>

namespace fqc::commands::test {

TEST(CompressionRequestTest, NormalizesStreamingStdinIntoSingleRequest) {
    CompressOptions options;
    options.inputPath = "-";
    options.outputPath = "out.fqc";
    options.streamingMode = true;
    options.enableReordering = false;
    options.threads = 4;

    const auto request = toCompressionRequest(options);

    EXPECT_EQ(request.mode, CompressionMode::kStreaming);
    EXPECT_EQ(request.input.kind, CompressionInputKind::kStdin);
    EXPECT_FALSE(request.paired);
    EXPECT_EQ(request.outputPath, std::filesystem::path("out.fqc"));
    EXPECT_FALSE(request.enableReordering);
    EXPECT_FALSE(request.saveReorderMap);
}

TEST(CompressionRequestTest, NormalizesPairedFilesWithoutPretendingSingleEnd) {
    CompressOptions options;
    options.inputPath = "r1.fastq";
    options.input2Path = "r2.fastq";
    options.outputPath = "paired.fqc";
    options.enableReordering = false;
    options.saveReorderMap = true;
    options.peLayout = PELayout::kConsecutive;

    const auto request = toCompressionRequest(options);

    EXPECT_EQ(request.mode, CompressionMode::kArchive);
    EXPECT_EQ(request.input.kind, CompressionInputKind::kPairedFiles);
    EXPECT_TRUE(request.paired);
    EXPECT_EQ(request.input.primaryPath, std::filesystem::path("r1.fastq"));
    EXPECT_EQ(request.input.secondaryPath, std::filesystem::path("r2.fastq"));
    EXPECT_EQ(request.input.archiveLayout, PELayout::kConsecutive);
    EXPECT_FALSE(request.enableReordering);
    EXPECT_FALSE(request.saveReorderMap);
}

TEST(CompressionRequestTest, NormalizesInterleavedSingleFileInput) {
    CompressOptions options;
    options.inputPath = "interleaved.fastq";
    options.outputPath = "paired.fqc";
    options.interleaved = true;

    const auto request = toCompressionRequest(options);

    EXPECT_EQ(request.mode, CompressionMode::kArchive);
    EXPECT_EQ(request.input.kind, CompressionInputKind::kInterleavedFile);
    EXPECT_TRUE(request.paired);
    EXPECT_EQ(request.input.primaryPath, std::filesystem::path("interleaved.fastq"));
    EXPECT_TRUE(request.input.secondaryPath.empty());
}

TEST(CompressionRequestTest, NormalizesPlainSingleFileInput) {
    CompressOptions options;
    options.inputPath = "reads.fastq";
    options.outputPath = "reads.fqc";
    options.paired = true;

    const auto request = toCompressionRequest(options);

    EXPECT_EQ(request.mode, CompressionMode::kArchive);
    EXPECT_EQ(request.input.kind, CompressionInputKind::kSingleFile);
    EXPECT_FALSE(request.paired);
    EXPECT_EQ(request.input.primaryPath, std::filesystem::path("reads.fastq"));
    EXPECT_TRUE(request.input.secondaryPath.empty());
}

TEST(CompressionRequestTest, NormalizesStdinInterleavedIntoPairedRequest) {
    CompressOptions options;
    options.inputPath = "-";
    options.outputPath = "paired.fqc";
    options.streamingMode = true;
    options.interleaved = true;

    const auto request = toCompressionRequest(options);

    EXPECT_TRUE(request.paired);
    EXPECT_EQ(request.input.kind, CompressionInputKind::kStdin);
}

TEST(CompressionRequestTest, DisablesReorderMapWhenReorderingIsDisabled) {
    CompressOptions options;
    options.inputPath = "reads.fastq";
    options.outputPath = "reads.fqc";
    options.enableReordering = false;
    options.saveReorderMap = true;

    const auto request = toCompressionRequest(options);

    EXPECT_FALSE(request.enableReordering);
    EXPECT_FALSE(request.saveReorderMap);
}

TEST(CompressionRequestTest, PreservesCommandLayerOptionsInNormalizedRequest) {
    CompressOptions options;
    options.inputPath = "reads.fastq";
    options.outputPath = "reads.fqc";
    options.compressionLevel = 5;
    options.threads = 8;
    options.memoryLimitMb = 1024;
    options.enableReordering = false;
    options.saveReorderMap = false;
    options.forceOverwrite = true;
    options.showProgress = false;
    options.qualityMode = QualityMode::kIllumina8;
    options.idMode = IDMode::kTokenize;
    options.longReadMode = ReadLengthClass::kLong;
    options.autoDetectLongRead = false;
    options.scanAllLengths = true;
    options.blockSize = 4096;
    options.blockSizeExplicit = true;
    options.maxBlockBases = 123456;
    options.inputBytesHint = 987654321;

    const auto request = toCompressionRequest(options);

    EXPECT_EQ(request.outputPath, std::filesystem::path("reads.fqc"));
    EXPECT_EQ(request.compressionLevel, 5);
    EXPECT_EQ(request.threads, 8);
    EXPECT_EQ(request.memoryLimitMb, 1024U);
    EXPECT_FALSE(request.enableReordering);
    EXPECT_FALSE(request.saveReorderMap);
    EXPECT_TRUE(request.forceOverwrite);
    EXPECT_FALSE(request.showProgress);
    EXPECT_EQ(request.qualityMode, QualityMode::kIllumina8);
    EXPECT_EQ(request.idMode, IDMode::kTokenize);
    EXPECT_EQ(request.requestedLengthClass, ReadLengthClass::kLong);
    EXPECT_FALSE(request.autoDetectLongRead);
    EXPECT_TRUE(request.scanAllLengths);
    EXPECT_EQ(request.blockSize, 4096U);
    EXPECT_TRUE(request.blockSizeExplicit);
    EXPECT_EQ(request.maxBlockBases, 123456U);
    EXPECT_EQ(request.inputBytesHint, 987654321U);
}

}  // namespace fqc::commands::test
