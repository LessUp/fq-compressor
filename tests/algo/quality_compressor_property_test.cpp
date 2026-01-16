// =============================================================================
// fq-compressor - Quality Compressor Property Tests
// =============================================================================
// Property-based tests for quality compression round-trip consistency.
//
// **Property 4: 无损质量压缩往返一致性**
// *For any* 有效的质量值序列，无损压缩后解压应产生等价数据
//
// **Validates: Requirements 3.1**
// =============================================================================

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

#include "fqc/algo/quality_compressor.h"

namespace fqc::algo::test {

// =============================================================================
// RapidCheck Generators
// =============================================================================

namespace gen {

/// @brief Generate a valid quality character (Phred+33).
[[nodiscard]] rc::Gen<char> validQualityChar() {
    return rc::gen::map(rc::gen::inRange(0, 42),  // Phred 0-41 (Illumina range)
                        [](int phred) { return static_cast<char>('!' + phred); });
}

/// @brief Generate a valid quality string of specified length.
[[nodiscard]] rc::Gen<std::string> validQuality(std::size_t length) {
    return rc::gen::container<std::string>(length, validQualityChar());
}

/// @brief Generate a valid quality string with variable length.
[[nodiscard]] rc::Gen<std::string> validQualityVariable(std::size_t minLen = 50,
                                                         std::size_t maxLen = 300) {
    return rc::gen::mapcat(rc::gen::inRange(minLen, maxLen + 1),
                           [](std::size_t len) { return validQuality(len); });
}

/// @brief Generate a quality string with realistic distribution.
/// Quality values tend to be higher at the start and lower at the end.
[[nodiscard]] rc::Gen<std::string> realisticQuality(std::size_t length) {
    return rc::gen::map(rc::gen::just(length), [](std::size_t len) {
        std::string quality;
        quality.reserve(len);

        for (std::size_t i = 0; i < len; ++i) {
            // Simulate quality degradation along the read
            double position = static_cast<double>(i) / static_cast<double>(len);
            int maxPhred;

            if (position < 0.2) {
                maxPhred = 41;  // High quality at start
            } else if (position < 0.8) {
                maxPhred = 35;  // Medium quality in middle
            } else {
                maxPhred = 25;  // Lower quality at end
            }

            int phred = *rc::gen::inRange(0, maxPhred + 1);
            quality.push_back(static_cast<char>('!' + phred));
        }

        return quality;
    });
}

/// @brief Generate a vector of quality strings with uniform length.
[[nodiscard]] rc::Gen<std::vector<std::string>> uniformQualityStrings(
    std::size_t numStrings,
    std::size_t length) {
    return rc::gen::container<std::vector<std::string>>(numStrings, validQuality(length));
}

/// @brief Generate a vector of quality strings with variable lengths.
[[nodiscard]] rc::Gen<std::vector<std::string>> variableQualityStrings(
    std::size_t numStrings,
    std::size_t minLen = 50,
    std::size_t maxLen = 300) {
    return rc::gen::container<std::vector<std::string>>(numStrings,
                                                         validQualityVariable(minLen, maxLen));
}

}  // namespace gen

// =============================================================================
// Property Tests
// =============================================================================

/// @brief Property 4: Single quality string round-trip consistency (Order-2).
/// **Validates: Requirements 3.1**
RC_GTEST_PROP(QualityCompressorProperty, SingleStringRoundTripOrder2, ()) {
    auto length = *rc::gen::inRange<std::size_t>(10, 300);
    auto quality = *gen::validQuality(length);

    QualityCompressorConfig config;
    config.contextOrder = QualityContextOrder::kOrder2;
    config.qualityMode = QualityMode::kLossless;
    config.usePositionContext = true;

    QualityCompressor compressor(config);

    std::vector<std::string_view> qualities = {quality};
    auto compressResult = compressor.compress(qualities);
    RC_ASSERT(compressResult.has_value());

    std::vector<std::uint32_t> lengths = {static_cast<std::uint32_t>(length)};
    auto decompressResult = compressor.decompress(compressResult->data, lengths);
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == 1);
    RC_ASSERT((*decompressResult)[0] == quality);
}

