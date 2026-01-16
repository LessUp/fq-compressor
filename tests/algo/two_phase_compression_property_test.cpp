// =============================================================================
// fq-compressor - Two-Phase Compression Property Tests
// =============================================================================
// Property-based tests for two-phase compression strategy.
//
// **Property 3: 序列压缩往返一致性**
// *For any* 有效的 DNA 序列集合，压缩后解压应产生等价序列
//
// **Property 3.1: Reorder Map 往返一致性**
// *For any* 重排序映射，存储后读取应产生等价映射
//
// **Validates: Requirements 1.1.2, 2.1**
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
#include "fqc/algo/global_analyzer.h"
#include "fqc/common/types.h"
#include "fqc/format/reorder_map.h"

namespace fqc::algo::test {

// =============================================================================
// RapidCheck Generators
// =============================================================================

namespace gen {

/// @brief Generate a valid DNA base.
[[nodiscard]] rc::Gen<char> validBase() {
    return rc::gen::element('A', 'C', 'G', 'T');
}

/// @brief Generate a valid DNA base including N.
[[nodiscard]] rc::Gen<char> validBaseWithN() {
    return rc::gen::element('A', 'C', 'G', 'T', 'N');
}

/// @brief Generate a valid DNA sequence of specified length.
[[nodiscard]] rc::Gen<std::string> validSequence(std::size_t length) {
    return rc::gen::container<std::string>(length, validBase());
}

/// @brief Generate a valid DNA sequence with variable length.
[[nodiscard]] rc::Gen<std::string> validSequenceRange(std::size_t minLen, std::size_t maxLen) {
    return rc::gen::mapcat(
        rc::gen::inRange(minLen, maxLen + 1),
        [](std::size_t len) { return validSequence(len); });
}

/// @brief Generate a valid quality string of specified length.
[[nodiscard]] rc::Gen<std::string> validQuality(std::size_t length) {
    return rc::gen::container<std::string>(
        length,
        rc::gen::map(rc::gen::inRange(0, 42),
                     [](int phred) { return static_cast<char>('!' + phred); }));
}

/// @brief Generate a valid read ID.
[[nodiscard]] rc::Gen<std::string> validReadId() {
    return rc::gen::map(
        rc::gen::container<std::string>(
            rc::gen::inRange(5, 30),
            rc::gen::oneOf(rc::gen::inRange('a', 'z' + 1),
                           rc::gen::inRange('A', 'Z' + 1),
                           rc::gen::inRange('0', '9' + 1),
                           rc::gen::element('_', '-', ':', '.'))),
        [](std::string s) {
            if (!s.empty() && std::isdigit(static_cast<unsigned char>(s[0]))) {
                s[0] = 'R';
            }
            return s;
        });
}

/// @brief Generate a valid ReadRecord with uniform length.
[[nodiscard]] rc::Gen<ReadRecord> validReadRecord(std::size_t seqLength) {
    return rc::gen::map(
        rc::gen::tuple(validReadId(), validSequence(seqLength), validQuality(seqLength)),
        [](const auto& tuple) {
            auto [id, seq, qual] = tuple;
            ReadRecord record;
            record.id = std::move(id);
            record.sequence = std::move(seq);
            record.quality = std::move(qual);
            return record;
        });
}

/// @brief Generate a vector of ReadRecords with uniform length.
[[nodiscard]] rc::Gen<std::vector<ReadRecord>> validReadRecords(std::size_t count,
                                                                  std::size_t seqLength) {
    return rc::gen::container<std::vector<ReadRecord>>(count, validReadRecord(seqLength));
}

/// @brief Generate a permutation of indices (for reorder map testing).
[[nodiscard]] rc::Gen<std::vector<ReadId>> validPermutation(std::size_t size) {
    return rc::gen::map(rc::gen::just(size), [](std::size_t n) {
        std::vector<ReadId> perm(n);
        std::iota(perm.begin(), perm.end(), 0);
        std::shuffle(perm.begin(), perm.end(), std::mt19937{std::random_device{}()});
        return perm;
    });
}

/// @brief Generate a forward/reverse map pair (consistent permutation).
[[nodiscard]] rc::Gen<std::pair<std::vector<ReadId>, std::vector<ReadId>>>
validReorderMapPair(std::size_t size) {
    return rc::gen::map(validPermutation(size), [](std::vector<ReadId> forwardMap) {
        std::vector<ReadId> reverseMap(forwardMap.size());
        for (std::size_t i = 0; i < forwardMap.size(); ++i) {
            reverseMap[forwardMap[i]] = static_cast<ReadId>(i);
        }
        return std::make_pair(std::move(forwardMap), std::move(reverseMap));
    });
}

}  // namespace gen

// =============================================================================
// Property Tests - Reorder Map
// =============================================================================

/// @brief Property 3.1: Reorder Map round-trip consistency.
/// **Validates: Requirements 2.1**
RC_GTEST_PROP(ReorderMapProperty, RoundTripConsistency, ()) {
    auto size = *rc::gen::inRange<std::size_t>(10, 1000);
    auto [forwardMap, reverseMap] = *gen::validReorderMapPair(size);

    // Create ReorderMapData
    format::ReorderMapData mapData(forwardMap, reverseMap);

    // Verify initial state
    RC_ASSERT(mapData.totalReads() == size);
    RC_ASSERT(mapData.isValid());

    // Serialize
    auto serialized = mapData.serialize();
    RC_ASSERT(!serialized.empty());

    // Deserialize
    auto result = format::ReorderMapData::deserialize(serialized);
    RC_ASSERT(result.has_value());

    // Verify round-trip
    RC_ASSERT(result->totalReads() == size);
    RC_ASSERT(result->forwardMap() == forwardMap);
    RC_ASSERT(result->reverseMap() == reverseMap);
}

/// @brief Property 3.1.1: Reorder Map query consistency.
/// **Validates: Requirements 2.1**
RC_GTEST_PROP(ReorderMapProperty, QueryConsistency, ()) {
    auto size = *rc::gen::inRange<std::size_t>(10, 500);
    auto [forwardMap, reverseMap] = *gen::validReorderMapPair(size);

    format::ReorderMapData mapData(forwardMap, reverseMap);

    // Verify forward -> reverse -> forward consistency
    for (std::size_t i = 0; i < size; ++i) {
        ReadId archiveId = mapData.getArchiveId(static_cast<ReadId>(i));
        ReadId originalId = mapData.getOriginalId(archiveId);
        RC_ASSERT(originalId == static_cast<ReadId>(i));
    }

    // Verify reverse -> forward -> reverse consistency
    for (std::size_t i = 0; i < size; ++i) {
        ReadId originalId = mapData.getOriginalId(static_cast<ReadId>(i));
        ReadId archiveId = mapData.getArchiveId(originalId);
        RC_ASSERT(archiveId == static_cast<ReadId>(i));
    }
}

/// @brief Property 3.1.2: Reorder Map compression efficiency.
/// **Validates: Requirements 2.1**
RC_GTEST_PROP(ReorderMapProperty, CompressionEfficiency, ()) {
    auto size = *rc::gen::inRange<std::size_t>(100, 5000);
    auto [forwardMap, reverseMap] = *gen::validReorderMapPair(size);

    format::ReorderMapData mapData(forwardMap, reverseMap);
    auto stats = mapData.getCompressionStats();

    // Target: ~4 bytes/read for both maps
    // Allow some margin (up to 8 bytes/read for worst case)
    RC_ASSERT(stats.bytesPerRead <= 8.0);

    // Verify compression ratio is positive
    RC_ASSERT(stats.compressionRatio > 0.0);
}

/// @brief Property 3.1.3: Reorder Map chunk concatenation.
/// **Validates: Requirements 2.1, 4.3**
RC_GTEST_PROP(ReorderMapProperty, ChunkConcatenation, ()) {
    auto numChunks = *rc::gen::inRange(2, 5);
    std::vector<format::ReorderMapData> chunks;
    std::vector<std::uint64_t> chunkSizes;

    std::uint64_t totalSize = 0;
    for (int i = 0; i < numChunks; ++i) {
        auto size = *rc::gen::inRange<std::size_t>(50, 200);
        auto [forwardMap, reverseMap] = *gen::validReorderMapPair(size);
        chunks.emplace_back(forwardMap, reverseMap);
        chunkSizes.push_back(size);
        totalSize += size;
    }

    // Combine chunks
    auto combined = format::ReorderMapData::combineChunks(chunks, chunkSizes);

    // Verify combined size
    RC_ASSERT(combined.totalReads() == totalSize);
    RC_ASSERT(combined.isValid());

    // Verify all IDs are covered
    std::vector<bool> seenOriginal(totalSize, false);
    std::vector<bool> seenArchive(totalSize, false);

    for (std::uint64_t i = 0; i < totalSize; ++i) {
        ReadId archiveId = combined.getArchiveId(static_cast<ReadId>(i));
        ReadId originalId = combined.getOriginalId(static_cast<ReadId>(i));

        RC_ASSERT(archiveId < totalSize);
        RC_ASSERT(originalId < totalSize);

        seenArchive[archiveId] = true;
        seenOriginal[originalId] = true;
    }

    // All IDs should be covered exactly once
    RC_ASSERT(std::all_of(seenOriginal.begin(), seenOriginal.end(), [](bool b) { return b; }));
    RC_ASSERT(std::all_of(seenArchive.begin(), seenArchive.end(), [](bool b) { return b; }));
}

// =============================================================================
// Property Tests - Block Compression
// =============================================================================

/// @brief Property 3: Sequence compression round-trip consistency.
/// **Validates: Requirements 1.1.2, 2.1**
RC_GTEST_PROP(BlockCompressionProperty, SequenceRoundTrip, ()) {
    auto numReads = *rc::gen::inRange(10, 100);
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 200);
    auto reads = *gen::validReadRecords(static_cast<std::size_t>(numReads), seqLength);

    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kShort;
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

/// @brief Property 3.2: Block compression with variable length reads.
/// **Validates: Requirements 1.1.2, 2.1**
RC_GTEST_PROP(BlockCompressionProperty, VariableLengthRoundTrip, ()) {
    auto numReads = *rc::gen::inRange(10, 50);
    std::vector<ReadRecord> reads;
    reads.reserve(numReads);

    for (int i = 0; i < numReads; ++i) {
        auto seqLength = *rc::gen::inRange<std::size_t>(50, 300);
        reads.push_back(*gen::validReadRecord(seqLength));
    }

    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kShort;
    config.qualityMode = QualityMode::kLossless;

    BlockCompressor compressor(config);

    // Compress
    auto compressResult = compressor.compress(reads, 0);
    RC_ASSERT(compressResult.has_value());

    // Should have aux stream for variable lengths
    if (!compressResult->hasUniformLength()) {
        RC_ASSERT(!compressResult->auxStream.empty());
    }

    // Decompress
    auto decompressResult = compressor.decompress(*compressResult);
    RC_ASSERT(decompressResult.has_value());

    // Verify round-trip
    RC_ASSERT(decompressResult->reads.size() == reads.size());

    for (std::size_t i = 0; i < reads.size(); ++i) {
        RC_ASSERT(decompressResult->reads[i].sequence == reads[i].sequence);
        RC_ASSERT(decompressResult->reads[i].quality == reads[i].quality);
    }
}

/// @brief Property 3.3: Block compression with quality discard mode.
/// **Validates: Requirements 3.4**
RC_GTEST_PROP(BlockCompressionProperty, QualityDiscardRoundTrip, ()) {
    auto numReads = *rc::gen::inRange(10, 50);
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 150);
    auto reads = *gen::validReadRecords(static_cast<std::size_t>(numReads), seqLength);

    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kShort;
    config.qualityMode = QualityMode::kDiscard;

