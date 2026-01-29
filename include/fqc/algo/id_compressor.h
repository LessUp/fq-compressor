// =============================================================================
// fq-compressor - ID Compressor Module
// =============================================================================
// Implements FASTQ ID (header) compression using tokenization and delta encoding.
//
// The ID compressor supports three modes:
// 1. Exact: Preserve original IDs exactly (default)
// 2. Tokenize: Split IDs into static/dynamic parts for better compression
// 3. Discard: Replace IDs with sequential numbers
//
// For Illumina-style headers (e.g., @SIM:1:FCX:1:1:1:1), tokenization
// identifies static parts (instrument, flowcell) and dynamic parts
// (tile, x, y coordinates) for efficient delta encoding.
//
// Requirements: 1.1.2 (ID compression)
// =============================================================================

#ifndef FQC_ALGO_ID_COMPRESSOR_H
#define FQC_ALGO_ID_COMPRESSOR_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"

namespace fqc::algo {

// =============================================================================
// Forward Declarations
// =============================================================================

class IDCompressorImpl;

// =============================================================================
// Constants
// =============================================================================

/// @brief Maximum number of tokens in an ID
inline constexpr std::size_t kMaxIdTokens = 32;

/// @brief Default delimiter for tokenization
inline constexpr char kDefaultDelimiter = ':';

/// @brief Common delimiters for ID tokenization
inline constexpr std::string_view kCommonDelimiters = ":_/| \t";

/// @brief Maximum ID length supported
inline constexpr std::size_t kMaxIdLength = 4096;

// =============================================================================
// Token Types
// =============================================================================

/// @brief Type of token in a parsed ID
enum class TokenType : std::uint8_t {
    /// @brief Static string (same across all IDs)
    kStatic = 0,

    /// @brief Dynamic integer (varies, delta-encoded)
    kDynamicInt = 1,

    /// @brief Dynamic string (varies, stored as-is or dictionary-encoded)
    kDynamicString = 2,

    /// @brief Delimiter character
    kDelimiter = 3
};

/// @brief A single token from a parsed ID
struct IDToken {
    /// @brief Token type
    TokenType type = TokenType::kStatic;

    /// @brief Token value (string for static/dynamic string, empty for int)
    std::string value;

    /// @brief Integer value (for kDynamicInt type)
    std::int64_t intValue = 0;

    /// @brief Position in original ID string
    std::size_t position = 0;

    /// @brief Length in original ID string
    std::size_t length = 0;

    /// @brief Default constructor
    IDToken() = default;

    /// @brief Construct a static token
    static IDToken makeStatic(std::string_view val, std::size_t pos) {
        IDToken token;
        token.type = TokenType::kStatic;
        token.value = std::string(val);
        token.position = pos;
        token.length = val.length();
        return token;
    }

    /// @brief Construct a dynamic integer token
    static IDToken makeDynamicInt(std::int64_t val, std::size_t pos, std::size_t len) {
        IDToken token;
        token.type = TokenType::kDynamicInt;
        token.intValue = val;
        token.position = pos;
        token.length = len;
        return token;
    }

    /// @brief Construct a dynamic string token
    static IDToken makeDynamicString(std::string_view val, std::size_t pos) {
        IDToken token;
        token.type = TokenType::kDynamicString;
        token.value = std::string(val);
        token.position = pos;
        token.length = val.length();
        return token;
    }

    /// @brief Construct a delimiter token
    static IDToken makeDelimiter(char delim, std::size_t pos) {
        IDToken token;
        token.type = TokenType::kDelimiter;
        token.value = std::string(1, delim);
        token.position = pos;
        token.length = 1;
        return token;
    }
};

// =============================================================================
// Parsed ID Structure
// =============================================================================

/// @brief A parsed ID broken into tokens
struct ParsedID {
    /// @brief Original ID string
    std::string original;

    /// @brief Tokens extracted from the ID
    std::vector<IDToken> tokens;

    /// @brief Clear all data
    void clear() noexcept {
        original.clear();
        tokens.clear();
    }

    /// @brief Reconstruct the ID from tokens
    [[nodiscard]] std::string reconstruct() const;
};

// =============================================================================
// ID Pattern (Template)
// =============================================================================

/// @brief Pattern describing the structure of IDs in a block
/// Used for tokenized compression mode
struct IDPattern {
    /// @brief Token types in order
    std::vector<TokenType> tokenTypes;

    /// @brief Static values for static tokens (index matches tokenTypes)
    std::vector<std::string> staticValues;

    /// @brief Delimiter characters (index matches delimiter tokens)
    std::vector<char> delimiters;

    /// @brief Number of dynamic integer fields
    std::size_t numDynamicInts = 0;

    /// @brief Number of dynamic string fields
    std::size_t numDynamicStrings = 0;

    /// @brief Check if pattern is valid
    [[nodiscard]] bool isValid() const noexcept {
        return !tokenTypes.empty();
    }

    /// @brief Clear the pattern
    void clear() noexcept {
        tokenTypes.clear();
        staticValues.clear();
        delimiters.clear();
        numDynamicInts = 0;
        numDynamicStrings = 0;
    }

