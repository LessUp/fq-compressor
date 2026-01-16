// =============================================================================
// fq-compressor - Decompress Command
// =============================================================================
// Command handler for FQC decompression.
//
// This module provides:
// - DecompressCommand: Main class for handling decompression
// - DecompressOptions: Configuration options for decompression
// - Support for range extraction, header-only mode, and error recovery
//
// Requirements: 6.2, 8.5
// =============================================================================

#ifndef FQC_COMMANDS_DECOMPRESS_COMMAND_H
#define FQC_COMMANDS_DECOMPRESS_COMMAND_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "fqc/common/error.h"
#include "fqc/common/types.h"

namespace fqc::commands {

// =============================================================================
// Range Specification
// =============================================================================

/// @brief Represents a range of reads to extract.
struct ReadRange {
    /// @brief Start read ID (1-based, inclusive).
    std::uint64_t start = 1;

    /// @brief End read ID (1-based, inclusive, 0 = end of file).
    std::uint64_t end = 0;

    /// @brief Check if range is valid.
    [[nodiscard]] bool isValid() const noexcept {
        return start > 0 && (end == 0 || end >= start);
    }

    /// @brief Check if range covers all reads.
    [[nodiscard]] bool isAll() const noexcept {
        return start == 1 && end == 0;
    }
};

/// @brief Parse a range string (e.g., "1:1000", "100:", ":500").
/// @param str Range string.
/// @return Parsed range.
/// @throws ArgumentError if format is invalid.
[[nodiscard]] ReadRange parseRange(std::string_view str);

// =============================================================================
// Decompression Options
// =============================================================================

/// @brief Configuration options for decompression.
struct DecompressOptions {
    /// @brief Input .fqc file path.
    std::filesystem::path inputPath;

    /// @brief Output FASTQ file path (or "-" for stdout).
    std::filesystem::path outputPath;

    /// @brief Second output file path for PE split (R2).
    /// If empty and splitPairedEnd is true, derived from outputPath.
    std::filesystem::path output2Path;

    /// @brief Number of threads (0 = auto).
    int threads = 0;

    /// @brief Read range to extract (nullopt = all).
    std::optional<ReadRange> range;

    /// @brief Output only read headers (IDs).
    bool headerOnly = false;

    /// @brief Output in original order (requires reorder map).
    bool originalOrder = false;

    /// @brief Skip corrupted blocks instead of failing.
    bool skipCorrupted = false;

    /// @brief Placeholder sequence for corrupted reads.
    std::string corruptedPlaceholder = "N";

    /// @brief Placeholder quality for corrupted reads.
    char corruptedQuality = '!';

    /// @brief Split paired-end output to separate files.
    bool splitPairedEnd = false;

    /// @brief Verify checksums during decompression.
    bool verifyChecksums = true;

    /// @brief Show progress bar.
    bool showProgress = true;

    /// @brief Overwrite existing output file.
    bool forceOverwrite = false;
};

// =============================================================================
// Decompression Statistics
// =============================================================================

/// @brief Statistics from decompression operation.
struct DecompressionStats {
    /// @brief Total reads output.
    std::uint64_t totalReads = 0;

    /// @brief Total bases output.
    std::uint64_t totalBases = 0;

    /// @brief Total blocks processed.
    std::uint32_t blocksProcessed = 0;

    /// @brief Corrupted blocks skipped.
    std::uint32_t corruptedBlocks = 0;

    /// @brief Checksum failures.
    std::uint32_t checksumFailures = 0;

    /// @brief Input bytes read.
    std::uint64_t inputBytes = 0;

    /// @brief Output bytes written.
    std::uint64_t outputBytes = 0;

    /// @brief Elapsed time in seconds.
    double elapsedSeconds = 0.0;

    /// @brief Throughput in MB/s.
    [[nodiscard]] double throughputMbps() const noexcept {
        return elapsedSeconds > 0
                   ? (static_cast<double>(outputBytes) / (1024 * 1024)) / elapsedSeconds
                   : 0.0;
    }
};

// =============================================================================
// DecompressCommand Class
// =============================================================================

/// @brief Command handler for FQC decompression.
class DecompressCommand {
public:
    /// @brief Construct with options.
    explicit DecompressCommand(DecompressOptions options);

    /// @brief Destructor.
    ~DecompressCommand();

    // Non-copyable, movable
    DecompressCommand(const DecompressCommand&) = delete;
    DecompressCommand& operator=(const DecompressCommand&) = delete;
    DecompressCommand(DecompressCommand&&) noexcept;
    DecompressCommand& operator=(DecompressCommand&&) noexcept;

    /// @brief Execute the decompression.
    /// @return Exit code (0 = success).
    [[nodiscard]] int execute();

    /// @brief Get decompression statistics.
    [[nodiscard]] const DecompressionStats& stats() const noexcept { return stats_; }

    /// @brief Get the options.
    [[nodiscard]] const DecompressOptions& options() const noexcept { return options_; }

private:
    /// @brief Validate options before execution.
    void validateOptions();

    /// @brief Open and validate the input archive.
    void openArchive();

    /// @brief Determine which blocks to process.
    void planExtraction();

    /// @brief Run the decompression pipeline.
    void runDecompression();

    /// @brief Print summary statistics.
    void printSummary() const;

    /// @brief Options.
    DecompressOptions options_;

    /// @brief Statistics.
    DecompressionStats stats_;

    /// @brief Block range to process.
    std::pair<BlockId, BlockId> blockRange_;
};

// =============================================================================
// Factory Function
// =============================================================================

/// @brief Create a decompress command from CLI options.
[[nodiscard]] std::unique_ptr<DecompressCommand> createDecompressCommand(
    const std::string& inputPath,
    const std::string& outputPath,
    const std::string& range,
    bool headerOnly,
    bool originalOrder,
    bool skipCorrupted,
    const std::string& corruptedPlaceholder,
    bool splitPe,
    int threads,
    bool force);

}  // namespace fqc::commands

#endif  // FQC_COMMANDS_DECOMPRESS_COMMAND_H
