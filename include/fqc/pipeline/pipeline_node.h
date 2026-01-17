// =============================================================================
// fq-compressor - Pipeline Node Abstractions
// =============================================================================
// Defines the individual pipeline stages (filters) for TBB parallel_pipeline.
//
// Pipeline stages:
// - ReaderNode: Serial input stage, reads FASTQ and produces ReadChunks
// - CompressorNode: Parallel processing stage, compresses chunks to blocks
// - WriterNode: Serial output stage, writes blocks to FQC file
//
// For decompression:
// - FQCReaderNode: Serial input stage, reads FQC blocks
// - DecompressorNode: Parallel processing stage, decompresses blocks
// - FASTQWriterNode: Serial output stage, writes FASTQ records
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#ifndef FQC_PIPELINE_PIPELINE_NODE_H
#define FQC_PIPELINE_PIPELINE_NODE_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "fqc/algo/block_compressor.h"
#include "fqc/algo/id_compressor.h"
#include "fqc/algo/quality_compressor.h"
#include "fqc/common/error.h"
#include "fqc/common/types.h"
#include "fqc/format/fqc_reader.h"
#include "fqc/format/fqc_writer.h"
#include "fqc/io/fastq_parser.h"
#include "fqc/pipeline/pipeline.h"

namespace fqc::pipeline {

// =============================================================================
// Forward Declarations
// =============================================================================

class ReaderNodeImpl;
class CompressorNodeImpl;
class WriterNodeImpl;
class FQCReaderNodeImpl;
class DecompressorNodeImpl;
class FASTQWriterNodeImpl;

// =============================================================================
// Node State
// =============================================================================

/// @brief State of a pipeline node
enum class NodeState : std::uint8_t {
    /// @brief Node is idle, ready to start
    kIdle = 0,

    /// @brief Node is running
    kRunning = 1,

    /// @brief Node has finished processing
    kFinished = 2,

    /// @brief Node encountered an error
    kError = 3,

    /// @brief Node was cancelled
    kCancelled = 4
};

/// @brief Convert NodeState to string
[[nodiscard]] constexpr std::string_view nodeStateToString(NodeState state) noexcept {
    switch (state) {
        case NodeState::kIdle: return "idle";
        case NodeState::kRunning: return "running";
        case NodeState::kFinished: return "finished";
        case NodeState::kError: return "error";
        case NodeState::kCancelled: return "cancelled";
    }
    return "unknown";
}

// =============================================================================
// Reader Node (Compression Input)
// =============================================================================

/// @brief Configuration for reader node
struct ReaderNodeConfig {
    /// @brief Block size (reads per chunk)
    std::size_t blockSize = kDefaultBlockSizeShort;

    /// @brief Input buffer size (bytes)
    std::size_t bufferSize = kDefaultInputBufferSize;

    /// @brief Read length class (for block size adjustment)
    ReadLengthClass readLengthClass = ReadLengthClass::kShort;

    /// @brief Maximum block bases (for long reads)
    std::size_t maxBlockBases = kDefaultMaxBlockBasesLong;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;
};

/// @brief Reader node for compression pipeline
///
/// Reads FASTQ input and produces chunks of reads for compression.
/// This is a serial (input) stage in the TBB pipeline.
///
/// Features:
/// - Supports plain and compressed (gzip) input
/// - Automatic format detection
/// - Configurable chunk size
/// - Memory-efficient streaming
class ReaderNode {
public:
    /// @brief Construct with configuration
    /// @param config Node configuration
    explicit ReaderNode(ReaderNodeConfig config = {});

    /// @brief Destructor
    ~ReaderNode();

    // Non-copyable, movable
    ReaderNode(const ReaderNode&) = delete;
    ReaderNode& operator=(const ReaderNode&) = delete;
    ReaderNode(ReaderNode&&) noexcept;
    ReaderNode& operator=(ReaderNode&&) noexcept;

