// =============================================================================
// fq-compressor - Paired-End Optimization Implementation
// =============================================================================
// Implementation of paired-end read compression optimizations.
//
// Requirements: 1.1.3
// =============================================================================

#include "fqc/algo/pe_optimizer.h"

#include <algorithm>
#include <cstring>

#include "fqc/common/logger.h"

namespace fqc::algo {

// =============================================================================
// Complement Lookup Table
// =============================================================================

namespace {

constexpr char kComplement[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   'T', 0,   'G', 0,   0,   0,   'C', 0,   0,   0,   0,   0,   0,   'N', 0,
    0,   0,   0,   0,   'A', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   't', 0,   'g', 0,   0,   0,   'c', 0,   0,   0,   0,   0,   0,   'n', 0,
    0,   0,   0,   0,   'a', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

}  // namespace

// =============================================================================
// PEEncodedPair Implementation
// =============================================================================

std::string PEEncodedPair::decodeR2Sequence(std::string_view r1Seq) const {
    if (!useComplementarity) {
        return seq2;
    }

    // Start with R1 reverse complement
    std::string result;
    result.reserve(r1Seq.length());
    for (auto it = r1Seq.rbegin(); it != r1Seq.rend(); ++it) {
        char c = kComplement[static_cast<unsigned char>(*it)];
        result.push_back(c ? c : 'N');
    }

    // Apply differences
    for (std::size_t i = 0; i < diffPositions.size(); ++i) {
        if (diffPositions[i] < result.length()) {
            result[diffPositions[i]] = diffBases[i];
        }
    }

    return result;
}

std::string PEEncodedPair::decodeR2Quality(std::string_view r1Qual) const {
    if (!useComplementarity) {
        return qual2;
    }

    // Start with reversed R1 quality
    std::string result(r1Qual.rbegin(), r1Qual.rend());

    // Apply quality deltas at diff positions
    for (std::size_t i = 0; i < diffPositions.size() && i < qualDelta.size(); ++i) {
        if (diffPositions[i] < result.length()) {
            int newQual = static_cast<int>(result[diffPositions[i]]) + qualDelta[i];
            newQual = std::clamp(newQual, 33, 126);  // Valid Phred+33 range
            result[diffPositions[i]] = static_cast<char>(newQual);
        }
    }

    return result;
}

// =============================================================================
// PEOptimizer Implementation
// =============================================================================

PEOptimizer::PEOptimizer(PEOptimizerConfig config) : config_(std::move(config)) {}

std::string PEOptimizer::reverseComplement(std::string_view seq) {
    std::string result;
    result.reserve(seq.length());
    for (auto it = seq.rbegin(); it != seq.rend(); ++it) {
        char c = kComplement[static_cast<unsigned char>(*it)];
        result.push_back(c ? c : 'N');
    }
    return result;
}

std::pair<std::vector<std::uint16_t>, std::vector<char>>
PEOptimizer::computeDiff(std::string_view seq1, std::string_view seq2) {
    std::vector<std::uint16_t> positions;
    std::vector<char> bases;

    std::size_t len = std::min(seq1.length(), seq2.length());
    for (std::size_t i = 0; i < len; ++i) {
        if (seq1[i] != seq2[i]) {
            positions.push_back(static_cast<std::uint16_t>(i));
            bases.push_back(seq2[i]);
        }
    }

    // Handle length differences
    for (std::size_t i = len; i < seq2.length(); ++i) {
        positions.push_back(static_cast<std::uint16_t>(i));
        bases.push_back(seq2[i]);
    }

    return {std::move(positions), std::move(bases)};
}

std::pair<bool, std::size_t> PEOptimizer::checkComplementarity(
    std::string_view r1Seq,
    std::string_view r2Seq) const {

    if (!config_.enableComplementarity) {
        return {false, 0};
    }

    // Check minimum overlap
    std::size_t minLen = std::min(r1Seq.length(), r2Seq.length());
    if (minLen < config_.minOverlap) {
        return {false, 0};
    }

    // Compute R1 reverse complement
    std::string r1RC = reverseComplement(r1Seq);

    // Count differences
    std::size_t diffCount = 0;
    for (std::size_t i = 0; i < minLen; ++i) {
        if (r1RC[i] != r2Seq[i]) {
            ++diffCount;
            if (diffCount > config_.complementarityThreshold) {
                return {false, diffCount};
            }
        }
    }

    // Add length difference
    diffCount += static_cast<std::size_t>(std::abs(static_cast<int>(r1Seq.length()) -
                          static_cast<int>(r2Seq.length())));

    bool beneficial = diffCount <= config_.complementarityThreshold;
    return {beneficial, diffCount};
}

PEEncodedPair PEOptimizer::encodePair(const io::PairedEndRecord& pair) const {
    PEEncodedPair encoded;
    encoded.id1 = pair.read1.id;
    encoded.seq1 = pair.read1.sequence;
    encoded.qual1 = pair.read1.quality;

    // Check if IDs are different
    if (!io::arePairedIds(pair.read1.id, pair.read2.id)) {
        encoded.id2 = pair.read2.id;
    }

    // Check if complementarity encoding is beneficial
    auto [beneficial, diffCount] = checkComplementarity(pair.read1.sequence,
                                                         pair.read2.sequence);

    if (beneficial) {
        encoded.useComplementarity = true;

        // Compute R1 reverse complement
        std::string r1RC = reverseComplement(pair.read1.sequence);

        // Compute differences
        auto [positions, bases] = computeDiff(r1RC, pair.read2.sequence);
        encoded.diffPositions = std::move(positions);
        encoded.diffBases = std::move(bases);

        // Compute quality deltas
        std::string r1QualRev(pair.read1.quality.rbegin(), pair.read1.quality.rend());
        for (std::size_t i = 0; i < encoded.diffPositions.size(); ++i) {
            std::uint16_t pos = encoded.diffPositions[i];
            if (pos < r1QualRev.length() && pos < pair.read2.quality.length()) {
                std::int8_t delta = static_cast<std::int8_t>(
                    pair.read2.quality[pos] - r1QualRev[pos]);
                encoded.qualDelta.push_back(delta);
            } else {
                encoded.qualDelta.push_back(0);
            }
        }

        // Update stats
        ++stats_.complementarityUsed;
        stats_.bytesSaved += pair.read2.sequence.length() - encoded.diffPositions.size() * 3;
    } else {
        encoded.useComplementarity = false;
        encoded.seq2 = pair.read2.sequence;
        encoded.qual2 = pair.read2.quality;
    }

    ++stats_.totalPairs;
    return encoded;
}

io::PairedEndRecord PEOptimizer::decodePair(const PEEncodedPair& encoded) const {
    io::PairedEndRecord pair;

    pair.read1.id = encoded.id1;
    pair.read1.sequence = encoded.seq1;
    pair.read1.quality = encoded.qual1;

    if (encoded.id2.empty()) {
        pair.read2.id = generateR2Id(encoded.id1);
    } else {
        pair.read2.id = encoded.id2;
    }

    pair.read2.sequence = encoded.decodeR2Sequence(encoded.seq1);
    pair.read2.quality = encoded.decodeR2Quality(encoded.qual1);

    return pair;
}

std::vector<ReadRecord> PEOptimizer::toInterleaved(
    std::span<const io::PairedEndRecord> pairs) const {

    std::vector<ReadRecord> reads;
    reads.reserve(pairs.size() * 2);

    for (const auto& pair : pairs) {
        reads.emplace_back(pair.read1.id, pair.read1.sequence, pair.read1.quality);
        reads.emplace_back(pair.read2.id, pair.read2.sequence, pair.read2.quality);
    }

    return reads;
}

std::vector<ReadRecord> PEOptimizer::toConsecutive(
    std::span<const io::PairedEndRecord> pairs) const {

    std::vector<ReadRecord> reads;
    reads.reserve(pairs.size() * 2);

    // First all R1
    for (const auto& pair : pairs) {
        reads.emplace_back(pair.read1.id, pair.read1.sequence, pair.read1.quality);
    }

    // Then all R2
    for (const auto& pair : pairs) {
        reads.emplace_back(pair.read2.id, pair.read2.sequence, pair.read2.quality);
    }

    return reads;
}

std::vector<io::PairedEndRecord> PEOptimizer::fromInterleaved(
    std::span<const ReadRecord> reads) const {

    std::vector<io::PairedEndRecord> pairs;
    pairs.reserve(reads.size() / 2);

    for (std::size_t i = 0; i + 1 < reads.size(); i += 2) {
        io::PairedEndRecord pair;
        pair.read1.id = reads[i].id;
        pair.read1.sequence = reads[i].sequence;
        pair.read1.quality = reads[i].quality;
        pair.read2.id = reads[i + 1].id;
        pair.read2.sequence = reads[i + 1].sequence;
        pair.read2.quality = reads[i + 1].quality;
        pairs.push_back(std::move(pair));
    }

    return pairs;
}

std::vector<io::PairedEndRecord> PEOptimizer::fromConsecutive(
    std::span<const ReadRecord> reads) const {

    std::size_t halfSize = reads.size() / 2;
    std::vector<io::PairedEndRecord> pairs;
    pairs.reserve(halfSize);

    for (std::size_t i = 0; i < halfSize; ++i) {
        io::PairedEndRecord pair;
        pair.read1.id = reads[i].id;
        pair.read1.sequence = reads[i].sequence;
        pair.read1.quality = reads[i].quality;
        pair.read2.id = reads[halfSize + i].id;
        pair.read2.sequence = reads[halfSize + i].sequence;
        pair.read2.quality = reads[halfSize + i].quality;
        pairs.push_back(std::move(pair));
    }

    return pairs;
}

// =============================================================================
// Utility Functions
// =============================================================================

bool isPairedReads(const ReadRecord& r1, const ReadRecord& r2) {
    return io::arePairedIds(r1.id, r2.id);
}

std::string generateR2Id(std::string_view r1Id) {
    std::string id(r1Id);

    // Check for existing /1 suffix
    if (id.length() >= 2 && id[id.length() - 1] == '1' &&
        (id[id.length() - 2] == '/' || id[id.length() - 2] == '.')) {
        id[id.length() - 1] = '2';
        return id;
    }

    // Check for space-separated format
    auto spacePos = id.find(' ');
    if (spacePos != std::string::npos && spacePos + 1 < id.length()) {
        if (id[spacePos + 1] == '1') {
            id[spacePos + 1] = '2';
            return id;
        }
    }

    // Default: append /2
    return id + "/2";
}

}  // namespace fqc::algo
