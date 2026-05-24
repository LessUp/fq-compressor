// =============================================================================
// fq-compressor - Compression Profile Tests
// =============================================================================

#include "fqc/commands/compression_profile.h"

#include "fqc/commands/compress_command.h"
#include "fqc/io/stream_factory.h"
#include "fqc/pipeline/pipeline.h"

#include <memory>
#include <sstream>

#include <gtest/gtest.h>

namespace fqc::commands::test {

namespace {

class OutputExistsOnlyFactory : public io::StreamFactory {
public:
    explicit OutputExistsOnlyFactory(std::string inputContent)
        : inputContent_(std::move(inputContent)) {}

    [[nodiscard]] auto createInputStream(const std::filesystem::path& path)
        -> std::unique_ptr<std::istream> override {
        if (path != "reads.fastq") {
            throw IOError("Unexpected input path: " + path.string());
        }

        return std::make_unique<std::istringstream>(inputContent_);
    }

    [[nodiscard]] auto createOutputStream(
        const std::filesystem::path& path,
        io::CompressionFormat format = io::CompressionFormat::kNone)
        -> std::unique_ptr<std::ostream> override {
        (void)path;
        (void)format;
        return std::make_unique<std::ostringstream>();
    }

    [[nodiscard]] auto detectCompression(const std::filesystem::path& path) const
        -> io::CompressionFormat override {
        return io::detectCompressionFormatFromExtension(path);
    }

    [[nodiscard]] auto outputExists(const std::filesystem::path& path) const -> bool override {
        return outputExists_ && path == "out.fqc";
    }

