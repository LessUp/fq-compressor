// =============================================================================
// fq-compressor - TBB Pipeline Module
// =============================================================================
// Implements the parallel compression/decompression pipeline using Intel TBB.
//
// The pipeline follows the Pigz model:
// 1. ReaderFilter (Serial) - Reads FASTQ and produces chunks of reads
// 2. CompressFilter (Parallel) - Compresses chunks to blocks
// 3. WriterFilter (Serial) - Writes blocks to disk in order
//
// Key features:
// - Block-level parallelism for compression
// - Memory-bounded operation with configurable limits
// - Progress reporting and cancellation support
// - Backpressure mechanism to prevent memory exhaustion
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#ifndef FQC_PIPELINE_PIPELINE_H
#define FQC_PIPELINE_PIPELINE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"

namespace fqc::pipeline {

// =============================================================================
// Forward Declarations
// =============================================================================

class CompressionPipelineImpl;
class DecompressionPipelineImpl;

// =============================================================================
// Constants
// =============================================================================

/// @brief Default number of in-flight blocks (for backpressure)
inline constexpr std::size_t kDefaultMaxInFlightBlocks = 8;

/// @brief Default input buffer size (bytes)
inline constexpr std::size_t kDefaultInputBufferSize = 64 * 1024 * 1024;  // 64MB

/// @brief Default output buffer size (bytes)
inline constexpr std::size_t kDefaultOutputBufferSize = 32 * 1024 * 1024;  // 32MB

/// @brief Minimum block size (reads)
inline constexpr std::size_t kMinBlockSize = 100;

/// @brief Maximum block size (reads)
inline constexpr std::size_t kMaxBlockSize = 1'000'000;

// =============================================================================
// Pipeline Stage Interface
// =============================================================================

/// @brief Base interface for pipeline stages
/// @tparam Input Input type for this stage
/// @tparam Output Output type for this stage
template <typename Input, typename Output>
class IPipelineStage {
public:
    virtual ~IPipelineStage() = default;

    /// @brief Process a single item
    /// @param input Input item
    /// @return Processed output or error
    [[nodiscard]] virtual Result<Output> process(Input input) = 0;

    /// @brief Check if stage is ready to process
    /// @return true if ready
    [[nodiscard]] virtual bool isReady() const noexcept { return true; }

    /// @brief Reset stage state
    virtual void reset() noexcept {}

    /// @brief Get stage name for logging
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

// =============================================================================
// Pipeline Token Types
// =============================================================================

/// @brief A chunk of reads to be processed
struct ReadChunk {
    /// @brief Read records in this chunk
    std::vector<ReadRecord> reads;

    /// @brief Chunk ID (sequential)
    std::uint32_t chunkId = 0;

    /// @brief Starting read ID (1-based, archive order)
    ReadId startReadId = 1;

    /// @brief Is this the last chunk?
    bool isLast = false;

    /// @brief Clear the chunk
    void clear() noexcept {
        reads.clear();
        chunkId = 0;
        startReadId = 1;
        isLast = false;
    }

    /// @brief Get number of reads
    [[nodiscard]] std::size_t size() const noexcept { return reads.size(); }

    /// @brief Check if empty
    [[nodiscard]] bool empty() const noexcept { return reads.empty(); }
};

/// @brief A compressed block ready for writing
struct CompressedBlock {
    /// @brief Block ID (globally continuous)
    BlockId blockId = 0;

    /// @brief Compressed ID stream
    std::vector<std::uint8_t> idStream;

    /// @brief Compressed sequence stream
    std::vector<std::uint8_t> seqStream;

    /// @brief Compressed quality stream
    std::vector<std::uint8_t> qualStream;

    /// @brief Compressed auxiliary stream (read lengths)
    std::vector<std::uint8_t> auxStream;

    /// @brief Number of reads in this block
    std::uint32_t readCount = 0;

    /// @brief Uniform read length (0 = variable)
    std::uint32_t uniformReadLength = 0;

    /// @brief Block checksum (xxHash64 of uncompressed data)
    std::uint64_t checksum = 0;

    /// @brief Codec IDs
    std::uint8_t codecIds = 0;
    std::uint8_t codecSeq = 0;
    std::uint8_t codecQual = 0;
    std::uint8_t codecAux = 0;

    /// @brief Starting read ID (archive order)
    ReadId startReadId = 1;

