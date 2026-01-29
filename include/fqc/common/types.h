// =============================================================================
// fq-compressor - Common Type Definitions
// =============================================================================
// Core type definitions for the fq-compressor library.
//
// This module defines:
// - ReadRecord: Represents a single FASTQ read (id, sequence, quality)
// - QualityMode: Enum for quality compression modes
// - IDMode: Enum for ID handling modes
// - ReadLengthClass: Enum for read length classification
// - PELayout: Enum for paired-end layout
// - CompressionLevel: Compression level type
// - BlockId, ReadId: Type aliases for IDs
// - C++20 Concepts for type constraints
//
// Naming Conventions (per project style guide):
// - Enums: PascalCase with kConstant values
// - Classes/Structs: PascalCase
// - Member variables: camelCase with trailing _
// - Constants: kConstant
//
// Requirements: 7.3 (Code style)
// =============================================================================

#ifndef FQC_COMMON_TYPES_H
#define FQC_COMMON_TYPES_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace fqc {

// =============================================================================
// Type Aliases
// =============================================================================

/// @brief Type alias for block identifiers.
/// @note Block IDs are globally continuous across chunks.
using BlockId = std::uint32_t;

/// @brief Type alias for read identifiers.
/// @note Read IDs use 1-based indexing (consistent with SAMtools, bedtools).
/// @note For PE data, each read is counted separately (Total = 2 * Pairs).
using ReadId = std::uint64_t;

/// @brief Type alias for compression level (1-9).
using CompressionLevel = std::uint8_t;

/// @brief Type alias for file offsets.
using FileOffset = std::uint64_t;

/// @brief Type alias for checksum values (xxHash64).
using Checksum = std::uint64_t;

// =============================================================================
// Constants
// =============================================================================

/// @brief Invalid block ID sentinel value.
inline constexpr BlockId kInvalidBlockId = std::numeric_limits<BlockId>::max();

/// @brief Invalid read ID sentinel value.
inline constexpr ReadId kInvalidReadId = std::numeric_limits<ReadId>::max();

/// @brief Default compression level.
inline constexpr CompressionLevel kDefaultCompressionLevel = 5;

/// @brief Minimum compression level.
inline constexpr CompressionLevel kMinCompressionLevel = 1;

/// @brief Maximum compression level.
inline constexpr CompressionLevel kMaxCompressionLevel = 9;

/// @brief Default block size for short reads (number of reads per block).
inline constexpr std::size_t kDefaultBlockSizeShort = 100'000;

/// @brief Default block size for medium reads.
inline constexpr std::size_t kDefaultBlockSizeMedium = 50'000;

/// @brief Default block size for long reads.
inline constexpr std::size_t kDefaultBlockSizeLong = 10'000;

/// @brief Spring ABC maximum read length limit.
/// @note Spring's ABC algorithm has a hard-coded limit of 511bp.
inline constexpr std::size_t kSpringMaxReadLength = 511;

/// @brief Threshold for medium read classification (bytes).
inline constexpr std::size_t kMediumReadThreshold = 1'024;  // 1KB

/// @brief Threshold for long read classification (bytes).
inline constexpr std::size_t kLongReadThreshold = 10'240;  // 10KB

/// @brief Threshold for ultra-long read classification (bytes).
inline constexpr std::size_t kUltraLongReadThreshold = 102'400;  // 100KB

/// @brief Default max block bases for long reads (bytes).
inline constexpr std::size_t kDefaultMaxBlockBasesLong = 200 * 1024 * 1024;  // 200MB

/// @brief Default max block bases for ultra-long reads (bytes).
inline constexpr std::size_t kDefaultMaxBlockBasesUltraLong = 50 * 1024 * 1024;  // 50MB

/// @brief Default memory limit (MB).
inline constexpr std::size_t kDefaultMemoryLimitMB = 8192;  // 8GB

/// @brief Default placeholder quality character for discard mode.
inline constexpr char kDefaultPlaceholderQual = '!';  // Phred 0