    auto setOutputExists(bool outputExists) -> void {
        outputExists_ = outputExists;
    }

private:
    std::string inputContent_;
    bool outputExists_ = false;
};

}  // namespace

TEST(CompressionProfileTest, DerivesStreamingDefaultsAndConfigs) {
    auto factory = std::make_shared<io::MemoryStreamFactory>();

    CompressOptions options;
    options.inputPath = "-";
    options.outputPath = "out.fqc";
    options.compressionLevel = 6;
    options.enableReordering = true;
    options.saveReorderMap = true;
    options.streamingMode = false;
    options.threads = 1;
    options.autoDetectLongRead = true;

    auto profileResult = buildCompressionProfile(options, factory);

    ASSERT_TRUE(profileResult.has_value());
    const auto& profile = profileResult.value();
    EXPECT_TRUE(profile.streamingMode());
    EXPECT_FALSE(profile.enableReordering());
    EXPECT_FALSE(profile.saveReorderMap());
    EXPECT_EQ(profile.readLengthClass(), ReadLengthClass::kMedium);
    EXPECT_EQ(profile.executionMode(), CompressionExecutionMode::kSingleThread);
    EXPECT_TRUE(profile.blockCompressorConfig().validate().has_value());
    EXPECT_TRUE(profile.pipelineConfig().validate().has_value());
}

TEST(CompressionProfileTest, DetectsReadLengthClassFromInputAndChoosesPipeline) {
    auto factory = std::make_shared<io::MemoryStreamFactory>();
    factory->setFileContent("reads.fastq",
                            "@r1\nACGTACGTACGT\n+\nFFFFFFFFFFFF\n"
                            "@r2\nTGCATGCATGCA\n+\nHHHHHHHHHHHH\n");

    CompressOptions options;
    options.inputPath = "reads.fastq";
    options.outputPath = "out.fqc";
    options.blockSize = 100000;
    options.compressionLevel = 5;
    options.threads = 4;
    options.enableReordering = true;
    options.saveReorderMap = true;
    options.longReadMode = ReadLengthClass::kLong;
    options.autoDetectLongRead = true;

    auto profileResult = buildCompressionProfile(options, factory);

    ASSERT_TRUE(profileResult.has_value());
    const auto& profile = profileResult.value();
    EXPECT_EQ(profile.readLengthClass(), ReadLengthClass::kShort);
    EXPECT_EQ(profile.executionMode(), CompressionExecutionMode::kPipeline);
    EXPECT_TRUE(profile.archivePreservesOrder());
    EXPECT_FALSE(profile.archiveHasReorderMap());
    EXPECT_TRUE(profile.pipelineConfig().archivePreservesOrder);
    EXPECT_FALSE(profile.pipelineConfig().archiveHasReorderMap);
}

TEST(CompressionProfileTest, RejectsExistingOutputReportedByInjectedFactory) {
    auto factory = std::make_shared<OutputExistsOnlyFactory>(
        "@r1\nACGT\n+\nFFFF\n"
        "@r2\nTGCA\n+\nHHHH\n");
    factory->setOutputExists(true);

    CompressOptions options;
    options.inputPath = "reads.fastq";
    options.outputPath = "out.fqc";
    options.compressionLevel = 5;
    options.threads = 1;
    options.enableReordering = true;
    options.autoDetectLongRead = false;
    options.longReadMode = ReadLengthClass::kShort;
    options.forceOverwrite = false;

    auto profileResult = buildCompressionProfile(options, factory);

    ASSERT_FALSE(profileResult.has_value());
    EXPECT_EQ(profileResult.error().code(), ErrorCode::kIOError);
    EXPECT_NE(profileResult.error().message().find("Output file already exists"),
              std::string::npos);
}

TEST(CompressionProfileTest, PreservesExplicitLengthClassWhenStreamingModeSkipsDetection) {
    auto factory = std::make_shared<io::MemoryStreamFactory>();
    factory->setFileContent("reads.fastq",
                            "@r1\nACGTACGTACGT\n+\nFFFFFFFFFFFF\n"
                            "@r2\nTGCATGCATGCA\n+\nHHHHHHHHHHHH\n");

    CompressOptions options;
    options.inputPath = "reads.fastq";
    options.outputPath = "out.fqc";
    options.streamingMode = true;
    options.enableReordering = true;
    options.saveReorderMap = true;
    options.autoDetectLongRead = true;
    options.longReadMode = ReadLengthClass::kLong;
    options.threads = 1;

    auto profileResult = buildCompressionProfile(options, factory);

    ASSERT_TRUE(profileResult.has_value());
    const auto& profile = profileResult.value();
    EXPECT_TRUE(profile.streamingMode());
    EXPECT_FALSE(profile.enableReordering());
    EXPECT_FALSE(profile.saveReorderMap());
    EXPECT_EQ(profile.readLengthClass(), ReadLengthClass::kLong);
}

TEST(CompressionProfileTest, PreservesExplicitLengthClassForStdinWhenAutoDetectDisabled) {
    auto factory = std::make_shared<io::MemoryStreamFactory>();

    CompressOptions options;
    options.inputPath = "-";
    options.outputPath = "out.fqc";
    options.autoDetectLongRead = false;
    options.longReadMode = ReadLengthClass::kLong;
    options.threads = 1;

    auto profileResult = buildCompressionProfile(options, factory);

    ASSERT_TRUE(profileResult.has_value());
    const auto& profile = profileResult.value();
    EXPECT_TRUE(profile.streamingMode());
    EXPECT_EQ(profile.readLengthClass(), ReadLengthClass::kLong);
}

TEST(CompressionProfileTest, LargeSingleEndShortReadsPreferPipelineAndDisableReordering) {
    auto factory = std::make_shared<io::MemoryStreamFactory>();
    factory->setFileContent("reads.fastq", "@r1\nACGT\n+\nFFFF\n");

    CompressOptions options;
    options.inputPath = "reads.fastq";
    options.outputPath = "out.fqc";
    options.threads = 1;
    options.enableReordering = true;
    options.saveReorderMap = true;
    options.autoDetectLongRead = false;
    options.longReadMode = ReadLengthClass::kShort;
    options.inputBytesHint = 128ULL * 1024ULL * 1024ULL;

    auto profileResult = buildCompressionProfile(options, factory);

    ASSERT_TRUE(profileResult.has_value());
    const auto& profile = profileResult.value();
    EXPECT_EQ(profile.executionMode(), CompressionExecutionMode::kPipeline);
    EXPECT_FALSE(profile.enableReordering());
    EXPECT_FALSE(profile.saveReorderMap());
    EXPECT_TRUE(profile.archivePreservesOrder());
    EXPECT_FALSE(profile.archiveHasReorderMap());
}

TEST(CompressionProfileTest, BuildsEffectivePlanFromNormalizedRequest) {
    auto factory = std::make_shared<io::MemoryStreamFactory>();
    factory->setFileContent("reads.fastq",
                            "@r1\nACGT\n+\nFFFF\n"
                            "@r2\nTGCA\n+\nHHHH\n");

    CompressionRequest request;
    request.input.kind = CompressionInputKind::kSingleFile;
    request.input.primaryPath = "reads.fastq";
    request.outputPath = "out.fqc";
    request.enableReordering = true;
    request.saveReorderMap = true;
    request.threads = 4;
    request.autoDetectLongRead = true;
    request.requestedLengthClass = ReadLengthClass::kLong;

    auto planResult = buildCompressionProfilePlan(request, factory);

    ASSERT_TRUE(planResult.has_value());
    const auto& plan = planResult.value();
    EXPECT_EQ(plan.request.mode, CompressionMode::kPipeline);
    EXPECT_EQ(plan.request.input.kind, CompressionInputKind::kSingleFile);
    EXPECT_EQ(plan.profile.readLengthClass(), ReadLengthClass::kShort);
    EXPECT_EQ(plan.profile.executionMode(), CompressionExecutionMode::kPipeline);
    EXPECT_TRUE(plan.profile.archivePreservesOrder());
    EXPECT_FALSE(plan.profile.archiveHasReorderMap());
}

}  // namespace fqc::commands::test
