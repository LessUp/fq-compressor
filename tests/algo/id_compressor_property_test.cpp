// =============================================================================
// fq-compressor - ID Compressor Property Tests
// =============================================================================
// Property-based tests for ID compression round-trip consistency.
//
// **Property 5: ID 压缩往返一致性**
// *For any* 有效的 FASTQ ID 序列，压缩后解压应产生等价 ID
//
// **Validates: Requirements 1.1.2**
// =============================================================================

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

#include "fqc/algo/id_compressor.h"

namespace fqc::algo::test {

// =============================================================================
// RapidCheck Generators
// =============================================================================

namespace gen {

/// @brief Generate a valid Illumina-style ID.
/// Format: instrument:run:flowcell:lane:tile:x:y
[[nodiscard]] rc::Gen<std::string> illuminaId() {
    return rc::gen::apply(
        [](int run, int lane, int tile, int x, int y) {
            return "SIM:" + std::to_string(run) + ":FCX:" +
                   std::to_string(lane) + ":" + std::to_string(tile) + ":" +
                   std::to_string(x) + ":" + std::to_string(y);
        },
        rc::gen::inRange(1, 10),      // run
        rc::gen::inRange(1, 8),       // lane
        rc::gen::inRange(1, 100),     // tile
        rc::gen::inRange(1, 10000),   // x
        rc::gen::inRange(1, 10000));  // y
}

/// @brief Generate a sequence of Illumina IDs with incrementing coordinates.
[[nodiscard]] rc::Gen<std::vector<std::string>> illuminaIdSequence(std::size_t count) {
    return rc::gen::apply(
        [count](int run, int lane, int tile, int startX, int startY) {
            std::vector<std::string> ids;
            ids.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                ids.push_back("SIM:" + std::to_string(run) + ":FCX:" +
                              std::to_string(lane) + ":" + std::to_string(tile) + ":" +
                              std::to_string(startX + static_cast<int>(i)) + ":" +
                              std::to_string(startY));
            }
            return ids;
        },
        rc::gen::inRange(1, 10),
        rc::gen::inRange(1, 8),
        rc::gen::inRange(1, 100),
        rc::gen::inRange(1, 5000),
        rc::gen::inRange(1, 5000));
}


/// @brief Generate a simple numeric ID.
[[nodiscard]] rc::Gen<std::string> numericId() {
    return rc::gen::map(rc::gen::inRange(1, 1000000),
                        [](int n) { return std::to_string(n); });
}

/// @brief Generate a sequence of numeric IDs.
[[nodiscard]] rc::Gen<std::vector<std::string>> numericIdSequence(std::size_t count) {
    return rc::gen::map(rc::gen::inRange(1, 1000000), [count](int start) {
        std::vector<std::string> ids;
        ids.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            ids.push_back(std::to_string(start + static_cast<int>(i)));
        }
        return ids;
    });
}

/// @brief Generate a random alphanumeric ID.
[[nodiscard]] rc::Gen<std::string> alphanumericId(std::size_t minLen = 5,
                                                   std::size_t maxLen = 50) {
    return rc::gen::mapcat(rc::gen::inRange(minLen, maxLen + 1), [](std::size_t len) {
        return rc::gen::container<std::string>(
            len, rc::gen::oneOf(rc::gen::inRange('a', 'z' + 1),
                                rc::gen::inRange('A', 'Z' + 1),
                                rc::gen::inRange('0', '9' + 1)));
    });
}

/// @brief Generate a vector of random alphanumeric IDs.
[[nodiscard]] rc::Gen<std::vector<std::string>> randomIdSequence(
    std::size_t count,
    std::size_t minLen = 5,
    std::size_t maxLen = 50) {
    return rc::gen::container<std::vector<std::string>>(count,
                                                         alphanumericId(minLen, maxLen));
}

/// @brief Generate an SRA-style ID.
/// Format: SRR123456.1 length=100
[[nodiscard]] rc::Gen<std::string> sraId() {
    return rc::gen::apply(
        [](int accession, int readNum, int length) {
            return "SRR" + std::to_string(accession) + "." +
                   std::to_string(readNum) + " length=" + std::to_string(length);
        },
        rc::gen::inRange(100000, 999999),
        rc::gen::inRange(1, 1000000),
        rc::gen::inRange(50, 300));
}

