#include "fqc/format/v2_archive.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <xxhash.h>
#include <zstd.h>

#include <gtest/gtest.h>

namespace fqc::format::v2::test {

namespace {

constexpr std::size_t kGlobalHeaderBytes = 32;
constexpr std::size_t kFrameHeaderBytes = 72;
constexpr std::size_t kFooterBytes = 40;
constexpr std::size_t kFrameIdOffset = kGlobalHeaderBytes + 8;
constexpr std::size_t kRecordCountOffset = kGlobalHeaderBytes + 12;
constexpr std::size_t kRawIdsSizeOffset = kGlobalHeaderBytes + 16;
constexpr std::size_t kIdsSizeOffset = kGlobalHeaderBytes + 24;
constexpr std::size_t kRawSequencesSizeOffset = kGlobalHeaderBytes + 32;
constexpr std::size_t kSequencesSizeOffset = kGlobalHeaderBytes + 40;
constexpr std::size_t kRawQualitiesSizeOffset = kGlobalHeaderBytes + 48;
constexpr std::size_t kQualitiesSizeOffset = kGlobalHeaderBytes + 56;
constexpr std::size_t kFrameChecksumOffset = kGlobalHeaderBytes + 64;
constexpr std::size_t kFirstPayloadOffset = kGlobalHeaderBytes + kFrameHeaderBytes;

struct FrameLayout {
    std::size_t idsOffset = 0;
    std::size_t sequencesOffset = 0;
    std::size_t qualitiesOffset = 0;
    std::size_t footerOffset = 0;
    std::size_t rawIdsSize = 0;
    std::size_t idsSize = 0;
    std::size_t rawSequencesSize = 0;
    std::size_t sequencesSize = 0;
    std::size_t rawQualitiesSize = 0;
    std::size_t qualitiesSize = 0;
};

[[nodiscard]] auto readU64(std::string_view bytes, std::size_t offset) -> std::uint64_t {
    std::uint64_t value = 0;
    for (unsigned index = 0; index < 8; ++index) {
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[offset + index]))
            << (index * 8U);
    }
    return value;
}

void writeU32(std::string& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<char>(value >> (index * 8U));
    }
}

void writeU64(std::string& bytes, std::size_t offset, std::uint64_t value) {
    for (unsigned index = 0; index < 8; ++index) {
        bytes[offset + index] = static_cast<char>(value >> (index * 8U));
    }
}

[[nodiscard]] auto frameLayout(std::string_view archive) -> FrameLayout {
    const auto rawIdsSize = static_cast<std::size_t>(readU64(archive, kRawIdsSizeOffset));
    const auto idsSize = static_cast<std::size_t>(readU64(archive, kIdsSizeOffset));
    const auto rawSequencesSize =
        static_cast<std::size_t>(readU64(archive, kRawSequencesSizeOffset));
    const auto sequencesSize = static_cast<std::size_t>(readU64(archive, kSequencesSizeOffset));
    const auto rawQualitiesSize =
        static_cast<std::size_t>(readU64(archive, kRawQualitiesSizeOffset));
    const auto qualitiesSize = static_cast<std::size_t>(readU64(archive, kQualitiesSizeOffset));
    const auto sequencesOffset = kFirstPayloadOffset + idsSize;
    const auto qualitiesOffset = sequencesOffset + sequencesSize;
    return {
        .idsOffset = kFirstPayloadOffset,
        .sequencesOffset = sequencesOffset,
        .qualitiesOffset = qualitiesOffset,
        .footerOffset = qualitiesOffset + qualitiesSize,
        .rawIdsSize = rawIdsSize,
        .idsSize = idsSize,
        .rawSequencesSize = rawSequencesSize,
        .sequencesSize = sequencesSize,
        .rawQualitiesSize = rawQualitiesSize,
        .qualitiesSize = qualitiesSize,
    };
}

[[nodiscard]] auto decompressStream(std::string_view archive,
                                    std::size_t offset,
                                    std::size_t encodedSize,
                                    std::size_t rawSize) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> raw(rawSize);
    const auto result =
        ZSTD_decompress(raw.data(), raw.size(), archive.data() + offset, encodedSize);
    EXPECT_EQ(ZSTD_isError(result), 0U);
    EXPECT_EQ(result, rawSize);
    return raw;
}

[[nodiscard]] auto compressStream(std::span<const std::uint8_t> raw) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> encoded(ZSTD_compressBound(raw.size()));
    const auto result = ZSTD_compress(encoded.data(), encoded.size(), raw.data(), raw.size(), 1);
    EXPECT_EQ(ZSTD_isError(result), 0U);
    if (ZSTD_isError(result) != 0U) {
        return {};
    }
    encoded.resize(result);
    return encoded;
}

[[nodiscard]] auto logicalChecksum(std::span<const std::uint8_t> ids,
                                   std::span<const std::uint8_t> sequences,
                                   std::span<const std::uint8_t> qualities) -> std::uint64_t {
    auto checksum = XXH64(ids.data(), ids.size(), 0);
    checksum = XXH64(sequences.data(), sequences.size(), checksum);
    return XXH64(qualities.data(), qualities.size(), checksum);
}

