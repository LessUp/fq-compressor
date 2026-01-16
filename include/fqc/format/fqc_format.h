// =============================================================================
// fq-compressor - FQC Archive Format Definitions
// =============================================================================
// Binary format definitions for the .fqc archive format.
//
// This module defines:
// - Magic Header constants (9 bytes)
// - GlobalHeader structure (variable length)
// - BlockHeader structure (104 bytes)
// - BlockIndex and IndexEntry structures
// - ReorderMap structure
// - FileFooter structure (32 bytes)
// - Flag bit definitions and helper functions
//
// File Layout:
// +----------------+
// |  Magic Header  |  (9 bytes)
// +----------------+
// | Global Header  |  (Variable Length)
// +----------------+
// |    Block 0     |
// +----------------+
// |    Block 1     |
// +----------------+
// |      ...       |
// +----------------+
// |    Block N     |
// +----------------+
// | Reorder Map    |  (Optional, Variable Length)
// +----------------+
// |   Block Index  |  (Variable Length)
// +----------------+
// |  File Footer   |  (Fixed Length)
// +----------------+
//
// Requirements: 2.1, 5.1, 5.2
// =============================================================================

#ifndef FQC_FORMAT_FQC_FORMAT_H
#define FQC_FORMAT_FQC_FORMAT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "fqc/common/types.h"