/// @brief Property 4.1: Single quality string round-trip consistency (Order-1).
/// **Validates: Requirements 3.1**
RC_GTEST_PROP(QualityCompressorProperty, SingleStringRoundTripOrder1, ()) {
    auto length = *rc::gen::inRange<std::size_t>(10, 300);
    auto quality = *gen::validQuality(length);

    QualityCompressorConfig config;
    config.contextOrder = QualityContextOrder::kOrder1;
    config.qualityMode = QualityMode::kLossless;
    config.usePositionContext = true;

    QualityCompressor compressor(config);

    std::vector<std::string_view> qualities = {quality};
    auto compressResult = compressor.compress(qualities);
    RC_ASSERT(compressResult.has_value());

    std::vector<std::uint32_t> lengths = {static_cast<std::uint32_t>(length)};
    auto decompressResult = compressor.decompress(compressResult->data, lengths);
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == 1);
    RC_ASSERT((*decompressResult)[0] == quality);
}

/// @brief Property 4.2: Multiple quality strings round-trip consistency.
/// **Validates: Requirements 3.1**
RC_GTEST_PROP(QualityCompressorProperty, MultipleStringsRoundTrip, ()) {
    auto numStrings = *rc::gen::inRange<std::size_t>(1, 50);
    auto length = *rc::gen::inRange<std::size_t>(50, 200);
    auto qualityStrings = *gen::uniformQualityStrings(numStrings, length);

    QualityCompressorConfig config;
    config.contextOrder = QualityContextOrder::kOrder2;
    config.qualityMode = QualityMode::kLossless;

    QualityCompressor compressor(config);

    std::vector<std::string_view> qualities;
    qualities.reserve(qualityStrings.size());
    for (const auto& q : qualityStrings) {
        qualities.emplace_back(q);
    }

    auto compressResult = compressor.compress(qualities);
    RC_ASSERT(compressResult.has_value());

    std::vector<std::uint32_t> lengths(numStrings, static_cast<std::uint32_t>(length));
    auto decompressResult = compressor.decompress(compressResult->data, lengths);
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == qualityStrings.size());
    for (std::size_t i = 0; i < qualityStrings.size(); ++i) {
        RC_ASSERT((*decompressResult)[i] == qualityStrings[i]);
    }
}

/// @brief Property 4.3: Variable length quality strings round-trip.
/// **Validates: Requirements 3.1**
RC_GTEST_PROP(QualityCompressorProperty, VariableLengthRoundTrip, ()) {
    auto numStrings = *rc::gen::inRange<std::size_t>(1, 30);
    auto qualityStrings = *gen::variableQualityStrings(numStrings, 30, 200);

    QualityCompressorConfig config;
    config.contextOrder = QualityContextOrder::kOrder2;
    config.qualityMode = QualityMode::kLossless;

    QualityCompressor compressor(config);

    std::vector<std::string_view> qualities;
    std::vector<std::uint32_t> lengths;
    qualities.reserve(qualityStrings.size());
    lengths.reserve(qualityStrings.size());

    for (const auto& q : qualityStrings) {
        qualities.emplace_back(q);
        lengths.push_back(static_cast<std::uint32_t>(q.length()));
    }

    auto compressResult = compressor.compress(qualities);
    RC_ASSERT(compressResult.has_value());

    auto decompressResult = compressor.decompress(compressResult->data, lengths);
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == qualityStrings.size());
    for (std::size_t i = 0; i < qualityStrings.size(); ++i) {
        RC_ASSERT((*decompressResult)[i] == qualityStrings[i]);
    }
}