    /// @brief Is this the last block?
    bool isLast = false;

    /// @brief Get total compressed size
    [[nodiscard]] std::size_t totalSize() const noexcept {
        return idStream.size() + seqStream.size() + qualStream.size() + auxStream.size();
    }

    /// @brief Clear the block
    void clear() noexcept {
        blockId = 0;
        idStream.clear();
        seqStream.clear();
        qualStream.clear();
        auxStream.clear();
        readCount = 0;
        uniformReadLength = 0;
        checksum = 0;
        codecIds = codecSeq = codecQual = codecAux = 0;
        startReadId = 1;
        isLast = false;
    }
};

// =============================================================================
// Pipeline Statistics
// =============================================================================

/// @brief Statistics collected during pipeline execution
struct PipelineStats {
    /// @brief Total reads processed
    std::uint64_t totalReads = 0;

    /// @brief Total blocks produced
    std::uint32_t totalBlocks = 0;

    /// @brief Total input bytes (uncompressed)
    std::uint64_t inputBytes = 0;

    /// @brief Total output bytes (compressed)
    std::uint64_t outputBytes = 0;

    /// @brief Processing time (milliseconds)
    std::uint64_t processingTimeMs = 0;

    /// @brief Peak memory usage (bytes)
    std::size_t peakMemoryBytes = 0;

    /// @brief Number of threads used
    std::size_t threadsUsed = 0;

    /// @brief Get compression ratio
    [[nodiscard]] double compressionRatio() const noexcept {
        if (inputBytes == 0) return 1.0;
        return static_cast<double>(outputBytes) / static_cast<double>(inputBytes);
    }

    /// @brief Get bits per base (for sequence compression)
    [[nodiscard]] double bitsPerBase() const noexcept {
        if (inputBytes == 0) return 0.0;
        // Approximate: assume ~50% of input is sequence
        return (static_cast<double>(outputBytes) * 8.0) / (static_cast<double>(inputBytes) * 0.5);
    }

    /// @brief Get throughput (MB/s)
    [[nodiscard]] double throughputMBps() const noexcept {
        if (processingTimeMs == 0) return 0.0;
        return (static_cast<double>(inputBytes) / (1024.0 * 1024.0)) /
               (static_cast<double>(processingTimeMs) / 1000.0);
    }
};

// =============================================================================
// Progress Callback
// =============================================================================

/// @brief Progress information for callbacks
struct ProgressInfo {
    /// @brief Reads processed so far
    std::uint64_t readsProcessed = 0;

    /// @brief Total reads (0 if unknown, e.g., streaming)
    std::uint64_t totalReads = 0;

    /// @brief Bytes processed
    std::uint64_t bytesProcessed = 0;

    /// @brief Total bytes (0 if unknown)
    std::uint64_t totalBytes = 0;

    /// @brief Current block being processed
    std::uint32_t currentBlock = 0;

    /// @brief Elapsed time (milliseconds)
    std::uint64_t elapsedMs = 0;

    /// @brief Get progress ratio (0.0-1.0)
    [[nodiscard]] double ratio() const noexcept {
        if (totalReads > 0) {
            return static_cast<double>(readsProcessed) / static_cast<double>(totalReads);
        }
        if (totalBytes > 0) {
            return static_cast<double>(bytesProcessed) / static_cast<double>(totalBytes);
        }
        return 0.0;
    }

    /// @brief Get estimated time remaining (milliseconds)
    [[nodiscard]] std::uint64_t estimatedRemainingMs() const noexcept {
        double r = ratio();
        if (r <= 0.0 || r >= 1.0) return 0;
        return static_cast<std::uint64_t>(static_cast<double>(elapsedMs) * (1.0 - r) / r);
    }
};

/// @brief Progress callback type
/// @param info Current progress information
/// @return true to continue, false to cancel
using ProgressCallback = std::function<bool(const ProgressInfo& info)>;

// =============================================================================
// Pipeline Configuration
// =============================================================================

/// @brief Configuration for compression pipeline
struct CompressionPipelineConfig {
    /// @brief Number of threads (0 = auto-detect)
    std::size_t numThreads = 0;

    /// @brief Maximum in-flight blocks (for backpressure)
    std::size_t maxInFlightBlocks = kDefaultMaxInFlightBlocks;

    /// @brief Input buffer size (bytes)
    std::size_t inputBufferSize = kDefaultInputBufferSize;