namespace fqc::format {

// =============================================================================
// Magic Header Constants
// =============================================================================

/// @brief Magic number bytes (8 bytes).
/// @note Format: 0x89 'F' 'Q' 'C' 0x0D 0x0A 0x1A 0x0A
/// @note Inspired by PNG/XZ magic number design:
///       - 0x89: High bit set to detect 7-bit transmission corruption
///       - 'FQC': ASCII identifier
///       - 0x0D 0x0A: CR-LF to detect line ending conversion
///       - 0x1A: Ctrl-Z to stop DOS TYPE command
///       - 0x0A: LF to detect CR-LF to LF conversion
inline constexpr std::array<std::uint8_t, 8> kMagicBytes = {
    0x89, 'F', 'Q', 'C', 0x0D, 0x0A, 0x1A, 0x0A
};

/// @brief Magic header size (magic bytes + version).
inline constexpr std::size_t kMagicHeaderSize = 9;

/// @brief Current format major version.
/// @note Major version changes indicate incompatible format changes.
inline constexpr std::uint8_t kFormatVersionMajor = 1;

/// @brief Current format minor version.
/// @note Minor version changes are backward compatible.
inline constexpr std::uint8_t kFormatVersionMinor = 0;

/// @brief Encode version as single byte (major:4bit, minor:4bit).
/// @param major Major version (0-15).
/// @param minor Minor version (0-15).
/// @return Encoded version byte.
[[nodiscard]] constexpr std::uint8_t encodeVersion(std::uint8_t major, std::uint8_t minor) noexcept {
    return static_cast<std::uint8_t>((major << 4) | (minor & 0x0F));
}

/// @brief Decode major version from version byte.
/// @param version Encoded version byte.
/// @return Major version (0-15).
[[nodiscard]] constexpr std::uint8_t decodeMajorVersion(std::uint8_t version) noexcept {
    return static_cast<std::uint8_t>(version >> 4);
}

/// @brief Decode minor version from version byte.
/// @param version Encoded version byte.
/// @return Minor version (0-15).
[[nodiscard]] constexpr std::uint8_t decodeMinorVersion(std::uint8_t version) noexcept {
    return static_cast<std::uint8_t>(version & 0x0F);
}

/// @brief Current format version (encoded).
inline constexpr std::uint8_t kCurrentVersion = encodeVersion(kFormatVersionMajor, kFormatVersionMinor);

// =============================================================================
// File Footer Constants
// =============================================================================

/// @brief File footer magic end marker.
/// @note "FQC_EOF\0" (8 bytes): 0x46 0x51 0x43 0x5F 0x45 0x4F 0x46 0x00
inline constexpr std::array<std::uint8_t, 8> kMagicEnd = {
    'F', 'Q', 'C', '_', 'E', 'O', 'F', '\0'
};

/// @brief File footer size (fixed).
inline constexpr std::size_t kFileFooterSize = 32;

// =============================================================================
// Global Header Flag Bit Definitions
// =============================================================================

/// @brief Global header flags namespace.
/// @note Flags are stored in GlobalHeader.flags (uint64).
namespace flags {

/// @brief Bit 0: Paired-end data flag.
/// @note 0 = Single-end, 1 = Paired-end.
inline constexpr std::uint64_t kIsPaired = 1ULL << 0;

/// @brief Bit 1: Preserve original read order flag.
/// @note 0 = Reordered for compression, 1 = Original order preserved.
inline constexpr std::uint64_t kPreserveOrder = 1ULL << 1;

/// @brief Bit 2: Legacy long read mode (reserved, must be 0).
/// @note Historical compatibility bit, always set to 0.
inline constexpr std::uint64_t kLegacyLongReadMode = 1ULL << 2;

/// @brief Bits 3-4: Quality mode mask.
/// @note 00 = Lossless, 01 = Illumina8, 10 = QVZ, 11 = Discard.
inline constexpr std::uint64_t kQualityModeMask = 0x3ULL << 3;
inline constexpr std::uint8_t kQualityModeShift = 3;

/// @brief Bits 5-6: ID mode mask.
/// @note 00 = Exact, 01 = Tokenize, 10 = Discard.
inline constexpr std::uint64_t kIdModeMask = 0x3ULL << 5;
inline constexpr std::uint8_t kIdModeShift = 5;

/// @brief Bit 7: Has reorder map flag.
/// @note 1 = Reorder map present for original order recovery.
inline constexpr std::uint64_t kHasReorderMap = 1ULL << 7;

/// @brief Bits 8-9: Paired-end layout mask.
/// @note 00 = Interleaved, 01 = Consecutive. Only valid when IS_PAIRED=1.
inline constexpr std::uint64_t kPeLayoutMask = 0x3ULL << 8;
inline constexpr std::uint8_t kPeLayoutShift = 8;

/// @brief Bits 10-11: Read length class mask.
/// @note 00 = Short, 01 = Medium, 10 = Long.
inline constexpr std::uint64_t kReadLengthClassMask = 0x3ULL << 10;
inline constexpr std::uint8_t kReadLengthClassShift = 10;

/// @brief Bit 12: Streaming mode flag.
/// @note 1 = Streaming mode (no global/block reordering).
/// @note Forces PRESERVE_ORDER=1 and HAS_REORDER_MAP=0.
inline constexpr std::uint64_t kStreamingMode = 1ULL << 12;

/// @brief Bits 13-63: Reserved for future use.
inline constexpr std::uint64_t kReservedMask = ~((1ULL << 13) - 1);

}  // namespace flags

// =============================================================================
// Flag Helper Functions
// =============================================================================

/// @brief Check if paired-end flag is set.
/// @param flagsValue The flags field value.
/// @return true if paired-end data.
[[nodiscard]] constexpr bool isPaired(std::uint64_t flagsValue) noexcept {
    return (flagsValue & flags::kIsPaired) != 0;
}

/// @brief Check if preserve order flag is set.
/// @param flagsValue The flags field value.
/// @return true if original order is preserved.
[[nodiscard]] constexpr bool isPreserveOrder(std::uint64_t flagsValue) noexcept {
    return (flagsValue & flags::kPreserveOrder) != 0;
}

/// @brief Check if has reorder map flag is set.
/// @param flagsValue The flags field value.
/// @return true if reorder map is present.
[[nodiscard]] constexpr bool hasReorderMap(std::uint64_t flagsValue) noexcept {
    return (flagsValue & flags::kHasReorderMap) != 0;
}

/// @brief Check if streaming mode flag is set.
/// @param flagsValue The flags field value.
/// @return true if streaming mode is enabled.
[[nodiscard]] constexpr bool isStreamingMode(std::uint64_t flagsValue) noexcept {
    return (flagsValue & flags::kStreamingMode) != 0;
}

/// @brief Extract quality mode from flags.
/// @param flagsValue The flags field value.
/// @return QualityMode enum value.
[[nodiscard]] constexpr QualityMode getQualityMode(std::uint64_t flagsValue) noexcept {
    return static_cast<QualityMode>(
        (flagsValue & flags::kQualityModeMask) >> flags::kQualityModeShift);
}

/// @brief Extract ID mode from flags.
/// @param flagsValue The flags field value.
/// @return IDMode enum value.
[[nodiscard]] constexpr IDMode getIdMode(std::uint64_t flagsValue) noexcept {
    return static_cast<IDMode>(
        (flagsValue & flags::kIdModeMask) >> flags::kIdModeShift);
}

/// @brief Extract PE layout from flags.
/// @param flagsValue The flags field value.
/// @return PELayout enum value.
[[nodiscard]] constexpr PELayout getPeLayout(std::uint64_t flagsValue) noexcept {
    return static_cast<PELayout>(
        (flagsValue & flags::kPeLayoutMask) >> flags::kPeLayoutShift);
}

/// @brief Extract read length class from flags.
/// @param flagsValue The flags field value.
/// @return ReadLengthClass enum value.
[[nodiscard]] constexpr ReadLengthClass getReadLengthClass(std::uint64_t flagsValue) noexcept {
    return static_cast<ReadLengthClass>(
        (flagsValue & flags::kReadLengthClassMask) >> flags::kReadLengthClassShift);
}

/// @brief Set quality mode in flags.
/// @param flagsValue The flags field value to modify.
/// @param mode The quality mode to set.
/// @return Modified flags value.
[[nodiscard]] constexpr std::uint64_t setQualityMode(std::uint64_t flagsValue,
                                                      QualityMode mode) noexcept {
    return (flagsValue & ~flags::kQualityModeMask) |
           (static_cast<std::uint64_t>(mode) << flags::kQualityModeShift);
}

/// @brief Set ID mode in flags.
/// @param flagsValue The flags field value to modify.
/// @param mode The ID mode to set.
/// @return Modified flags value.
[[nodiscard]] constexpr std::uint64_t setIdMode(std::uint64_t flagsValue, IDMode mode) noexcept {
    return (flagsValue & ~flags::kIdModeMask) |
           (static_cast<std::uint64_t>(mode) << flags::kIdModeShift);
}

/// @brief Set PE layout in flags.
/// @param flagsValue The flags field value to modify.
/// @param layout The PE layout to set.
/// @return Modified flags value.
[[nodiscard]] constexpr std::uint64_t setPeLayout(std::uint64_t flagsValue,
                                                   PELayout layout) noexcept {
    return (flagsValue & ~flags::kPeLayoutMask) |
           (static_cast<std::uint64_t>(layout) << flags::kPeLayoutShift);
}

/// @brief Set read length class in flags.
/// @param flagsValue The flags field value to modify.
/// @param lengthClass The read length class to set.
/// @return Modified flags value.
[[nodiscard]] constexpr std::uint64_t setReadLengthClass(std::uint64_t flagsValue,
                                                          ReadLengthClass lengthClass) noexcept {
    return (flagsValue & ~flags::kReadLengthClassMask) |
           (static_cast<std::uint64_t>(lengthClass) << flags::kReadLengthClassShift);
}

/// @brief Build flags value from individual settings.
/// @param isPaired Paired-end data flag.
/// @param preserveOrder Preserve original order flag.
/// @param qualityMode Quality compression mode.
/// @param idMode ID handling mode.
/// @param hasReorderMap Has reorder map flag.
/// @param peLayout Paired-end layout.
/// @param readLengthClass Read length classification.
/// @param streamingMode Streaming mode flag.
/// @return Combined flags value.
[[nodiscard]] constexpr std::uint64_t buildFlags(
    bool isPaired,
    bool preserveOrder,
    QualityMode qualityMode,
    IDMode idMode,
    bool hasReorderMap,
    PELayout peLayout,
    ReadLengthClass readLengthClass,
    bool streamingMode) noexcept {
    std::uint64_t result = 0;
    if (isPaired) result |= flags::kIsPaired;
    if (preserveOrder) result |= flags::kPreserveOrder;
    result = setQualityMode(result, qualityMode);
    result = setIdMode(result, idMode);
    if (hasReorderMap) result |= flags::kHasReorderMap;
    result = setPeLayout(result, peLayout);
    result = setReadLengthClass(result, readLengthClass);
    if (streamingMode) result |= flags::kStreamingMode;
    return result;
}

// =============================================================================
// Codec Constants
// =============================================================================

/// @brief Codec params terminator byte.
/// @note CodecParams section ends with 0xFF.
inline constexpr std::uint8_t kCodecParamsTerminator = 0xFF;

/// @brief Encode codec as (family:4bit, version:4bit).
/// @param family Codec family (0-15).
/// @param version Codec version (0-15).
/// @return Encoded codec byte.
[[nodiscard]] constexpr std::uint8_t encodeCodec(CodecFamily family, std::uint8_t version) noexcept {
    return static_cast<std::uint8_t>((static_cast<std::uint8_t>(family) << 4) | (version & 0x0F));
}

/// @brief Decode codec family from codec byte.
/// @param codec Encoded codec byte.
/// @return Codec family.
[[nodiscard]] constexpr CodecFamily decodeCodecFamily(std::uint8_t codec) noexcept {
    return static_cast<CodecFamily>(codec >> 4);
}

/// @brief Decode codec version from codec byte.
/// @param codec Encoded codec byte.
/// @return Codec version (0-15).
[[nodiscard]] constexpr std::uint8_t decodeCodecVersion(std::uint8_t codec) noexcept {
    return static_cast<std::uint8_t>(codec & 0x0F);
}

// =============================================================================
// GlobalHeader Structure
// =============================================================================

/// @brief Global header structure.
/// @note Variable length due to optional filename and codec params.
/// @note Minimum size: 34 bytes (no filename, no codec params).
///
/// Layout:
/// - header_size (uint32): Total header size including optional fields
/// - flags (uint64): Feature flags
/// - compression_algo (uint8): Primary compression algorithm family
/// - checksum_type (uint8): Checksum algorithm type
/// - reserved (uint16): Alignment padding
/// - total_read_count (uint64): Total number of reads
/// - original_filename_len (uint16): Length of original filename
/// - original_filename (variable): UTF-8 encoded, no null terminator
/// - timestamp (uint64): Unix timestamp
/// - codec_params (optional): Codec parameters, ends with 0xFF
struct GlobalHeader {
    /// @brief Total header size in bytes (including optional fields).
    /// @note Used for forward compatibility - skip unknown fields.
    std::uint32_t headerSize = 0;