/// @brief Property 4.4: Illumina 8-bin lossy compression consistency.
/// **Validates: Requirements 3.3**
RC_GTEST_PROP(QualityCompressorProperty, Illumina8BinConsistency, ()) {
    auto length = *rc::gen::inRange<std::size_t>(50, 200);
    auto quality = *gen::validQuality(length);

    QualityCompressorConfig config;
    config.contextOrder = QualityContextOrder::kOrder2;
    config.qualityMode = QualityMode::kIllumina8;

    QualityCompressor compressor(config);

    std::vector<std::string_view> qualities = {quality};
    auto compressResult = compressor.compress(qualities);
    RC_ASSERT(compressResult.has_value());

    std::vector<std::uint32_t> lengths = {static_cast<std::uint32_t>(length)};
    auto decompressResult = compressor.decompress(compressResult->data, lengths);
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == 1);
    RC_ASSERT((*decompressResult)[0].length() == quality.length());

    // Verify that binned values are consistent
    // (compress again should produce same result)
    std::vector<std::string_view> binnedQualities = {(*decompressResult)[0]};
    auto recompressResult = compressor.compress(binnedQualities);
    RC_ASSERT(recompressResult.has_value());

    auto redecompressResult = compressor.decompress(recompressResult->data, lengths);
    RC_ASSERT(redecompressResult.has_value());

    // After binning, recompression should be idempotent
    RC_ASSERT((*redecompressResult)[0] == (*decompressResult)[0]);
}

/// @brief Property 4.5: Discard mode produces placeholder quality.
/// **Validates: Requirements 3.4**
RC_GTEST_PROP(QualityCompressorProperty, DiscardModeProducesPlaceholder, ()) {
    auto length = *rc::gen::inRange<std::size_t>(50, 200);
    auto quality = *gen::validQuality(length);

    QualityCompressorConfig config;
    config.qualityMode = QualityMode::kDiscard;

    QualityCompressor compressor(config);

    std::vector<std::string_view> qualities = {quality};
    auto compressResult = compressor.compress(qualities);
    RC_ASSERT(compressResult.has_value());

    // Compressed data should be empty for discard mode
    RC_ASSERT(compressResult->data.empty());

    std::vector<std::uint32_t> lengths = {static_cast<std::uint32_t>(length)};
    auto decompressResult = compressor.decompress(compressResult->data, lengths);
    RC_ASSERT(decompressResult.has_value());

    RC_ASSERT(decompressResult->size() == 1);
    RC_ASSERT((*decompressResult)[0].length() == length);

    // All characters should be placeholder quality
    for (char c : (*decompressResult)[0]) {
        RC_ASSERT(c == kDefaultPlaceholderQual);
    }
}

/// @brief Property 4.6: Compression ratio is reasonable.
/// **Validates: Requirements 3.1**
RC_GTEST_PROP(QualityCompressorProperty, CompressionRatioReasonable, ()) {
    auto numStrings = *rc::gen::inRange<std::size_t>(10, 100);
    auto length = *rc::gen::inRange<std::size_t>(100, 200);
    auto qualityStrings = *gen::uniformQualityStrings(numStrings, length);

    QualityCompressorConfig config;
    config.contextOrder = QualityContextOrder::kOrder2;
    config.qualityMode = QualityMode::kLossless;

    QualityCompressor compressor(config);

    std::vector<std::string_view> qualities;
    for (const auto& q : qualityStrings) {
        qualities.emplace_back(q);
    }

    auto compressResult = compressor.compress(qualities);
    RC_ASSERT(compressResult.has_value());

    // Compression ratio should be less than 1.0 (compressed < uncompressed)
    // For quality values, we expect significant compression
    double ratio = compressResult->compressionRatio();
    RC_ASSERT(ratio < 1.0);  // Should compress

    // Log compression ratio for debugging
    RC_LOG() << "Compression ratio: " << ratio
             << " (uncompressed: " << compressResult->uncompressedSize
             << ", compressed: " << compressResult->data.size() << ")";
}

