// =============================================================================
// fq-compressor - Compression Pipeline Integration Tests
// =============================================================================

#include "fqc/format/archive.h"
#include "fqc/pipeline/compress_pipeline.h"

#include <sstream>
#include <string>

#include <gtest/gtest.h>

using fqc::format::ArchiveWriter;
using fqc::pipeline::CompressPipeline;

namespace {

[[nodiscard]] auto makeFastq(int count) -> std::string {
    std::string fastq;
    for (int i = 0; i < count; ++i) {
        fastq += "@read" + std::to_string(i) + " comment\n";
        fastq += "ACGTACGT\n";
        fastq += "+\n";
        fastq += "IIIIIIII\n";
    }
    return fastq;
}

}  // namespace

TEST(CompressPipelineTest, BasicRun) {
    constexpr int kRecordCount = 8;
    std::istringstream input(makeFastq(kRecordCount));
    std::ostringstream output;

    ArchiveWriter writer(output, {.profile = fqc::format::DatasetProfile::kIllumina});
    CompressPipeline pipeline(4096);

    auto result = pipeline.run(input, writer);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->recordCount, kRecordCount);
    EXPECT_GE(result->frameCount, 1U);
    EXPECT_FALSE(output.str().empty());
}

TEST(CompressPipelineTest, EmptyInput) {
    std::istringstream input;
    std::ostringstream output;

    ArchiveWriter writer(output, {.profile = fqc::format::DatasetProfile::kIllumina});
    CompressPipeline pipeline(4096);

    auto result = pipeline.run(input, writer);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->recordCount, 0U);
    EXPECT_EQ(result->frameCount, 0U);
}

TEST(CompressPipelineTest, MultipleFrames) {
    constexpr int kRecordCount = 100;
    std::istringstream input(makeFastq(kRecordCount));
    std::ostringstream output;

    ArchiveWriter writer(output, {.profile = fqc::format::DatasetProfile::kIllumina});
    CompressPipeline pipeline(256);

    auto result = pipeline.run(input, writer);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->recordCount, kRecordCount);
    EXPECT_GT(result->frameCount, 1U);
}
