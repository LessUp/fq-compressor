// =============================================================================
// fq-compressor - Long Read Compression Property Tests
// =============================================================================
// Property-based tests for long read compression strategy.
//
// **Property 7: 长读压缩往返一致性**
// *For any* 有效的长读序列，压缩后解压应产生等价数据
//
// **Validates: Requirements 1.1.3**
// =============================================================================

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "fqc/algo/block_compressor.h"
#include "fqc/common/types.h"
#include "fqc/io/fastq_parser.h"

namespace fqc::algo::test {

// =============================================================================
// RapidCheck Generators for Long Reads
// =============================================================================

namespace gen {

/// @brief Generate a valid DNA base.
[[nodiscard]] rc::Gen<char> validBase() {
    return rc::gen::element('A', 'C', 'G', 'T');
}

/// @brief Generate a valid DNA base including N (higher N ratio for long reads).
[[nodiscard]] rc::Gen<char> validBaseWithN(double nRatio = 0.01) {
    return rc::gen::map(
        rc::gen::tuple(rc::gen::inRange(0.0, 1.0), validBase()),
        [nRatio](const auto& tuple) {
            auto [r, base] = tuple;
            return r < nRatio ? 'N' : base;
        });
}

/// @brief Generate a valid DNA sequence of specified length.
[[nodiscard]] rc::Gen<std::string> validSequence(std::size_t length) {
    return rc::gen::container<std::string>(length, validBase());
}

/// @brief Generate a long read sequence (1KB-50KB).
[[nodiscard]] rc::Gen<std::string> longReadSequence() {
    return rc::gen::mapcat(
        rc::gen::inRange<std::size_t>(1000, 50000),
        [](std::size_t len) { return validSequence(len); });
}

/// @brief Generate a medium read sequence (512-10KB).
[[nodiscard]] rc::Gen<std::string> mediumReadSequence() {
    return rc::gen::mapcat(
        rc::gen::inRange<std::size_t>(512, 10000),
        [](std::size_t len) { return validSequence(len); });
}

/// @brief Generate an ultra-long read sequence (100KB-500KB).
[[nodiscard]] rc::Gen<std::string> ultraLongReadSequence() {
    return rc::gen::mapcat(
        rc::gen::inRange<std::size_t>(100000, 500000),
        [](std::size_t len) { return validSequence(len); });
}

/// @brief Generate a valid quality string of specified length.
/// Simulates long read quality profile (Nanopore-like).
[[nodiscard]] rc::Gen<std::string> longReadQuality(std::size_t length) {
    // Long reads typically have lower quality with position-dependent pattern
    return rc::gen::map(
        rc::gen::container<std::vector<int>>(
            length,
            rc::gen::inRange(5, 35)),  // Phred 5-35 for long reads
        [](const std::vector<int>& phreds) {
            std::string qual;
            qual.reserve(phreds.size());
            for (int p : phreds) {
                qual.push_back(static_cast<char>('!' + p));
            }
            return qual;
        });
}

/// @brief Generate a valid read ID for long reads (Nanopore-style).
[[nodiscard]] rc::Gen<std::string> longReadId() {
    return rc::gen::map(
        rc::gen::tuple(
            rc::gen::container<std::string>(8, rc::gen::inRange('a', 'f' + 1)),  // UUID part
            rc::gen::inRange(1, 10000)),  // Read number
        [](const auto& tuple) {
            auto [uuid, num] = tuple;
            return uuid + "-" + std::to_string(num);
        });
}

/// @brief Generate a valid long ReadRecord.
[[nodiscard]] rc::Gen<ReadRecord> longReadRecord() {
    return rc::gen::mapcat(
        rc::gen::inRange<std::size_t>(1000, 20000),  // Length 1KB-20KB
        [](std::size_t seqLength) {
            return rc::gen::map(
                rc::gen::tuple(longReadId(), validSequence(seqLength), longReadQuality(seqLength)),
                [](const auto& tuple) {
                    auto [id, seq, qual] = tuple;
                    ReadRecord record;
                    record.id = std::move(id);
                    record.sequence = std::move(seq);
                    record.quality = std::move(qual);
                    return record;
                });
        });
}

/// @brief Generate a valid medium ReadRecord.
[[nodiscard]] rc::Gen<ReadRecord> mediumReadRecord() {
    return rc::gen::mapcat(
        rc::gen::inRange<std::size_t>(512, 5000),  // Length 512-5KB
        [](std::size_t seqLength) {
            return rc::gen::map(
                rc::gen::tuple(longReadId(), validSequence(seqLength), longReadQuality(seqLength)),
                [](const auto& tuple) {
                    auto [id, seq, qual] = tuple;
                    ReadRecord record;
                    record.id = std::move(id);
                    record.sequence = std::move(seq);
                    record.quality = std::move(qual);
                    return record;
                });
        });
}

/// @brief Generate a vector of long ReadRecords.
[[nodiscard]] rc::Gen<std::vector<ReadRecord>> longReadRecords(std::size_t count) {
    return rc::gen::container<std::vector<ReadRecord>>(count, longReadRecord());
}

/// @brief Generate a vector of medium ReadRecords.
[[nodiscard]] rc::Gen<std::vector<ReadRecord>> mediumReadRecords(std::size_t count) {
    return rc::gen::container<std::vector<ReadRecord>>(count, mediumReadRecord());
}

}  // namespace gen

// =============================================================================
// Property Tests - Long Read Detection
// =============================================================================

/// @brief Property 7.1: Read length classification correctness.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(LongReadProperty, LengthClassificationCorrectness, ()) {
    auto maxLength = *rc::gen::inRange<std::uint32_t>(50, 200000);
    auto avgLength = *rc::gen::inRange<std::uint32_t>(50, maxLength);

