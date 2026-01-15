// =============================================================================
// fq-compressor - Reorder Map Module
// =============================================================================
// Implements bidirectional mapping for read reordering support.
//
// This module provides:
// - Forward Map: original_id -> archive_id (for querying original position)
// - Reverse Map: archive_id -> original_id (for original order output)
// - Delta + Varint compression encoding (~2 bytes/read per map)
// - Serialization/deserialization for .fqc archive format
// - Chunk-wise concatenation with offset accumulation for divide-and-conquer mode
//
// Encoding Strategy:
// - Delta encoding: First value stored as-is, subsequent values as differences
// - Varint encoding: Variable-length integer encoding (like protobuf)
// - Target compression ratio: ~4 bytes/read (two maps at ~2 bytes/read each)
//
// Requirements: 2.1
// =============================================================================

#ifndef FQC_FORMAT_REORDER_MAP_H
#define FQC_FORMAT_REORDER_MAP_H

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"
#include "fqc/format/fqc_format.h"

namespace fqc::format {

// =============================================================================
// Constants
// =============================================================================

/// @brief Current reorder map version
inline constexpr std::uint32_t kReorderMapVersion = 1;

/// @brief Maximum bytes per varint (for uint64_t)
inline constexpr std::size_t kMaxVarintBytes = 10;

/// @brief Target compression ratio (bytes per read for both maps)
inline constexpr double kTargetBytesPerRead = 4.0;

// =============================================================================
// Varint Encoding/Decoding Utilities
// =============================================================================

/// @brief Encode a 64-bit unsigned integer as varint
/// @param value The value to encode
/// @param output Output buffer (must have at least kMaxVarintBytes capacity)
/// @return Number of bytes written
[[nodiscard]] std::size_t encodeVarint(std::uint64_t value, std::uint8_t* output) noexcept;

/// @brief Decode a varint from a byte buffer
/// @param input Input buffer
/// @param inputSize Size of input buffer
/// @param value Output value
/// @return Number of bytes consumed, or 0 on error
[[nodiscard]] std::size_t decodeVarint(const std::uint8_t* input, std::size_t inputSize,
                                        std::uint64_t& value) noexcept;

/// @brief Encode a signed 64-bit integer as zigzag varint
/// @param value The signed value to encode
/// @param output Output buffer (must have at least kMaxVarintBytes capacity)
/// @return Number of bytes written
[[nodiscard]] std::size_t encodeSignedVarint(std::int64_t value, std::uint8_t* output) noexcept;

/// @brief Decode a zigzag varint to signed 64-bit integer
/// @param input Input buffer
/// @param inputSize Size of input buffer
/// @param value Output signed value
/// @return Number of bytes consumed, or 0 on error
[[nodiscard]] std::size_t decodeSignedVarint(const std::uint8_t* input, std::size_t inputSize,
                                              std::int64_t& value) noexcept;

// =============================================================================
// Delta Encoding/Decoding Utilities
// =============================================================================

/// @brief Encode a sequence of read IDs using delta + varint encoding
/// @param ids Input sequence of read IDs
/// @return Compressed byte buffer
[[nodiscard]] std::vector<std::uint8_t> deltaEncode(std::span<const ReadId> ids);

/// @brief Decode a delta + varint encoded sequence back to read IDs
/// @param encoded Compressed byte buffer
/// @param count Expected number of IDs to decode
/// @return Decoded read IDs, or error on failure
[[nodiscard]] Result<std::vector<ReadId>> deltaDecode(std::span<const std::uint8_t> encoded,
                                                       std::uint64_t count);

// =============================================================================
// ReorderMapData Class
// =============================================================================

/// @brief In-memory representation of reorder map data
///
/// This class holds the bidirectional mapping between original and archive IDs:
/// - Forward Map: original_id -> archive_id (for querying)
/// - Reverse Map: archive_id -> original_id (for original order output)
///
/// Usage:
/// @code
/// // Create from global analysis result
/// ReorderMapData mapData(forwardMap, reverseMap);
///
/// // Query mappings
/// ReadId archiveId = mapData.getArchiveId(originalId);
/// ReadId originalId = mapData.getOriginalId(archiveId);
///
/// // Serialize to bytes
/// auto serialized = mapData.serialize();
///
/// // Deserialize from bytes
/// auto mapData = ReorderMapData::deserialize(bytes, totalReads);
/// @endcode
class ReorderMapData {
public:
    /// @brief Default constructor (empty map)
    ReorderMapData() = default;

    /// @brief Construct from forward and reverse maps
    /// @param forwardMap Forward map: original_id -> archive_id
    /// @param reverseMap Reverse map: archive_id -> original_id
    ReorderMapData(std::vector<ReadId> forwardMap, std::vector<ReadId> reverseMap);

    /// @brief Construct from global analysis result
    /// @param forwardMap Forward map span
    /// @param reverseMap Reverse map span
    ReorderMapData(std::span<const ReadId> forwardMap, std::span<const ReadId> reverseMap);

    // Default copy/move operations
    ReorderMapData(const ReorderMapData&) = default;
    ReorderMapData& operator=(const ReorderMapData&) = default;
    ReorderMapData(ReorderMapData&&) noexcept = default;
    ReorderMapData& operator=(ReorderMapData&&) noexcept = default;
    ~ReorderMapData() = default;

    // =========================================================================
    // Query Operations
    // =========================================================================

    /// @brief Get archive ID for an original read ID
    /// @param originalId Original read ID (0-indexed)
    /// @return Archive ID (0-indexed), or originalId if out of range
    [[nodiscard]] ReadId getArchiveId(ReadId originalId) const noexcept;