    /// @brief Check if all IDs match this pattern
    /// @param ids Vector of IDs to check
    /// @return true if all IDs match the pattern structure
    [[nodiscard]] bool matchesAll(std::span<const std::string_view> ids) const;
};

// =============================================================================
// Compressed ID Data
// =============================================================================

/// @brief Compressed ID data for a block
struct CompressedIDData {
    /// @brief Compressed data bytes
    std::vector<std::uint8_t> data;

    /// @brief Number of IDs
    std::uint32_t numIds = 0;

    /// @brief Total uncompressed size (bytes)
    std::uint64_t uncompressedSize = 0;

    /// @brief ID mode used
    IDMode idMode = IDMode::kExact;

    /// @brief Pattern used (for tokenize mode)
    std::optional<IDPattern> pattern;

    /// @brief Clear all data
    void clear() noexcept {
        data.clear();
        numIds = 0;
        uncompressedSize = 0;
        pattern.reset();
    }

    /// @brief Get compression ratio
    [[nodiscard]] double compressionRatio() const noexcept {
        if (uncompressedSize == 0) return 1.0;
        return static_cast<double>(data.size()) / static_cast<double>(uncompressedSize);
    }
};

// =============================================================================
// ID Compressor Configuration
// =============================================================================

/// @brief Configuration for ID compression
struct IDCompressorConfig {
    /// @brief ID handling mode
    IDMode idMode = IDMode::kExact;

    /// @brief Compression level (1-9)
    CompressionLevel compressionLevel = kDefaultCompressionLevel;

    /// @brief Use Zstd instead of LZMA for compression
    bool useZstd = true;

    /// @brief Zstd compression level (1-22)
    int zstdLevel = 3;

    /// @brief LZMA compression level (0-9)
    int lzmaLevel = 6;

    /// @brief Delimiters to use for tokenization
    std::string delimiters = std::string(kCommonDelimiters);

    /// @brief Minimum pattern match ratio for tokenization (0.0-1.0)
    /// If fewer than this ratio of IDs match the detected pattern,
    /// fall back to exact mode for this block
    double minPatternMatchRatio = 0.95;

    /// @brief ID prefix for discard mode reconstruction
    std::string idPrefix;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;
};

// =============================================================================
// ID Compressor Class
// =============================================================================

/// @brief Main class for ID compression
///
/// The IDCompressor implements three compression strategies:
/// 1. Exact: Compress IDs as-is using general-purpose compression
/// 2. Tokenize: Parse IDs into tokens, delta-encode integers
/// 3. Discard: Store nothing, reconstruct sequential IDs on decompression
///
/// For Illumina-style headers, tokenization can achieve significant
/// compression by delta-encoding the coordinate fields.
///
/// Usage:
/// @code
/// IDCompressorConfig config;
/// config.idMode = IDMode::kTokenize;
///
/// IDCompressor compressor(config);
///
/// // Compress IDs
/// std::vector<std::string_view> ids = {...};
/// auto result = compressor.compress(ids);
///
/// // Decompress
/// auto decompressed = compressor.decompress(result->data, result->numIds);
/// @endcode
class IDCompressor {
public:
    /// @brief Construct with configuration
    /// @param config Compression configuration
    explicit IDCompressor(IDCompressorConfig config = {});

    /// @brief Destructor
    ~IDCompressor();

    // Non-copyable, movable
    IDCompressor(const IDCompressor&) = delete;
    IDCompressor& operator=(const IDCompressor&) = delete;
    IDCompressor(IDCompressor&&) noexcept;
    IDCompressor& operator=(IDCompressor&&) noexcept;

    /// @brief Compress a vector of IDs
    /// @param ids Vector of ID strings (without '@' prefix)
    /// @return Compressed ID data or error
    [[nodiscard]] Result<CompressedIDData> compress(std::span<const std::string_view> ids);

    /// @brief Compress a vector of IDs (string version)
    /// @param ids Vector of ID strings (without '@' prefix)
    /// @return Compressed ID data or error
    [[nodiscard]] Result<CompressedIDData> compress(std::span<const std::string> ids);

    /// @brief Decompress ID data
    /// @param data Compressed data
    /// @param numIds Number of IDs to decompress
    /// @return Decompressed ID strings or error
    [[nodiscard]] Result<std::vector<std::string>> decompress(
        std::span<const std::uint8_t> data,
        std::uint32_t numIds);

    /// @brief Decompress ID data with mode hint
    /// @param data Compressed data
    /// @param numIds Number of IDs to decompress
    /// @param mode ID mode used during compression
    /// @return Decompressed ID strings or error
    [[nodiscard]] Result<std::vector<std::string>> decompress(
        std::span<const std::uint8_t> data,
        std::uint32_t numIds,
        IDMode mode);

    /// @brief Get current configuration
    [[nodiscard]] const IDCompressorConfig& config() const noexcept;