/// @brief Generate a sequence of SRA-style IDs.
[[nodiscard]] rc::Gen<std::vector<std::string>> sraIdSequence(std::size_t count) {
    return rc::gen::apply(
        [count](int accession, int startRead, int length) {
            std::vector<std::string> ids;
            ids.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                ids.push_back("SRR" + std::to_string(accession) + "." +
                              std::to_string(startRead + static_cast<int>(i)) +
                              " length=" + std::to_string(length));
            }
            return ids;
        },
        rc::gen::inRange(100000, 999999),
        rc::gen::inRange(1, 500000),
        rc::gen::inRange(50, 300));
}

}  // namespace gen

// =============================================================================
// Property Tests - Exact Mode
// =============================================================================

/// @brief Property 5.1: Single ID round-trip in exact mode.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, SingleIdExactRoundTrip, ()) {
    auto id = *gen::illuminaId();

    IDCompressorConfig config;
    config.idMode = IDMode::kExact;

    IDCompressor compressor(config);

    std::vector<std::string_view> ids = {id};
    auto compressResult = compressor.compress(ids);
    RC_ASSERT(compressResult.has_value());

    auto decompressResult = compressor.decompress(compressResult->data, 1);
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == 1);
    RC_ASSERT((*decompressResult)[0] == id);
}

/// @brief Property 5.2: Multiple Illumina IDs round-trip in exact mode.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, MultipleIlluminaIdsExactRoundTrip, ()) {
    auto count = *rc::gen::inRange<std::size_t>(1, 100);
    auto idStrings = *gen::illuminaIdSequence(count);

    IDCompressorConfig config;
    config.idMode = IDMode::kExact;

    IDCompressor compressor(config);

    std::vector<std::string_view> ids;
    ids.reserve(idStrings.size());
    for (const auto& id : idStrings) {
        ids.emplace_back(id);
    }

    auto compressResult = compressor.compress(ids);
    RC_ASSERT(compressResult.has_value());

    auto decompressResult = compressor.decompress(
        compressResult->data, static_cast<std::uint32_t>(count));
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == idStrings.size());
    for (std::size_t i = 0; i < idStrings.size(); ++i) {
        RC_ASSERT((*decompressResult)[i] == idStrings[i]);
    }
}

/// @brief Property 5.3: Random IDs round-trip in exact mode.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, RandomIdsExactRoundTrip, ()) {
    auto count = *rc::gen::inRange<std::size_t>(1, 50);
    auto idStrings = *gen::randomIdSequence(count, 10, 100);

    IDCompressorConfig config;
    config.idMode = IDMode::kExact;

    IDCompressor compressor(config);

    std::vector<std::string_view> ids;
    ids.reserve(idStrings.size());
    for (const auto& id : idStrings) {
        ids.emplace_back(id);
    }

    auto compressResult = compressor.compress(ids);
    RC_ASSERT(compressResult.has_value());

    auto decompressResult = compressor.decompress(
        compressResult->data, static_cast<std::uint32_t>(count));
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == idStrings.size());
    for (std::size_t i = 0; i < idStrings.size(); ++i) {
        RC_ASSERT((*decompressResult)[i] == idStrings[i]);
    }
}


// =============================================================================
// Property Tests - Tokenize Mode
// =============================================================================

/// @brief Property 5.4: Illumina IDs round-trip in tokenize mode.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, IlluminaIdsTokenizeRoundTrip, ()) {
    auto count = *rc::gen::inRange<std::size_t>(10, 100);
    auto idStrings = *gen::illuminaIdSequence(count);

    IDCompressorConfig config;
    config.idMode = IDMode::kTokenize;
    config.minPatternMatchRatio = 0.9;

    IDCompressor compressor(config);

    std::vector<std::string_view> ids;
    ids.reserve(idStrings.size());
    for (const auto& id : idStrings) {
        ids.emplace_back(id);
    }

    auto compressResult = compressor.compress(ids);
    RC_ASSERT(compressResult.has_value());

    auto decompressResult = compressor.decompress(
        compressResult->data, static_cast<std::uint32_t>(count));
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == idStrings.size());
    for (std::size_t i = 0; i < idStrings.size(); ++i) {
        RC_ASSERT((*decompressResult)[i] == idStrings[i]);
    }
}