// =============================================================================
// Quality Mode Enumeration
// =============================================================================

/// @brief Quality value compression modes.
/// @note Stored in GlobalHeader.Flags bits 3-4.
enum class QualityMode : std::uint8_t {
    /// @brief Lossless quality preservation (default).
    kLossless = 0,

    /// @brief Illumina 8-bin lossy compression.
    /// @note Standard binning scheme, minimal impact on downstream analysis.
    kIllumina8 = 1,

    /// @brief QVZ model-based lossy compression.
    /// @note Higher compression ratio with configurable quality loss.
    kQvz = 2,

    /// @brief Discard quality values entirely.
    /// @note Decompression fills with placeholder quality (default '!').
    kDiscard = 3
};

/// @brief Convert QualityMode to string representation.
/// @param mode The quality mode.
/// @return String representation of the mode.
[[nodiscard]] constexpr std::string_view qualityModeToString(QualityMode mode) noexcept {
    switch (mode) {
        case QualityMode::kLossless:
            return "lossless";
        case QualityMode::kIllumina8:
            return "illumina8";
        case QualityMode::kQvz:
            return "qvz";
        case QualityMode::kDiscard:
            return "discard";
    }
    return "unknown";
}

// =============================================================================
// ID Mode Enumeration
// =============================================================================

/// @brief ID (header) handling modes.
/// @note Stored in GlobalHeader.Flags bits 5-6.
enum class IDMode : std::uint8_t {
    /// @brief Preserve exact original IDs (default).
    kExact = 0,

    /// @brief Tokenize and reconstruct IDs.
    /// @note Splits static/dynamic parts for better compression.
    kTokenize = 1,

    /// @brief Discard IDs, replace with sequential numbers.
    /// @note Decompression rebuilds IDs as @1, @2, @3... or @1/1, @1/2...
    kDiscard = 2
};

/// @brief Convert IDMode to string representation.
/// @param mode The ID mode.
/// @return String representation of the mode.
[[nodiscard]] constexpr std::string_view idModeToString(IDMode mode) noexcept {
    switch (mode) {
        case IDMode::kExact:
            return "exact";
        case IDMode::kTokenize:
            return "tokenize";
        case IDMode::kDiscard:
            return "discard";
    }
    return "unknown";
}

// =============================================================================
// Read Length Class Enumeration
// =============================================================================

/// @brief Read length classification for compression strategy selection.
/// @note Stored in GlobalHeader.Flags bits 10-11.
/// @note Classification priority (high to low):
///       1. max >= 100KB → Long (Ultra-long strategy)
///       2. max >= 10KB → Long
///       3. max > 511 → Medium (Spring compatibility protection)
///       4. median >= 1KB → Medium
///       5. Otherwise → Short
enum class ReadLengthClass : std::uint8_t {
    /// @brief Short reads (max <= 511 and median < 1KB).
    /// @note Uses Spring ABC + global reordering.
    kShort = 0,

    /// @brief Medium reads (max > 511 or 1KB <= median < 10KB).
    /// @note Uses Zstd, reordering disabled.
    kMedium = 1,

    /// @brief Long reads (max >= 10KB, includes ultra-long >= 100KB).
    /// @note Uses Zstd, reordering disabled.
    kLong = 2
};

/// @brief Convert ReadLengthClass to string representation.
/// @param lengthClass The read length class.
/// @return String representation of the class.
[[nodiscard]] constexpr std::string_view readLengthClassToString(
    ReadLengthClass lengthClass) noexcept {
    switch (lengthClass) {
        case ReadLengthClass::kShort:
            return "short";
        case ReadLengthClass::kMedium:
            return "medium";
        case ReadLengthClass::kLong:
            return "long";
    }
    return "unknown";
}

// =============================================================================
// Paired-End Layout Enumeration
// =============================================================================

