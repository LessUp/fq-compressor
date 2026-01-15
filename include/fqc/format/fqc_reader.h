// =============================================================================
// fq-compressor - FQC Archive Reader
// =============================================================================
// Reader for the .fqc archive format with random access support.
//
// This module provides:
// - FQCReader: Main class for reading .fqc archives
// - Random access via Block Index
// - Checksum verification (optional)
// - Header-only mode (only decode ID stream)
// - Selective stream decoding (id, seq, qual, or combinations)
//
// Usage:
//   FQCReader reader("/path/to/archive.fqc");
//   reader.open();
//   auto header = reader.globalHeader();
//   for (BlockId i = 0; i < reader.blockCount(); ++i) {
//       auto block = reader.readBlock(i);
//       // process block...
//   }
//
// Requirements: 2.1, 2.2, 2.3, 5.1, 5.2
// =============================================================================

#ifndef FQC_FORMAT_FQC_READER_H
#define FQC_FORMAT_FQC_READER_H

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"
#include "fqc/format/fqc_format.h"

namespace fqc::format {

// =============================================================================
// Stream Selection Flags
// =============================================================================

/// @brief Flags for selective stream decoding.
enum class StreamSelection : std::uint8_t {
    kNone = 0,
    kIds = 1 << 0,
    kSequence = 1 << 1,
    kQuality = 1 << 2,
    kAux = 1 << 3,
    kAll = kIds | kSequence | kQuality | kAux
};

/// @brief Bitwise OR for StreamSelection.
[[nodiscard]] constexpr StreamSelection operator|(StreamSelection a, StreamSelection b) noexcept {
    return static_cast<StreamSelection>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

/// @brief Bitwise AND for StreamSelection.
[[nodiscard]] constexpr StreamSelection operator&(StreamSelection a, StreamSelection b) noexcept {
    return static_cast<StreamSelection>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

/// @brief Check if a stream is selected.
[[nodiscard]] constexpr bool hasStream(StreamSelection selection, StreamSelection stream) noexcept {
    return (static_cast<std::uint8_t>(selection) & static_cast<std::uint8_t>(stream)) != 0;
}

// =============================================================================
// Block Data Structure
// =============================================================================

/// @brief Represents raw block data read from archive.
struct BlockData {
    /// @brief Block header.
    BlockHeader header;

    /// @brief Compressed ID stream data.
    std::vector<std::uint8_t> idsData;

    /// @brief Compressed sequence stream data.
    std::vector<std::uint8_t> seqData;

    /// @brief Compressed quality stream data.
    std::vector<std::uint8_t> qualData;

    /// @brief Compressed auxiliary stream data.
    std::vector<std::uint8_t> auxData;

    /// @brief Check if block data is empty.
    [[nodiscard]] bool empty() const noexcept {
        return idsData.empty() && seqData.empty() && qualData.empty() && auxData.empty();
    }
};

// =============================================================================
// Reorder Map Data Structure
// =============================================================================

/// @brief Represents loaded reorder map data.
struct ReorderMapData {
    /// @brief Reorder map header.
    ReorderMap header;

    /// @brief Decompressed forward map (original_id -> archive_id).
    std::vector<std::uint64_t> forwardMap;

    /// @brief Decompressed reverse map (archive_id -> original_id).
    std::vector<std::uint64_t> reverseMap;

    /// @brief Check if reorder map is loaded.
    [[nodiscard]] bool isLoaded() const noexcept {
        return !forwardMap.empty() && !reverseMap.empty();
    }

    /// @brief Lookup archive ID from original ID.
    /// @param originalId Original read ID (1-based).
    /// @return Archive ID, or kInvalidReadId if not found.
    [[nodiscard]] ReadId lookupArchiveId(ReadId originalId) const noexcept {
        if (originalId == 0 || originalId > forwardMap.size()) {
            return kInvalidReadId;
        }
        return forwardMap[originalId - 1];  // Convert to 0-based index
    }

    /// @brief Lookup original ID from archive ID.
    /// @param archiveId Archive read ID (1-based).
    /// @return Original ID, or kInvalidReadId if not found.
    [[nodiscard]] ReadId lookupOriginalId(ReadId archiveId) const noexcept {
        if (archiveId == 0 || archiveId > reverseMap.size()) {
            return kInvalidReadId;
        }
        return reverseMap[archiveId - 1];  // Convert to 0-based index
    }
};

// =============================================================================
// FQCReader Class
// =============================================================================

/// @brief Reader for .fqc archive format.
/// @note Supports random access via Block Index.
/// @note Checksum verification is optional.
///
/// Thread Safety:
/// - Not thread-safe for concurrent reads
/// - Use separate reader instances for parallel access
class FQCReader {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /// @brief Construct a reader for the specified archive path.
    /// @param archivePath Path to the .fqc archive file.
    explicit FQCReader(std::filesystem::path archivePath);

    /// @brief Destructor.
    ~FQCReader();

    // Non-copyable, movable
    FQCReader(const FQCReader&) = delete;
    FQCReader& operator=(const FQCReader&) = delete;
    FQCReader(FQCReader&&) noexcept;
    FQCReader& operator=(FQCReader&&) noexcept;

    // =========================================================================
    // Opening and Closing
    // =========================================================================

    /// @brief Open the archive and read metadata.
    /// @throws IOError if file cannot be opened.
    /// @throws FormatError if file format is invalid.
    /// @note Reads magic header, global header, footer, and block index.
    void open();

    /// @brief Close the archive.
    void close() noexcept;

    /// @brief Check if the archive is open.
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    // =========================================================================
    // Metadata Access
    // =========================================================================

    /// @brief Get the archive file path.
    [[nodiscard]] const std::filesystem::path& archivePath() const noexcept { return archivePath_; }

    /// @brief Get the format version.
    [[nodiscard]] std::uint8_t version() const noexcept { return version_; }

    /// @brief Get the global header.
    /// @throws FormatError if archive is not open.
    [[nodiscard]] const GlobalHeader& globalHeader() const;

    /// @brief Get the file footer.
    /// @throws FormatError if archive is not open.
    [[nodiscard]] const FileFooter& footer() const;

    /// @brief Get the block index.
    /// @throws FormatError if archive is not open.
    [[nodiscard]] const std::vector<IndexEntry>& blockIndex() const;

    /// @brief Get the number of blocks.
    [[nodiscard]] std::size_t blockCount() const noexcept { return blockIndex_.size(); }

    /// @brief Get the total read count.
    [[nodiscard]] std::uint64_t totalReadCount() const noexcept;

    /// @brief Get the original filename.
    [[nodiscard]] const std::string& originalFilename() const noexcept { return originalFilename_; }

    /// @brief Get the timestamp.
    [[nodiscard]] std::uint64_t timestamp() const noexcept { return timestamp_; }

    // =========================================================================
    // Block Reading
    // =========================================================================

    /// @brief Read a block by ID.
    /// @param blockId Block ID (0-based).
    /// @param selection Which streams to read (default: all).
    /// @return Block data with selected streams.
    /// @throws IOError on read failure.
    /// @throws FormatError if block ID is invalid.
    [[nodiscard]] BlockData readBlock(BlockId blockId,
                                       StreamSelection selection = StreamSelection::kAll);

    /// @brief Read only the block header.
    /// @param blockId Block ID (0-based).
    /// @return Block header.
    /// @throws IOError on read failure.
    /// @throws FormatError if block ID is invalid.
    [[nodiscard]] BlockHeader readBlockHeader(BlockId blockId);

    /// @brief Seek to the start of a block.
    /// @param blockId Block ID (0-based).
    /// @throws IOError on seek failure.
    /// @throws FormatError if block ID is invalid.
    void seekToBlock(BlockId blockId);

    // =========================================================================
    // Random Access
    // =========================================================================

    /// @brief Find the block containing a specific read.
    /// @param archiveId Archive read ID (1-based).
    /// @return Block ID, or kInvalidBlockId if not found.
    [[nodiscard]] BlockId findBlockForRead(ReadId archiveId) const noexcept;

    /// @brief Get the index entry for a block.
    /// @param blockId Block ID (0-based).
    /// @return Index entry, or nullopt if invalid.
    [[nodiscard]] std::optional<IndexEntry> getIndexEntry(BlockId blockId) const noexcept;

    /// @brief Get the range of archive IDs in a block.
    /// @param blockId Block ID (0-based).
    /// @return Pair of (start, end) archive IDs (1-based, end is exclusive).
    [[nodiscard]] std::pair<ReadId, ReadId> getBlockReadRange(BlockId blockId) const noexcept;

    // =========================================================================
    // Reorder Map
    // =========================================================================

    /// @brief Check if reorder map is present.
    [[nodiscard]] bool hasReorderMap() const noexcept;

    /// @brief Load the reorder map.
    /// @throws IOError on read failure.
    /// @throws FormatError if reorder map is not present or invalid.
    void loadReorderMap();

    /// @brief Get the loaded reorder map.
    /// @return Reorder map data, or nullopt if not loaded.
    [[nodiscard]] const std::optional<ReorderMapData>& reorderMap() const noexcept {
        return reorderMap_;
    }

    /// @brief Lookup archive ID from original ID.
    /// @param originalId Original read ID (1-based).
    /// @return Archive ID, or kInvalidReadId if not found or map not loaded.
    [[nodiscard]] ReadId lookupArchiveId(ReadId originalId) const noexcept;

    /// @brief Lookup original ID from archive ID.
    /// @param archiveId Archive read ID (1-based).
    /// @return Original ID, or kInvalidReadId if not found or map not loaded.
    [[nodiscard]] ReadId lookupOriginalId(ReadId archiveId) const noexcept;

    // =========================================================================
    // Checksum Verification
    // =========================================================================

    /// @brief Verify the global checksum.
    /// @return true if checksum matches.
    /// @throws IOError on read failure.
    [[nodiscard]] bool verifyGlobalChecksum();

    /// @brief Verify a block's checksum.
    /// @param blockId Block ID (0-based).
    /// @param uncompressedData Uncompressed block data for verification.
    /// @return true if checksum matches.
    /// @note Requires uncompressed data because checksum is over uncompressed streams.
    [[nodiscard]] bool verifyBlockChecksum(BlockId blockId,
                                            std::span<const std::uint8_t> idsData,
                                            std::span<const std::uint8_t> seqData,
                                            std::span<const std::uint8_t> qualData,
                                            std::span<const std::uint8_t> auxData);

    /// @brief Quick verification of file structure.
    /// @return true if magic and footer are valid.
    [[nodiscard]] bool verifyQuick();

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /// @brief Read and validate magic header.
    void readMagicHeader();

    /// @brief Read global header.
    void readGlobalHeader();

    /// @brief Read file footer.
    void readFileFooter();

    /// @brief Read block index.
    void readBlockIndex();

    /// @brief Read raw bytes from file.
    void readBytes(void* buffer, std::size_t size);

    /// @brief Read a value in little-endian format.
    template <typename T>
    [[nodiscard]] T readLE();

    /// @brief Seek to absolute position.
    void seekTo(std::uint64_t position);

    /// @brief Get current file position.
    [[nodiscard]] std::uint64_t currentPosition() const;

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// @brief Archive file path.
    std::filesystem::path archivePath_;

    /// @brief Input file stream.
    std::ifstream stream_;

    /// @brief Whether archive is open.
    bool isOpen_ = false;

    /// @brief Format version.
    std::uint8_t version_ = 0;

    /// @brief Global header.
    GlobalHeader globalHeader_;

    /// @brief Original filename.
    std::string originalFilename_;

    /// @brief Timestamp.
    std::uint64_t timestamp_ = 0;

    /// @brief File footer.
    FileFooter footer_;

    /// @brief Block index entries.
    std::vector<IndexEntry> blockIndex_;

    /// @brief Loaded reorder map (optional).
    std::optional<ReorderMapData> reorderMap_;

    /// @brief File size in bytes.
    std::uint64_t fileSize_ = 0;
};

}  // namespace fqc::format

#endif  // FQC_FORMAT_FQC_READER_H