    /// @brief Output buffer size (bytes)
    std::size_t outputBufferSize = kDefaultOutputBufferSize;

    /// @brief Memory limit (MB, 0 = no limit)
    std::size_t memoryLimitMB = kDefaultMemoryLimitMB;

    /// @brief Block size (reads per block)
    std::size_t blockSize = kDefaultBlockSizeShort;

    /// @brief Read length class
    ReadLengthClass readLengthClass = ReadLengthClass::kShort;

    /// @brief Quality compression mode
    QualityMode qualityMode = QualityMode::kLossless;

    /// @brief ID handling mode
    IDMode idMode = IDMode::kExact;

    /// @brief Compression level (1-9)
    CompressionLevel compressionLevel = kDefaultCompressionLevel;

    /// @brief Enable read reordering
    bool enableReorder = true;

    /// @brief Save reorder map
    bool saveReorderMap = true;

    /// @brief Streaming mode (no global analysis)
    bool streamingMode = false;

    /// @brief Progress callback
    ProgressCallback progressCallback;

    /// @brief Progress callback interval (milliseconds)
    std::uint32_t progressIntervalMs = 500;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;

    /// @brief Get effective number of threads
    [[nodiscard]] std::size_t effectiveThreads() const noexcept;

    /// @brief Get effective block size based on read length class
    [[nodiscard]] std::size_t effectiveBlockSize() const noexcept;
};

/// @brief Configuration for decompression pipeline
struct DecompressionPipelineConfig {
    /// @brief Number of threads (0 = auto-detect)
    std::size_t numThreads = 0;

    /// @brief Maximum in-flight blocks
    std::size_t maxInFlightBlocks = kDefaultMaxInFlightBlocks;

    /// @brief Output buffer size (bytes)
    std::size_t outputBufferSize = kDefaultOutputBufferSize;

    /// @brief Range start (1-based, 0 = from beginning)
    ReadId rangeStart = 0;

    /// @brief Range end (1-based, 0 = to end)
    ReadId rangeEnd = 0;

    /// @brief Output in original order (requires reorder map)
    bool originalOrder = false;

    /// @brief Header-only mode (IDs only)
    bool headerOnly = false;

    /// @brief Verify checksums
    bool verifyChecksums = true;

    /// @brief Skip corrupted blocks
    bool skipCorrupted = false;

    /// @brief Placeholder quality for discard mode
    char placeholderQual = kDefaultPlaceholderQual;

    /// @brief Progress callback
    ProgressCallback progressCallback;

    /// @brief Progress callback interval (milliseconds)
    std::uint32_t progressIntervalMs = 500;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;

    /// @brief Get effective number of threads
    [[nodiscard]] std::size_t effectiveThreads() const noexcept;
};

// =============================================================================
// Compression Pipeline
// =============================================================================

/// @brief Main compression pipeline class
///
/// Implements a TBB-based parallel pipeline for FASTQ compression:
/// - Reader stage: Parses FASTQ input (serial)
/// - Compressor stage: Compresses blocks (parallel)
/// - Writer stage: Writes to output file (serial)
///
/// Usage:
/// @code
/// CompressionPipelineConfig config;
/// config.numThreads = 4;
/// config.blockSize = 100000;
///
/// CompressionPipeline pipeline(config);
/// auto result = pipeline.run("input.fastq", "output.fqc");
/// if (result) {
///     auto stats = pipeline.stats();
///     // Use stats...
/// }
/// @endcode
class CompressionPipeline {
public:
    /// @brief Construct with configuration
    /// @param config Pipeline configuration
    explicit CompressionPipeline(CompressionPipelineConfig config = {});

    /// @brief Destructor
    ~CompressionPipeline();

    // Non-copyable, movable
    CompressionPipeline(const CompressionPipeline&) = delete;
    CompressionPipeline& operator=(const CompressionPipeline&) = delete;
    CompressionPipeline(CompressionPipeline&&) noexcept;
    CompressionPipeline& operator=(CompressionPipeline&&) noexcept;

    /// @brief Run compression pipeline
    /// @param inputPath Input FASTQ file path (or "-" for stdin)
    /// @param outputPath Output FQC file path
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult run(
        const std::filesystem::path& inputPath,
        const std::filesystem::path& outputPath);