[[nodiscard]] auto rollingChecksum(std::uint64_t frameChecksum) -> std::uint64_t {
    std::array<std::uint8_t, 8> encoded{};
    for (unsigned index = 0; index < encoded.size(); ++index) {
        encoded[index] = static_cast<std::uint8_t>(frameChecksum >> (index * 8U));
    }
    return XXH64(encoded.data(), encoded.size(), 0);
}

[[nodiscard]] auto replaceFirstIdStream(std::string_view archive,
                                        std::span<const std::uint8_t> rawIds,
                                        bool repairChecksums) -> std::string {
    const auto layout = frameLayout(archive);
    const auto rawSequences = decompressStream(
        archive, layout.sequencesOffset, layout.sequencesSize, layout.rawSequencesSize);
    const auto rawQualities = decompressStream(
        archive, layout.qualitiesOffset, layout.qualitiesSize, layout.rawQualitiesSize);
    const auto encodedIds = compressStream(rawIds);

    std::string rebuilt(archive.substr(0, kFirstPayloadOffset));
    writeU64(rebuilt, kRawIdsSizeOffset, rawIds.size());
    writeU64(rebuilt, kIdsSizeOffset, encodedIds.size());

    std::uint64_t checksum = readU64(archive, kFrameChecksumOffset);
    if (repairChecksums) {
        checksum = logicalChecksum(rawIds, rawSequences, rawQualities);
        writeU64(rebuilt, kFrameChecksumOffset, checksum);
    }

    rebuilt.append(reinterpret_cast<const char*>(encodedIds.data()), encodedIds.size());
    rebuilt.append(
        archive.substr(layout.sequencesOffset, layout.sequencesSize + layout.qualitiesSize));
    rebuilt.append(archive.substr(layout.footerOffset, kFooterBytes));
    if (repairChecksums) {
        writeU64(rebuilt, rebuilt.size() - 8, rollingChecksum(checksum));
    }
    return rebuilt;
}

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

[[nodiscard]] auto writeArchiveFrames(const ArchiveOptions& options,
                                      const std::vector<ReadRecord>& records,
                                      std::size_t recordsPerFrame) -> std::string {
    std::ostringstream output(std::ios::binary);
    ArchiveWriter writer(output, options);
    for (std::size_t offset = 0; offset < records.size(); offset += recordsPerFrame) {
        const auto count = std::min(recordsPerFrame, records.size() - offset);
        EXPECT_TRUE(writer.writeFrame(std::span(records).subspan(offset, count)));
    }
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

TEST(V2ArchiveTest, ClassifiesCorruptionInEachArchiveRegion) {
    const auto valid = writeArchive({.profile = DatasetProfile::kIllumina}, makeRecords());

    auto globalHeader = valid;
    globalHeader[24] ^= 0x01;
    std::istringstream globalInput(globalHeader, std::ios::binary);
    ArchiveReader globalReader(globalInput);
    auto globalResult = globalReader.open();
    ASSERT_FALSE(globalResult);
    EXPECT_EQ(globalResult.error().code(), ErrorCode::kChecksumMismatch);

    auto frameHeader = valid;
    writeU32(frameHeader, kFrameIdOffset, 1);
    std::istringstream frameInput(frameHeader, std::ios::binary);
    ArchiveReader frameReader(frameInput);
    ASSERT_TRUE(frameReader.open());
    auto frameResult = frameReader.readFrame();
    ASSERT_FALSE(frameResult);
    EXPECT_EQ(frameResult.error().code(), ErrorCode::kFormatError);

    const auto layout = frameLayout(valid);
    auto rawIds = decompressStream(valid, layout.idsOffset, layout.idsSize, layout.rawIdsSize);
    ASSERT_GT(rawIds.size(), 2U);
    rawIds[2] ^= 0x01;
    const auto payload = replaceFirstIdStream(valid, rawIds, false);
    std::istringstream payloadInput(payload, std::ios::binary);
    ArchiveReader payloadReader(payloadInput);
    ASSERT_TRUE(payloadReader.open());
    auto payloadResult = payloadReader.readFrame();
    ASSERT_FALSE(payloadResult);
    EXPECT_EQ(payloadResult.error().code(), ErrorCode::kChecksumMismatch);

    auto footer = valid;
    footer.back() ^= 0x01;
    std::istringstream footerInput(footer, std::ios::binary);
    ArchiveReader footerReader(footerInput);
    ASSERT_TRUE(footerReader.open());
    ASSERT_TRUE(footerReader.readFrame());
    auto footerResult = footerReader.readFrame();
    ASSERT_FALSE(footerResult);
    EXPECT_EQ(footerResult.error().code(), ErrorCode::kChecksumMismatch);
}

TEST(V2ArchiveTest, RejectsHostileFrameMetadata) {
    const auto valid = writeArchive({.profile = DatasetProfile::kIllumina}, makeRecords());
    const auto expectFormatError = [](std::string archive, std::size_t maxFrameBytes) {
        std::istringstream input(std::move(archive), std::ios::binary);
        ArchiveReader reader(input, maxFrameBytes);
        EXPECT_TRUE(reader.open());
        auto result = reader.readFrame();
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), ErrorCode::kFormatError);
    };

    auto oversizedRaw = valid;
    writeU64(oversizedRaw, kRawIdsSizeOffset, std::numeric_limits<std::uint64_t>::max());
    expectFormatError(std::move(oversizedRaw), 1024 * 1024);

    auto oversizedEncoded = valid;
    writeU64(oversizedEncoded, kIdsSizeOffset, std::numeric_limits<std::uint64_t>::max());
    expectFormatError(std::move(oversizedEncoded), 1024 * 1024);

    auto recordCount = valid;
    writeU32(recordCount, kRecordCountOffset, 3);
    expectFormatError(std::move(recordCount), kDefaultMaxFrameBytes);

    auto frameId = valid;
    writeU32(frameId, kFrameIdOffset, 9);
    expectFormatError(std::move(frameId), kDefaultMaxFrameBytes);

    auto paired =
        writeArchive({.profile = DatasetProfile::kIllumina, .paired = true}, makeRecords());
    writeU32(paired, kRecordCountOffset, 3);
    expectFormatError(std::move(paired), kDefaultMaxFrameBytes);
}