/// @brief Paired-end read storage layout.
/// @note Stored in GlobalHeader.Flags bits 8-9.
/// @note Only valid when IS_PAIRED flag is set.
enum class PELayout : std::uint8_t {
    /// @brief Interleaved layout (default): R1_0, R2_0, R1_1, R2_1, ...
    /// @note Paired reads are physically adjacent, better for compression.
    kInterleaved = 0,

    /// @brief Consecutive layout: R1_0, R1_1, ..., R1_N, R2_0, R2_1, ..., R2_N
    /// @note Better for single-end access, but lower sequence compression ratio.
    kConsecutive = 1
};

/// @brief Convert PELayout to string representation.
/// @param layout The PE layout.
/// @return String representation of the layout.
[[nodiscard]] constexpr std::string_view peLayoutToString(PELayout layout) noexcept {
    switch (layout) {
        case PELayout::kInterleaved:
            return "interleaved";
        case PELayout::kConsecutive:
            return "consecutive";
    }
    return "unknown";
}

// =============================================================================
// Checksum Type Enumeration
// =============================================================================

/// @brief Checksum algorithm types.
/// @note Stored in GlobalHeader.ChecksumType.
enum class ChecksumType : std::uint8_t {
    /// @brief xxHash64 (default, fast and high quality).
    kXxHash64 = 0
};

/// @brief Convert ChecksumType to string representation.
/// @param type The checksum type.
/// @return String representation of the type.
[[nodiscard]] constexpr std::string_view checksumTypeToString(ChecksumType type) noexcept {
    switch (type) {
        case ChecksumType::kXxHash64:
            return "xxh64";
    }
    return "unknown";
}

// =============================================================================
// Codec Family Enumeration
// =============================================================================

/// @brief Codec family identifiers.
/// @note Stored in BlockHeader codec_* fields (high 4 bits = family).
/// @note Family changes are incompatible; version changes are backward compatible.
enum class CodecFamily : std::uint8_t {
    /// @brief No compression (for debugging).
    kRaw = 0x0,

    /// @brief Spring ABC for sequence compression.
    kAbcV1 = 0x1,

    /// @brief Statistical Context Mixing for quality compression.
    kScmV1 = 0x2,

    /// @brief Delta + LZMA for ID compression.
    kDeltaLzma = 0x3,

    /// @brief Delta + Zstd for ID compression.
    kDeltaZstd = 0x4,

    /// @brief Delta + Varint for auxiliary data (lengths).
    kDeltaVarint = 0x5,

    /// @brief Overlap-based compression for long reads.
    kOverlapV1 = 0x6,

    /// @brief Plain Zstd for sequence fallback.
    kZstdPlain = 0x7,

    /// @brief SCM Order-1 for low-memory quality compression.
    kScmOrder1 = 0x8,

    /// @brief External/custom codec.
    kExternal = 0xE,

    /// @brief Reserved for future use.
    kReserved = 0xF
};

// =============================================================================
// ReadRecord Structure
// =============================================================================

/// @brief Represents a single FASTQ read record.
/// @note A FASTQ record consists of 4 lines:
///       1. ID line (starts with '@')
///       2. Sequence line (DNA bases: A, C, G, T, N)
///       3. Plus line ('+', optionally followed by ID)
///       4. Quality line (Phred+33 encoded ASCII)
struct ReadRecord {
    /// @brief Read identifier (without '@' prefix).
    std::string id;

    /// @brief DNA sequence (A, C, G, T, N characters).
    std::string sequence;

    /// @brief Quality scores (Phred+33 encoded ASCII).
    /// @note Length must equal sequence length.
    std::string quality;

    /// @brief Default constructor.
    ReadRecord() = default;

    /// @brief Construct a ReadRecord with all fields.
    /// @param id_ Read identifier.
    /// @param sequence_ DNA sequence.
    /// @param quality_ Quality scores.
    ReadRecord(std::string id_, std::string sequence_, std::string quality_)
        : id(std::move(id_)), sequence(std::move(sequence_)), quality(std::move(quality_)) {}

