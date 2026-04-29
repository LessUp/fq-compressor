// =============================================================================
// fq-compressor - Block Compressor Regression Tests
// =============================================================================
// Regression tests for short-read ABC contig building safeguards.
// =============================================================================

#include "fqc/algo/block_compressor.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <zstd.h>

#include <gtest/gtest.h>

namespace fqc::algo::test {

namespace {

constexpr std::size_t kFarMatchGapCount = 1024;

std::string makeIndexedSequence(std::size_t index, std::size_t digits = 12) {
    static constexpr std::array<char, 4> kBases = {'A', 'C', 'G', 'T'};

    std::string sequence(digits + 2, 'A');
    sequence.front() = 'A';
    sequence.back() = 'C';

    for (std::size_t pos = 0; pos < digits; ++pos) {
        sequence[digits - pos] = kBases[index & 0x3U];
        index >>= 2U;
    }

    return sequence;
}

ReadRecord makeRead(std::string id, std::string sequence) {
    std::string quality(sequence.size(), 'F');
    return {std::move(id), "", std::move(sequence), std::move(quality)};
}

std::optional<std::uint32_t> readAbcContigCount(std::span<const std::uint8_t> seqStream) {
    if (seqStream.empty()) {
        return std::nullopt;
    }

    const std::size_t decompressedSize =
        ZSTD_getFrameContentSize(seqStream.data(), seqStream.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
        decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN || decompressedSize < sizeof(std::uint32_t)) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> buffer(decompressedSize);
    const std::size_t actualSize =
        ZSTD_decompress(buffer.data(), buffer.size(), seqStream.data(), seqStream.size());
    if (ZSTD_isError(actualSize) || actualSize < sizeof(std::uint32_t)) {
        return std::nullopt;
    }

    std::uint32_t numContigs = 0;
    std::memcpy(&numContigs, buffer.data(), sizeof(numContigs));
    return numContigs;
}

BlockCompressorConfig makeExactShortReadConfig() {
    BlockCompressorConfig config;
    config.readLengthClass = ReadLengthClass::kShort;
    config.qualityMode = QualityMode::kLossless;
    config.idMode = IDMode::kExact;
    config.maxShift = 0;
    config.consensusHammingThreshold = 0;
    return config;
}

void expectRoundTrip(BlockCompressor& compressor,
                     const std::vector<ReadRecord>& reads,
                     const CompressedBlockData& compressed) {
    auto decompressed = compressor.decompress(compressed);
    ASSERT_TRUE(decompressed.has_value());
    ASSERT_EQ(decompressed->reads.size(), reads.size());

    for (std::size_t i = 0; i < reads.size(); ++i) {
        EXPECT_EQ(decompressed->reads[i].id, reads[i].id);
        EXPECT_EQ(decompressed->reads[i].sequence, reads[i].sequence);
        EXPECT_EQ(decompressed->reads[i].quality, reads[i].quality);
    }
}

}  // namespace

TEST(BlockCompressorRegressionTest, NearbyMatchStaysInSameContig) {
    auto config = makeExactShortReadConfig();

    const std::string seed = makeIndexedSequence(0);
    std::vector<ReadRecord> reads;
    reads.push_back(makeRead("seed", seed));
    reads.push_back(makeRead("gap", makeIndexedSequence(1)));
    reads.push_back(makeRead("near_match", seed));

    BlockCompressor compressor(config);
    auto compressed = compressor.compress(reads, 0);
    ASSERT_TRUE(compressed.has_value());
    ASSERT_EQ(format::decodeCodecFamily(compressed->codecSeq), CodecFamily::kAbcV1);

    auto numContigs = readAbcContigCount(compressed->seqStream);
    ASSERT_TRUE(numContigs.has_value());
    EXPECT_EQ(*numContigs, 2u);

    expectRoundTrip(compressor, reads, *compressed);
}

TEST(BlockCompressorRegressionTest, FarMatchOutsideLookaheadStartsNewContig) {
    auto config = makeExactShortReadConfig();

    const std::string seed = makeIndexedSequence(0);
    std::vector<ReadRecord> reads;
    reads.reserve(kFarMatchGapCount + 2);
    reads.push_back(makeRead("seed", seed));

    for (std::size_t i = 1; i <= kFarMatchGapCount; ++i) {
        reads.push_back(makeRead("gap_" + std::to_string(i), makeIndexedSequence(i)));
    }

    reads.push_back(makeRead("far_match", seed));

    BlockCompressor compressor(config);
    auto compressed = compressor.compress(reads, 0);
    ASSERT_TRUE(compressed.has_value());
    ASSERT_EQ(format::decodeCodecFamily(compressed->codecSeq), CodecFamily::kAbcV1);

    auto numContigs = readAbcContigCount(compressed->seqStream);
    ASSERT_TRUE(numContigs.has_value());
    EXPECT_EQ(*numContigs, static_cast<std::uint32_t>(reads.size()));

    expectRoundTrip(compressor, reads, *compressed);
}

}  // namespace fqc::algo::test