    BlockCompressor compressor(config);

    // Compress
    auto compressResult = compressor.compress(reads, 0);
    RC_ASSERT(compressResult.has_value());

    // Quality should be discarded
    RC_ASSERT(compressResult->isQualityDiscarded());

    // Decompress
    auto decompressResult = compressor.decompress(*compressResult);
    RC_ASSERT(decompressResult.has_value());

    // Verify sequences match, quality is placeholder
    RC_ASSERT(decompressResult->reads.size() == reads.size());

    for (std::size_t i = 0; i < reads.size(); ++i) {
        RC_ASSERT(decompressResult->reads[i].sequence == reads[i].sequence);
        // Quality should be placeholder (all '!')
        RC_ASSERT(decompressResult->reads[i].quality.size() == reads[i].sequence.size());
        RC_ASSERT(std::all_of(decompressResult->reads[i].quality.begin(),
                              decompressResult->reads[i].quality.end(),
                              [](char c) { return c == '!'; }));
    }
}

/// @brief Property 3.4: Block compression checksum integrity.
/// **Validates: Requirements 5.1, 5.2**
RC_GTEST_PROP(BlockCompressionProperty, ChecksumIntegrity, ()) {
    auto numReads = *rc::gen::inRange(10, 50);
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 150);
    auto reads = *gen::validReadRecords(static_cast<std::size_t>(numReads), seqLength);

    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kShort;
    config.qualityMode = QualityMode::kLossless;

    BlockCompressor compressor(config);

    // Compress
    auto compressResult = compressor.compress(reads, 0);
    RC_ASSERT(compressResult.has_value());

    // Checksum should be non-zero
    RC_ASSERT(compressResult->blockChecksum != 0);

    // Same input should produce same checksum
    auto compressResult2 = compressor.compress(reads, 0);
    RC_ASSERT(compressResult2.has_value());
    RC_ASSERT(compressResult2->blockChecksum == compressResult->blockChecksum);
}

