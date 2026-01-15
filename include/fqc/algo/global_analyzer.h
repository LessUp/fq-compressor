// =============================================================================
// fq-compressor - Global Analyzer Module (Phase 1)
// =============================================================================
// Implements Phase 1 of the two-phase compression strategy:
// 1. Scan all reads and extract minimizers
// 2. Build minimizer -> bucket mapping
// 3. Perform global reordering decision (Approximate Hamiltonian Path)
// 4. Generate Reorder Map: original_id -> archive_id
// 5. Divide block boundaries (default 100K reads per block)
//
// Memory budget: ~24 bytes/read (Minimizer index + Reorder Map)
//
// This module integrates with Spring core algorithms from vendor/spring-core/
// for minimizer bucketing and reordering.
//
// Requirements: 1.1.2, 4.3
// =============================================================================

#ifndef FQC_ALGO_GLOBAL_ANALYZER_H
#define FQC_ALGO_GLOBAL_ANALYZER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"

namespace fqc::algo {

// =============================================================================
// Forward Declarations
// =============================================================================

class GlobalAnalyzerImpl;

// =============================================================================
// Constants
// =============================================================================

/// @brief Default minimizer k-mer size
inline constexpr std::size_t kDefaultMinimizerK = 23;

/// @brief Default minimizer window size
inline constexpr std::size_t kDefaultMinimizerW = 12;

/// @brief Memory per read for Phase 1 (bytes)
/// Includes: Minimizer index (~16 bytes) + Reorder Map (~8 bytes)
inline constexpr std::size_t kMemoryPerReadPhase1 = 24;

/// @brief Maximum Hamming distance threshold for read matching
inline constexpr std::size_t kDefaultHammingThreshold = 4;

/// @brief Maximum number of reads to search in each dictionary bin
inline constexpr std::size_t kDefaultMaxSearchReorder = 1000;

/// @brief Number of dictionaries for reordering
inline constexpr std::size_t kDefaultNumDictionaries = 2;

// =============================================================================
// Minimizer Structure
// =============================================================================

/// @brief Represents a minimizer extracted from a read
struct Minimizer {
    /// @brief The minimizer hash value
    std::uint64_t hash;

    /// @brief Position in the read where minimizer starts
    std::uint16_t position;

    /// @brief Whether the minimizer is from reverse complement
    bool isReverseComplement;

    /// @brief Default constructor
    constexpr Minimizer() noexcept : hash(0), position(0), isReverseComplement(false) {}

    /// @brief Construct with values
    constexpr Minimizer(std::uint64_t h, std::uint16_t pos, bool rc) noexcept
        : hash(h), position(pos), isReverseComplement(rc) {}

    /// @brief Equality comparison
    [[nodiscard]] constexpr bool operator==(const Minimizer& other) const noexcept = default;

    /// @brief Less-than comparison (for sorting)
    [[nodiscard]] constexpr bool operator<(const Minimizer& other) const noexcept {
        return hash < other.hash;
    }
};

// =============================================================================
// Block Boundary Structure
// =============================================================================

/// @brief Represents a block boundary in the reordered sequence
struct BlockBoundary {
    /// @brief Block ID (0-indexed)
    BlockId blockId;

    /// @brief Start archive ID (inclusive, 0-indexed)
    ReadId archiveIdStart;

    /// @brief End archive ID (exclusive)
    ReadId archiveIdEnd;

    /// @brief Number of reads in this block
    [[nodiscard]] constexpr std::size_t readCount() const noexcept {
        return static_cast<std::size_t>(archiveIdEnd - archiveIdStart);
    }
};

// =============================================================================
// Global Analysis Result
// =============================================================================

/// @brief Result of global analysis phase
struct GlobalAnalysisResult {
    /// @brief Total number of reads analyzed
    std::uint64_t totalReads;

    /// @brief Maximum read length encountered
    std::size_t maxReadLength;

    /// @brief Detected read length class
    ReadLengthClass lengthClass;

    /// @brief Whether reordering was performed
    bool reorderingPerformed;

    /// @brief Number of blocks created
    std::size_t numBlocks;

    /// @brief Block boundaries
    std::vector<BlockBoundary> blockBoundaries;

    /// @brief Forward reorder map: original_id -> archive_id
    /// @note Empty if reordering was not performed
    std::vector<ReadId> forwardMap;

    /// @brief Reverse reorder map: archive_id -> original_id
    /// @note Empty if reordering was not performed
    std::vector<ReadId> reverseMap;

    /// @brief Memory used for analysis (bytes)
    std::size_t memoryUsed;

    /// @brief Check if reorder maps are available
    [[nodiscard]] bool hasReorderMaps() const noexcept {
        return !forwardMap.empty() && !reverseMap.empty();
    }

    /// @brief Get archive ID for an original read ID
    /// @param originalId Original read ID (0-indexed)
    /// @return Archive ID (0-indexed), or originalId if no reordering
    [[nodiscard]] ReadId getArchiveId(ReadId originalId) const noexcept {
        if (forwardMap.empty() || originalId >= forwardMap.size()) {
            return originalId;
        }
        return forwardMap[originalId];
    }

