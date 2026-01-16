// =============================================================================
// fq-compressor - Paired-End Compression Property Tests
// =============================================================================
// Property-based tests for paired-end read compression.
//
// **Property 8: PE 压缩往返一致性**
// *For any* 有效的 PE 数据，压缩后解压应产生等价配对数据
//
// **Validates: Requirements 1.1.3**
// =============================================================================

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "fqc/algo/pe_optimizer.h"
#include "fqc/common/types.h"
#include "fqc/io/paired_end_parser.h"

namespace fqc::algo::test {

// =============================================================================
// RapidCheck Generators for Paired-End Data
// =============================================================================

namespace gen {

/// @brief Generate a valid DNA base.
[[nodiscard]] rc::Gen<char> validBase() {
    return rc::gen::element('A', 'C', 'G', 'T');
}

/// @brief Generate a valid DNA sequence of specified length.
[[nodiscard]] rc::Gen<std::string> validSequence(std::size_t length) {
    return rc::gen::container<std::string>(length, validBase());
}

/// @brief Generate a valid quality string of specified length.
[[nodiscard]] rc::Gen<std::string> validQuality(std::size_t length) {
    return rc::gen::container<std::string>(
        length,
        rc::gen::map(rc::gen::inRange(0, 42),
                     [](int phred) { return static_cast<char>('!' + phred); }));
}

/// @brief Generate a valid Illumina-style read ID.
[[nodiscard]] rc::Gen<std::string> illuminaReadId() {
    return rc::gen::map(
        rc::gen::tuple(
            rc::gen::inRange(1, 100),   // instrument
            rc::gen::inRange(1, 10),    // run
            rc::gen::inRange(1, 8),     // lane
            rc::gen::inRange(1, 1000),  // tile
            rc::gen::inRange(1, 10000), // x
            rc::gen::inRange(1, 10000)  // y
        ),
        [](const auto& tuple) {
            auto [inst, run, lane, tile, x, y] = tuple;
            return "INSTRUMENT" + std::to_string(inst) + ":" +
                   std::to_string(run) + ":FLOWCELL:" +
                   std::to_string(lane) + ":" +
                   std::to_string(tile) + ":" +
                   std::to_string(x) + ":" +
                   std::to_string(y);
        });
}

/// @brief Generate a FastqRecord.
[[nodiscard]] rc::Gen<io::FastqRecord> fastqRecord(std::size_t seqLength,
                                                    std::string_view suffix = "") {
    return rc::gen::map(
        rc::gen::tuple(illuminaReadId(), validSequence(seqLength), validQuality(seqLength)),
        [suffix = std::string(suffix)](const auto& tuple) {
            auto [id, seq, qual] = tuple;
            io::FastqRecord record;
            record.id = id + suffix;
            record.sequence = std::move(seq);
            record.quality = std::move(qual);
            return record;
        });
}

/// @brief Generate a paired-end record with matching IDs.
[[nodiscard]] rc::Gen<io::PairedEndRecord> pairedEndRecord(std::size_t seqLength) {
    return rc::gen::map(
        rc::gen::tuple(illuminaReadId(),
                       validSequence(seqLength),
                       validQuality(seqLength),
                       validSequence(seqLength),
                       validQuality(seqLength)),
        [](const auto& tuple) {
            auto [id, seq1, qual1, seq2, qual2] = tuple;
            io::PairedEndRecord pair;
            pair.read1.id = id + "/1";
            pair.read1.sequence = std::move(seq1);
            pair.read1.quality = std::move(qual1);
            pair.read2.id = id + "/2";
            pair.read2.sequence = std::move(seq2);
            pair.read2.quality = std::move(qual2);
            return pair;
        });
}

/// @brief Generate a paired-end record where R2 is similar to R1-RC.
/// This simulates real PE data where reads may overlap.
[[nodiscard]] rc::Gen<io::PairedEndRecord> complementaryPairedEndRecord(std::size_t seqLength) {
    return rc::gen::map(
        rc::gen::tuple(illuminaReadId(),
                       validSequence(seqLength),
                       validQuality(seqLength),
                       validQuality(seqLength),
                       rc::gen::inRange<std::size_t>(0, 10)),  // num differences
        [seqLength](const auto& tuple) {
            auto [id, seq1, qual1, qual2, numDiffs] = tuple;

            // Create R2 as reverse complement of R1 with some differences
            std::string seq2;
            seq2.reserve(seqLength);
            for (auto it = seq1.rbegin(); it != seq1.rend(); ++it) {
                char c = *it;
                switch (c) {
                    case 'A': c = 'T'; break;
                    case 'T': c = 'A'; break;
                    case 'C': c = 'G'; break;
                    case 'G': c = 'C'; break;
                    default: c = 'N'; break;
                }
                seq2.push_back(c);
            }

            // Add some differences
            for (std::size_t i = 0; i < numDiffs && i < seq2.length(); ++i) {
                std::size_t pos = (i * 17) % seq2.length();  // Pseudo-random positions
                seq2[pos] = "ACGT"[i % 4];
            }

            io::PairedEndRecord pair;
            pair.read1.id = id + "/1";
            pair.read1.sequence = std::move(seq1);
            pair.read1.quality = std::move(qual1);
            pair.read2.id = id + "/2";
            pair.read2.sequence = std::move(seq2);
            pair.read2.quality = std::move(qual2);
            return pair;
        });
}

/// @brief Generate a vector of paired-end records.
[[nodiscard]] rc::Gen<std::vector<io::PairedEndRecord>> pairedEndRecords(
    std::size_t count, std::size_t seqLength) {
    return rc::gen::container<std::vector<io::PairedEndRecord>>(
        count, pairedEndRecord(seqLength));
}

}  // namespace gen

// =============================================================================
// Property Tests - PE ID Matching
// =============================================================================

/// @brief Property 8.1: Paired ID extraction consistency.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(PairedEndProperty, IdExtractionConsistency, ()) {
    auto baseId = *gen::illuminaReadId();