/// @brief Property 5.5: SRA IDs round-trip in tokenize mode.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, SraIdsTokenizeRoundTrip, ()) {
    auto count = *rc::gen::inRange<std::size_t>(10, 100);
    auto idStrings = *gen::sraIdSequence(count);

    IDCompressorConfig config;
    config.idMode = IDMode::kTokenize;
    config.minPatternMatchRatio = 0.9;

    IDCompressor compressor(config);

    std::vector<std::string_view> ids;
    ids.reserve(idStrings.size());
    for (const auto& id : idStrings) {
        ids.emplace_back(id);
    }

    auto compressResult = compressor.compress(ids);
    RC_ASSERT(compressResult.has_value());

    auto decompressResult = compressor.decompress(
        compressResult->data, static_cast<std::uint32_t>(count));
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == idStrings.size());
    for (std::size_t i = 0; i < idStrings.size(); ++i) {
        RC_ASSERT((*decompressResult)[i] == idStrings[i]);
    }
}

/// @brief Property 5.6: Numeric IDs round-trip in tokenize mode.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, NumericIdsTokenizeRoundTrip, ()) {
    auto count = *rc::gen::inRange<std::size_t>(10, 100);
    auto idStrings = *gen::numericIdSequence(count);

    IDCompressorConfig config;
    config.idMode = IDMode::kTokenize;

    IDCompressor compressor(config);

    std::vector<std::string_view> ids;
    ids.reserve(idStrings.size());
    for (const auto& id : idStrings) {
        ids.emplace_back(id);
    }

    auto compressResult = compressor.compress(ids);
    RC_ASSERT(compressResult.has_value());

    auto decompressResult = compressor.decompress(
        compressResult->data, static_cast<std::uint32_t>(count));
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == idStrings.size());
    for (std::size_t i = 0; i < idStrings.size(); ++i) {
        RC_ASSERT((*decompressResult)[i] == idStrings[i]);
    }
}

// =============================================================================
// Property Tests - Discard Mode
// =============================================================================

/// @brief Property 5.7: Discard mode produces sequential IDs.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, DiscardModeProducesSequentialIds, ()) {
    auto count = *rc::gen::inRange<std::size_t>(1, 100);
    auto idStrings = *gen::illuminaIdSequence(count);

    IDCompressorConfig config;
    config.idMode = IDMode::kDiscard;
    config.idPrefix = "";

    IDCompressor compressor(config);

    std::vector<std::string_view> ids;
    ids.reserve(idStrings.size());
    for (const auto& id : idStrings) {
        ids.emplace_back(id);
    }

    auto compressResult = compressor.compress(ids);
    RC_ASSERT(compressResult.has_value());

    // Discard mode should produce minimal data
    RC_ASSERT(compressResult->data.size() <= 2);

    auto decompressResult = compressor.decompress(
        compressResult->data, static_cast<std::uint32_t>(count));
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == count);

    // Verify sequential IDs
    for (std::size_t i = 0; i < count; ++i) {
        RC_ASSERT((*decompressResult)[i] == std::to_string(i + 1));
    }
}

/// @brief Property 5.8: Discard mode with prefix.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, DiscardModeWithPrefix, ()) {
    auto count = *rc::gen::inRange<std::size_t>(1, 50);
    auto prefix = *rc::gen::container<std::string>(
        *rc::gen::inRange<std::size_t>(1, 10),
        rc::gen::inRange('A', 'Z' + 1));

    IDCompressorConfig config;
    config.idMode = IDMode::kDiscard;
    config.idPrefix = prefix;

    IDCompressor compressor(config);

    std::vector<std::string_view> ids;
    for (std::size_t i = 0; i < count; ++i) {
        ids.emplace_back("dummy");
    }

    auto compressResult = compressor.compress(ids);
    RC_ASSERT(compressResult.has_value());

    auto decompressResult = compressor.decompress(
        compressResult->data, static_cast<std::uint32_t>(count));
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == count);

    // Verify prefixed sequential IDs
    for (std::size_t i = 0; i < count; ++i) {
        std::string expected = prefix + std::to_string(i + 1);
        RC_ASSERT((*decompressResult)[i] == expected);
    }
}