// =============================================================================
// Property Tests - Delta Encoding
// =============================================================================

/// @brief Property 3.5: Delta encoding round-trip consistency.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(DeltaEncodingProperty, RoundTripConsistency, ()) {
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 200);
    auto readSeq = *gen::validSequence(seqLength);
    auto consensusSeq = *gen::validSequence(seqLength);

    // Compute delta
    auto delta = computeDelta(readSeq, consensusSeq, 0, false);

    // Reconstruct
    auto reconstructed = reconstructFromDelta(delta, consensusSeq);

    // Verify round-trip
    RC_ASSERT(reconstructed == readSeq);
}

/// @brief Property 3.6: Delta encoding with shift.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(DeltaEncodingProperty, ShiftedRoundTrip, ()) {
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 150);
    auto shift = *rc::gen::inRange(-10, 11);
    auto readSeq = *gen::validSequence(seqLength);

    // Create consensus that's longer to accommodate shift
    auto consensusLength = seqLength + static_cast<std::size_t>(std::abs(shift)) + 10;
    auto consensusSeq = *gen::validSequence(consensusLength);

    // Compute delta with shift
    auto delta = computeDelta(readSeq, consensusSeq, shift, false);

    // Reconstruct
    auto reconstructed = reconstructFromDelta(delta, consensusSeq);

    // Verify round-trip
    RC_ASSERT(reconstructed == readSeq);
}