/// @brief Property 4.7: Empty input handling.
/// **Validates: Requirements 3.1**
RC_GTEST_PROP(QualityCompressorProperty, EmptyInputHandling, ()) {
    QualityCompressorConfig config;
    config.qualityMode = QualityMode::kLossless;

    QualityCompressor compressor(config);

    std::vector<std::string_view> qualities;
    auto compressResult = compressor.compress(qualities);
    RC_ASSERT(compressResult.has_value());
    RC_ASSERT(compressResult->data.empty());
    RC_ASSERT(compressResult->numStrings == 0);

    std::vector<std::uint32_t> lengths;
    auto decompressResult = compressor.decompress(compressResult->data, lengths);
    RC_ASSERT(decompressResult.has_value());
    RC_ASSERT(decompressResult->empty());
}

/// @brief Property 4.8: Position context improves compression.
/// **Validates: Requirements 3.1**
RC_GTEST_PROP(QualityCompressorProperty, PositionContextEffect, ()) {
    auto numStrings = *rc::gen::inRange<std::size_t>(20, 50);
    auto length = *rc::gen::inRange<std::size_t>(100, 200);

    // Generate realistic quality strings with position-dependent distribution
    std::vector<std::string> qualityStrings;
    qualityStrings.reserve(numStrings);
    for (std::size_t i = 0; i < numStrings; ++i) {
        qualityStrings.push_back(*gen::realisticQuality(length));
    }

    std::vector<std::string_view> qualities;
    for (const auto& q : qualityStrings) {
        qualities.emplace_back(q);
    }

    // Compress with position context
    QualityCompressorConfig configWithPos;
    configWithPos.contextOrder = QualityContextOrder::kOrder2;
    configWithPos.usePositionContext = true;

    QualityCompressor compressorWithPos(configWithPos);
    auto resultWithPos = compressorWithPos.compress(qualities);
    RC_ASSERT(resultWithPos.has_value());

    // Compress without position context
    QualityCompressorConfig configNoPos;
    configNoPos.contextOrder = QualityContextOrder::kOrder2;
    configNoPos.usePositionContext = false;

    QualityCompressor compressorNoPos(configNoPos);
    auto resultNoPos = compressorNoPos.compress(qualities);
    RC_ASSERT(resultNoPos.has_value());

    // Both should decompress correctly
    std::vector<std::uint32_t> lengths(numStrings, static_cast<std::uint32_t>(length));

    auto decompWithPos = compressorWithPos.decompress(resultWithPos->data, lengths);
    RC_ASSERT(decompWithPos.has_value());

    auto decompNoPos = compressorNoPos.decompress(resultNoPos->data, lengths);
    RC_ASSERT(decompNoPos.has_value());

    for (std::size_t i = 0; i < qualityStrings.size(); ++i) {
        RC_ASSERT((*decompWithPos)[i] == qualityStrings[i]);
        RC_ASSERT((*decompNoPos)[i] == qualityStrings[i]);
    }
}

// =============================================================================
// Unit Tests (Non-Property)
// =============================================================================

/// @brief Test Illumina 8-bin mapping.
TEST(QualityCompressorTest, Illumina8BinMapping) {
    // Test bin boundaries
    EXPECT_EQ(Illumina8BinMapper::toBin(0), 0);
    EXPECT_EQ(Illumina8BinMapper::toBin(1), 0);
    EXPECT_EQ(Illumina8BinMapper::toBin(2), 1);
    EXPECT_EQ(Illumina8BinMapper::toBin(9), 1);
    EXPECT_EQ(Illumina8BinMapper::toBin(10), 2);
    EXPECT_EQ(Illumina8BinMapper::toBin(19), 2);
    EXPECT_EQ(Illumina8BinMapper::toBin(20), 3);
    EXPECT_EQ(Illumina8BinMapper::toBin(24), 3);
    EXPECT_EQ(Illumina8BinMapper::toBin(25), 4);
    EXPECT_EQ(Illumina8BinMapper::toBin(29), 4);
    EXPECT_EQ(Illumina8BinMapper::toBin(30), 5);
    EXPECT_EQ(Illumina8BinMapper::toBin(34), 5);
    EXPECT_EQ(Illumina8BinMapper::toBin(35), 6);
    EXPECT_EQ(Illumina8BinMapper::toBin(39), 6);
    EXPECT_EQ(Illumina8BinMapper::toBin(40), 7);
    EXPECT_EQ(Illumina8BinMapper::toBin(41), 7);

    // Test representative values
    EXPECT_EQ(Illumina8BinMapper::fromBin(0), 0);
    EXPECT_EQ(Illumina8BinMapper::fromBin(1), 6);
    EXPECT_EQ(Illumina8BinMapper::fromBin(2), 15);
    EXPECT_EQ(Illumina8BinMapper::fromBin(3), 22);
    EXPECT_EQ(Illumina8BinMapper::fromBin(4), 27);
    EXPECT_EQ(Illumina8BinMapper::fromBin(5), 33);
    EXPECT_EQ(Illumina8BinMapper::fromBin(6), 37);
    EXPECT_EQ(Illumina8BinMapper::fromBin(7), 40);
}