    /// @brief Open input file
    /// @param path Input file path (or "-" for stdin)
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult open(const std::filesystem::path& path);

    /// @brief Open paired-end input files
    /// @param path1 First input file (R1)
    /// @param path2 Second input file (R2)
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult openPaired(
        const std::filesystem::path& path1,
        const std::filesystem::path& path2);

    /// @brief Read next chunk of reads
    /// @return ReadChunk or nullopt if EOF, or error
    [[nodiscard]] Result<std::optional<ReadChunk>> readChunk();

    /// @brief Check if more data is available
    [[nodiscard]] bool hasMore() const noexcept;

    /// @brief Get current state
    [[nodiscard]] NodeState state() const noexcept;

    /// @brief Get total reads read so far
    [[nodiscard]] std::uint64_t totalReadsRead() const noexcept;

    /// @brief Get total bytes read so far
    [[nodiscard]] std::uint64_t totalBytesRead() const noexcept;

    /// @brief Get estimated total reads (0 if unknown)
    [[nodiscard]] std::uint64_t estimatedTotalReads() const noexcept;

    /// @brief Close input
    void close() noexcept;

    /// @brief Reset node state
    void reset() noexcept;

    /// @brief Get configuration
    [[nodiscard]] const ReaderNodeConfig& config() const noexcept;

private:
    std::unique_ptr<ReaderNodeImpl> impl_;
};

// =============================================================================
// Compressor Node (Compression Processing)
// =============================================================================

/// @brief Configuration for compressor node
struct CompressorNodeConfig {
    /// @brief Read length class
    ReadLengthClass readLengthClass = ReadLengthClass::kShort;

    /// @brief Quality compression mode
    QualityMode qualityMode = QualityMode::kLossless;

    /// @brief ID handling mode
    IDMode idMode = IDMode::kExact;

    /// @brief Compression level (1-9)
    CompressionLevel compressionLevel = kDefaultCompressionLevel;

    /// @brief Zstd compression level
    int zstdLevel = 3;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;
};

/// @brief Compressor node for compression pipeline
///
/// Compresses chunks of reads into compressed blocks.
/// This is a parallel stage in the TBB pipeline.
///
/// Features:
/// - Thread-safe compression
/// - Supports multiple compression strategies
/// - Configurable quality and ID modes
class CompressorNode {
public:
    /// @brief Construct with configuration
    /// @param config Node configuration
    explicit CompressorNode(CompressorNodeConfig config = {});

    /// @brief Destructor
    ~CompressorNode();

    // Non-copyable, movable
    CompressorNode(const CompressorNode&) = delete;
    CompressorNode& operator=(const CompressorNode&) = delete;
    CompressorNode(CompressorNode&&) noexcept;
    CompressorNode& operator=(CompressorNode&&) noexcept;

    /// @brief Compress a chunk of reads
    /// @param chunk Input chunk
    /// @return Compressed block or error
    [[nodiscard]] Result<CompressedBlock> compress(ReadChunk chunk);

    /// @brief Get current state
    [[nodiscard]] NodeState state() const noexcept;

    /// @brief Get total blocks compressed
    [[nodiscard]] std::uint32_t totalBlocksCompressed() const noexcept;

    /// @brief Reset node state
    void reset() noexcept;

    /// @brief Get configuration
    [[nodiscard]] const CompressorNodeConfig& config() const noexcept;

private:
    std::unique_ptr<CompressorNodeImpl> impl_;
};

// =============================================================================
// Writer Node (Compression Output)
// =============================================================================

/// @brief Configuration for writer node
struct WriterNodeConfig {
    /// @brief Output buffer size (bytes)
    std::size_t bufferSize = kDefaultOutputBufferSize;

