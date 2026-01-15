// =============================================================================
// fq-compressor - Compress Command
// =============================================================================
// Command handler for FASTQ compression.
//
// This module provides:
// - CompressCommand: Main class for handling compression
// - CompressOptions: Configuration options for compression
// - Integration with compression engine (placeholder)
//
// Requirements: 6.2
// =============================================================================

#ifndef FQC_COMMANDS_COMPRESS_COMMAND_H
#define FQC_COMMANDS_COMPRESS_COMMAND_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "fqc/common/error.h"
#include "fqc/common/types.h"

namespace fqc::commands {

// =============================================================================
// Compression Options
// =============================================================================

/// @brief Quality compression mode.
enum class QualityCompressionMode : std::uint8_t {
    kLossless = 0,     ///< Preserve exact quality values
    kIllumina8 = 1,    ///< Illumina 8-bin quantization
    kQvz = 2,          ///< QVZ lossy compression
    kDiscard = 3       ///< Discard quality values
};

/// @brief Parse quality mode from string.
[[nodiscard]] QualityCompressionMode parseQualityMode(std::string_view str);

/// @brief Get string representation of quality mode.
[[nodiscard]] std::string_view qualityModeToString(QualityCompressionMode mode);

/// @brief Configuration options for compression.
struct CompressOptions {
    /// @brief Input file path (or "-" for stdin).
    std::filesystem::path inputPath;

    /// @brief Output file path.
    std::filesystem::path outputPath;

    /// @brief Compression level (1-9).
    int compressionLevel = 6;

    /// @brief Number of threads (0 = auto).
    int threads = 0;

    /// @brief Memory limit in MB (0 = no limit).
    std::size_t memoryLimitMb = 0;

    /// @brief Enable global read reordering.
    bool enableReordering = true;

    /// @brief Streaming mode (no global analysis).
    bool streamingMode = false;

    /// @brief Quality compression mode.
    QualityCompressionMode qualityMode = QualityCompressionMode::kLossless;

    /// @brief Long read handling mode.
    ReadLengthClass longReadMode = ReadLengthClass::kShort;

    /// @brief Auto-detect long read mode.
    bool autoDetectLongRead = true;

    /// @brief Maximum bases per block (for long reads).
    std::size_t maxBlockBases = 0;

    /// @brief Block size in reads.
    std::size_t blockSize = 100000;

    /// @brief Overwrite existing output file.
    bool forceOverwrite = false;

    /// @brief Show progress bar.
    bool showProgress = true;

    /// @brief Validate input FASTQ.
    bool validateInput = true;

    /// @brief Collect and store statistics.
    bool collectStats = true;
};

// =============================================================================
// Compression Statistics
// =============================================================================

/// @brief Statistics from compression operation.
struct CompressionStats {
    /// @brief Total input reads.
    std::uint64_t totalReads = 0;

    /// @brief Total input bases.
    std::uint64_t totalBases = 0;

    /// @brief Total input bytes.
    std::uint64_t inputBytes = 0;

    /// @brief Total output bytes.
    std::uint64_t outputBytes = 0;

    /// @brief Number of blocks written.
    std::uint32_t blocksWritten = 0;

    /// @brief Compression ratio (input/output).
    [[nodiscard]] double compressionRatio() const noexcept {
        return outputBytes > 0 ? static_cast<double>(inputBytes) / outputBytes : 0.0;
    }

    /// @brief Bits per base.
    [[nodiscard]] double bitsPerBase() const noexcept {
        return totalBases > 0 ? (static_cast<double>(outputBytes) * 8.0) / totalBases : 0.0;
    }

    /// @brief Elapsed time in seconds.
    double elapsedSeconds = 0.0;

    /// @brief Throughput in MB/s.
    [[nodiscard]] double throughputMbps() const noexcept {
        return elapsedSeconds > 0 ? (static_cast<double>(inputBytes) / (1024 * 1024)) / elapsedSeconds
                                  : 0.0;
    }
};

// =============================================================================
// CompressCommand Class
// =============================================================================

/// @brief Command handler for FASTQ compression.
class CompressCommand {
public:
    /// @brief Construct with options.
    explicit CompressCommand(CompressOptions options);

    /// @brief Destructor.
    ~CompressCommand();

    // Non-copyable, movable
    CompressCommand(const CompressCommand&) = delete;
    CompressCommand& operator=(const CompressCommand&) = delete;
    CompressCommand(CompressCommand&&) noexcept;
    CompressCommand& operator=(CompressCommand&&) noexcept;

    /// @brief Execute the compression.
    /// @return Exit code (0 = success).
    [[nodiscard]] int execute();

    /// @brief Get compression statistics.
    [[nodiscard]] const CompressionStats& stats() const noexcept { return stats_; }

    /// @brief Get the options.
    [[nodiscard]] const CompressOptions& options() const noexcept { return options_; }

private:
    /// @brief Validate options before execution.
    void validateOptions();

    /// @brief Detect read length class from input.
    void detectReadLengthClass();

    /// @brief Setup compression parameters based on detected class.
    void setupCompressionParams();

    /// @brief Run the compression pipeline.
    void runCompression();

    /// @brief Print summary statistics.
    void printSummary() const;

    /// @brief Options.
    CompressOptions options_;

    /// @brief Statistics.
    CompressionStats stats_;

    /// @brief Detected read length class.
    std::optional<ReadLengthClass> detectedLengthClass_;
};

// =============================================================================
// Factory Function
// =============================================================================

/// @brief Create a compress command from CLI options.
/// @param inputPath Input file path.
/// @param outputPath Output file path.
/// @param level Compression level.
/// @param reorder Enable reordering.
/// @param streaming Streaming mode.
/// @param qualityMode Quality compression mode string.
/// @param longReadMode Long read mode string.
/// @param threads Number of threads.
/// @param memoryLimit Memory limit in MB.
/// @param force Force overwrite.
/// @return Configured CompressCommand.
[[nodiscard]] std::unique_ptr<CompressCommand> createCompressCommand(
    const std::string& inputPath,
    const std::string& outputPath,
    int level,
    bool reorder,
    bool streaming,
    const std::string& qualityMode,
    const std::string& longReadMode,
    int threads,
    std::size_t memoryLimit,
    bool force);

}  // namespace fqc::commands

#endif  // FQC_COMMANDS_COMPRESS_COMMAND_H