    /// @brief Feature flags (see flags namespace).
    std::uint64_t flags = 0;

    /// @brief Primary compression algorithm family ID.
    /// @note For quick identification; actual codecs in BlockHeader.
    std::uint8_t compressionAlgo = 0;

    /// @brief Checksum algorithm type.
    std::uint8_t checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);

    /// @brief Reserved for alignment (must be 0).
    std::uint16_t reserved = 0;

    /// @brief Total number of reads in the archive.
    /// @note For PE data: Total = 2 * Pairs (each read counted separately).
    std::uint64_t totalReadCount = 0;

    /// @brief Length of original filename in bytes.
    std::uint16_t originalFilenameLen = 0;

    // Variable length fields (not in struct, handled separately):
    // - original_filename: UTF-8 string, no null terminator
    // - timestamp: uint64_t Unix timestamp
    // - codec_params: optional, ends with 0xFF

    /// @brief Minimum header size (fixed fields only, no filename).
    static constexpr std::size_t kMinSize = 
        sizeof(headerSize) +           // 4
        sizeof(flags) +                // 8
        sizeof(compressionAlgo) +      // 1
        sizeof(checksumType) +         // 1
        sizeof(reserved) +             // 2
        sizeof(totalReadCount) +       // 8
        sizeof(originalFilenameLen) +  // 2
        sizeof(std::uint64_t);         // timestamp: 8
    // Total: 34 bytes