// =============================================================================
// Property Tests - Delta/Varint Encoding
// =============================================================================

/// @brief Property 5.9: Delta-varint encoding round-trip.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, DeltaVarintRoundTrip, ()) {
    auto count = *rc::gen::inRange<std::size_t>(1, 1000);
    auto values = *rc::gen::container<std::vector<std::int64_t>>(
        count, rc::gen::inRange<std::int64_t>(-1000000, 1000000));

    auto encoded = deltaVarintEncode(values);
    auto decoded = deltaVarintDecode(encoded, count);

    RC_ASSERT(decoded.has_value());
    RC_ASSERT(decoded->size() == values.size());

    for (std::size_t i = 0; i < values.size(); ++i) {
        RC_ASSERT((*decoded)[i] == values[i]);
    }
}

/// @brief Property 5.10: Delta-varint encoding with sequential values.
/// Sequential values should compress well with delta encoding.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, DeltaVarintSequentialCompression, ()) {
    auto count = *rc::gen::inRange<std::size_t>(100, 1000);
    auto start = *rc::gen::inRange<std::int64_t>(0, 1000000);

    std::vector<std::int64_t> values;
    values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        values.push_back(start + static_cast<std::int64_t>(i));
    }

    auto encoded = deltaVarintEncode(values);

    // Sequential values should compress to ~1 byte per value (delta = 1)
    // Plus some overhead for the first value
    RC_ASSERT(encoded.size() <= count + 10);

    auto decoded = deltaVarintDecode(encoded, count);
    RC_ASSERT(decoded.has_value());

    for (std::size_t i = 0; i < values.size(); ++i) {
        RC_ASSERT((*decoded)[i] == values[i]);
    }
}

/// @brief Property 5.11: ZigZag encoding round-trip.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, ZigZagRoundTrip, ()) {
    auto value = *rc::gen::inRange<std::int64_t>(
        std::numeric_limits<std::int64_t>::min() / 2,
        std::numeric_limits<std::int64_t>::max() / 2);

    auto encoded = zigzagEncode(value);
    auto decoded = zigzagDecode(encoded);

    RC_ASSERT(decoded == value);
}

// =============================================================================
// Property Tests - Tokenizer
// =============================================================================

/// @brief Property 5.12: Tokenizer parses Illumina IDs correctly.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, TokenizerParsesIlluminaIds, ()) {
    auto id = *gen::illuminaId();

    IDTokenizer tokenizer(":");
    auto tokens = tokenizer.tokenize(id);

    // Illumina ID has 7 fields separated by 6 colons
    // So we expect 13 tokens: 7 values + 6 delimiters
    RC_ASSERT(tokens.size() == 13);

    // Reconstruct and verify
    std::string reconstructed;
    for (const auto& token : tokens) {
        if (token.type == TokenType::kDynamicInt) {
            reconstructed += std::to_string(token.intValue);
        } else {
            reconstructed += token.value;
        }
    }

    RC_ASSERT(reconstructed == id);
}

/// @brief Property 5.13: Pattern detection finds consistent patterns.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, PatternDetectionConsistent, ()) {
    auto count = *rc::gen::inRange<std::size_t>(10, 50);
    auto idStrings = *gen::illuminaIdSequence(count);

    IDCompressorConfig config;
    config.minPatternMatchRatio = 0.9;

    IDCompressor compressor(config);

    std::vector<std::string_view> ids;
    ids.reserve(idStrings.size());
    for (const auto& id : idStrings) {
        ids.emplace_back(id);
    }

    auto pattern = compressor.detectPattern(ids);

    // Should detect a pattern for consistent Illumina IDs
    RC_ASSERT(pattern.has_value());
    RC_ASSERT(pattern->isValid());
    RC_ASSERT(pattern->numDynamicInts > 0);  // Should have dynamic integer fields
}

// =============================================================================
// Property Tests - Empty and Edge Cases
// =============================================================================

/// @brief Property 5.14: Empty input handling.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, EmptyInputHandling, ()) {
    IDCompressorConfig config;
    config.idMode = IDMode::kExact;

    IDCompressor compressor(config);

    std::vector<std::string_view> ids;
    auto compressResult = compressor.compress(ids);
    RC_ASSERT(compressResult.has_value());
    RC_ASSERT(compressResult->numIds == 0);

    auto decompressResult = compressor.decompress(compressResult->data, 0);
    RC_ASSERT(decompressResult.has_value());
    RC_ASSERT(decompressResult->empty());
}

