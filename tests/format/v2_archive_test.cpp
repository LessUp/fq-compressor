#include "fqc/format/v2_archive.h"

#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace fqc::format::v2::test {

namespace {

[[nodiscard]] auto makeRecords() -> std::vector<ReadRecord> {
    return {
        {"read_1", "1:N:0:ACGT", "ACGTNacgt", "!#IJKLMNO"},
        {"read_2", "2:N:0:TGCA", "TGCARYSWK", "JKLMNOPQR"},
        {"read_3", "", "NNNNACGT", "!!!!!!!!"},
        {"read_4", "comment", "ACGTACGT", "~~~~~~~~"},
    };
}

[[nodiscard]] auto writeArchive(const ArchiveOptions& options,
                                const std::vector<ReadRecord>& records) -> std::string {
    std::ostringstream output(std::ios::binary);
    ArchiveWriter writer(output, options);
    EXPECT_TRUE(writer.writeFrame(records));
    EXPECT_TRUE(writer.finish());
    return output.str();
}

}  // namespace

TEST(V2ArchiveTest, RoundTripsAllRecordFieldsAndFooter) {
    const auto records = makeRecords();
    const auto archive = writeArchive(
        {.profile = DatasetProfile::kOnt, .paired = true, .maxFrameBytes = 1024 * 1024}, records);

    std::istringstream input(archive, std::ios::binary);
    ArchiveReader reader(input, 1024 * 1024);
    auto metadata = reader.open();
    ASSERT_TRUE(metadata);
    EXPECT_EQ(metadata->version, kArchiveVersion);
    EXPECT_EQ(metadata->profile, DatasetProfile::kOnt);
    EXPECT_TRUE(metadata->paired);

    auto frame = reader.readFrame();
    ASSERT_TRUE(frame);
    ASSERT_TRUE(frame->has_value());
    EXPECT_EQ(**frame, records);

    auto end = reader.readFrame();
    ASSERT_TRUE(end);
    EXPECT_FALSE(end->has_value());
    EXPECT_TRUE(reader.finished());
    EXPECT_EQ(reader.stats().frameCount, 1U);
    EXPECT_EQ(reader.stats().recordCount, records.size());
}

TEST(V2ArchiveTest, SupportsMultipleFramesAndEmptyArchive) {
    const auto records = makeRecords();
    std::ostringstream output(std::ios::binary);
    ArchiveWriter writer(output, {.profile = DatasetProfile::kIllumina});
    ASSERT_TRUE(writer.writeFrame(std::span(records).first(2)));
    ASSERT_TRUE(writer.writeFrame(std::span(records).last(2)));
    ASSERT_TRUE(writer.finish());
    EXPECT_EQ(writer.stats().frameCount, 2U);

    std::istringstream input(output.str(), std::ios::binary);
    ArchiveReader reader(input);
    ASSERT_TRUE(reader.open());
    ASSERT_TRUE(reader.readFrame()->has_value());
    ASSERT_TRUE(reader.readFrame()->has_value());
    ASSERT_FALSE(reader.readFrame()->has_value());

    std::ostringstream emptyOutput(std::ios::binary);
    ArchiveWriter emptyWriter(emptyOutput, {.profile = DatasetProfile::kPacBioHiFi});
    ASSERT_TRUE(emptyWriter.finish());
    std::istringstream emptyInput(emptyOutput.str(), std::ios::binary);
    ArchiveReader emptyReader(emptyInput);
    ASSERT_TRUE(emptyReader.open());
    auto emptyEnd = emptyReader.readFrame();
    ASSERT_TRUE(emptyEnd);
    EXPECT_FALSE(emptyEnd->has_value());
}

TEST(V2ArchiveTest, RejectsIncompletePair) {
    const auto records = makeRecords();
    std::ostringstream output(std::ios::binary);
    ArchiveWriter writer(output, {.profile = DatasetProfile::kIllumina, .paired = true});
    auto result = writer.writeFrame(std::span(records).first(1));
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::kInvalidArgument);
}

TEST(V2ArchiveTest, RejectsInvalidProfileAndLogicalRecords) {
    std::ostringstream invalidProfileOutput(std::ios::binary);
    ArchiveWriter invalidProfileWriter(invalidProfileOutput,
                                       {.profile = static_cast<DatasetProfile>(0)});
    auto profileResult = invalidProfileWriter.finish();
    ASSERT_FALSE(profileResult);
    EXPECT_EQ(profileResult.error().code(), ErrorCode::kInvalidArgument);

    const std::vector<ReadRecord> invalidRecords = {
        {"", "", "ACGT", "IIII"},
        {"read with space", "", "ACGT", "IIII"},
        {"read", "bad\ncomment", "ACGT", "IIII"},
        {"read", "", "ACGZ", "IIII"},
        {"read", "", "ACGT", "III\n"},
    };
    for (const auto& record : invalidRecords) {
        std::ostringstream output(std::ios::binary);
        ArchiveWriter writer(output, {.profile = DatasetProfile::kIllumina});
        auto result = writer.writeFrame(std::span(&record, 1));
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), ErrorCode::kInvalidArgument);
    }
}

