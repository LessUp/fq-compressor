// =============================================================================
// fq-compressor - Paired-End Optimization Module
// =============================================================================
// Optimizations for paired-end read compression:
// - R1/R2 complementarity exploitation (store R2 as diff from R1-RC)
// - Paired reordering (move R1/R2 pairs together)
//
// Requirements: 1.1.3
// =============================================================================

#ifndef FQC_ALGO_PE_OPTIMIZER_H
#define FQC_ALGO_PE_OPTIMIZER_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"
#include "fqc/io/paired_end_parser.h"

namespace fqc::algo {

// =============================================================================
// Constants
// =============================================================================

/// @brief Threshold for using complementarity encoding (max differences).
inline constexpr std::size_t kComplementarityThreshold = 50;

/// @brief Minimum overlap for complementarity detection.
inline constexpr std::size_t kMinComplementarityOverlap = 20;

// =============================================================================
// PE Encoded Pair
// =============================================================================

/// @brief Encoded paired-end read pair.
/// R1 is stored normally, R2 is stored as difference from R1 reverse complement.
struct PEEncodedPair {
    /// @brief Read 1 ID.
    std::string id1;

    /// @brief Read 1 sequence.
    std::string seq1;

    /// @brief Read 1 quality.
    std::string qual1;

    /// @brief Read 2 ID (may be empty if derived from id1).
    std::string id2;

    /// @brief Whether R2 uses complementarity encoding.
    bool useComplementarity = false;

    /// @brief If complementarity: positions where R2 differs from R1-RC.
    std::vector<std::uint16_t> diffPositions;

    /// @brief If complementarity: bases at diff positions.
    std::vector<char> diffBases;

    /// @brief If complementarity: quality delta at diff positions.
    std::vector<std::int8_t> qualDelta;

    /// @brief If not complementarity: raw R2 sequence.
    std::string seq2;

    /// @brief If not complementarity: raw R2 quality.
    std::string qual2;

    /// @brief Get the decoded R2 sequence.
    [[nodiscard]] std::string decodeR2Sequence(std::string_view r1Seq) const;

    /// @brief Get the decoded R2 quality.
    [[nodiscard]] std::string decodeR2Quality(std::string_view r1Qual) const;
};

// =============================================================================
// PE Optimizer Configuration
// =============================================================================

/// @brief Configuration for PE optimization.
struct PEOptimizerConfig {
    /// @brief Enable R1/R2 complementarity encoding.
    bool enableComplementarity = true;

    /// @brief Maximum differences for complementarity encoding.
    std::size_t complementarityThreshold = kComplementarityThreshold;

    /// @brief Minimum overlap required.
    std::size_t minOverlap = kMinComplementarityOverlap;

    /// @brief PE storage layout.
    PELayout layout = PELayout::kInterleaved;
};

// =============================================================================
// PE Optimizer Class
// =============================================================================

/// @brief Optimizer for paired-end read compression.
///
/// Provides:
/// - Complementarity encoding: Store R2 as diff from R1 reverse complement
/// - Layout conversion: Interleaved <-> Consecutive
///
/// Usage:
/// @code
/// PEOptimizer optimizer(config);
/// for (const auto& pair : pairs) {
///     auto encoded = optimizer.encodePair(pair);
///     // ... compress encoded pair
/// }
/// @endcode
class PEOptimizer {
public:
    /// @brief Construct with configuration.
    explicit PEOptimizer(PEOptimizerConfig config = {});

    /// @brief Encode a paired-end record.
    /// @param pair The input pair.
    /// @return Encoded pair with complementarity optimization.
    [[nodiscard]] PEEncodedPair encodePair(const io::PairedEndRecord& pair) const;

    /// @brief Decode an encoded pair back to PairedEndRecord.
    /// @param encoded The encoded pair.
    /// @return Decoded pair.
    [[nodiscard]] io::PairedEndRecord decodePair(const PEEncodedPair& encoded) const;

    /// @brief Check if complementarity encoding is beneficial for a pair.
    /// @param r1Seq R1 sequence.
    /// @param r2Seq R2 sequence.
    /// @return Pair of (beneficial, diffCount).
    [[nodiscard]] std::pair<bool, std::size_t> checkComplementarity(
        std::string_view r1Seq,
        std::string_view r2Seq) const;

    /// @brief Convert read pairs to interleaved layout.
    /// @param pairs Input pairs.
    /// @return Vector of reads in interleaved order (R1, R2, R1, R2, ...).
    [[nodiscard]] std::vector<ReadRecord> toInterleaved(
        std::span<const io::PairedEndRecord> pairs) const;

    /// @brief Convert read pairs to consecutive layout.
    /// @param pairs Input pairs.
    /// @return Vector of reads in consecutive order (all R1, then all R2).
    [[nodiscard]] std::vector<ReadRecord> toConsecutive(
        std::span<const io::PairedEndRecord> pairs) const;

    /// @brief Convert interleaved reads back to pairs.
    /// @param reads Interleaved reads.
    /// @return Vector of pairs.
    [[nodiscard]] std::vector<io::PairedEndRecord> fromInterleaved(
        std::span<const ReadRecord> reads) const;

    /// @brief Convert consecutive reads back to pairs.
    /// @param reads Consecutive reads (first half R1, second half R2).
    /// @return Vector of pairs.
    [[nodiscard]] std::vector<io::PairedEndRecord> fromConsecutive(
        std::span<const ReadRecord> reads) const;

    /// @brief Get current configuration.
    [[nodiscard]] const PEOptimizerConfig& config() const noexcept { return config_; }

    /// @brief Get statistics.
    struct Stats {
        std::uint64_t totalPairs = 0;
        std::uint64_t complementarityUsed = 0;
        std::uint64_t bytesSaved = 0;
    };
    [[nodiscard]] const Stats& stats() const noexcept { return stats_; }

    /// @brief Reset statistics.
    void resetStats() noexcept { stats_ = Stats{}; }

private:
    PEOptimizerConfig config_;
    mutable Stats stats_;

    /// @brief Compute reverse complement of a sequence.
    [[nodiscard]] static std::string reverseComplement(std::string_view seq);

    /// @brief Compute diff between two sequences.
    [[nodiscard]] static std::pair<std::vector<std::uint16_t>, std::vector<char>>
    computeDiff(std::string_view seq1, std::string_view seq2);
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Check if two reads appear to be from the same pair.
/// @param r1 First read.
/// @param r2 Second read.
/// @return true if reads appear to be paired.
[[nodiscard]] bool isPairedReads(const ReadRecord& r1, const ReadRecord& r2);

/// @brief Generate R2 ID from R1 ID.
/// @param r1Id R1 read ID.
/// @return Generated R2 ID (e.g., with /2 suffix).
[[nodiscard]] std::string generateR2Id(std::string_view r1Id);

}  // namespace fqc::algo

#endif  // FQC_ALGO_PE_OPTIMIZER_H