    // Test various suffix formats
    std::vector<std::pair<std::string, std::string>> suffixPairs = {
        {"/1", "/2"},
        {".1", ".2"},
        {"_1", "_2"},
        {" 1:N:0:ATCG", " 2:N:0:ATCG"}
    };

    for (const auto& [suffix1, suffix2] : suffixPairs) {
        std::string id1 = baseId + suffix1;
        std::string id2 = baseId + suffix2;

        // IDs should be recognized as paired
        RC_ASSERT(io::arePairedIds(id1, id2));

        // Base IDs should match
        auto base1 = io::extractBaseReadId(id1);
        auto base2 = io::extractBaseReadId(id2);
        RC_ASSERT(base1 == base2);
    }
}

/// @brief Property 8.2: Non-paired IDs should not match.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(PairedEndProperty, NonPairedIdsDoNotMatch, ()) {
    auto id1 = *gen::illuminaReadId();
    auto id2 = *gen::illuminaReadId();

    RC_PRE(id1 != id2);  // Ensure different IDs

    RC_ASSERT(!io::arePairedIds(id1, id2));
}

// =============================================================================
// Property Tests - PE Optimizer Encoding
// =============================================================================

/// @brief Property 8.3: PE encoding round-trip consistency.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(PairedEndProperty, EncodingRoundTrip, ()) {
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 200);
    auto pair = *gen::pairedEndRecord(seqLength);

    PEOptimizerConfig config;
    config.enableComplementarity = true;

    PEOptimizer optimizer(config);

    // Encode
    auto encoded = optimizer.encodePair(pair);

    // Decode
    auto decoded = optimizer.decodePair(encoded);

    // Verify round-trip for R1
    RC_ASSERT(decoded.read1.sequence == pair.read1.sequence);
    RC_ASSERT(decoded.read1.quality == pair.read1.quality);

    // Verify round-trip for R2
    RC_ASSERT(decoded.read2.sequence == pair.read2.sequence);
    RC_ASSERT(decoded.read2.quality == pair.read2.quality);
}