    /// @brief Use atomic write (temp file + rename)
    bool atomicWrite = true;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;
};

/// @brief Writer node for compression pipeline
///
/// Writes compressed blocks to FQC output file.
/// This is a serial (output) stage in the TBB pipeline.
///
/// Features:
/// - Ordered block writing
/// - Atomic file operations
/// - Index and footer generation
class WriterNode {
public:
    /// @brief Construct with configuration
    /// @param config Node configuration
    explicit WriterNode(WriterNodeConfig config = {});

    /// @brief Destructor
    ~WriterNode();

    // Non-copyable, movable
    WriterNode(const WriterNode&) = delete;
    WriterNode& operator=(const WriterNode&) = delete;
    WriterNode(WriterNode&&) noexcept;
    WriterNode& operator=(WriterNode&&) noexcept;

    /// @brief Open output file
    /// @param path Output file path
    /// @param globalHeader Global header to write
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult open(
        const std::filesystem::path& path,
        const format::GlobalHeader& globalHeader);

    /// @brief Write a compressed block
    /// @param block Compressed block to write
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult writeBlock(CompressedBlock block);

    /// @brief Finalize output (write index and footer)
    /// @param reorderMap Optional reorder map to write
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult finalize(
        std::optional<std::span<const std::uint8_t>> reorderMap = std::nullopt);

    /// @brief Get current state
    [[nodiscard]] NodeState state() const noexcept;

    /// @brief Get total blocks written
    [[nodiscard]] std::uint32_t totalBlocksWritten() const noexcept;

    /// @brief Get total bytes written
    [[nodiscard]] std::uint64_t totalBytesWritten() const noexcept;

    /// @brief Close output (without finalizing)
    void close() noexcept;

    /// @brief Reset node state
    void reset() noexcept;

    /// @brief Get configuration
    [[nodiscard]] const WriterNodeConfig& config() const noexcept;

private:
    std::unique_ptr<WriterNodeImpl> impl_;
};

// =============================================================================
// FQC Reader Node (Decompression Input)
// =============================================================================

/// @brief Configuration for FQC reader node
struct FQCReaderNodeConfig {
    /// @brief Range start (1-based, 0 = from beginning)
    ReadId rangeStart = 0;

    /// @brief Range end (1-based, 0 = to end)
    ReadId rangeEnd = 0;

    /// @brief Verify checksums
    bool verifyChecksums = true;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;
};

/// @brief FQC reader node for decompression pipeline
///
/// Reads compressed blocks from FQC input file.
/// This is a serial (input) stage in the TBB pipeline.
///
/// Features:
/// - Random access support
/// - Range extraction
/// - Checksum verification
class FQCReaderNode {
public:
    /// @brief Construct with configuration
    /// @param config Node configuration
    explicit FQCReaderNode(FQCReaderNodeConfig config = {});

    /// @brief Destructor
    ~FQCReaderNode();

    // Non-copyable, movable
    FQCReaderNode(const FQCReaderNode&) = delete;
    FQCReaderNode& operator=(const FQCReaderNode&) = delete;
    FQCReaderNode(FQCReaderNode&&) noexcept;
    FQCReaderNode& operator=(FQCReaderNode&&) noexcept;

    /// @brief Open input file
    /// @param path Input FQC file path
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult open(const std::filesystem::path& path);

    /// @brief Read next compressed block
    /// @return Compressed block data or nullopt if EOF, or error
    [[nodiscard]] Result<std::optional<CompressedBlock>> readBlock();

    /// @brief Check if more blocks are available
    [[nodiscard]] bool hasMore() const noexcept;

    /// @brief Get global header
    [[nodiscard]] const format::GlobalHeader& globalHeader() const noexcept;

    /// @brief Get reorder map (if available)
    [[nodiscard]] std::optional<std::span<const std::uint8_t>> reorderMap() const noexcept;

    /// @brief Get current state
    [[nodiscard]] NodeState state() const noexcept;

    /// @brief Get total blocks read
    [[nodiscard]] std::uint32_t totalBlocksRead() const noexcept;

    /// @brief Close input
    void close() noexcept;