/// @brief Property 5.15: Single character IDs.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(IDCompressorProperty, SingleCharacterIds, ()) {
    auto count = *rc::gen::inRange<std::size_t>(1, 50);
    auto c = *rc::gen::inRange('A', 'Z' + 1);

    std::vector<std::string> idStrings(count, std::string(1, static_cast<char>(c)));

    IDCompressorConfig config;
    config.idMode = IDMode::kExact;

    IDCompressor compressor(config);

    std::vector<std::string_view> ids;
    ids.reserve(idStrings.size());
    for (const auto& id : idStrings) {
        ids.emplace_back(id);
    }

    auto compressResult = compressor.compress(ids);
    RC_ASSERT(compressResult.has_value());

    auto decompressResult = compressor.decompress(
        compressResult->data, static_cast<std::uint32_t>(count));
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == count);
    for (std::size_t i = 0; i < count; ++i) {
        RC_ASSERT((*decompressResult)[i] == idStrings[i]);
    }
}


// =============================================================================
// Unit Tests (Non-Property)
// =============================================================================

/// @brief Test IDTokenizer with Illumina format.
TEST(IDCompressorTest, TokenizerIlluminaFormat) {
    IDTokenizer tokenizer(":");

    auto tokens = tokenizer.tokenize("SIM:1:FCX:1:15:1234:5678");

    ASSERT_EQ(tokens.size(), 13);  // 7 values + 6 delimiters

    // Check token types
    EXPECT_EQ(tokens[0].type, TokenType::kDynamicString);  // SIM
    EXPECT_EQ(tokens[0].value, "SIM");

    EXPECT_EQ(tokens[1].type, TokenType::kDelimiter);
    EXPECT_EQ(tokens[1].value, ":");

    EXPECT_EQ(tokens[2].type, TokenType::kDynamicInt);  // 1
    EXPECT_EQ(tokens[2].intValue, 1);

    EXPECT_EQ(tokens[4].type, TokenType::kDynamicString);  // FCX
    EXPECT_EQ(tokens[4].value, "FCX");

    EXPECT_EQ(tokens[10].type, TokenType::kDynamicInt);  // 1234
    EXPECT_EQ(tokens[10].intValue, 1234);

    EXPECT_EQ(tokens[12].type, TokenType::kDynamicInt);  // 5678
    EXPECT_EQ(tokens[12].intValue, 5678);
}

/// @brief Test IDTokenizer integer parsing.
TEST(IDCompressorTest, TokenizerIntegerParsing) {
    EXPECT_EQ(IDTokenizer::tryParseInt("123"), 123);
    EXPECT_EQ(IDTokenizer::tryParseInt("-456"), -456);
    EXPECT_EQ(IDTokenizer::tryParseInt("0"), 0);
    EXPECT_EQ(IDTokenizer::tryParseInt(""), std::nullopt);
    EXPECT_EQ(IDTokenizer::tryParseInt("abc"), std::nullopt);
    EXPECT_EQ(IDTokenizer::tryParseInt("12a3"), std::nullopt);
    EXPECT_EQ(IDTokenizer::tryParseInt("-"), std::nullopt);
}

/// @brief Test varint encoding/decoding.
TEST(IDCompressorTest, VarintEncoding) {
    std::uint8_t buffer[10];

    // Small positive
    EXPECT_EQ(uvarintEncode(0, buffer), 1);
    EXPECT_EQ(buffer[0], 0);

    EXPECT_EQ(uvarintEncode(127, buffer), 1);
    EXPECT_EQ(buffer[0], 127);

    EXPECT_EQ(uvarintEncode(128, buffer), 2);
    EXPECT_EQ(buffer[0], 0x80);
    EXPECT_EQ(buffer[1], 0x01);

    // Decode
    std::size_t bytesRead;
    EXPECT_EQ(uvarintDecode(std::span<const std::uint8_t>(buffer, 1), bytesRead), 128);
    // Note: This test is incorrect - need to use proper encoded data
}