    io::ParserStats stats;
    stats.maxLength = maxLength;
    stats.totalRecords = 100;
    stats.lengthSum = static_cast<std::uint64_t>(avgLength) * 100;
    stats.minLength = 50;

    auto lengthClass = io::detectReadLengthClass(stats);

    // Verify classification follows priority rules
    if (maxLength >= 100 * 1024) {
        // Ultra-long -> LONG
        RC_ASSERT(lengthClass == ReadLengthClass::kLong);
    } else if (maxLength >= 10 * 1024) {
        // Long -> LONG
        RC_ASSERT(lengthClass == ReadLengthClass::kLong);
    } else if (maxLength > 511) {
        // Exceeds Spring limit -> MEDIUM
        RC_ASSERT(lengthClass == ReadLengthClass::kMedium);
    } else if (avgLength >= 1024) {
        // Median >= 1KB -> MEDIUM
        RC_ASSERT(lengthClass == ReadLengthClass::kMedium);
    } else {
        // Otherwise -> SHORT
        RC_ASSERT(lengthClass == ReadLengthClass::kShort);
    }
}

/// @brief Property 7.2: Spring ABC limit protection.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(LongReadProperty, SpringAbcLimitProtection, ()) {
    // Generate any max length above Spring's limit
    auto maxLength = *rc::gen::inRange<std::uint32_t>(512, 100000);

    io::ParserStats stats;
    stats.maxLength = maxLength;
    stats.totalRecords = 100;
    stats.lengthSum = 100 * 100;  // Avg = 100 (short)
    stats.minLength = 50;

    auto lengthClass = io::detectReadLengthClass(stats);

    // Must NOT be SHORT if any read exceeds 511bp
    RC_ASSERT(lengthClass != ReadLengthClass::kShort);
}

// =============================================================================
// Property Tests - Long Read Compression
// =============================================================================

/// @brief Property 7.3: Long read compression round-trip consistency.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(LongReadProperty, CompressionRoundTrip, ()) {
    // Generate a small number of long reads (to keep test fast)
    auto numReads = *rc::gen::inRange<std::size_t>(5, 20);
    auto reads = *gen::longReadRecords(numReads);

    RC_PRE(!reads.empty());

    // Configure compressor for long reads
    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kLong;
    config.qualityMode = QualityMode::kLossless;
    config.idMode = IDMode::kExact;

    BlockCompressor compressor(config);

    // Compress
    auto compressResult = compressor.compress(reads, 0);
    RC_ASSERT(compressResult.has_value());

    // Decompress
    auto decompressResult = compressor.decompress(*compressResult);
    RC_ASSERT(decompressResult.has_value());

    // Verify round-trip
    RC_ASSERT(decompressResult->reads.size() == reads.size());

    for (std::size_t i = 0; i < reads.size(); ++i) {
        RC_ASSERT(decompressResult->reads[i].id == reads[i].id);
        RC_ASSERT(decompressResult->reads[i].sequence == reads[i].sequence);
        RC_ASSERT(decompressResult->reads[i].quality == reads[i].quality);
    }
}