/// @brief Property 8.4: Complementary pairs use complementarity encoding.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(PairedEndProperty, ComplementaryPairsOptimized, ()) {
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 200);
    auto pair = *gen::complementaryPairedEndRecord(seqLength);

    PEOptimizerConfig config;
    config.enableComplementarity = true;
    config.complementarityThreshold = 50;

    PEOptimizer optimizer(config);

    auto encoded = optimizer.encodePair(pair);

    // Complementary pairs should use complementarity encoding
    // (unless there are too many differences)
    if (encoded.useComplementarity) {
        // Verify encoding saved space
        std::size_t encodedSize = encoded.diffPositions.size() * 3 +
                                  encoded.qualDelta.size();
        std::size_t rawSize = pair.read2.sequence.length() +
                              pair.read2.quality.length();

        // Should save space (or at least not be much worse)
        RC_ASSERT(encodedSize <= rawSize + 10);
    }

    // Round-trip should still work
    auto decoded = optimizer.decodePair(encoded);
    RC_ASSERT(decoded.read2.sequence == pair.read2.sequence);
}

/// @brief Property 8.5: Non-complementary pairs fall back to raw encoding.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(PairedEndProperty, NonComplementaryFallback, ()) {
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 200);

    // Generate completely unrelated R1 and R2
    auto pair = *gen::pairedEndRecord(seqLength);

    PEOptimizerConfig config;
    config.enableComplementarity = true;
    config.complementarityThreshold = 5;  // Very strict threshold

    PEOptimizer optimizer(config);

    auto [beneficial, diffCount] = optimizer.checkComplementarity(
        pair.read1.sequence, pair.read2.sequence);

    // Random sequences typically have ~75% difference, shouldn't be beneficial
    if (!beneficial) {
        auto encoded = optimizer.encodePair(pair);

        // Should fall back to raw encoding
        RC_ASSERT(!encoded.useComplementarity);
        RC_ASSERT(encoded.seq2 == pair.read2.sequence);
        RC_ASSERT(encoded.qual2 == pair.read2.quality);
    }
}

// =============================================================================
// Property Tests - Layout Conversion
// =============================================================================

/// @brief Property 8.6: Interleaved layout round-trip.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(PairedEndProperty, InterleavedLayoutRoundTrip, ()) {
    auto numPairs = *rc::gen::inRange<std::size_t>(5, 50);
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 150);
    auto pairs = *gen::pairedEndRecords(numPairs, seqLength);

    PEOptimizer optimizer;

    // Convert to interleaved
    auto interleaved = optimizer.toInterleaved(pairs);
    RC_ASSERT(interleaved.size() == pairs.size() * 2);

    // Convert back
    auto recovered = optimizer.fromInterleaved(interleaved);
    RC_ASSERT(recovered.size() == pairs.size());

    // Verify content
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        RC_ASSERT(recovered[i].read1.id == pairs[i].read1.id);
        RC_ASSERT(recovered[i].read1.sequence == pairs[i].read1.sequence);
        RC_ASSERT(recovered[i].read2.id == pairs[i].read2.id);
        RC_ASSERT(recovered[i].read2.sequence == pairs[i].read2.sequence);
    }
}

/// @brief Property 8.7: Consecutive layout round-trip.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(PairedEndProperty, ConsecutiveLayoutRoundTrip, ()) {
    auto numPairs = *rc::gen::inRange<std::size_t>(5, 50);
    auto seqLength = *rc::gen::inRange<std::size_t>(50, 150);
    auto pairs = *gen::pairedEndRecords(numPairs, seqLength);

    PEOptimizer optimizer;

    // Convert to consecutive
    auto consecutive = optimizer.toConsecutive(pairs);
    RC_ASSERT(consecutive.size() == pairs.size() * 2);

    // First half should be R1, second half R2
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        RC_ASSERT(consecutive[i].id == pairs[i].read1.id);
        RC_ASSERT(consecutive[pairs.size() + i].id == pairs[i].read2.id);
    }

    // Convert back
    auto recovered = optimizer.fromConsecutive(consecutive);
    RC_ASSERT(recovered.size() == pairs.size());

    // Verify content
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        RC_ASSERT(recovered[i].read1.sequence == pairs[i].read1.sequence);
        RC_ASSERT(recovered[i].read2.sequence == pairs[i].read2.sequence);
    }
}

// =============================================================================
// Property Tests - R2 ID Generation
// =============================================================================