/// @brief Property 3.7: Delta encoding with reverse complement.
/// **Validates: Requirements 1.1.2**
RC_GTEST_PROP(DeltaEncodingProperty, ReverseComplementRoundTrip, ()) {
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 150);
    auto readSeq = *gen::validSequence(seqLength);
    auto consensusSeq = *gen::validSequence(seqLength);

    // Compute delta with reverse complement
    auto delta = computeDelta(readSeq, consensusSeq, 0, true);

    // Reconstruct
    auto reconstructed = reconstructFromDelta(delta, consensusSeq);

    // Verify round-trip
    RC_ASSERT(reconstructed == readSeq);
}

// =============================================================================
// Property Tests - Varint Encoding
// =============================================================================

/// @brief Property: Varint encoding round-trip consistency.
/// **Validates: Requirements 2.1**
RC_GTEST_PROP(VarintEncodingProperty, RoundTripConsistency, ()) {
    auto value = *rc::gen::arbitrary<std::uint64_t>();

    std::uint8_t buffer[format::kMaxVarintBytes];
    auto bytesWritten = format::encodeVarint(value, buffer);

    RC_ASSERT(bytesWritten > 0);
    RC_ASSERT(bytesWritten <= format::kMaxVarintBytes);

    std::uint64_t decoded = 0;
    auto bytesRead = format::decodeVarint(buffer, bytesWritten, decoded);

    RC_ASSERT(bytesRead == bytesWritten);
    RC_ASSERT(decoded == value);
}