    /// @brief Get original ID for an archive read ID
    /// @param archiveId Archive read ID (0-indexed)
    /// @return Original ID (0-indexed), or archiveId if out of range
    [[nodiscard]] ReadId getOriginalId(ReadId archiveId) const noexcept;

    /// @brief Get total number of reads in the map
    [[nodiscard]] std::uint64_t totalReads() const noexcept;

    /// @brief Check if the map is empty
    [[nodiscard]] bool empty() const noexcept;

    /// @brief Check if the map is valid (forward and reverse maps have same size)
    [[nodiscard]] bool isValid() const noexcept;

    // =========================================================================
    // Access to Raw Maps
    // =========================================================================

    /// @brief Get the forward map (original_id -> archive_id)
    [[nodiscard]] const std::vector<ReadId>& forwardMap() const noexcept { return forwardMap_; }

    /// @brief Get the reverse map (archive_id -> original_id)
    [[nodiscard]] const std::vector<ReadId>& reverseMap() const noexcept { return reverseMap_; }

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Serialize the reorder map to bytes
    /// @return Serialized byte buffer (header + compressed maps)
    [[nodiscard]] std::vector<std::uint8_t> serialize() const;

    /// @brief Deserialize a reorder map from bytes
    /// @param data Serialized byte buffer
    /// @return Deserialized ReorderMapData, or error on failure
    [[nodiscard]] static Result<ReorderMapData> deserialize(std::span<const std::uint8_t> data);

    /// @brief Get the serialized size estimate (for pre-allocation)
    /// @return Estimated serialized size in bytes
    [[nodiscard]] std::size_t estimateSerializedSize() const noexcept;

    // =========================================================================
    // Chunk Concatenation (for divide-and-conquer mode)
    // =========================================================================

    /// @brief Concatenate another chunk's reorder map with offset adjustment
    /// @param other The chunk's reorder map to append
    /// @param archiveIdOffset Offset to add to archive IDs in the appended chunk
    /// @param originalIdOffset Offset to add to original IDs in the appended chunk
    /// @note This is used in divide-and-conquer mode to merge chunk maps
    void appendChunk(const ReorderMapData& other, ReadId archiveIdOffset,
                     ReadId originalIdOffset);

    /// @brief Create a combined reorder map from multiple chunks
    /// @param chunks Vector of chunk reorder maps
    /// @param chunkSizes Vector of chunk sizes (number of reads per chunk)
    /// @return Combined reorder map with globally continuous IDs
    [[nodiscard]] static ReorderMapData combineChunks(
        std::span<const ReorderMapData> chunks,
        std::span<const std::uint64_t> chunkSizes);

    // =========================================================================
    // Statistics
    // =========================================================================

    /// @brief Get compression statistics
    struct CompressionStats {
        std::uint64_t totalReads;
        std::size_t forwardMapCompressedSize;
        std::size_t reverseMapCompressedSize;
        std::size_t totalCompressedSize;
        double bytesPerRead;
        double compressionRatio;  // uncompressed / compressed
    };

    /// @brief Calculate compression statistics
    [[nodiscard]] CompressionStats getCompressionStats() const;

private:
    std::vector<ReadId> forwardMap_;  // original_id -> archive_id
    std::vector<ReadId> reverseMap_;  // archive_id -> original_id
};

// =============================================================================
// ReorderMapIO Class
// =============================================================================

/// @brief I/O operations for reorder map serialization
///
/// This class provides stream-based I/O for reading and writing reorder maps
/// to/from .fqc archive files.
class ReorderMapIO {
public:
    /// @brief Write a reorder map to an output stream
    /// @param stream Output stream (must be in binary mode)
    /// @param mapData The reorder map data to write
    /// @return VoidResult indicating success or error
    [[nodiscard]] static VoidResult write(std::ostream& stream, const ReorderMapData& mapData);

    /// @brief Read a reorder map from an input stream
    /// @param stream Input stream (must be in binary mode)
    /// @return ReorderMapData, or error on failure
    [[nodiscard]] static Result<ReorderMapData> read(std::istream& stream);

    /// @brief Read only the reorder map header from a stream
    /// @param stream Input stream (must be in binary mode)
    /// @return ReorderMap header structure, or error on failure
    [[nodiscard]] static Result<ReorderMap> readHeader(std::istream& stream);

    /// @brief Write a reorder map to a byte buffer
    /// @param mapData The reorder map data to write
    /// @return Serialized byte buffer
    [[nodiscard]] static std::vector<std::uint8_t> toBytes(const ReorderMapData& mapData);

    /// @brief Read a reorder map from a byte buffer
    /// @param data Input byte buffer
    /// @return ReorderMapData, or error on failure
    [[nodiscard]] static Result<ReorderMapData> fromBytes(std::span<const std::uint8_t> data);
};

// =============================================================================
// Validation Functions
// =============================================================================

/// @brief Validate a reorder map header
/// @param header The header to validate
/// @return VoidResult indicating success or validation error
[[nodiscard]] VoidResult validateReorderMapHeader(const ReorderMap& header);

/// @brief Validate reorder map data consistency
/// @param mapData The map data to validate
/// @return VoidResult indicating success or validation error
[[nodiscard]] VoidResult validateReorderMapData(const ReorderMapData& mapData);

/// @brief Verify that forward and reverse maps are consistent inverses
/// @param forwardMap Forward map: original_id -> archive_id
/// @param reverseMap Reverse map: archive_id -> original_id
/// @return VoidResult indicating success or validation error
[[nodiscard]] VoidResult verifyMapConsistency(std::span<const ReadId> forwardMap,
                                               std::span<const ReadId> reverseMap);

}  // namespace fqc::format

#endif  // FQC_FORMAT_REORDER_MAP_H