    /// @brief Check if the record is valid.
    /// @return true if sequence and quality have matching non-zero lengths.
    [[nodiscard]] bool isValid() const noexcept {
        return !sequence.empty() && sequence.length() == quality.length();
    }

    /// @brief Get the length of the read (sequence length).
    /// @return Length of the sequence.
    [[nodiscard]] std::size_t length() const noexcept { return sequence.length(); }

    /// @brief Clear all fields.
    void clear() noexcept {
        id.clear();
        sequence.clear();
        quality.clear();
    }

    /// @brief Equality comparison.
    [[nodiscard]] bool operator==(const ReadRecord& other) const noexcept = default;
};

// =============================================================================
// ReadRecordView Structure
// =============================================================================

/// @brief Non-owning view of a FASTQ read record.
/// @note Useful for zero-copy parsing and processing.
struct ReadRecordView {
    /// @brief Read identifier (without '@' prefix).
    std::string_view id;

    /// @brief DNA sequence.
    std::string_view sequence;

    /// @brief Quality scores.
    std::string_view quality;

    /// @brief Default constructor.
    constexpr ReadRecordView() = default;

    /// @brief Construct from string_views.
    constexpr ReadRecordView(std::string_view id_, std::string_view sequence_,
                             std::string_view quality_)
        : id(id_), sequence(sequence_), quality(quality_) {}

    /// @brief Construct from a ReadRecord.
    ReadRecordView(const ReadRecord& record)  // NOLINT(google-explicit-constructor)
        : id(record.id), sequence(record.sequence), quality(record.quality) {}

    /// @brief Check if the view is valid.
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return !sequence.empty() && sequence.length() == quality.length();
    }

    /// @brief Get the length of the read.
    [[nodiscard]] constexpr std::size_t length() const noexcept { return sequence.length(); }

    /// @brief Convert to owning ReadRecord.
    [[nodiscard]] ReadRecord toRecord() const {
        return ReadRecord{std::string(id), std::string(sequence), std::string(quality)};
    }
};

// =============================================================================
// C++20 Concepts
// =============================================================================

/// @brief Concept for types that can be used as DNA bases.
/// @note Valid bases are: A, C, G, T, N (case-insensitive).
template <typename T>
concept DnaBase = std::same_as<T, char> || std::same_as<T, unsigned char>;

/// @brief Concept for types that represent a DNA sequence.
template <typename T>
concept DnaSequence = requires(T seq) {
    { seq.data() } -> std::convertible_to<const char*>;
    { seq.size() } -> std::convertible_to<std::size_t>;
    { seq.empty() } -> std::convertible_to<bool>;
};

/// @brief Concept for types that represent quality scores.
template <typename T>
concept QualitySequence = requires(T qual) {
    { qual.data() } -> std::convertible_to<const char*>;
    { qual.size() } -> std::convertible_to<std::size_t>;
    { qual.empty() } -> std::convertible_to<bool>;
};

/// @brief Concept for read record types.
template <typename T>
concept ReadRecordLike = requires(const T& record) {
    { record.id } -> std::convertible_to<std::string_view>;
    { record.sequence } -> std::convertible_to<std::string_view>;
    { record.quality } -> std::convertible_to<std::string_view>;
    { record.length() } -> std::convertible_to<std::size_t>;
    { record.isValid() } -> std::convertible_to<bool>;
};

/// @brief Concept for types that can be serialized to bytes.
template <typename T>
concept Serializable = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

/// @brief Concept for compression level values.
template <typename T>
concept CompressionLevelValue =
    std::integral<T> && requires(T level) {
        requires sizeof(T) <= sizeof(CompressionLevel);
    };

/// @brief Concept for block ID values.
template <typename T>
concept BlockIdValue = std::integral<T> && std::convertible_to<T, BlockId>;

/// @brief Concept for read ID values.
template <typename T>
concept ReadIdValue = std::integral<T> && std::convertible_to<T, ReadId>;