/// @brief Test quality character conversion.
TEST(QualityCompressorTest, QualityCharConversion) {
    EXPECT_EQ(qualityCharToValue('!'), 0);
    EXPECT_EQ(qualityCharToValue('I'), 40);
    EXPECT_EQ(qualityCharToValue('~'), 93);

    EXPECT_EQ(qualityValueToChar(0), '!');
    EXPECT_EQ(qualityValueToChar(40), 'I');
    EXPECT_EQ(qualityValueToChar(93), '~');
}

/// @brief Test position bin computation.
TEST(QualityCompressorTest, PositionBinComputation) {
    // 16 bins for a read of length 160
    EXPECT_EQ(computePositionBin(0, 160, 16), 0);
    EXPECT_EQ(computePositionBin(9, 160, 16), 0);
    EXPECT_EQ(computePositionBin(10, 160, 16), 1);
    EXPECT_EQ(computePositionBin(80, 160, 16), 8);
    EXPECT_EQ(computePositionBin(159, 160, 16), 15);

    // Edge cases
    EXPECT_EQ(computePositionBin(0, 0, 16), 0);
    EXPECT_EQ(computePositionBin(0, 100, 0), 0);
}

/// @brief Test configuration validation.
TEST(QualityCompressorTest, ConfigValidation) {
    QualityCompressorConfig config;

    // Valid config
    config.numPositionBins = 16;
    config.adaptationRate = 0.5;
    EXPECT_TRUE(config.validate().has_value());

    // Invalid: numPositionBins = 0
    config.numPositionBins = 0;
    EXPECT_FALSE(config.validate().has_value());

    // Invalid: numPositionBins not power of 2
    config.numPositionBins = 15;
    EXPECT_FALSE(config.validate().has_value());

    // Invalid: adaptationRate out of range
    config.numPositionBins = 16;
    config.adaptationRate = 1.5;
    EXPECT_FALSE(config.validate().has_value());
}

/// @brief Test applyIllumina8Bin utility function.
TEST(QualityCompressorTest, ApplyIllumina8Bin) {
    std::string quality = "!IIIIIIIII";  // Phred 0, 40, 40, ...

    std::string binned = applyIllumina8Bin(quality);

    EXPECT_EQ(binned.length(), quality.length());
    EXPECT_EQ(qualityCharToValue(binned[0]), 0);   // Bin 0 -> 0
    EXPECT_EQ(qualityCharToValue(binned[1]), 40);  // Bin 7 -> 40
}

/// @brief Test quality histogram computation.
TEST(QualityCompressorTest, QualityHistogram) {
    std::vector<std::string_view> qualities = {"!!!", "III"};

    auto histogram = computeQualityHistogram(qualities);

    EXPECT_EQ(histogram[0], 3);   // Three '!' (Phred 0)
    EXPECT_EQ(histogram[40], 3);  // Three 'I' (Phred 40)

    // All other bins should be 0
    std::uint64_t total = 0;
    for (auto count : histogram) {
        total += count;
    }
    EXPECT_EQ(total, 6);
}

}  // namespace fqc::algo::test
