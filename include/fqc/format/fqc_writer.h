// =============================================================================
// fq-compressor - FQC Archive Writer
// =============================================================================
// Writer for the .fqc archive format with atomic write support.
//
// This module provides:
// - FQCWriter: Main class for writing .fqc archives
// - Atomic write support using temporary files (.fqc.tmp)
// - Signal handling for cleanup on SIGINT/SIGTERM
// - xxHash64 checksum calculation for blocks and global file
// - Block index construction and writing
//
// Usage:
//   FQCWriter writer("/path/to/output.fqc");
//   writer.writeGlobalHeader(header, "original.fastq", timestamp);
//   for (const auto& block : blocks) {
//       writer.writeBlock(blockHeader, payload);
//   }
//   writer.finalize();  // Writes index, footer, renames temp to final
//
// Requirements: 2.1, 5.1, 5.2, 8.1, 8.2
// =============================================================================

#ifndef FQC_FORMAT_FQC_WRITER_H
#define FQC_FORMAT_FQC_WRITER_H

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"
#include "fqc/format/fqc_format.h"

namespace fqc::format {

// =============================================================================
// Forward Declarations
// =============================================================================

class FQCWriter;

// =============================================================================
// Signal Handler Management
// =============================================================================

/// @brief Register a writer for signal-based cleanup.
/// @param writer Pointer to the writer to register.
/// @note Thread-safe. Multiple writers can be registered.
void registerWriterForCleanup(FQCWriter* writer);

/// @brief Unregister a writer from signal-based cleanup.
/// @param writer Pointer to the writer to unregister.
/// @note Thread-safe.
void unregisterWriterForCleanup(FQCWriter* writer);

/// @brief Install signal handlers for SIGINT and SIGTERM.
/// @note Called automatically when first writer is created.
/// @note Thread-safe, idempotent.
void installSignalHandlers();

// =============================================================================
// Block Payload Structure
// =============================================================================

/// @brief Represents the compressed payload data for a block.
/// @note Contains compressed streams for IDs, sequences, quality, and aux data.
struct BlockPayload {
    /// @brief Compressed ID stream data.
    std::vector<std::uint8_t> idsData;

    /// @brief Compressed sequence stream data.
    std::vector<std::uint8_t> seqData;

    /// @brief Compressed quality stream data.
    std::vector<std::uint8_t> qualData;

    /// @brief Compressed auxiliary stream data (read lengths).
    std::vector<std::uint8_t> auxData;

    /// @brief Get total compressed size.
    [[nodiscard]] std::size_t totalSize() const noexcept {
        return idsData.size() + seqData.size() + qualData.size() + auxData.size();
    }

    /// @brief Check if payload is empty.
    [[nodiscard]] bool empty() const noexcept {
        return idsData.empty() && seqData.empty() && qualData.empty() && auxData.empty();
    }
};

// =============================================================================
// FQCWriter Class
// =============================================================================

/// @brief Writer for .fqc archive format.
/// @note Implements atomic write using temporary file + rename strategy.
/// @note Registers for signal handling to cleanup on SIGINT/SIGTERM.
///
/// Thread Safety:
/// - Not thread-safe for concurrent writes
/// - Signal handling is thread-safe
///
/// Error Handling:
/// - Throws IOError on file operation failures
/// - Throws FormatError on invalid data
/// - Automatically cleans up temp file on destruction if not finalized
class FQCWriter {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /// @brief Construct a writer for the specified output path.
    /// @param outputPath Path to the final output file.
    /// @throws IOError if temporary file cannot be created.
    explicit FQCWriter(std::filesystem::path outputPath);

    /// @brief Destructor - cleans up temp file if not finalized.
    ~FQCWriter();

    // Non-copyable, non-movable (due to signal handler registration)
    FQCWriter(const FQCWriter&) = delete;
    FQCWriter& operator=(const FQCWriter&) = delete;
    FQCWriter(FQCWriter&&) = delete;
    FQCWriter& operator=(FQCWriter&&) = delete;

    // =========================================================================
    // Writing Operations
    // =========================================================================

    /// @brief Write the magic header and global header.
    /// @param header The global header structure.
    /// @param originalFilename Original input filename (UTF-8).
    /// @param timestamp Unix timestamp of compression.
    /// @throws IOError on write failure.
    /// @throws FormatError if header is invalid.
    /// @note Must be called before any writeBlock() calls.
    void writeGlobalHeader(const GlobalHeader& header,
                           std::string_view originalFilename,
                           std::uint64_t timestamp);

    /// @brief Write a compressed block.
    /// @param header The block header (will be updated with offsets/sizes).
    /// @param payload The compressed block payload.
    /// @throws IOError on write failure.
    /// @throws FormatError if header is invalid.
    /// @note Automatically updates block index and global checksum.
    void writeBlock(BlockHeader header, const BlockPayload& payload);

    /// @brief Write a compressed block with raw payload data.
    /// @param header The block header.
    /// @param payloadData Raw payload bytes (concatenated streams).
    /// @throws IOError on write failure.
    /// @note Use this overload when payload is already serialized.
    void writeBlock(const BlockHeader& header, std::span<const std::uint8_t> payloadData);

    /// @brief Write the reorder map (optional).
    /// @param mapHeader The reorder map header.
    /// @param forwardMapData Compressed forward map data.
    /// @param reverseMapData Compressed reverse map data.
    /// @throws IOError on write failure.
    /// @note Must be called after all blocks and before finalize().
    void writeReorderMap(const ReorderMap& mapHeader,
                         std::span<const std::uint8_t> forwardMapData,
                         std::span<const std::uint8_t> reverseMapData);