    /// @brief Reset node state
    void reset() noexcept;

    /// @brief Get configuration
    [[nodiscard]] const FQCReaderNodeConfig& config() const noexcept;

private:
    std::unique_ptr<FQCReaderNodeImpl> impl_;
};

// =============================================================================
// Decompressor Node (Decompression Processing)
// =============================================================================

/// @brief Configuration for decompressor node
struct DecompressorNodeConfig {
    /// @brief Skip corrupted blocks
    bool skipCorrupted = false;

    /// @brief Placeholder quality for discard mode
    char placeholderQual = kDefaultPlaceholderQual;

    /// @brief ID prefix for discard mode
    std::string idPrefix;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;
};

/// @brief Decompressor node for decompression pipeline
///
/// Decompresses compressed blocks into read records.
/// This is a parallel stage in the TBB pipeline.
///
/// Features:
/// - Thread-safe decompression
/// - Error recovery (skip corrupted)
/// - Quality placeholder support
class DecompressorNode {
public:
    /// @brief Construct with configuration
    /// @param config Node configuration
    explicit DecompressorNode(DecompressorNodeConfig config = {});

    /// @brief Destructor
    ~DecompressorNode();

    // Non-copyable, movable
    DecompressorNode(const DecompressorNode&) = delete;
    DecompressorNode& operator=(const DecompressorNode&) = delete;
    DecompressorNode(DecompressorNode&&) noexcept;
    DecompressorNode& operator=(DecompressorNode&&) noexcept;

    /// @brief Decompress a block
    /// @param block Compressed block
    /// @param globalHeader Global header for context
    /// @return Decompressed read chunk or error
    [[nodiscard]] Result<ReadChunk> decompress(
        CompressedBlock block,
        const format::GlobalHeader& globalHeader);

    /// @brief Get current state
    [[nodiscard]] NodeState state() const noexcept;

    /// @brief Get total blocks decompressed
    [[nodiscard]] std::uint32_t totalBlocksDecompressed() const noexcept;

    /// @brief Reset node state
    void reset() noexcept;

    /// @brief Get configuration
    [[nodiscard]] const DecompressorNodeConfig& config() const noexcept;

private:
    std::unique_ptr<DecompressorNodeImpl> impl_;
};

// =============================================================================
// FASTQ Writer Node (Decompression Output)
// =============================================================================

/// @brief Configuration for FASTQ writer node
struct FASTQWriterNodeConfig {
    /// @brief Output buffer size (bytes)
    std::size_t bufferSize = kDefaultOutputBufferSize;

    /// @brief Line width (0 = no wrapping)
    std::size_t lineWidth = 0;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;
};

/// @brief FASTQ writer node for decompression pipeline
///
/// Writes decompressed reads to FASTQ output file.
/// This is a serial (output) stage in the TBB pipeline.
///
/// Features:
/// - Ordered output
/// - Buffered writing
/// - Optional line wrapping
class FASTQWriterNode {
public:
    /// @brief Construct with configuration
    /// @param config Node configuration
    explicit FASTQWriterNode(FASTQWriterNodeConfig config = {});

    /// @brief Destructor
    ~FASTQWriterNode();

    // Non-copyable, movable
    FASTQWriterNode(const FASTQWriterNode&) = delete;
    FASTQWriterNode& operator=(const FASTQWriterNode&) = delete;
    FASTQWriterNode(FASTQWriterNode&&) noexcept;
    FASTQWriterNode& operator=(FASTQWriterNode&&) noexcept;

    /// @brief Open output file
    /// @param path Output file path (or "-" for stdout)
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult open(const std::filesystem::path& path);

    /// @brief Open paired-end output files
    /// @param path1 First output file (R1)
    /// @param path2 Second output file (R2)
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult openPaired(
        const std::filesystem::path& path1,
        const std::filesystem::path& path2);