    /// @brief Get original ID for an archive read ID
    /// @param archiveId Archive read ID (0-indexed)
    /// @return Original ID (0-indexed), or archiveId if no reordering
    [[nodiscard]] ReadId getOriginalId(ReadId archiveId) const noexcept {
        if (reverseMap.empty() || archiveId >= reverseMap.size()) {
            return archiveId;
        }
        return reverseMap[archiveId];
    }

    /// @brief Find the block containing a given archive ID
    /// @param archiveId Archive read ID (0-indexed)
    /// @return Block ID, or kInvalidBlockId if not found
    [[nodiscard]] BlockId findBlock(ReadId archiveId) const noexcept;
};

// =============================================================================
// Global Analyzer Configuration
// =============================================================================

/// @brief Configuration for global analysis
struct GlobalAnalyzerConfig {
    /// @brief Number of reads per block (default: 100K for short reads)
    std::size_t readsPerBlock = kDefaultBlockSizeShort;

    /// @brief Memory limit in bytes (0 = no limit)
    std::size_t memoryLimit = 0;

    /// @brief Number of threads (0 = auto-detect)
    std::size_t numThreads = 0;

    /// @brief Enable reordering for better compression
    bool enableReorder = true;

    /// @brief Minimizer k-mer size
    std::size_t minimizerK = kDefaultMinimizerK;

    /// @brief Minimizer window size
    std::size_t minimizerW = kDefaultMinimizerW;

    /// @brief Hamming distance threshold for matching
    std::size_t hammingThreshold = kDefaultHammingThreshold;

    /// @brief Maximum reads to search in each bin
    std::size_t maxSearchReorder = kDefaultMaxSearchReorder;

    /// @brief Number of dictionaries for reordering
    std::size_t numDictionaries = kDefaultNumDictionaries;

    /// @brief Read length class (auto-detected if not specified)
    std::optional<ReadLengthClass> readLengthClass;

    /// @brief Progress callback (called periodically with progress 0.0-1.0)
    std::function<void(double)> progressCallback;

    /// @brief Validate configuration
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult validate() const;

    /// @brief Calculate memory budget for given number of reads
    /// @param numReads Number of reads
    /// @return Estimated memory usage in bytes
    [[nodiscard]] std::size_t estimateMemory(std::size_t numReads) const noexcept {
        return numReads * kMemoryPerReadPhase1;
    }

    /// @brief Calculate maximum reads that fit in memory budget
    /// @return Maximum number of reads, or SIZE_MAX if no limit
    [[nodiscard]] std::size_t maxReadsInMemory() const noexcept {
        if (memoryLimit == 0) {
            return SIZE_MAX;
        }
        return memoryLimit / kMemoryPerReadPhase1;
    }
};

// =============================================================================
// Read Data Interface
// =============================================================================

/// @brief Interface for providing read data to the analyzer
/// @note This allows the analyzer to work with different data sources
///       (in-memory, file-based, streaming)
class IReadDataProvider {
public:
    virtual ~IReadDataProvider() = default;

    /// @brief Get total number of reads
    [[nodiscard]] virtual std::uint64_t totalReads() const = 0;

    /// @brief Get a read sequence by index
    /// @param index Read index (0-indexed)
    /// @return Read sequence, or empty string_view if index out of range
    [[nodiscard]] virtual std::string_view getSequence(std::uint64_t index) const = 0;

    /// @brief Get read length by index
    /// @param index Read index (0-indexed)
    /// @return Read length, or 0 if index out of range
    [[nodiscard]] virtual std::size_t getLength(std::uint64_t index) const = 0;

    /// @brief Check if all reads have the same length
    [[nodiscard]] virtual bool hasUniformLength() const = 0;

    /// @brief Get uniform length (if hasUniformLength() is true)
    [[nodiscard]] virtual std::size_t uniformLength() const = 0;

    /// @brief Get maximum read length
    [[nodiscard]] virtual std::size_t maxLength() const = 0;
};

// =============================================================================
// In-Memory Read Data Provider
// =============================================================================

/// @brief Read data provider for in-memory read collections
class InMemoryReadProvider : public IReadDataProvider {
public:
    /// @brief Construct from a vector of ReadRecord
    explicit InMemoryReadProvider(std::span<const ReadRecord> reads);

    /// @brief Construct from a vector of sequences (strings)
    explicit InMemoryReadProvider(std::span<const std::string> sequences);

    ~InMemoryReadProvider() override = default;

    [[nodiscard]] std::uint64_t totalReads() const override;
    [[nodiscard]] std::string_view getSequence(std::uint64_t index) const override;
    [[nodiscard]] std::size_t getLength(std::uint64_t index) const override;
    [[nodiscard]] bool hasUniformLength() const override;
    [[nodiscard]] std::size_t uniformLength() const override;
    [[nodiscard]] std::size_t maxLength() const override;

private:
    std::span<const ReadRecord> reads_;
    std::span<const std::string> sequences_;
    bool useReadRecords_;
    std::size_t maxLength_;
    std::size_t uniformLength_;
    bool hasUniformLength_;