/// @brief Property: Signed varint encoding round-trip consistency.
/// **Validates: Requirements 2.1**
RC_GTEST_PROP(VarintEncodingProperty, SignedRoundTripConsistency, ()) {
    auto value = *rc::gen::arbitrary<std::int64_t>();

    std::uint8_t buffer[format::kMaxVarintBytes];
    auto bytesWritten = format::encodeSignedVarint(value, buffer);

    RC_ASSERT(bytesWritten > 0);
    RC_ASSERT(bytesWritten <= format::kMaxVarintBytes);

    std::int64_t decoded = 0;
    auto bytesRead = format::decodeSignedVarint(buffer, bytesWritten, decoded);

    RC_ASSERT(bytesRead == bytesWritten);
    RC_ASSERT(decoded == value);
}

/// @brief Property: Delta encoding of ID sequences.
/// **Validates: Requirements 2.1**
RC_GTEST_PROP(DeltaVarintProperty, IdSequenceRoundTrip, ()) {
    auto size = *rc::gen::inRange<std::size_t>(10, 500);
    std::vector<ReadId> ids(size);
    std::iota(ids.begin(), ids.end(), 0);

    // Optionally shuffle to create non-sequential IDs
    if (*rc::gen::element(false, true)) {
        std::shuffle(ids.begin(), ids.end(), std::mt19937{std::random_device{}()});
    }

    // Encode
    auto encoded = format::deltaEncode(ids);
    RC_ASSERT(!encoded.empty());

    // Decode
    auto decoded = format::deltaDecode(encoded, size);
    RC_ASSERT(decoded.has_value());
    RC_ASSERT(*decoded == ids);
}

// =============================================================================
// Property Tests - Utility Functions
// =============================================================================

/// @brief Property: Hamming distance symmetry.
RC_GTEST_PROP(UtilityProperty, HammingDistanceSymmetry, ()) {
    auto length = *rc::gen::inRange<std::size_t>(10, 100);
    auto seq1 = *gen::validSequence(length);
    auto seq2 = *gen::validSequence(length);

    auto dist1 = hammingDistance(seq1, seq2);
    auto dist2 = hammingDistance(seq2, seq1);

    RC_ASSERT(dist1 == dist2);
}

/// @brief Property: Hamming distance identity.
RC_GTEST_PROP(UtilityProperty, HammingDistanceIdentity, ()) {
    auto length = *rc::gen::inRange<std::size_t>(10, 100);
    auto seq = *gen::validSequence(length);

    auto dist = hammingDistance(seq, seq);
    RC_ASSERT(dist == 0);
}

/// @brief Property: Reverse complement involution.
RC_GTEST_PROP(UtilityProperty, ReverseComplementInvolution, ()) {
    auto length = *rc::gen::inRange<std::size_t>(10, 100);
    auto seq = *gen::validSequence(length);

    auto rc1 = reverseComplement(seq);
    auto rc2 = reverseComplement(rc1);

    RC_ASSERT(rc2 == seq);
}

/// @brief Property: Noise encoding round-trip.
RC_GTEST_PROP(UtilityProperty, NoiseEncodingRoundTrip, ()) {
    auto refBase = *rc::gen::element('A', 'C', 'G', 'T');
    auto readBase = *rc::gen::element('A', 'C', 'G', 'T');

    auto noise = encodeNoise(refBase, readBase);
    auto decoded = decodeNoise(refBase, noise);

    RC_ASSERT(decoded == readBase);
}

// =============================================================================
// Unit Tests (Non-Property)
// =============================================================================

