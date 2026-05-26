// =============================================================================
// fq-compressor - Compression Engine Tests
// =============================================================================

#include "fqc/commands/compression_engine.h"

#include "fqc/io/stream_factory.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <gtest/gtest.h>

namespace fqc::commands::test {

namespace {

TEST(CompressionEngineTest, PlansEffectivePipelineRequestFromNormalizedInput) {
    auto factory = std::make_shared<io::MemoryStreamFactory>();
    factory->setFileContent("reads.fastq",
                            "@r1\nACGTACGTACGT\n+\nFFFFFFFFFFFF\n"
                            "@r2\nTGCATGCATGCA\n+\nHHHHHHHHHHHH\n");

    CompressionRequest request;
    request.input.kind = CompressionInputKind::kSingleFile;
    request.input.primaryPath = "reads.fastq";
    request.outputPath = "out.fqc";
    request.enableReordering = true;
    request.saveReorderMap = true;
    request.threads = 4;
    request.autoDetectLongRead = true;
    request.requestedLengthClass = ReadLengthClass::kLong;

    CompressionEngine engine(factory);
    auto planResult = engine.plan(request);

    ASSERT_TRUE(planResult.has_value());
    const auto& plan = planResult.value();
    EXPECT_EQ(plan.request.mode, CompressionMode::kPipeline);
    EXPECT_EQ(plan.request.input.kind, CompressionInputKind::kSingleFile);
    EXPECT_EQ(plan.profile.readLengthClass(), ReadLengthClass::kShort);
    EXPECT_EQ(plan.profile.executionMode(), CompressionExecutionMode::kPipeline);
    EXPECT_TRUE(plan.profile.archivePreservesOrder());
    EXPECT_FALSE(plan.profile.archiveHasReorderMap());
    EXPECT_TRUE(plan.request.enableReordering);
    EXPECT_TRUE(plan.request.saveReorderMap);
}

TEST(CompressionEngineTest, ExecutesSingleThreadArchivePlanAndReportsStats) {
    const auto artifactDir = std::filesystem::current_path() / ".test-artifacts";
    std::filesystem::create_directories(artifactDir);

    const auto inputPath = artifactDir / "compression_engine_input.fastq";
    const auto outputPath = artifactDir / "compression_engine_output.fqc";
    std::error_code ec;
    std::filesystem::remove(inputPath, ec);
    std::filesystem::remove(outputPath, ec);
    std::filesystem::remove(outputPath.string() + ".tmp", ec);

    {
        std::ofstream out(inputPath);
        ASSERT_TRUE(out.is_open());
        out << "@r1\nACGT\n+\nFFFF\n";
        out << "@r2\nTGCA\n+\nHHHH\n";
    }

    CompressionRequest request;
    request.input.kind = CompressionInputKind::kSingleFile;
    request.input.primaryPath = inputPath;
    request.outputPath = outputPath;
    request.forceOverwrite = true;
    request.showProgress = false;
    request.enableReordering = false;
    request.saveReorderMap = false;
    request.autoDetectLongRead = false;
    request.requestedLengthClass = ReadLengthClass::kShort;
    request.threads = 1;
    request.blockSize = 2;
    request.blockSizeExplicit = true;

    CompressionEngine engine;
    auto statsResult = engine.execute(request);

    ASSERT_TRUE(statsResult.has_value());
    EXPECT_EQ(statsResult->totalReads, 2u);
    EXPECT_EQ(statsResult->totalBases, 8u);
    EXPECT_GT(statsResult->inputBytes, 0u);
    EXPECT_GT(statsResult->outputBytes, 0u);
    EXPECT_EQ(statsResult->outputBytes, std::filesystem::file_size(outputPath));
    EXPECT_EQ(statsResult->blocksWritten, 1u);
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::filesystem::remove(inputPath, ec);
    std::filesystem::remove(outputPath, ec);
    std::filesystem::remove(outputPath.string() + ".tmp", ec);
}

TEST(CompressionEngineTest, ExecutesSingleThreadUsingInjectedStreamFactoryInput) {
    auto factory = std::make_shared<io::MemoryStreamFactory>();
    factory->setFileContent("reads.fastq", "@r1\nACGT\n+\nFFFF\n@r2\nTGCA\n+\nHHHH\n");

    const auto artifactDir = std::filesystem::current_path() / ".test-artifacts";
    std::filesystem::create_directories(artifactDir);
    const auto outputPath = artifactDir / "compression_engine_factory_output.fqc";

    std::error_code ec;
    std::filesystem::remove(outputPath, ec);
    std::filesystem::remove(outputPath.string() + ".tmp", ec);

    CompressionRequest request;
    request.input.kind = CompressionInputKind::kSingleFile;
    request.input.primaryPath = "reads.fastq";
    request.outputPath = outputPath;
    request.forceOverwrite = true;
    request.showProgress = false;
    request.enableReordering = false;
    request.saveReorderMap = false;
    request.autoDetectLongRead = false;
    request.requestedLengthClass = ReadLengthClass::kShort;
    request.threads = 1;
    request.blockSize = 2;
    request.blockSizeExplicit = true;

    CompressionEngine engine(factory);
    auto statsResult = engine.execute(request);

    ASSERT_TRUE(statsResult.has_value());
    EXPECT_EQ(statsResult->totalReads, 2u);
    EXPECT_EQ(statsResult->totalBases, 8u);
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::filesystem::remove(outputPath, ec);
    std::filesystem::remove(outputPath.string() + ".tmp", ec);
}

}  // namespace

}  // namespace fqc::commands::test