TEST(V2ArchiveTest, DetectsCorruptionAndTruncation) {
    const auto records = makeRecords();
    auto archive = writeArchive({.profile = DatasetProfile::kPacBioClr}, records);
    archive[archive.size() / 2] ^= 0x5A;

    std::istringstream corruptInput(archive, std::ios::binary);
    ArchiveReader corruptReader(corruptInput);
    ASSERT_TRUE(corruptReader.open());
    auto corruptFrame = corruptReader.readFrame();
    ASSERT_FALSE(corruptFrame);
    EXPECT_TRUE(corruptFrame.error().code() == ErrorCode::kDecompressionFailed ||
                corruptFrame.error().code() == ErrorCode::kChecksumMismatch);

    const auto valid = writeArchive({.profile = DatasetProfile::kOnt}, records);
    std::istringstream truncatedInput(valid.substr(0, valid.size() - 5), std::ios::binary);
    ArchiveReader truncatedReader(truncatedInput);
    ASSERT_TRUE(truncatedReader.open());
    ASSERT_TRUE(truncatedReader.readFrame());
    auto truncatedFooter = truncatedReader.readFrame();
    ASSERT_FALSE(truncatedFooter);
    EXPECT_EQ(truncatedFooter.error().code(), ErrorCode::kFormatError);
}

TEST(V2ArchiveTest, RejectsBytesAfterFooter) {
    const auto records = makeRecords();
    auto archive = writeArchive({.profile = DatasetProfile::kIllumina}, records);
    archive.append("trailing");

    std::istringstream input(archive, std::ios::binary);
    ArchiveReader reader(input);
    ASSERT_TRUE(reader.open());
    ASSERT_TRUE(reader.readFrame());
    auto footer = reader.readFrame();
    ASSERT_FALSE(footer);
    EXPECT_EQ(footer.error().code(), ErrorCode::kFormatError);
}

TEST(V2ArchiveTest, RejectsLegacyOrUnknownInput) {
    std::istringstream input("FQC1 legacy bytes", std::ios::binary);
    ArchiveReader reader(input);
    auto result = reader.open();
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().code() == ErrorCode::kInvalidFormat ||
                result.error().code() == ErrorCode::kFormatError);
}

TEST(V2ArchiveTest, RejectsFramesThatExceedCompressionOrDecodeMemoryBudget) {
    std::vector<ReadRecord> records = {
        {"large", "", std::string(4 * 1024, 'n'), std::string(4 * 1024, 'I')},
    };
    std::ostringstream limitedOutput(std::ios::binary);
    ArchiveWriter limitedWriter(
        limitedOutput,
        {.profile = DatasetProfile::kOnt, .memoryLimitBytes = 16 * 1024 * 1024 + 1024});
    auto writeResult = limitedWriter.writeFrame(records);
    ASSERT_FALSE(writeResult);
    EXPECT_EQ(writeResult.error().code(), ErrorCode::kInvalidArgument);

    const auto archive = writeArchive({.profile = DatasetProfile::kOnt}, records);
    std::istringstream limitedInput(archive, std::ios::binary);
    ArchiveReader limitedReader(limitedInput, kDefaultMaxFrameBytes, 16 * 1024 * 1024 + 1024);
    ASSERT_TRUE(limitedReader.open());
    auto readResult = limitedReader.readFrame();
    ASSERT_FALSE(readResult);
    EXPECT_EQ(readResult.error().code(), ErrorCode::kFormatError);
}

TEST(V2ArchiveTest, ParsesProfilesStrictly) {
    EXPECT_EQ(parseProfile("illumina").value(), DatasetProfile::kIllumina);
    EXPECT_EQ(parseProfile("ont").value(), DatasetProfile::kOnt);
    EXPECT_EQ(parseProfile("pacbio-hifi").value(), DatasetProfile::kPacBioHiFi);
    EXPECT_EQ(parseProfile("pacbio-clr").value(), DatasetProfile::kPacBioClr);
    EXPECT_FALSE(parseProfile("auto"));
}

}  // namespace fqc::format::v2::test