    /// @brief Check if the header is valid.
    /// @return true if header passes basic validation.
    [[nodiscard]] bool isValid() const noexcept {
        // Reserved must be 0
        if (reserved != 0) return false;
        // Legacy long read mode bit must be 0
        if ((flags & flags::kLegacyLongReadMode) != 0) return false;
        // Header size must be at least minimum
        if (headerSize < kMinSize) return false;
        return true;
    }
};

static_assert(GlobalHeader::kMinSize == 34, "GlobalHeader minimum size must be 34 bytes");

// =============================================================================
// BlockHeader Structure
// =============================================================================

/// @brief Block header structure (104 bytes fixed).
/// @note Each block contains compressed data for a batch of reads.
/// @note Columnar storage: separate streams for IDs, sequences, quality, aux.
///
/// Checksum calculation:
/// - block_xxhash64 is computed over uncompressed logical streams
/// - Order: ID stream || Sequence stream || Quality stream || Aux stream
struct BlockHeader {
    /// @brief Header size in bytes (for forward compatibility).
    std::uint32_t headerSize = kSize;

    /// @brief Block identifier (globally continuous across chunks).
    std::uint32_t blockId = 0;

    /// @brief Checksum algorithm type (same domain as GlobalHeader).
    std::uint8_t checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);

    /// @brief Codec for ID stream (family:4bit, version:4bit).
    std::uint8_t codecIds = encodeCodec(CodecFamily::kDeltaLzma, 0);

    /// @brief Codec for sequence stream.
    std::uint8_t codecSeq = encodeCodec(CodecFamily::kAbcV1, 0);

    /// @brief Codec for quality stream.
    std::uint8_t codecQual = encodeCodec(CodecFamily::kScmV1, 0);

    /// @brief Codec for auxiliary stream (read lengths).
    std::uint8_t codecAux = encodeCodec(CodecFamily::kDeltaVarint, 0);

    /// @brief Reserved for alignment (must be 0).
    std::uint8_t reserved1 = 0;

    /// @brief Reserved for alignment (must be 0).
    std::uint16_t reserved2 = 0;

    /// @brief xxHash64 of uncompressed logical streams.
    /// @note Computed over: ID || Seq || Qual || Aux (uncompressed).
    std::uint64_t blockXxhash64 = 0;

    /// @brief Number of reads in this block.
    std::uint32_t uncompressedCount = 0;

    /// @brief Uniform read length (0 = variable, use Aux stream).
    /// @note If all reads have same length, store here to skip Aux.
    std::uint32_t uniformReadLength = 0;

    /// @brief Total compressed payload size in bytes.
    std::uint64_t compressedSize = 0;

    // Stream offsets (relative to payload start)
    /// @brief Offset to ID stream (relative to payload start).
    std::uint64_t offsetIds = 0;

    /// @brief Offset to sequence stream.
    std::uint64_t offsetSeq = 0;

    /// @brief Offset to quality stream.
    std::uint64_t offsetQual = 0;

    /// @brief Offset to auxiliary stream (read lengths).
    std::uint64_t offsetAux = 0;

    // Stream sizes (compressed)
    /// @brief Compressed size of ID stream.
    std::uint64_t sizeIds = 0;

    /// @brief Compressed size of sequence stream.
    std::uint64_t sizeSeq = 0;

    /// @brief Compressed size of quality stream.
    /// @note 0 with codec=RAW indicates quality discarded.
    std::uint64_t sizeQual = 0;

    /// @brief Compressed size of auxiliary stream.
    /// @note 0 = uniform length (use uniformReadLength).
    std::uint64_t sizeAux = 0;

    /// @brief Fixed block header size.
    static constexpr std::size_t kSize = 104;

    /// @brief Check if the header is valid.
    /// @return true if header passes basic validation.
    [[nodiscard]] bool isValid() const noexcept {
        // Reserved fields must be 0
        if (reserved1 != 0 || reserved2 != 0) return false;
        // Header size must match expected
        if (headerSize < kSize) return false;
        return true;
    }

    /// @brief Check if reads have uniform length.
    /// @return true if all reads have same length.
    [[nodiscard]] bool hasUniformLength() const noexcept {
        return uniformReadLength > 0 && sizeAux == 0;
    }

    /// @brief Check if quality was discarded.
    /// @return true if quality stream is empty with RAW codec.
    [[nodiscard]] bool isQualityDiscarded() const noexcept {
        return sizeQual == 0 && decodeCodecFamily(codecQual) == CodecFamily::kRaw;
    }
};