    /// @brief Write a chunk of reads
    /// @param chunk Read chunk to write
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult writeChunk(ReadChunk chunk);

    /// @brief Flush output buffers
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult flush();

    /// @brief Get current state
    [[nodiscard]] NodeState state() const noexcept;

    /// @brief Get total reads written
    [[nodiscard]] std::uint64_t totalReadsWritten() const noexcept;

    /// @brief Get total bytes written
    [[nodiscard]] std::uint64_t totalBytesWritten() const noexcept;

    /// @brief Close output
    void close() noexcept;

    /// @brief Reset node state
    void reset() noexcept;

    /// @brief Get configuration
    [[nodiscard]] const FASTQWriterNodeConfig& config() const noexcept;

private:
    std::unique_ptr<FASTQWriterNodeImpl> impl_;
};

// =============================================================================
// Block Ordering Queue
// =============================================================================

/// @brief Thread-safe queue for maintaining block order
///
/// Used to ensure blocks are written in the correct order
/// even when compressed out of order by parallel workers.
template <typename T>
class OrderedQueue {
public:
    /// @brief Construct with expected starting ID
    /// @param startId Starting block/chunk ID
    explicit OrderedQueue(std::uint32_t startId = 0)
        : nextExpectedId_(startId) {}

    /// @brief Push an item with its ID
    /// @param id Item ID
    /// @param item Item to push
    void push(std::uint32_t id, T item) {
        std::lock_guard lock(mutex_);
        pending_[id] = std::move(item);
    }

    /// @brief Try to pop the next expected item
    /// @return Item if available, nullopt otherwise
    std::optional<T> tryPop() {
        std::lock_guard lock(mutex_);
        auto it = pending_.find(nextExpectedId_);
        if (it == pending_.end()) {
            return std::nullopt;
        }
        T item = std::move(it->second);
        pending_.erase(it);
        ++nextExpectedId_;
        return item;
    }

    /// @brief Get next expected ID
    [[nodiscard]] std::uint32_t nextExpectedId() const noexcept {
        std::lock_guard lock(mutex_);
        return nextExpectedId_;
    }

    /// @brief Check if queue is empty
    [[nodiscard]] bool empty() const noexcept {
        std::lock_guard lock(mutex_);
        return pending_.empty();
    }

    /// @brief Get number of pending items
    [[nodiscard]] std::size_t size() const noexcept {
        std::lock_guard lock(mutex_);
        return pending_.size();
    }

    /// @brief Clear the queue
    void clear() noexcept {
        std::lock_guard lock(mutex_);
        pending_.clear();
        nextExpectedId_ = 0;
    }

private:
    mutable std::mutex mutex_;
    std::map<std::uint32_t, T> pending_;
    std::uint32_t nextExpectedId_ = 0;
};

// =============================================================================
// Backpressure Controller
// =============================================================================

/// @brief Controls backpressure in the pipeline
///
/// Prevents memory exhaustion by limiting the number of
/// in-flight items between pipeline stages.
class BackpressureController {
public:
    /// @brief Construct with limit
    /// @param maxInFlight Maximum in-flight items
    explicit BackpressureController(std::size_t maxInFlight = kDefaultMaxInFlightBlocks);

    /// @brief Acquire a slot (blocks if at limit)
    void acquire();

    /// @brief Try to acquire a slot (non-blocking)
    /// @return true if acquired, false if at limit
    [[nodiscard]] bool tryAcquire();

    /// @brief Release a slot
    void release();

    /// @brief Get current in-flight count
    [[nodiscard]] std::size_t inFlight() const noexcept;

    /// @brief Get maximum in-flight limit
    [[nodiscard]] std::size_t maxInFlight() const noexcept;

    /// @brief Reset controller
    void reset() noexcept;

private:
    std::size_t maxInFlight_;
    std::atomic<std::size_t> inFlight_{0};
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace fqc::pipeline

#endif  // FQC_PIPELINE_PIPELINE_NODE_H
