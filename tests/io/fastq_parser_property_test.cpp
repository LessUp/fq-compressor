// =============================================================================
// fq-compressor - FASTQ Parser Property Tests
// =============================================================================
// Property-based tests for FASTQ parsing round-trip consistency.
//
// **Property 2: FASTQ 解析往返一致性**
// *For any* 有效的 FASTQ 记录序列，解析后重新格式化应产生等价输出
//
// **Validates: Requirements 1.1.1**
// =============================================================================

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "fqc/io/fastq_parser.h"

namespace fqc::io::test {

// =============================================================================
// Test Utilities
// =============================================================================

/// @brief Format a FASTQ record as a string.
[[nodiscard]] std::string formatFastqRecord(const FastqRecord& record) {
    std::ostringstream oss;
    oss << '@' << record.id;
    if (!record.comment.empty()) {
        oss << ' ' << record.comment;
    }
    oss << '\n';
    oss << record.sequence << '\n';
    oss << "+\n";
    oss << record.quality << '\n';
    return oss.str();
}

/// @brief Format multiple FASTQ records as a string.
[[nodiscard]] std::string formatFastqRecords(const std::vector<FastqRecord>& records) {
    std::ostringstream oss;
    for (const auto& record : records) {
        oss << formatFastqRecord(record);
    }
    return oss.str();
}

// =============================================================================
// RapidCheck Generators
// =============================================================================

namespace gen {

/// @brief Generate a valid DNA base.
[[nodiscard]] rc::Gen<char> validBase() {
    return rc::gen::element('A', 'C', 'G', 'T', 'N');
}

/// @brief Generate a valid DNA sequence.
[[nodiscard]] rc::Gen<std::string> validSequence(std::size_t minLen = 10, std::size_t maxLen = 500) {
    return rc::gen::container<std::string>(rc::gen::inRange(minLen, maxLen + 1), validBase());
}

/// @brief Generate a valid quality character (Phred+33).
[[nodiscard]] rc::Gen<char> validQualityChar() {
    return rc::gen::map(rc::gen::inRange(0, 42),  // Phred 0-41 (Illumina range)
                        [](int phred) { return static_cast<char>('!' + phred); });
}

/// @brief Generate a valid quality string.
[[nodiscard]] rc::Gen<std::string> validQuality(std::size_t length) {
    return rc::gen::container<std::string>(length, validQualityChar());
}

/// @brief Generate a valid read ID (alphanumeric, no spaces).
[[nodiscard]] rc::Gen<std::string> validReadId() {
    return rc::gen::map(
        rc::gen::container<std::string>(
            rc::gen::inRange(1, 50),
            rc::gen::oneOf(rc::gen::inRange('a', 'z' + 1), rc::gen::inRange('A', 'Z' + 1),
                           rc::gen::inRange('0', '9' + 1), rc::gen::element('_', '-', ':', '.'))),
        [](std::string s) {
            // Ensure first char is not a digit
            if (!s.empty() && std::isdigit(static_cast<unsigned char>(s[0]))) {
                s[0] = 'R';
            }
            return s;
        });
}

/// @brief Generate an optional comment.
[[nodiscard]] rc::Gen<std::string> optionalComment() {
    return rc::gen::oneOf(
        rc::gen::just(std::string{}),
        rc::gen::container<std::string>(
            rc::gen::inRange(1, 30),
            rc::gen::oneOf(rc::gen::inRange('a', 'z' + 1), rc::gen::inRange('A', 'Z' + 1),
                           rc::gen::inRange('0', '9' + 1), rc::gen::element('_', '-', ':', '='))));
}

/// @brief Generate a valid FASTQ record.
[[nodiscard]] rc::Gen<FastqRecord> validFastqRecord() {
    return rc::gen::map(
        rc::gen::tuple(validReadId(), optionalComment(),
                       rc::gen::inRange<std::size_t>(10, 300)),  // sequence length
        [](const auto& tuple) {
            auto [id, comment, seqLen] = tuple;
            FastqRecord record;
            record.id = id;
            record.comment = comment;
            record.sequence = *validSequence(seqLen, seqLen);
            record.quality = *validQuality(seqLen);
            return record;
        });
}

/// @brief Generate an Illumina-style read ID.
[[nodiscard]] rc::Gen<std::string> illuminaReadId() {
    return rc::gen::map(
        rc::gen::tuple(rc::gen::inRange(1, 10),     // run
                       rc::gen::inRange(1, 9),      // lane
                       rc::gen::inRange(1, 100),    // tile
                       rc::gen::inRange(1, 10000),  // x
                       rc::gen::inRange(1, 10000)   // y
                       ),
        [](const auto& tuple) {
            auto [run, lane, tile, x, y] = tuple;
            return "SIM:" + std::to_string(run) + ":FCX:" + std::to_string(lane) + ":" +
                   std::to_string(tile) + ":" + std::to_string(x) + ":" + std::to_string(y);
        });
}

/// @brief Generate an Illumina-style FASTQ record.
[[nodiscard]] rc::Gen<FastqRecord> illuminaFastqRecord() {
    return rc::gen::map(
        rc::gen::tuple(illuminaReadId(), rc::gen::inRange<std::size_t>(50, 151)),  // typical Illumina
        [](const auto& tuple) {
            auto [id, seqLen] = tuple;
            FastqRecord record;
            record.id = id;
            record.comment = "1:N:0:ATCACG";  // Typical Illumina comment
            record.sequence = *validSequence(seqLen, seqLen);
            record.quality = *validQuality(seqLen);
            return record;
        });
}

/// @brief Generate a long read (for testing length detection).
[[nodiscard]] rc::Gen<FastqRecord> longReadRecord() {
    return rc::gen::map(
        rc::gen::tuple(validReadId(), rc::gen::inRange<std::size_t>(1000, 10000)),
        [](const auto& tuple) {
            auto [id, seqLen] = tuple;
            FastqRecord record;
            record.id = id;
            record.sequence = *validSequence(seqLen, seqLen);
            record.quality = *validQuality(seqLen);
            return record;
        });
}

}  // namespace gen

// =============================================================================
// Property Tests
// =============================================================================

/// @brief Property 2: Single record round-trip consistency.
/// **Validates: Requirements 1.1.1**
RC_GTEST_PROP(FastqParserProperty, SingleRecordRoundTrip, ()) {
    auto record = *gen::validFastqRecord();

    // Format to string
    std::string fastqStr = formatFastqRecord(record);

    // Parse back
    auto stream = std::make_unique<std::istringstream>(fastqStr);
    FastqParser parser(std::move(stream));
    parser.open();

    auto parsed = parser.readRecord();
    RC_ASSERT(parsed.has_value());

    // Verify fields match
    RC_ASSERT(parsed->id == record.id);
    RC_ASSERT(parsed->comment == record.comment);
    RC_ASSERT(parsed->sequence == record.sequence);
    RC_ASSERT(parsed->quality == record.quality);
    RC_ASSERT(parsed->isValid());
}

/// @brief Property 2.1: Multiple records round-trip consistency.
/// **Validates: Requirements 1.1.1**
RC_GTEST_PROP(FastqParserProperty, MultipleRecordsRoundTrip, ()) {
    auto numRecords = *rc::gen::inRange(1, 20);
    std::vector<FastqRecord> records;
    records.reserve(numRecords);

    for (int i = 0; i < numRecords; ++i) {
        records.push_back(*gen::validFastqRecord());
    }

    // Format to string
    std::string fastqStr = formatFastqRecords(records);

    // Parse back
    auto stream = std::make_unique<std::istringstream>(fastqStr);
    FastqParser parser(std::move(stream));
    parser.open();

    auto parsed = parser.readAll();
    RC_ASSERT(parsed.size() == records.size());

    for (std::size_t i = 0; i < records.size(); ++i) {
        RC_ASSERT(parsed[i].id == records[i].id);
        RC_ASSERT(parsed[i].comment == records[i].comment);
        RC_ASSERT(parsed[i].sequence == records[i].sequence);
        RC_ASSERT(parsed[i].quality == records[i].quality);
    }
}

/// @brief Property 2.2: Chunked reading consistency.
/// **Validates: Requirements 1.1.1**
RC_GTEST_PROP(FastqParserProperty, ChunkedReadingConsistency, ()) {
    auto numRecords = *rc::gen::inRange(5, 50);
    auto chunkSize = *rc::gen::inRange(1, 10);

    std::vector<FastqRecord> records;
    records.reserve(numRecords);

    for (int i = 0; i < numRecords; ++i) {
        records.push_back(*gen::validFastqRecord());
    }

    std::string fastqStr = formatFastqRecords(records);

    // Parse with chunked reading
    auto stream = std::make_unique<std::istringstream>(fastqStr);
    FastqParser parser(std::move(stream));
    parser.open();

    std::vector<FastqRecord> parsed;
    while (auto chunk = parser.readChunk(chunkSize)) {
        for (auto& rec : *chunk) {
            parsed.push_back(std::move(rec));
        }
    }

    RC_ASSERT(parsed.size() == records.size());

    for (std::size_t i = 0; i < records.size(); ++i) {
        RC_ASSERT(parsed[i].id == records[i].id);
        RC_ASSERT(parsed[i].sequence == records[i].sequence);
        RC_ASSERT(parsed[i].quality == records[i].quality);
    }
}

/// @brief Property 2.3: Statistics collection accuracy.
/// **Validates: Requirements 1.1.1**
RC_GTEST_PROP(FastqParserProperty, StatisticsAccuracy, ()) {
    auto numRecords = *rc::gen::inRange(1, 30);
    std::vector<FastqRecord> records;
    records.reserve(numRecords);

    std::uint64_t expectedBases = 0;
    std::uint32_t expectedMin = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t expectedMax = 0;

    for (int i = 0; i < numRecords; ++i) {
        auto record = *gen::validFastqRecord();
        auto len = static_cast<std::uint32_t>(record.length());
        expectedBases += len;
        expectedMin = std::min(expectedMin, len);
        expectedMax = std::max(expectedMax, len);
        records.push_back(std::move(record));
    }

    std::string fastqStr = formatFastqRecords(records);

    auto stream = std::make_unique<std::istringstream>(fastqStr);
    ParserOptions opts;
    opts.collectStats = true;
    FastqParser parser(std::move(stream), opts);
    parser.open();

    parser.readAll();

    const auto& stats = parser.stats();
    RC_ASSERT(stats.totalRecords == static_cast<std::uint64_t>(numRecords));
    RC_ASSERT(stats.totalBases == expectedBases);
    RC_ASSERT(stats.minLength == expectedMin);
    RC_ASSERT(stats.maxLength == expectedMax);
}

/// @brief Property 2.4: Illumina format parsing.
/// **Validates: Requirements 1.1.1**
RC_GTEST_PROP(FastqParserProperty, IlluminaFormatParsing, ()) {
    auto record = *gen::illuminaFastqRecord();

    std::string fastqStr = formatFastqRecord(record);

    auto stream = std::make_unique<std::istringstream>(fastqStr);
    FastqParser parser(std::move(stream));
    parser.open();

    auto parsed = parser.readRecord();
    RC_ASSERT(parsed.has_value());
    RC_ASSERT(parsed->id == record.id);
    RC_ASSERT(parsed->comment == record.comment);
    RC_ASSERT(parsed->sequence == record.sequence);
    RC_ASSERT(parsed->quality == record.quality);
}

/// @brief Property 2.5: Read length class detection.
/// **Validates: Requirements 1.1.1**
RC_GTEST_PROP(FastqParserProperty, ReadLengthClassDetection, ()) {
    // Generate short reads
    auto numRecords = *rc::gen::inRange(5, 20);
    std::vector<FastqRecord> records;

    for (int i = 0; i < numRecords; ++i) {
        records.push_back(*gen::validFastqRecord());  // 10-300bp
    }

    std::string fastqStr = formatFastqRecords(records);

    auto stream = std::make_unique<std::istringstream>(fastqStr);
    ParserOptions opts;
    opts.collectStats = true;
    FastqParser parser(std::move(stream), opts);
    parser.open();

    parser.readAll();

    auto lengthClass = detectReadLengthClass(parser.stats());

    // Short reads should be classified as SHORT
    RC_ASSERT(lengthClass == ReadLengthClass::kShort);
}

/// @brief Property 2.6: forEach callback consistency.
/// **Validates: Requirements 1.1.1**
RC_GTEST_PROP(FastqParserProperty, ForEachCallbackConsistency, ()) {
    auto numRecords = *rc::gen::inRange(1, 20);
    std::vector<FastqRecord> records;

    for (int i = 0; i < numRecords; ++i) {
        records.push_back(*gen::validFastqRecord());
    }

    std::string fastqStr = formatFastqRecords(records);

    auto stream = std::make_unique<std::istringstream>(fastqStr);
    FastqParser parser(std::move(stream));
    parser.open();

    std::vector<FastqRecord> collected;
    auto count = parser.forEach([&collected](const FastqRecord& rec) {
        collected.push_back(rec);
        return true;
    });

    RC_ASSERT(count == static_cast<std::uint64_t>(numRecords));
    RC_ASSERT(collected.size() == records.size());

    for (std::size_t i = 0; i < records.size(); ++i) {
        RC_ASSERT(collected[i].id == records[i].id);
        RC_ASSERT(collected[i].sequence == records[i].sequence);
    }
}

/// @brief Property 2.7: Early termination via callback.
/// **Validates: Requirements 1.1.1**
RC_GTEST_PROP(FastqParserProperty, EarlyTerminationCallback, ()) {
    auto numRecords = *rc::gen::inRange(5, 20);
    auto stopAfter = *rc::gen::inRange(1, numRecords);

    std::vector<FastqRecord> records;
    for (int i = 0; i < numRecords; ++i) {
        records.push_back(*gen::validFastqRecord());
    }

    std::string fastqStr = formatFastqRecords(records);

    auto stream = std::make_unique<std::istringstream>(fastqStr);
    FastqParser parser(std::move(stream));
    parser.open();

    int processed = 0;
    parser.forEach([&processed, stopAfter](const FastqRecord&) {
        ++processed;
        return processed < stopAfter;
    });

    RC_ASSERT(processed == stopAfter);
}

// =============================================================================
// Unit Tests (Non-Property)
// =============================================================================

/// @brief Test empty file handling.
TEST(FastqParserTest, EmptyFile) {
    auto stream = std::make_unique<std::istringstream>("");
    FastqParser parser(std::move(stream));
    parser.open();

    auto record = parser.readRecord();
    EXPECT_FALSE(record.has_value());
    EXPECT_TRUE(parser.eof());
}

/// @brief Test invalid format - missing @ prefix.
TEST(FastqParserTest, InvalidFormatMissingAtPrefix) {
    std::string invalid = "ID\nACGT\n+\nIIII\n";
    auto stream = std::make_unique<std::istringstream>(invalid);
    FastqParser parser(std::move(stream));
    parser.open();

    EXPECT_THROW(parser.readRecord(), FormatError);
}

/// @brief Test invalid format - quality length mismatch.
TEST(FastqParserTest, InvalidFormatQualityLengthMismatch) {
    std::string invalid = "@ID\nACGT\n+\nIII\n";  // Quality too short
    auto stream = std::make_unique<std::istringstream>(invalid);
    FastqParser parser(std::move(stream));
    parser.open();

    EXPECT_THROW(parser.readRecord(), FormatError);
}

/// @brief Test invalid format - missing plus line.
TEST(FastqParserTest, InvalidFormatMissingPlusLine) {
    std::string invalid = "@ID\nACGT\nIIII\n";  // Missing + line
    auto stream = std::make_unique<std::istringstream>(invalid);
    FastqParser parser(std::move(stream));
    parser.open();

    EXPECT_THROW(parser.readRecord(), FormatError);
}

/// @brief Test sequence validation.
TEST(FastqParserTest, SequenceValidation) {
    std::string invalid = "@ID\nACGTX\n+\nIIIII\n";  // Invalid base 'X'
    auto stream = std::make_unique<std::istringstream>(invalid);
    ParserOptions opts;
    opts.validateSequence = true;
    FastqParser parser(std::move(stream), opts);
    parser.open();

    EXPECT_THROW(parser.readRecord(), FormatError);
}

/// @brief Test quality validation.
TEST(FastqParserTest, QualityValidation) {
    std::string invalid = "@ID\nACGT\n+\n \n";  // Invalid quality (space)
    auto stream = std::make_unique<std::istringstream>(invalid);
    ParserOptions opts;
    opts.validateQuality = true;
    FastqParser parser(std::move(stream), opts);
    parser.open();

    // Note: The space will be trimmed, causing length mismatch
    EXPECT_THROW(parser.readRecord(), FormatError);
}

/// @brief Test utility functions.
TEST(FastqParserTest, UtilityFunctions) {
    EXPECT_TRUE(isValidBase('A'));
    EXPECT_TRUE(isValidBase('C'));
    EXPECT_TRUE(isValidBase('G'));
    EXPECT_TRUE(isValidBase('T'));
    EXPECT_TRUE(isValidBase('N'));
    EXPECT_TRUE(isValidBase('a'));
    EXPECT_FALSE(isValidBase('X'));
    EXPECT_FALSE(isValidBase(' '));

    EXPECT_TRUE(isValidQuality('!'));
    EXPECT_TRUE(isValidQuality('I'));
    EXPECT_TRUE(isValidQuality('~'));
    EXPECT_FALSE(isValidQuality(' '));

    EXPECT_EQ(qualityToPhred('!'), 0);
    EXPECT_EQ(qualityToPhred('I'), 40);

    EXPECT_EQ(phredToQuality(0), '!');
    EXPECT_EQ(phredToQuality(40), 'I');
}

/// @brief Test read length class detection.
TEST(FastqParserTest, ReadLengthClassDetection) {
    // Short reads
    ParserStats shortStats;
    shortStats.maxLength = 150;
    shortStats.totalRecords = 100;
    shortStats.lengthSum = 15000;  // avg 150
    EXPECT_EQ(detectReadLengthClass(shortStats), ReadLengthClass::kShort);

    // Medium reads (max > 511)
    ParserStats mediumStats;
    mediumStats.maxLength = 600;
    mediumStats.totalRecords = 100;
    mediumStats.lengthSum = 50000;
    EXPECT_EQ(detectReadLengthClass(mediumStats), ReadLengthClass::kMedium);

    // Long reads
    ParserStats longStats;
    longStats.maxLength = 15000;
    longStats.totalRecords = 100;
    longStats.lengthSum = 1000000;
    EXPECT_EQ(detectReadLengthClass(longStats), ReadLengthClass::kLong);
}

}  // namespace fqc::io::test