// =============================================================================
// Compression Options Structure
// =============================================================================

/// @brief Options for compression operations.
struct CompressionOptions {
    /// @brief Compression level (1-9).
    CompressionLevel level = kDefaultCompressionLevel;

    /// @brief Quality compression mode.
    QualityMode qualityMode = QualityMode::kLossless;

    /// @brief ID handling mode.
    IDMode idMode = IDMode::kExact;

    /// @brief Enable read reordering for better compression.
    bool enableReorder = true;

    /// @brief Save reorder map for original order recovery.
    bool saveReorderMap = true;

    /// @brief Force streaming mode (no global reordering).
    bool streamingMode = false;

    /// @brief Number of reads per block.
    std::size_t blockSize = kDefaultBlockSizeShort;

    /// @brief Memory limit in MB.
    std::size_t memoryLimitMB = kDefaultMemoryLimitMB;

    /// @brief Number of threads (0 = auto-detect).
    std::size_t threads = 0;

    /// @brief Paired-end layout.
    PELayout peLayout = PELayout::kInterleaved;

    /// @brief Read length class (auto-detected if not specified).
    std::optional<ReadLengthClass> readLengthClass;

    /// @brief Maximum block bases for long reads.
    std::size_t maxBlockBases = kDefaultMaxBlockBasesLong;
};

/// @brief Options for decompression operations.
struct DecompressionOptions {
    /// @brief Start read ID for range extraction (1-based, inclusive).
    ReadId rangeStart = 1;

    /// @brief End read ID for range extraction (1-based, inclusive).
    /// @note 0 means extract all reads.
    ReadId rangeEnd = 0;

    /// @brief Output in original order (requires reorder map).
    bool originalOrder = false;

    /// @brief Extract only headers (IDs).
    bool headerOnly = false;

    /// @brief Verify checksums during decompression.
    bool verify = true;

    /// @brief Skip corrupted blocks instead of failing.
    bool skipCorrupted = false;

    /// @brief Placeholder quality character for discard mode.
    char placeholderQual = kDefaultPlaceholderQual;

    /// @brief ID prefix for discard mode reconstruction.
    std::string idPrefix;

    /// @brief Number of threads (0 = auto-detect).
    std::size_t threads = 0;
};

// =============================================================================
// Static Assertions
// =============================================================================

// Verify enum sizes match expected bit widths
static_assert(sizeof(QualityMode) == 1, "QualityMode must be 1 byte");
static_assert(sizeof(IDMode) == 1, "IDMode must be 1 byte");
static_assert(sizeof(ReadLengthClass) == 1, "ReadLengthClass must be 1 byte");
static_assert(sizeof(PELayout) == 1, "PELayout must be 1 byte");
static_assert(sizeof(ChecksumType) == 1, "ChecksumType must be 1 byte");
static_assert(sizeof(CodecFamily) == 1, "CodecFamily must be 1 byte");

// Verify type aliases
static_assert(sizeof(BlockId) == 4, "BlockId must be 4 bytes");
static_assert(sizeof(ReadId) == 8, "ReadId must be 8 bytes");
static_assert(sizeof(CompressionLevel) == 1, "CompressionLevel must be 1 byte");
static_assert(sizeof(FileOffset) == 8, "FileOffset must be 8 bytes");
static_assert(sizeof(Checksum) == 8, "Checksum must be 8 bytes");

// Verify concepts work with expected types
static_assert(DnaSequence<std::string>, "std::string must satisfy DnaSequence");
static_assert(DnaSequence<std::string_view>, "std::string_view must satisfy DnaSequence");
static_assert(QualitySequence<std::string>, "std::string must satisfy QualitySequence");
static_assert(ReadRecordLike<ReadRecord>, "ReadRecord must satisfy ReadRecordLike");
static_assert(ReadRecordLike<ReadRecordView>, "ReadRecordView must satisfy ReadRecordLike");

}  // namespace fqc

#endif  // FQC_COMMON_TYPES_H