/// @brief Test empty reorder map handling.
TEST(ReorderMapTest, EmptyMap) {
    format::ReorderMapData mapData;

    EXPECT_TRUE(mapData.empty());
    EXPECT_EQ(mapData.totalReads(), 0);
    EXPECT_TRUE(mapData.isValid());

    // Serialize empty map
    auto serialized = mapData.serialize();
    EXPECT_FALSE(serialized.empty());

    // Deserialize
    auto result = format::ReorderMapData::deserialize(serialized);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

/// @brief Test identity reorder map (no reordering).
TEST(ReorderMapTest, IdentityMap) {
    std::size_t size = 100;
    std::vector<ReadId> identity(size);
    std::iota(identity.begin(), identity.end(), 0);

    format::ReorderMapData mapData(identity, identity);

    EXPECT_EQ(mapData.totalReads(), size);
    EXPECT_TRUE(mapData.isValid());

    // All queries should return same ID
    for (std::size_t i = 0; i < size; ++i) {
        EXPECT_EQ(mapData.getArchiveId(static_cast<ReadId>(i)), static_cast<ReadId>(i));
        EXPECT_EQ(mapData.getOriginalId(static_cast<ReadId>(i)), static_cast<ReadId>(i));
    }
}

/// @brief Test reorder map validation.
TEST(ReorderMapTest, Validation) {
    // Valid map
    std::vector<ReadId> forward = {2, 0, 1};
    std::vector<ReadId> reverse = {1, 2, 0};

    auto result = format::verifyMapConsistency(forward, reverse);
    EXPECT_TRUE(result.has_value());

    // Invalid map (inconsistent)
    std::vector<ReadId> badReverse = {0, 1, 2};
    auto badResult = format::verifyMapConsistency(forward, badReverse);
    EXPECT_FALSE(badResult.has_value());
}

/// @brief Test block compressor with empty input.
TEST(BlockCompressorTest, EmptyInput) {
    BlockCompressorConfig config;
    BlockCompressor compressor(config);

    std::vector<ReadRecord> emptyReads;
    auto result = compressor.compress(emptyReads, 0);

    // Empty input should succeed with empty output
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->readCount, 0);
}

/// @brief Test block compressor configuration validation.
TEST(BlockCompressorTest, ConfigValidation) {
    BlockCompressorConfig config;
    config.compressionLevel = 5;
    config.readLengthClass = ReadLengthClass::kShort;

    auto result = config.validate();
    EXPECT_TRUE(result.has_value());

    // Invalid compression level
    config.compressionLevel = 100;
    result = config.validate();
    EXPECT_FALSE(result.has_value());
}

/// @brief Test consensus sequence building.
TEST(ConsensusTest, InitFromRead) {
    ConsensusSequence consensus;
    consensus.initFromRead("ACGT");

    EXPECT_EQ(consensus.sequence, "ACGT");
    EXPECT_EQ(consensus.contributingReads, 1);
    EXPECT_EQ(consensus.baseCounts.size(), 4);
}

/// @brief Test consensus sequence update.
TEST(ConsensusTest, AddRead) {
    ConsensusSequence consensus;
    consensus.initFromRead("ACGT");
    consensus.addRead("ACGT", 0, false);  // Same read
    consensus.recomputeConsensus();

    EXPECT_EQ(consensus.sequence, "ACGT");
    EXPECT_EQ(consensus.contributingReads, 2);

    // Add different read
    consensus.addRead("ACGA", 0, false);  // Last base different
    consensus.recomputeConsensus();

    // Majority should still be T at position 3
    EXPECT_EQ(consensus.sequence, "ACGT");
    EXPECT_EQ(consensus.contributingReads, 3);
}

/// @brief Test Hamming distance calculation.
TEST(UtilityTest, HammingDistance) {
    EXPECT_EQ(hammingDistance("ACGT", "ACGT"), 0);
    EXPECT_EQ(hammingDistance("ACGT", "ACGA"), 1);
    EXPECT_EQ(hammingDistance("ACGT", "TGCA"), 4);

    // Early exit
    EXPECT_EQ(hammingDistance("ACGT", "TGCA", 2), 3);  // Exceeds max
}

/// @brief Test reverse complement.
TEST(UtilityTest, ReverseComplement) {
    EXPECT_EQ(reverseComplement("ACGT"), "ACGT");  // Palindrome
    EXPECT_EQ(reverseComplement("AAAA"), "TTTT");
    EXPECT_EQ(reverseComplement("CCCC"), "GGGG");
    EXPECT_EQ(reverseComplement("AACG"), "CGTT");
}

/// @brief Test noise encoding.
TEST(UtilityTest, NoiseEncoding) {
    // Same base should encode to '0'
    EXPECT_EQ(encodeNoise('A', 'A'), '0');
    EXPECT_EQ(encodeNoise('C', 'C'), '0');

    // Different bases should encode to '1', '2', or '3'
    char noise = encodeNoise('A', 'C');
    EXPECT_GE(noise, '0');
    EXPECT_LE(noise, '3');

    // Round-trip
    EXPECT_EQ(decodeNoise('A', noise), 'C');
}

}  // namespace fqc::algo::test