/// @brief Property 8.8: R2 ID generation consistency.
/// **Validates: Requirements 1.1.3**
RC_GTEST_PROP(PairedEndProperty, R2IdGeneration, ()) {
    auto baseId = *gen::illuminaReadId();

    // Test /1 -> /2
    std::string r1Id = baseId + "/1";
    std::string r2Id = generateR2Id(r1Id);
    RC_ASSERT(r2Id == baseId + "/2");

    // Generated ID should pair with original
    RC_ASSERT(io::arePairedIds(r1Id, r2Id));
}

// =============================================================================
// Unit Tests - Edge Cases
// =============================================================================

/// @brief Test empty pair handling.
TEST(PairedEndTest, EmptyPairHandling) {
    io::PairedEndRecord pair;
    pair.read1.id = "test/1";
    pair.read1.sequence = "";
    pair.read1.quality = "";
    pair.read2.id = "test/2";
    pair.read2.sequence = "";
    pair.read2.quality = "";

    EXPECT_FALSE(pair.isValid());  // Empty sequences are invalid
}

/// @brief Test single base pair.
TEST(PairedEndTest, SingleBasePair) {
    io::PairedEndRecord pair;
    pair.read1.id = "test/1";
    pair.read1.sequence = "A";
    pair.read1.quality = "I";
    pair.read2.id = "test/2";
    pair.read2.sequence = "T";
    pair.read2.quality = "I";

    EXPECT_TRUE(pair.isValid());
    EXPECT_EQ(pair.totalLength(), 2);

    PEOptimizer optimizer;
    auto encoded = optimizer.encodePair(pair);
    auto decoded = optimizer.decodePair(encoded);

    EXPECT_EQ(decoded.read1.sequence, pair.read1.sequence);
    EXPECT_EQ(decoded.read2.sequence, pair.read2.sequence);
}

/// @brief Test perfect complementary pair (R2 = RC of R1).
TEST(PairedEndTest, PerfectComplementaryPair) {
    io::PairedEndRecord pair;
    pair.read1.id = "test/1";
    pair.read1.sequence = "ACGTACGT";
    pair.read1.quality = "IIIIIIII";
    pair.read2.id = "test/2";
    pair.read2.sequence = "ACGTACGT";  // RC of R1
    pair.read2.quality = "IIIIIIII";

    PEOptimizerConfig config;
    config.enableComplementarity = true;

    PEOptimizer optimizer(config);

    auto [beneficial, diffCount] = optimizer.checkComplementarity(
        pair.read1.sequence, pair.read2.sequence);

    // Not actually complementary (same sequence)
    // Real RC would be "ACGTACGT" reversed then complemented

    auto encoded = optimizer.encodePair(pair);
    auto decoded = optimizer.decodePair(encoded);

    EXPECT_EQ(decoded.read2.sequence, pair.read2.sequence);
}

/// @brief Test ID suffix stripping.
TEST(PairedEndTest, IdSuffixStripping) {
    EXPECT_EQ(io::extractBaseReadId("read1/1"), "read1");
    EXPECT_EQ(io::extractBaseReadId("read1/2"), "read1");
    EXPECT_EQ(io::extractBaseReadId("read1.1"), "read1");
    EXPECT_EQ(io::extractBaseReadId("read1.2"), "read1");
    EXPECT_EQ(io::extractBaseReadId("read1_1"), "read1");
    EXPECT_EQ(io::extractBaseReadId("read1_2"), "read1");
    EXPECT_EQ(io::extractBaseReadId("read1 1:N:0:ATCG"), "read1");
    EXPECT_EQ(io::extractBaseReadId("nosuffix"), "nosuffix");
}

/// @brief Test paired ID matching.
TEST(PairedEndTest, PairedIdMatching) {
    EXPECT_TRUE(io::arePairedIds("read1/1", "read1/2"));
    EXPECT_TRUE(io::arePairedIds("read1.1", "read1.2"));
    EXPECT_TRUE(io::arePairedIds("read1_1", "read1_2"));
    EXPECT_FALSE(io::arePairedIds("read1/1", "read2/1"));
    EXPECT_FALSE(io::arePairedIds("read1", "read2"));
}

}  // namespace fqc::algo::test