TEST(V2ArchiveTest, RejectsTruncatedVarintInAuthenticatedLogicalStream) {
    const std::vector<ReadRecord> records = {
        {"read", "", "ACGT", "!!!!"},
    };
    const auto valid = writeArchive({.profile = DatasetProfile::kIllumina}, records);
    const std::array<std::uint8_t, 2> truncatedIds = {1, 0x80};
    const auto archive = replaceFirstIdStream(valid, truncatedIds, true);

    std::istringstream input(archive, std::ios::binary);
    ArchiveReader reader(input);
    ASSERT_TRUE(reader.open());
    auto result = reader.readFrame();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::kFormatError);
}

TEST(V2ArchiveTest, SeededPairedRoundTripIsDeterministicAcrossFrames) {
    constexpr std::string_view kIupac = "ACGTRYSWKMBDHVNacgtryswkmbdhvn";
    std::mt19937 random(0xF0C2026U);
    std::uniform_int_distribution<std::size_t> lengthDistribution(1, 2'048);
    std::uniform_int_distribution<std::size_t> baseDistribution(0, kIupac.size() - 1);
    std::uniform_int_distribution<int> qualityDistribution('!', '~');

    std::vector<ReadRecord> records;
    records.reserve(64);
    for (std::size_t index = 0; index < 64; ++index) {
        const auto length = std::max(kIupac.size(), lengthDistribution(random));
        std::string sequence(length, 'A');
        std::string quality(length, '!');
        for (std::size_t position = 0; position < length; ++position) {
            sequence[position] = kIupac[baseDistribution(random)];
            if ((index + position) % 17 == 0) {
                quality[position] = '!';
            } else if ((index + position) % 19 == 0) {
                quality[position] = '~';
            } else {
                quality[position] = static_cast<char>(qualityDistribution(random));
            }
        }
        if (index == 0) {
            sequence.replace(0, kIupac.size(), kIupac);
            quality.front() = '!';
            quality.back() = '~';
        }
        records.emplace_back("seeded_" + std::to_string(index),
                             index % 3 == 0 ? "comment" : "",
                             std::move(sequence),
                             std::move(quality));
    }

    const ArchiveOptions options{
        .profile = DatasetProfile::kPacBioClr,
        .paired = true,
        .maxFrameBytes = 4 * 1024 * 1024,
        .memoryLimitBytes = 64 * 1024 * 1024,
    };
    const auto first = writeArchiveFrames(options, records, 8);
    const auto second = writeArchiveFrames(options, records, 8);
    EXPECT_EQ(first, second);

    std::istringstream input(first, std::ios::binary);
    ArchiveReader reader(input, 4 * 1024 * 1024, 64 * 1024 * 1024);
    ASSERT_TRUE(reader.open());
    std::vector<ReadRecord> restored;
    while (true) {
        auto frame = reader.readFrame();
        ASSERT_TRUE(frame);
        if (!frame->has_value()) {
            break;
        }
        restored.insert(restored.end(), (**frame).begin(), (**frame).end());
    }
    EXPECT_EQ(reader.stats().frameCount, 8U);
    EXPECT_EQ(restored, records);
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

    auto unsupported = writeArchive({.profile = DatasetProfile::kIllumina}, makeRecords());
    unsupported[17] = 0x7F;
    std::istringstream unsupportedInput(unsupported, std::ios::binary);
    ArchiveReader unsupportedReader(unsupportedInput);
    auto unsupportedResult = unsupportedReader.open();
    ASSERT_FALSE(unsupportedResult);
    EXPECT_EQ(unsupportedResult.error().code(), ErrorCode::kUnsupportedCodec);
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