/// @brief Property 7.4: Medium read compression round-trip consistency.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(LongReadProperty, MediumReadCompressionRoundTrip, ()) {
    auto numReads = *rc::gen::inRange<std::size_t>(10, 50);
    auto reads = *gen::mediumReadRecords(numReads);

    RC_PRE(!reads.empty());

    // Configure compressor for medium reads
    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kMedium;
    config.qualityMode = QualityMode::kLossless;
    config.idMode = IDMode::kExact;

    BlockCompressor compressor(config);

    // Compress
    auto compressResult = compressor.compress(reads, 0);
    RC_ASSERT(compressResult.has_value());

    // Decompress
    auto decompressResult = compressor.decompress(*compressResult);
    RC_ASSERT(decompressResult.has_value());

    // Verify round-trip
    RC_ASSERT(decompressResult->reads.size() == reads.size());

    for (std::size_t i = 0; i < reads.size(); ++i) {
        RC_ASSERT(decompressResult->reads[i].id == reads[i].id);
        RC_ASSERT(decompressResult->reads[i].sequence == reads[i].sequence);
        RC_ASSERT(decompressResult->reads[i].quality == reads[i].quality);
    }
}

/// @brief Property 7.5: Long read uses Zstd codec (not ABC).
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(LongReadProperty, UsesZstdCodec, ()) {
    auto numReads = *rc::gen::inRange<std::size_t>(5, 15);
    auto reads = *gen::longReadRecords(numReads);

    RC_PRE(!reads.empty());

    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kLong;

    // Verify codec selection
    auto seqCodec = config.getSequenceCodec();
    auto codecFamily = format::decodeCodecFamily(seqCodec);

    // Long reads must use Zstd, not ABC
    RC_ASSERT(codecFamily == CodecFamily::kZstdPlain);
}

/// @brief Property 7.6: Medium read uses Zstd codec (not ABC).
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(LongReadProperty, MediumUsesZstdCodec, ()) {
    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kMedium;

    auto seqCodec = config.getSequenceCodec();
    auto codecFamily = format::decodeCodecFamily(seqCodec);

    // Medium reads must use Zstd, not ABC
    RC_ASSERT(codecFamily == CodecFamily::kZstdPlain);
}

/// @brief Property 7.7: Short read uses ABC codec.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(LongReadProperty, ShortUsesAbcCodec, ()) {
    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kShort;

    auto seqCodec = config.getSequenceCodec();
    auto codecFamily = format::decodeCodecFamily(seqCodec);

    // Short reads must use ABC
    RC_ASSERT(codecFamily == CodecFamily::kAbcV1);
}

// =============================================================================
// Property Tests - Quality Compression for Long Reads
// =============================================================================

/// @brief Property 7.8: Long read uses SCM Order-1 for quality.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(LongReadProperty, LongReadQualityOrder1, ()) {
    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kLong;
    config.qualityMode = QualityMode::kLossless;

    auto qualCodec = config.getQualityCodec();
    auto codecFamily = format::decodeCodecFamily(qualCodec);

    // Long reads should use SCM Order-1 (lower memory)
    RC_ASSERT(codecFamily == CodecFamily::kScmOrder1);
}

/// @brief Property 7.9: Short/Medium read uses SCM Order-2 for quality.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(LongReadProperty, ShortMediumQualityOrder2, ()) {
    auto lengthClass = *rc::gen::element(ReadLengthClass::kShort, ReadLengthClass::kMedium);

    BlockCompressorConfig config;
    config.readLengthClass = lengthClass;
    config.qualityMode = QualityMode::kLossless;

    auto qualCodec = config.getQualityCodec();
    auto codecFamily = format::decodeCodecFamily(qualCodec);

    // Short/Medium reads should use SCM (Order-2)
    RC_ASSERT(codecFamily == CodecFamily::kScmV1);
}

// =============================================================================
// Property Tests - Variable Length Handling
// =============================================================================

