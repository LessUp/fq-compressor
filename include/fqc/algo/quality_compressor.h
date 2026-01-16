// =============================================================================
// fq-compressor - Quality Compressor Module
// =============================================================================
// Implements quality value compression using Statistical Context Mixing (SCM).
//
// The SCM algorithm uses context-based arithmetic coding where the context
// is derived from:
// - Previous quality values (Order-1 or Order-2)
// - Position within the read
// - Optionally, the DNA base at the current position
//
// This approach is inspired by fqzcomp5's quality compression strategy.
//
// Requirements: 3.1 (Lossless quality compression)
// =============================================================================

#ifndef FQC_ALGO_QUALITY_COMPRESSOR_H
#define FQC_ALGO_QUALITY_COMPRESSOR_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"

namespace fqc::algo {

// =============================================================================
// Forward Declarations
// =============================================================================

class QualityCompressorImpl;
class ArithmeticEncoder;
class ArithmeticDecoder;

// =============================================================================
// Constants
// =============================================================================

/// @brief Minimum quality value (Phred+33 encoding, '!' = 0)
inline constexpr std::uint8_t kMinQualityValue = 0;

/// @brief Maximum quality value (Phred+33 encoding, typically 41 for Illumina)
inline constexpr std::uint8_t kMaxQualityValue = 93;  // '~' - '!' = 93

/// @brief Default quality value for unknown/missing quality
inline constexpr std::uint8_t kDefaultQualityValue = 0;

/// @brief Number of quality symbols (0-93 for Phred+33)
inline constexpr std::size_t kNumQualitySymbols = 94;

/// @brief Number of position bins for context
inline constexpr std::size_t kNumPositionBins = 16;

/// @brief Maximum read length for position context
inline constexpr std::size_t kMaxPositionContext = 1024;

/// @brief Order-1 context size (previous quality value)
inline constexpr std::size_t kOrder1ContextSize = kNumQualitySymbols;

/// @brief Order-2 context size (two previous quality values)
inline constexpr std::size_t kOrder2ContextSize = kNumQualitySymbols * kNumQualitySymbols;

// =============================================================================
// Quality Compressor Configuration
// =============================================================================

/// @brief Context order for quality compression
enum class QualityContextOrder : std::uint8_t {
    /// @brief Order-0: No context (baseline)
    kOrder0 = 0,

    /// @brief Order-1: Previous quality value as context
    kOrder1 = 1,

    /// @brief Order-2: Two previous quality values as context
    kOrder2 = 2
};

/// @brief Configuration for quality compression
struct QualityCompressorConfig {
    /// @brief Context order (Order-1 or Order-2)
    QualityContextOrder contextOrder = QualityContextOrder::kOrder2;

    /// @brief Use position context (position within read)
    bool usePositionContext = true;

    /// @brief Number of position bins (power of 2)
    std::size_t numPositionBins = kNumPositionBins;

    /// @brief Use DNA base context (base at current position)
    bool useBaseContext = false;

    /// @brief Quality mode (lossless, illumina8, qvz, discard)
    QualityMode qualityMode = QualityMode::kLossless;

    /// @brief Adaptive model update rate (0.0 = static, 1.0 = fully adaptive)
    double adaptationRate = 0.5;

    /// @brief Validate configuration
    [[nodiscard]] VoidResult validate() const;
};

// =============================================================================
// Illumina 8-bin Quality Mapping
// =============================================================================

/// @brief Illumina 8-bin quality mapping table
/// Maps Phred quality scores to 8 bins as per Illumina specification
struct Illumina8BinMapper {
    /// @brief Map quality value to bin
    /// @param quality Original quality value (0-93)
    /// @return Binned quality value
    [[nodiscard]] static std::uint8_t toBin(std::uint8_t quality) noexcept;

    /// @brief Get representative quality for a bin
    /// @param bin Bin index (0-7)
    /// @return Representative quality value
    [[nodiscard]] static std::uint8_t fromBin(std::uint8_t bin) noexcept;

    /// @brief Illumina 8-bin boundaries
    /// Bins: [0-1], [2-9], [10-19], [20-24], [25-29], [30-34], [35-39], [40+]
    static constexpr std::array<std::uint8_t, 8> kBinBoundaries = {
        2, 10, 20, 25, 30, 35, 40, 94
    };

    /// @brief Representative quality values for each bin
    static constexpr std::array<std::uint8_t, 8> kBinRepresentatives = {
        0, 6, 15, 22, 27, 33, 37, 40
    };
};

// =============================================================================
// Compressed Quality Data
// =============================================================================

/// @brief Compressed quality data for a block
struct CompressedQualityData {
    /// @brief Compressed data bytes
    std::vector<std::uint8_t> data;

    /// @brief Number of quality strings
    std::uint32_t numStrings = 0;

    /// @brief Total uncompressed size (bytes)
    std::uint64_t uncompressedSize = 0;

    /// @brief Context order used
    QualityContextOrder contextOrder = QualityContextOrder::kOrder2;

    /// @brief Quality mode used
    QualityMode qualityMode = QualityMode::kLossless;