    /// @brief Run compression with paired-end input
    /// @param input1Path First input file (R1)
    /// @param input2Path Second input file (R2)
    /// @param outputPath Output FQC file path
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult runPaired(
        const std::filesystem::path& input1Path,
        const std::filesystem::path& input2Path,
        const std::filesystem::path& outputPath);

    /// @brief Cancel running pipeline
    void cancel() noexcept;

    /// @brief Check if pipeline is running
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Check if pipeline was cancelled
    [[nodiscard]] bool isCancelled() const noexcept;

    /// @brief Get pipeline statistics
    [[nodiscard]] const PipelineStats& stats() const noexcept;

    /// @brief Get current configuration
    [[nodiscard]] const CompressionPipelineConfig& config() const noexcept;

    /// @brief Update configuration (only when not running)
    /// @param config New configuration
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult setConfig(CompressionPipelineConfig config);

    /// @brief Reset pipeline state
    void reset() noexcept;

private:
    std::unique_ptr<CompressionPipelineImpl> impl_;
};

// =============================================================================
// Decompression Pipeline
// =============================================================================

/// @brief Main decompression pipeline class
///
/// Implements a TBB-based parallel pipeline for FQC decompression:
/// - Reader stage: Reads FQC blocks (serial)
/// - Decompressor stage: Decompresses blocks (parallel)
/// - Writer stage: Writes FASTQ output (serial)
///
/// Supports:
/// - Full file decompression
/// - Range extraction (random access)
/// - Original order output (with reorder map)
/// - Header-only extraction
class DecompressionPipeline {
public:
    /// @brief Construct with configuration
    /// @param config Pipeline configuration
    explicit DecompressionPipeline(DecompressionPipelineConfig config = {});

    /// @brief Destructor
    ~DecompressionPipeline();

    // Non-copyable, movable
    DecompressionPipeline(const DecompressionPipeline&) = delete;
    DecompressionPipeline& operator=(const DecompressionPipeline&) = delete;
    DecompressionPipeline(DecompressionPipeline&&) noexcept;
    DecompressionPipeline& operator=(DecompressionPipeline&&) noexcept;

    /// @brief Run decompression pipeline
    /// @param inputPath Input FQC file path
    /// @param outputPath Output FASTQ file path (or "-" for stdout)
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult run(
        const std::filesystem::path& inputPath,
        const std::filesystem::path& outputPath);

    /// @brief Run decompression with paired-end output
    /// @param inputPath Input FQC file path
    /// @param output1Path First output file (R1)
    /// @param output2Path Second output file (R2)
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult runPaired(
        const std::filesystem::path& inputPath,
        const std::filesystem::path& output1Path,
        const std::filesystem::path& output2Path);

    /// @brief Cancel running pipeline
    void cancel() noexcept;

    /// @brief Check if pipeline is running
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Check if pipeline was cancelled
    [[nodiscard]] bool isCancelled() const noexcept;

    /// @brief Get pipeline statistics
    [[nodiscard]] const PipelineStats& stats() const noexcept;

    /// @brief Get current configuration
    [[nodiscard]] const DecompressionPipelineConfig& config() const noexcept;

    /// @brief Update configuration (only when not running)
    /// @param config New configuration
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult setConfig(DecompressionPipelineConfig config);

    /// @brief Reset pipeline state
    void reset() noexcept;

private:
    std::unique_ptr<DecompressionPipelineImpl> impl_;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Get recommended number of threads for current system
/// @return Recommended thread count
[[nodiscard]] std::size_t recommendedThreadCount() noexcept;

/// @brief Get recommended block size for given read length class
/// @param lengthClass Read length classification
/// @return Recommended block size (reads)
[[nodiscard]] std::size_t recommendedBlockSize(ReadLengthClass lengthClass) noexcept;

/// @brief Estimate memory usage for compression
/// @param config Pipeline configuration
/// @param estimatedReads Estimated number of reads
/// @return Estimated memory usage (bytes)
[[nodiscard]] std::size_t estimateMemoryUsage(
    const CompressionPipelineConfig& config,
    std::size_t estimatedReads);

/// @brief Check if system has enough memory for configuration
/// @param config Pipeline configuration
/// @param estimatedReads Estimated number of reads
/// @return true if memory is sufficient
[[nodiscard]] bool hasEnoughMemory(
    const CompressionPipelineConfig& config,
    std::size_t estimatedReads);

}  // namespace fqc::pipeline

#endif  // FQC_PIPELINE_PIPELINE_H