    /// @brief Update configuration
    /// @param config New configuration
    /// @return VoidResult indicating success or validation error
    [[nodiscard]] VoidResult setConfig(IDCompressorConfig config);

    /// @brief Reset compressor state (for reuse)
    void reset() noexcept;

    /// @brief Detect ID pattern from a sample of IDs
    /// @param ids Sample IDs to analyze
    /// @return Detected pattern or nullopt if no consistent pattern found
    [[nodiscard]] std::optional<IDPattern> detectPattern(
        std::span<const std::string_view> ids) const;

    /// @brief Parse a single ID into tokens
    /// @param id The ID string to parse
    /// @return Parsed ID with tokens
    [[nodiscard]] ParsedID parseId(std::string_view id) const;

private:
    std::unique_ptr<IDCompressorImpl> impl_;
};

// =============================================================================
// ID Tokenizer Class
// =============================================================================

/// @brief Tokenizer for parsing FASTQ IDs
///
/// Supports common ID formats:
/// - Illumina: @instrument:run:flowcell:lane:tile:x:y
/// - Illumina (new): @instrument:run:flowcell:lane:tile:x:y read:filtered:control:index
/// - SRA: @SRR123456.1 length=100
/// - Generic: Any delimiter-separated format
class IDTokenizer {
public:
    /// @brief Construct with delimiters
    /// @param delimiters Characters to use as delimiters
    explicit IDTokenizer(std::string_view delimiters = kCommonDelimiters);

    /// @brief Tokenize an ID string
    /// @param id The ID string (without '@' prefix)
    /// @return Vector of tokens
    [[nodiscard]] std::vector<IDToken> tokenize(std::string_view id) const;

    /// @brief Detect if a string is a valid integer
    /// @param str String to check
    /// @return Integer value if valid, nullopt otherwise
    [[nodiscard]] static std::optional<std::int64_t> tryParseInt(std::string_view str);

    /// @brief Check if a character is a delimiter
    /// @param c Character to check
    /// @return true if c is a delimiter
    [[nodiscard]] bool isDelimiter(char c) const noexcept;

    /// @brief Get the delimiters
    [[nodiscard]] std::string_view delimiters() const noexcept { return delimiters_; }

private:
    std::string delimiters_;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Generate a reconstructed ID for discard mode
/// @param archiveId Archive ID (1-based)
/// @param isPaired Whether this is paired-end data
/// @param peLayout Paired-end layout
/// @param prefix Optional prefix for the ID
/// @return Reconstructed ID string (without '@' prefix)
[[nodiscard]] std::string generateDiscardId(
    ReadId archiveId,
    bool isPaired,
    PELayout peLayout,
    std::string_view prefix = "");

/// @brief Encode a vector of integers using delta + varint encoding
/// @param values Vector of integer values
/// @return Encoded bytes
[[nodiscard]] std::vector<std::uint8_t> deltaVarintEncode(std::span<const std::int64_t> values);

/// @brief Decode delta + varint encoded integers
/// @param data Encoded data
/// @param count Number of integers to decode
/// @return Decoded integer values or error
[[nodiscard]] Result<std::vector<std::int64_t>> deltaVarintDecode(
    std::span<const std::uint8_t> data,
    std::size_t count);

/// @brief Encode a single integer as varint
/// @param value Integer value to encode
/// @param output Output buffer (must have at least 10 bytes)
/// @return Number of bytes written
std::size_t varintEncode(std::int64_t value, std::uint8_t* output);

/// @brief Decode a single varint
/// @param data Input data
/// @param bytesRead Output: number of bytes consumed
/// @return Decoded integer value
std::int64_t varintDecode(std::span<const std::uint8_t> data, std::size_t& bytesRead);

/// @brief Encode unsigned integer as varint
/// @param value Unsigned integer value
/// @param output Output buffer
/// @return Number of bytes written
std::size_t uvarintEncode(std::uint64_t value, std::uint8_t* output);

/// @brief Decode unsigned varint
/// @param data Input data
/// @param bytesRead Output: number of bytes consumed
/// @return Decoded unsigned integer value
std::uint64_t uvarintDecode(std::span<const std::uint8_t> data, std::size_t& bytesRead);

/// @brief ZigZag encode a signed integer for varint encoding
/// @param value Signed integer
/// @return ZigZag encoded unsigned integer
[[nodiscard]] inline constexpr std::uint64_t zigzagEncode(std::int64_t value) noexcept {
    return static_cast<std::uint64_t>((value << 1) ^ (value >> 63));
}

/// @brief ZigZag decode an unsigned integer back to signed
/// @param value ZigZag encoded unsigned integer
/// @return Decoded signed integer
[[nodiscard]] inline constexpr std::int64_t zigzagDecode(std::uint64_t value) noexcept {
    // Cast to avoid sign conversion warning: -(value & 1) needs explicit handling
    // (value >> 1) is unsigned, XOR with signed result needs care
    return static_cast<std::int64_t>(value >> 1) ^
           -static_cast<std::int64_t>(value & 1);
}

}  // namespace fqc::algo

#endif  // FQC_ALGO_ID_COMPRESSOR_H