    /// @brief Clear all data
    void clear() noexcept {
        data.clear();
        numStrings = 0;
        uncompressedSize = 0;
    }

    /// @brief Get compression ratio
    [[nodiscard]] double compressionRatio() const noexcept {
        if (uncompressedSize == 0) return 1.0;
        return static_cast<double>(data.size()) / static_cast<double>(uncompressedSize);
    }
};

// =============================================================================
// Quality Compressor Class
// =============================================================================

/// @brief Main class for quality value compression using SCM
///
/// The QualityCompressor implements Statistical Context Mixing (SCM) for
/// quality value compression. It uses adaptive arithmetic coding with
/// context derived from:
/// - Previous quality values (Order-1 or Order-2)
/// - Position within the read
/// - Optionally, the DNA base at the current position
///
/// Usage:
/// @code
/// QualityCompressorConfig config;
/// config.contextOrder = QualityContextOrder::kOrder2;
/// config.usePositionContext = true;
///
/// QualityCompressor compressor(config);
///
/// // Compress quality strings
/// std::vector<std::string_view> qualities = {...};
/// auto result = compressor.compress(qualities);
///
/// // Decompress
/// auto decompressed = compressor.decompress(result->data, lengths);
/// @endcode
class QualityCompressor {
public:
    /// @brief Construct with configuration
    /// @param config Compression configuration
    explicit QualityCompressor(QualityCompressorConfig config = {});

    /// @brief Destructor
    ~QualityCompressor();

    // Non-copyable, movable
    QualityCompressor(const QualityCompressor&) = delete;
    QualityCompressor& operator=(const QualityCompressor&) = delete;
    QualityCompressor(QualityCompressor&&) noexcept;
    QualityCompressor& operator=(QualityCompressor&&) noexcept;

    /// @brief Compress quality strings
    /// @param qualities Vector of quality strings (Phred+33 encoded)
    /// @return Compressed quality data or error
    [[nodiscard]] Result<CompressedQualityData> compress(
        std::span<const std::string_view> qualities);

    /// @brief Compress quality strings with DNA sequences for base context
    /// @param qualities Vector of quality strings
    /// @param sequences Vector of DNA sequences (for base context)
    /// @return Compressed quality data or error
    [[nodiscard]] Result<CompressedQualityData> compress(
        std::span<const std::string_view> qualities,
        std::span<const std::string_view> sequences);

    /// @brief Decompress quality data
    /// @param data Compressed data
    /// @param lengths Length of each quality string
    /// @return Decompressed quality strings or error
    [[nodiscard]] Result<std::vector<std::string>> decompress(
        std::span<const std::uint8_t> data,
        std::span<const std::uint32_t> lengths);

    /// @brief Decompress quality data with uniform length
    /// @param data Compressed data
    /// @param numStrings Number of quality strings
    /// @param uniformLength Length of each quality string
    /// @return Decompressed quality strings or error
    [[nodiscard]] Result<std::vector<std::string>> decompress(
        std::span<const std::uint8_t> data,
        std::uint32_t numStrings,
        std::uint32_t uniformLength);

    /// @brief Get current configuration
    [[nodiscard]] const QualityCompressorConfig& config() const noexcept;

    /// @brief Update configuration
    /// @param config New configuration
    /// @return VoidResult indicating success or validation error
    [[nodiscard]] VoidResult setConfig(QualityCompressorConfig config);

    /// @brief Reset compressor state (for reuse)
    void reset() noexcept;

private:
    std::unique_ptr<QualityCompressorImpl> impl_;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Convert Phred+33 encoded quality character to numeric value
/// @param c Quality character ('!' to '~')
/// @return Numeric quality value (0-93)
[[nodiscard]] inline constexpr std::uint8_t qualityCharToValue(char c) noexcept {
    return static_cast<std::uint8_t>(c - '!');
}

/// @brief Convert numeric quality value to Phred+33 encoded character
/// @param value Numeric quality value (0-93)
/// @return Quality character ('!' to '~')
[[nodiscard]] inline constexpr char qualityValueToChar(std::uint8_t value) noexcept {
    return static_cast<char>(value + '!');
}

/// @brief Compute position bin for context
/// @param position Position within read
/// @param readLength Total read length
/// @param numBins Number of position bins
/// @return Position bin index
[[nodiscard]] inline std::size_t computePositionBin(
    std::size_t position,
    std::size_t readLength,
    std::size_t numBins) noexcept {
    if (readLength == 0 || numBins == 0) return 0;
    return (position * numBins) / readLength;
}

/// @brief Apply Illumina 8-bin mapping to quality string
/// @param quality Input quality string
/// @return Binned quality string
[[nodiscard]] std::string applyIllumina8Bin(std::string_view quality);

/// @brief Compute quality statistics for a block
/// @param qualities Vector of quality strings
/// @return Histogram of quality values
[[nodiscard]] std::array<std::uint64_t, kNumQualitySymbols> computeQualityHistogram(
    std::span<const std::string_view> qualities);

}  // namespace fqc::algo

#endif  // FQC_ALGO_QUALITY_COMPRESSOR_H