// Verify BlockHeader size at compile time
static_assert(sizeof(BlockHeader) == 104, "BlockHeader must be exactly 104 bytes");
static_assert(BlockHeader::kSize == 104, "BlockHeader::kSize must be 104");

// =============================================================================
// IndexEntry Structure
// =============================================================================

/// @brief Block index entry structure.
/// @note Each entry describes one block's location and contents.
#pragma pack(push, 1)
struct IndexEntry {
    /// @brief Absolute file offset to block start.
    std::uint64_t offset = 0;

    /// @brief Compressed size of the block (redundant for fast scanning).
    std::uint64_t compressedSize = 0;

    /// @brief Starting archive read ID for this block.
    /// @note Archive order (post-reordering if enabled).
    std::uint64_t archiveIdStart = 0;

    /// @brief Number of reads in this block.
    std::uint32_t readCount = 0;

    /// @brief Fixed entry size.
    static constexpr std::size_t kSize = 28;

    /// @brief Get the ending archive read ID (exclusive).
    /// @return One past the last read ID in this block.
    [[nodiscard]] std::uint64_t archiveIdEnd() const noexcept {
        return archiveIdStart + readCount;
    }

    /// @brief Check if a read ID falls within this block.
    /// @param archiveId The archive read ID to check (1-based).
    /// @return true if the read is in this block.
    [[nodiscard]] bool containsRead(std::uint64_t archiveId) const noexcept {
        return archiveId >= archiveIdStart && archiveId < archiveIdEnd();
    }
};
#pragma pack(pop)