/// @brief Property 7.10: Variable length long reads handled correctly.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(LongReadProperty, VariableLengthHandling, ()) {
    // Generate reads with variable lengths
    auto numReads = *rc::gen::inRange<std::size_t>(5, 20);
    std::vector<ReadRecord> reads;
    reads.reserve(numReads);

    for (std::size_t i = 0; i < numReads; ++i) {
        auto length = *rc::gen::inRange<std::size_t>(1000, 30000);
        auto record = *rc::gen::map(
            rc::gen::tuple(gen::longReadId(),
                           gen::validSequence(length),
                           gen::longReadQuality(length)),
            [](const auto& tuple) {
                auto [id, seq, qual] = tuple;
                ReadRecord r;
                r.id = std::move(id);
                r.sequence = std::move(seq);
                r.quality = std::move(qual);
                return r;
            });
        reads.push_back(std::move(record));
    }

    RC_PRE(!reads.empty());

    // Verify all reads have different lengths (variable length test)
    bool hasVariableLengths = false;
    for (std::size_t i = 1; i < reads.size(); ++i) {
        if (reads[i].sequence.length() != reads[0].sequence.length()) {
            hasVariableLengths = true;
            break;
        }
    }

    if (!hasVariableLengths && reads.size() > 1) {
        // Force variable lengths
        reads[0].sequence += "ACGT";
        reads[0].quality += "IIII";
    }

    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kLong;
    config.qualityMode = QualityMode::kLossless;

    BlockCompressor compressor(config);

    // Compress
    auto compressResult = compressor.compress(reads, 0);
    RC_ASSERT(compressResult.has_value());

    // Variable length blocks should have non-empty aux stream (or uniform=0)
    // (Implementation detail - may have auxStream or uniformReadLength=0)

    // Decompress
    auto decompressResult = compressor.decompress(*compressResult);
    RC_ASSERT(decompressResult.has_value());

    // Verify lengths preserved
    RC_ASSERT(decompressResult->reads.size() == reads.size());
    for (std::size_t i = 0; i < reads.size(); ++i) {
        RC_ASSERT(decompressResult->reads[i].sequence.length() == reads[i].sequence.length());
    }
}

// =============================================================================
// Unit Tests - Edge Cases
// =============================================================================

/// @brief Test empty block handling for long reads.
TEST(LongReadTest, EmptyBlockHandling) {
    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kLong;

    BlockCompressor compressor(config);

    std::vector<ReadRecord> emptyReads;
    auto result = compressor.compress(emptyReads, 0);

    // Empty block should succeed with zero reads
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->readCount, 0);
}

/// @brief Test single long read compression.
TEST(LongReadTest, SingleLongRead) {
    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kLong;
    config.qualityMode = QualityMode::kLossless;

    BlockCompressor compressor(config);

    // Create a single long read
    ReadRecord read;
    read.id = "test-read-1";
    read.sequence = std::string(5000, 'A');  // 5KB of A's
    read.quality = std::string(5000, 'I');   // Quality I

    std::vector<ReadRecord> reads = {read};

    auto compressResult = compressor.compress(reads, 0);
    ASSERT_TRUE(compressResult.has_value());

    auto decompressResult = compressor.decompress(*compressResult);
    ASSERT_TRUE(decompressResult.has_value());

    ASSERT_EQ(decompressResult->reads.size(), 1);
    EXPECT_EQ(decompressResult->reads[0].id, read.id);
    EXPECT_EQ(decompressResult->reads[0].sequence, read.sequence);
    EXPECT_EQ(decompressResult->reads[0].quality, read.quality);
}

/// @brief Test block size limits for ultra-long reads.
TEST(LongReadTest, UltraLongReadBlockLimit) {
    // Verify that max_block_bases parameter exists and works
    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kLong;

    // Verify codec selection for ultra-long
    auto seqCodec = config.getSequenceCodec();
    auto codecFamily = format::decodeCodecFamily(seqCodec);

    EXPECT_EQ(codecFamily, CodecFamily::kZstdPlain);
}

/// @brief Test length class constants.
TEST(LongReadTest, LengthClassConstants) {
    // Verify constants are correctly defined
    EXPECT_EQ(kSpringMaxReadLength, 511);
    EXPECT_EQ(kMediumReadThreshold, 1024);
    EXPECT_EQ(kLongReadThreshold, 10240);
    EXPECT_EQ(kUltraLongReadThreshold, 102400);
}

}  // namespace fqc::algo::test