    /// @brief Finalize the archive.
    /// @throws IOError on write failure or rename failure.
    /// @note Writes block index, file footer, and renames temp to final.
    /// @note After this call, the writer is in a finalized state.
    void finalize();

    /// @brief Abort writing and clean up temporary file.
    /// @note Safe to call multiple times.
    /// @note Called automatically by destructor if not finalized.
    void abort() noexcept;

    // =========================================================================
    // State Queries
    // =========================================================================

    /// @brief Check if the writer has been finalized.
    /// @return true if finalize() was called successfully.
    [[nodiscard]] bool isFinalized() const noexcept { return finalized_; }

    /// @brief Check if the writer has been aborted.
    /// @return true if abort() was called.
    [[nodiscard]] bool isAborted() const noexcept { return aborted_; }

    /// @brief Get the final output path.
    /// @return Path to the final output file.
    [[nodiscard]] const std::filesystem::path& outputPath() const noexcept { return outputPath_; }

    /// @brief Get the temporary file path.
    /// @return Path to the temporary file.
    [[nodiscard]] const std::filesystem::path& tempPath() const noexcept { return tempPath_; }

    /// @brief Get the current file position.
    /// @return Current write position in bytes.
    [[nodiscard]] std::uint64_t currentPosition() const;

    /// @brief Get the number of blocks written.
    /// @return Number of blocks written so far.
    [[nodiscard]] std::size_t blockCount() const noexcept { return index_.size(); }

    /// @brief Get the total read count written.
    /// @return Total number of reads across all blocks.
    [[nodiscard]] std::uint64_t totalReadCount() const noexcept { return totalReadCount_; }

    // =========================================================================
    // Checksum Access
    // =========================================================================

    /// @brief Get the current global checksum state.
    /// @return Current xxHash64 checksum value.
    /// @note This is the running checksum, not the final value.
    [[nodiscard]] std::uint64_t currentGlobalChecksum() const noexcept;

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /// @brief Write raw bytes to the file.
    /// @param data Pointer to data.
    /// @param size Number of bytes to write.
    /// @throws IOError on write failure.
    void writeBytes(const void* data, std::size_t size);

    /// @brief Write raw bytes and update global checksum.
    /// @param data Pointer to data.
    /// @param size Number of bytes to write.
    /// @throws IOError on write failure.
    void writeBytesWithChecksum(const void* data, std::size_t size);

    /// @brief Write a value in little-endian format.
    /// @tparam T The value type (must be trivially copyable).
    /// @param value The value to write.
    template <typename T>
    void writeLE(T value);

    /// @brief Write a value in little-endian format with checksum update.
    /// @tparam T The value type (must be trivially copyable).
    /// @param value The value to write.
    template <typename T>
    void writeLEWithChecksum(T value);

    /// @brief Write the block index.
    /// @throws IOError on write failure.
    void writeBlockIndex();

    /// @brief Write the file footer.
    /// @throws IOError on write failure.
    void writeFileFooter();

    /// @brief Cleanup temporary file.
    void cleanupTempFile() noexcept;

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// @brief Final output file path.
    std::filesystem::path outputPath_;

    /// @brief Temporary file path (.fqc.tmp).
    std::filesystem::path tempPath_;

    /// @brief Output file stream.
    std::ofstream stream_;

    /// @brief Block index entries.
    std::vector<IndexEntry> index_;

    /// @brief Reorder map offset (0 if not present).
    std::uint64_t reorderMapOffset_ = 0;

    /// @brief Total read count across all blocks.
    std::uint64_t totalReadCount_ = 0;

    /// @brief Next archive ID for block index.
    std::uint64_t nextArchiveId_ = 1;  // 1-based indexing

    /// @brief xxHash64 state for global checksum.
    /// @note Using void* to avoid exposing xxHash header.
    void* xxhashState_ = nullptr;

    /// @brief Whether global header has been written.
    bool headerWritten_ = false;

    /// @brief Whether the writer has been finalized.
    std::atomic<bool> finalized_{false};

    /// @brief Whether the writer has been aborted.
    std::atomic<bool> aborted_{false};

    /// @brief Mutex for signal handler access.
    mutable std::mutex mutex_;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Calculate xxHash64 checksum of data.
/// @param data Pointer to data.
/// @param size Size of data in bytes.
/// @param seed Optional seed value (default 0).
/// @return xxHash64 checksum value.
[[nodiscard]] std::uint64_t calculateXxHash64(const void* data,
                                               std::size_t size,
                                               std::uint64_t seed = 0);

/// @brief Calculate xxHash64 checksum of multiple data segments.
/// @param segments Vector of data spans.
/// @param seed Optional seed value (default 0).
/// @return xxHash64 checksum value.
[[nodiscard]] std::uint64_t calculateXxHash64(
    const std::vector<std::span<const std::uint8_t>>& segments,
    std::uint64_t seed = 0);

/// @brief Calculate block checksum from uncompressed logical streams.
/// @param idsData Uncompressed ID stream data.
/// @param seqData Uncompressed sequence stream data.
/// @param qualData Uncompressed quality stream data.
/// @param auxData Uncompressed auxiliary stream data.
/// @return xxHash64 checksum of concatenated streams.
[[nodiscard]] std::uint64_t calculateBlockChecksum(
    std::span<const std::uint8_t> idsData,
    std::span<const std::uint8_t> seqData,
    std::span<const std::uint8_t> qualData,
    std::span<const std::uint8_t> auxData);

}  // namespace fqc::format

#endif  // FQC_FORMAT_FQC_WRITER_H
