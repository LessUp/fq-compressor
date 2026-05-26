// =============================================================================
// fq-compressor - Algorithm Regression Tests
// =============================================================================

#include "fqc/algo/global_analyzer.h"
#include "fqc/algo/pe_optimizer.h"
#include "fqc/algo/quality_compressor.h"

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace fqc::algo::test {

TEST(AlgoRegressionTest, LosslessQualityDecompressionRejectsEmptyPayloadForNonEmptyLengths) {
    QualityCompressorConfig config;
    config.qualityMode = QualityMode::kLossless;

    QualityCompressor compressor(config);
    const std::vector<std::uint32_t> lengths = {4};

    const auto result = compressor.decompress({}, lengths);

    EXPECT_FALSE(result.has_value());
}

TEST(AlgoRegressionTest, GenerateR2IdPreservesUnderscoreSuffixConvention) {
    const std::string r1Id = "instrument:run:flowcell_1";
    const auto r2Id = generateR2Id(r1Id);

    EXPECT_EQ(r2Id, "instrument:run:flowcell_2");
    EXPECT_TRUE(io::arePairedIds(r1Id, r2Id));
}

TEST(AlgoRegressionTest, ExtractMinimizersPreservesPositionsBeyondUint16Range) {
    std::string sequence(70000, 'A');

    const auto minimizers = extractMinimizers(sequence, 1, 1);

    ASSERT_FALSE(minimizers.empty());
    EXPECT_EQ(minimizers.back().position, 69999u);
}

TEST(AlgoRegressionTest, EncodeDecodePairHandlesDiffPositionsBeyondUint16Range) {
    io::PairedEndRecord pair;
    pair.read1.id = "long-read/1";
    pair.read1.sequence.assign(70000, 'A');
    pair.read1.quality.assign(70000, 'I');

    pair.read2.id = "long-read/2";
    pair.read2.sequence.assign(70000, 'T');
    pair.read2.sequence[66000] = 'G';
    pair.read2.quality.assign(70000, 'I');

    PEOptimizer optimizer;
    const auto encoded = optimizer.encodePair(pair);
    const auto decoded = optimizer.decodePair(encoded);

    EXPECT_EQ(decoded.read2.sequence, pair.read2.sequence);
    EXPECT_EQ(decoded.read2.quality, pair.read2.quality);
}

}  // namespace fqc::algo::test
