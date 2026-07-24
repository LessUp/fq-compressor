// =============================================================================
// fq-compressor - Compression Pipeline Integration Tests
// =============================================================================

#include "fqc/common/error.h"
#include "fqc/format/archive.h"
#include "fqc/pipeline/compress_pipeline.h"

#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using fqc::ErrorCode;
using fqc::ReadRecord;
using fqc::format::ArchiveWriter;
using fqc::format::DatasetProfile;
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

[[nodiscard]] auto makeRecord(std::string id) -> ReadRecord {
    return {std::move(id), "", "ACGTACGT", "IIIIIIII"};
}

}  // namespace

TEST(CompressPipelineTest, BasicRun) {
    constexpr int kRecordCount = 8;
    std::istringstream input(makeFastq(kRecordCount));
    std::ostringstream output;

    ArchiveWriter writer(output, {.profile = DatasetProfile::kIllumina});
    CompressPipeline pipeline(4096);

    auto result = pipeline.run(input, nullptr, {}, writer);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->recordCount, kRecordCount);
    EXPECT_GE(result->frameCount, 1U);
    EXPECT_FALSE(output.str().empty());
}

TEST(CompressPipelineTest, EmptyInput) {
    std::istringstream input;
    std::ostringstream output;

    ArchiveWriter writer(output, {.profile = DatasetProfile::kIllumina});
    CompressPipeline pipeline(4096);

    auto result = pipeline.run(input, nullptr, {}, writer);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->recordCount, 0U);
    EXPECT_EQ(result->frameCount, 0U);
}

TEST(CompressPipelineTest, MultipleFrames) {
    constexpr int kRecordCount = 100;
    std::istringstream input(makeFastq(kRecordCount));
    std::ostringstream output;

    ArchiveWriter writer(output, {.profile = DatasetProfile::kIllumina});
    CompressPipeline pipeline(256);

    auto result = pipeline.run(input, nullptr, {}, writer);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->recordCount, kRecordCount);
    EXPECT_GT(result->frameCount, 1U);
}

TEST(CompressPipelineTest, InitialRecordsEmittedFirst) {
    std::vector<ReadRecord> initial{makeRecord("s0"), makeRecord("s1")};
    std::istringstream input(makeFastq(3));
    std::ostringstream output;

    ArchiveWriter writer(output, {.profile = DatasetProfile::kIllumina});
    CompressPipeline pipeline(4096);

    auto result = pipeline.run(input, nullptr, initial, writer);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->recordCount, 5U);  // 2 initial + 3 streamed
}

TEST(CompressPipelineTest, PairedRun) {
    std::istringstream primary(makeFastq(5));
    std::istringstream mate(makeFastq(5));
    std::ostringstream output;

    ArchiveWriter writer(output, {.profile = DatasetProfile::kIllumina, .paired = true});
    CompressPipeline pipeline(4096, true);

    auto result = pipeline.run(primary, &mate, {}, writer);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->recordCount, 10U);
}

TEST(CompressPipelineTest, WriterFailureAbortsReaderWithoutDeadlock) {
    // Enough records to fill the queue depth several times over, so the reader
    // is blocked on a full push when the writer fails. The writer must abort
    // the queue so the reader unblocks; otherwise run() would hang forever on
    // reader.join().
    std::istringstream input(makeFastq(5000));
    std::ostringstream output;
    output.setstate(std::ios::failbit);  // force writeFrame to fail immediately

    ArchiveWriter writer(output, {.profile = DatasetProfile::kIllumina});
    CompressPipeline pipeline(256);

    auto result = pipeline.run(input, nullptr, {}, writer);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::kIOError);
}