// Verify IndexEntry size
static_assert(sizeof(IndexEntry) == 28, "IndexEntry must be exactly 28 bytes");
static_assert(IndexEntry::kSize == 28, "IndexEntry::kSize must be 28");

// =============================================================================
// BlockIndex Structure
// =============================================================================

/// @brief Block index header structure.
/// @note Contains header_size and entry_size for forward compatibility.
/// @note Followed by array of IndexEntry.
///
/// Forward compatibility rules:
/// - entry_size > IndexEntry::kSize: skip trailing extension fields
/// - entry_size < required fields: report EXIT_FORMAT_ERROR (3)
struct BlockIndex {
    /// @brief Header size in bytes (for forward compatibility).
    std::uint32_t headerSize = kHeaderSize;

    /// @brief Size of each IndexEntry in bytes.
    /// @note For forward compatibility - allows adding fields to IndexEntry.
    std::uint32_t entrySize = IndexEntry::kSize;

    /// @brief Number of blocks in the archive.
    std::uint64_t numBlocks = 0;

    // Followed by: IndexEntry entries[numBlocks]

    /// @brief Fixed header size (excluding entries).
    static constexpr std::size_t kHeaderSize = 16;

    /// @brief Minimum required entry size.
    static constexpr std::size_t kMinEntrySize = IndexEntry::kSize;

    /// @brief Check if the index header is valid.
    /// @return true if header passes basic validation.
    [[nodiscard]] bool isValid() const noexcept {
        // Header size must be at least minimum
        if (headerSize < kHeaderSize) return false;
        // Entry size must be at least minimum required
        if (entrySize < kMinEntrySize) return false;
        return true;
    }

    /// @brief Calculate total index size in bytes.
    /// @return Total size including header and all entries.
    [[nodiscard]] std::size_t totalSize() const noexcept {
        return headerSize + (numBlocks * entrySize);
    }
};

// Verify BlockIndex header size
static_assert(sizeof(BlockIndex) == 16, "BlockIndex header must be exactly 16 bytes");
static_assert(BlockIndex::kHeaderSize == 16, "BlockIndex::kHeaderSize must be 16");

// =============================================================================
// ReorderMap Structure
// =============================================================================

/// @brief Reorder map header structure.
/// @note Optional, present when PRESERVE_ORDER=0 and HAS_REORDER_MAP=1.
/// @note Contains bidirectional mapping for order recovery.
///
/// Maps:
/// - Forward: original_id -> archive_id (for querying original position)
/// - Reverse: archive_id -> original_id (for original order output)
///
/// Encoding: Delta + Varint compression (~2 bytes/read per map)
struct ReorderMap {
    /// @brief Header size in bytes (for forward compatibility).
    std::uint32_t headerSize = kHeaderSize;

    /// @brief Reorder map format version.
    std::uint32_t version = 1;

    /// @brief Total number of reads.
    std::uint64_t totalReads = 0;

    /// @brief Compressed size of forward map.
    std::uint64_t forwardMapSize = 0;

    /// @brief Compressed size of reverse map.
    std::uint64_t reverseMapSize = 0;

    // Followed by:
    // - forward_map[forwardMapSize]: compressed original_id -> archive_id
    // - reverse_map[reverseMapSize]: compressed archive_id -> original_id