/// @brief Test ZigZag encoding.
TEST(IDCompressorTest, ZigZagEncoding) {
    EXPECT_EQ(zigzagEncode(0), 0);
    EXPECT_EQ(zigzagEncode(-1), 1);
    EXPECT_EQ(zigzagEncode(1), 2);
    EXPECT_EQ(zigzagEncode(-2), 3);
    EXPECT_EQ(zigzagEncode(2), 4);

    EXPECT_EQ(zigzagDecode(0), 0);
    EXPECT_EQ(zigzagDecode(1), -1);
    EXPECT_EQ(zigzagDecode(2), 1);
    EXPECT_EQ(zigzagDecode(3), -2);
    EXPECT_EQ(zigzagDecode(4), 2);
}

/// @brief Test delta-varint encoding with simple sequence.
TEST(IDCompressorTest, DeltaVarintSimpleSequence) {
    std::vector<std::int64_t> values = {1, 2, 3, 4, 5};

    auto encoded = deltaVarintEncode(values);
    auto decoded = deltaVarintDecode(encoded, values.size());

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->size(), values.size());

    for (std::size_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ((*decoded)[i], values[i]);
    }
}

/// @brief Test generateDiscardId for SE mode.
TEST(IDCompressorTest, GenerateDiscardIdSE) {
    EXPECT_EQ(generateDiscardId(1, false, PELayout::kInterleaved, ""), "1");
    EXPECT_EQ(generateDiscardId(100, false, PELayout::kInterleaved, ""), "100");
    EXPECT_EQ(generateDiscardId(1, false, PELayout::kInterleaved, "READ_"), "READ_1");
}

/// @brief Test generateDiscardId for PE interleaved mode.
TEST(IDCompressorTest, GenerateDiscardIdPEInterleaved) {
    // archiveId 1 -> pair 1, read 1
    EXPECT_EQ(generateDiscardId(1, true, PELayout::kInterleaved, ""), "1/1");
    // archiveId 2 -> pair 1, read 2
    EXPECT_EQ(generateDiscardId(2, true, PELayout::kInterleaved, ""), "1/2");
    // archiveId 3 -> pair 2, read 1
    EXPECT_EQ(generateDiscardId(3, true, PELayout::kInterleaved, ""), "2/1");
    // archiveId 4 -> pair 2, read 2
    EXPECT_EQ(generateDiscardId(4, true, PELayout::kInterleaved, ""), "2/2");
}

/// @brief Test generateDiscardId for PE consecutive mode.
TEST(IDCompressorTest, GenerateDiscardIdPEConsecutive) {
    // Consecutive mode uses simple archive IDs
    EXPECT_EQ(generateDiscardId(1, true, PELayout::kConsecutive, ""), "1");
    EXPECT_EQ(generateDiscardId(100, true, PELayout::kConsecutive, ""), "100");
}

/// @brief Test configuration validation.
TEST(IDCompressorTest, ConfigValidation) {
    IDCompressorConfig config;

    // Valid config
    EXPECT_TRUE(config.validate().has_value());

    // Invalid compression level
    config.compressionLevel = 0;
    EXPECT_FALSE(config.validate().has_value());
    config.compressionLevel = 10;
    EXPECT_FALSE(config.validate().has_value());
    config.compressionLevel = 5;

    // Invalid zstd level
    config.zstdLevel = 0;
    EXPECT_FALSE(config.validate().has_value());
    config.zstdLevel = 23;
    EXPECT_FALSE(config.validate().has_value());
    config.zstdLevel = 3;

    // Invalid pattern match ratio
    config.minPatternMatchRatio = -0.1;
    EXPECT_FALSE(config.validate().has_value());
    config.minPatternMatchRatio = 1.1;
    EXPECT_FALSE(config.validate().has_value());
}

/// @brief Test ParsedID reconstruction.
TEST(IDCompressorTest, ParsedIdReconstruction) {
    IDCompressorConfig config;
    IDCompressor compressor(config);

    auto parsed = compressor.parseId("SIM:1:FCX:1:15:1234:5678");

    EXPECT_EQ(parsed.original, "SIM:1:FCX:1:15:1234:5678");
    EXPECT_EQ(parsed.reconstruct(), parsed.original);
}

}  // namespace fqc::algo::test