    void computeStats();
};

// =============================================================================
// Global Analyzer Class
// =============================================================================

/// @brief Main class for Phase 1 global analysis
///
/// The GlobalAnalyzer performs the following steps:
/// 1. Extract minimizers from all reads
/// 2. Build minimizer -> bucket mapping using BBHash
/// 3. Perform global reordering using Approximate Hamiltonian Path
/// 4. Generate bidirectional reorder maps
/// 5. Divide reads into blocks
///
/// Usage:
/// @code
/// GlobalAnalyzerConfig config;
/// config.readsPerBlock = 100000;
/// config.enableReorder = true;
///
/// GlobalAnalyzer analyzer(config);
/// auto result = analyzer.analyze(readProvider);
/// if (result) {
///     // Use result->forwardMap, result->reverseMap, result->blockBoundaries
/// }
/// @endcode
class GlobalAnalyzer {
public:
    /// @brief Construct with configuration
    /// @param config Analysis configuration
    explicit GlobalAnalyzer(GlobalAnalyzerConfig config = {});

    /// @brief Destructor
    ~GlobalAnalyzer();

    // Non-copyable, movable
    GlobalAnalyzer(const GlobalAnalyzer&) = delete;
    GlobalAnalyzer& operator=(const GlobalAnalyzer&) = delete;
    GlobalAnalyzer(GlobalAnalyzer&&) noexcept;
    GlobalAnalyzer& operator=(GlobalAnalyzer&&) noexcept;

    /// @brief Perform global analysis on read data
    /// @param provider Read data provider
    /// @return Analysis result or error
    [[nodiscard]] Result<GlobalAnalysisResult> analyze(const IReadDataProvider& provider);

    /// @brief Perform global analysis on in-memory reads
    /// @param reads Vector of read records
    /// @return Analysis result or error
    [[nodiscard]] Result<GlobalAnalysisResult> analyze(std::span<const ReadRecord> reads);

    /// @brief Perform global analysis on in-memory sequences
    /// @param sequences Vector of sequence strings
    /// @return Analysis result or error
    [[nodiscard]] Result<GlobalAnalysisResult> analyze(std::span<const std::string> sequences);

    /// @brief Get current configuration
    [[nodiscard]] const GlobalAnalyzerConfig& config() const noexcept;

    /// @brief Update configuration
    /// @param config New configuration
    /// @return VoidResult indicating success or validation error
    [[nodiscard]] VoidResult setConfig(GlobalAnalyzerConfig config);

    /// @brief Cancel ongoing analysis (thread-safe)
    void cancel() noexcept;

    /// @brief Check if analysis was cancelled
    [[nodiscard]] bool isCancelled() const noexcept;

    /// @brief Reset cancellation flag
    void resetCancellation() noexcept;

private:
    std::unique_ptr<GlobalAnalyzerImpl> impl_;
};

// =============================================================================
// Minimizer Extraction Functions
// =============================================================================

/// @brief Extract minimizers from a DNA sequence
/// @param sequence DNA sequence
/// @param k K-mer size
/// @param w Window size
/// @return Vector of minimizers
[[nodiscard]] std::vector<Minimizer> extractMinimizers(std::string_view sequence,
                                                        std::size_t k = kDefaultMinimizerK,
                                                        std::size_t w = kDefaultMinimizerW);

/// @brief Compute canonical k-mer hash (min of forward and reverse complement)
/// @param sequence K-mer sequence
/// @return Canonical hash value
[[nodiscard]] std::uint64_t computeKmerHash(std::string_view sequence);

/// @brief Compute reverse complement of a DNA sequence
/// @param sequence Input sequence
/// @return Reverse complement
[[nodiscard]] std::string reverseComplement(std::string_view sequence);

// =============================================================================
// Read Length Classification
// =============================================================================

/// @brief Classify reads based on length statistics
/// @param provider Read data provider
/// @param sampleSize Number of reads to sample (0 = all reads)
/// @return Detected read length class
[[nodiscard]] ReadLengthClass classifyReadLength(const IReadDataProvider& provider,
                                                  std::size_t sampleSize = 1000);

/// @brief Classify reads based on median and max length
/// @param medianLength Median read length
/// @param maxLength Maximum read length
/// @return Detected read length class
[[nodiscard]] ReadLengthClass classifyReadLength(std::size_t medianLength,
                                                  std::size_t maxLength);

/// @brief Get recommended block size for a read length class
/// @param lengthClass Read length class
/// @return Recommended reads per block
[[nodiscard]] std::size_t recommendedBlockSize(ReadLengthClass lengthClass) noexcept;

/// @brief Check if reordering should be enabled for a read length class
/// @param lengthClass Read length class
/// @return true if reordering is beneficial
[[nodiscard]] bool shouldEnableReorder(ReadLengthClass lengthClass) noexcept;

}  // namespace fqc::algo

#endif  // FQC_ALGO_GLOBAL_ANALYZER_H