    /// @brief Fixed header size (excluding map data).
    static constexpr std::size_t kHeaderSize = 32;

    /// @brief Current reorder map version.
    static constexpr std::uint32_t kCurrentVersion = 1;

    /// @brief Check if the reorder map header is valid.
    /// @return true if header passes basic validation.
    [[nodiscard]] bool isValid() const noexcept {
        // Header size must be at least minimum
        if (headerSize < kHeaderSize) return false;
        // Version must be supported
        if (version > kCurrentVersion) return false;
        return true;
    }

    /// @brief Calculate total reorder map size in bytes.
    /// @return Total size including header and both maps.
    [[nodiscard]] std::size_t totalSize() const noexcept {
        return headerSize + forwardMapSize + reverseMapSize;
    }
};

// Verify ReorderMap header size
static_assert(sizeof(ReorderMap) == 32, "ReorderMap header must be exactly 32 bytes");
static_assert(ReorderMap::kHeaderSize == 32, "ReorderMap::kHeaderSize must be 32");

// =============================================================================
// FileFooter Structure
// =============================================================================

/// @brief File footer structure (32 bytes fixed).
/// @note Located at end of file: fseek(file, -32, SEEK_END).
///
/// Checksum calculation:
/// - global_checksum covers [file start, footer start)
/// - Includes: Magic, GlobalHeader, all Blocks, ReorderMap, BlockIndex
/// - Excludes: FileFooter itself
struct FileFooter {
    /// @brief Absolute file offset to BlockIndex start.
    std::uint64_t indexOffset = 0;

    /// @brief Absolute file offset to ReorderMap start (0 = not present).
    std::uint64_t reorderMapOffset = 0;

    /// @brief xxHash64 of entire file (excluding footer).
    std::uint64_t globalChecksum = 0;

    /// @brief End-of-file magic marker.
    std::array<std::uint8_t, 8> magicEnd = kMagicEnd;

    /// @brief Fixed footer size.
    static constexpr std::size_t kSize = 32;

    /// @brief Check if the footer is valid.
    /// @return true if footer passes basic validation.
    [[nodiscard]] bool isValid() const noexcept {
        // Magic end must match
        return magicEnd == kMagicEnd;
    }

    /// @brief Check if reorder map is present.
    /// @return true if reorder map offset is non-zero.
    [[nodiscard]] bool hasReorderMap() const noexcept {
        return reorderMapOffset != 0;
    }
};

// Verify FileFooter size
static_assert(sizeof(FileFooter) == 32, "FileFooter must be exactly 32 bytes");
static_assert(FileFooter::kSize == 32, "FileFooter::kSize must be 32");

// =============================================================================
// Validation Functions
// =============================================================================

/// @brief Validate magic header bytes.
/// @param data Pointer to 8 bytes of magic data.
/// @return true if magic bytes match expected.
[[nodiscard]] inline bool validateMagic(const std::uint8_t* data) noexcept {
    for (std::size_t i = 0; i < kMagicBytes.size(); ++i) {
        if (data[i] != kMagicBytes[i]) return false;
    }
    return true;
}

/// @brief Validate magic header bytes from array.
/// @param magic Array of 8 magic bytes.
/// @return true if magic bytes match expected.
[[nodiscard]] inline bool validateMagic(const std::array<std::uint8_t, 8>& magic) noexcept {
    return magic == kMagicBytes;
}

/// @brief Validate format version compatibility.
/// @param version Encoded version byte from file.
/// @return true if version is compatible with current reader.
[[nodiscard]] inline bool isVersionCompatible(std::uint8_t version) noexcept {
    const auto major = decodeMajorVersion(version);
    // Major version must match for compatibility
    return major == kFormatVersionMajor;
}

/// @brief Check if version is newer than current.
/// @param version Encoded version byte from file.
/// @return true if file version is newer than reader.
[[nodiscard]] inline bool isVersionNewer(std::uint8_t version) noexcept {
    const auto major = decodeMajorVersion(version);
    const auto minor = decodeMinorVersion(version);
    if (major > kFormatVersionMajor) return true;
    if (major == kFormatVersionMajor && minor > kFormatVersionMinor) return true;
    return false;
}

}  // namespace fqc::format

#endif  // FQC_FORMAT_FQC_FORMAT_H
