// =============================================================================
// fq-compressor - Quality Compressor Implementation
// =============================================================================
// Implements quality value compression using Statistical Context Mixing (SCM).
//
// The implementation uses adaptive arithmetic coding with context models
// based on previous quality values and position within the read.
//
// Requirements: 3.1 (Lossless quality compression)
// =============================================================================

#include "fqc/algo/quality_compressor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numeric>

#include <zstd.h>

#include "fqc/common/logger.h"

namespace fqc::algo {

// =============================================================================
// Arithmetic Coding Constants
// =============================================================================

namespace {

/// @brief Precision bits for arithmetic coding
constexpr std::uint32_t kCodeBits = 32;

/// @brief Top value for arithmetic coding range
constexpr std::uint64_t kTopValue = (1ULL << kCodeBits) - 1;

/// @brief First quarter of range
constexpr std::uint64_t kFirstQuarter = kTopValue / 4 + 1;

/// @brief Half of range
constexpr std::uint64_t kHalf = 2 * kFirstQuarter;

/// @brief Third quarter of range
constexpr std::uint64_t kThirdQuarter = 3 * kFirstQuarter;

/// @brief Maximum frequency count before rescaling
constexpr std::uint32_t kMaxFrequency = 16383;

/// @brief Initial frequency for each symbol
constexpr std::uint32_t kInitialFrequency = 1;

/// @brief Adaptation increment
constexpr std::uint32_t kAdaptIncrement = 8;

}  // namespace

// =============================================================================
// Adaptive Frequency Model
// =============================================================================

/// @brief Adaptive frequency model for arithmetic coding
class AdaptiveModel {
public:
    explicit AdaptiveModel(std::size_t numSymbols)
        : numSymbols_(numSymbols)
        , frequencies_(numSymbols + 1, kInitialFrequency)
        , cumulative_(numSymbols + 1, 0) {
        updateCumulative();
    }

    /// @brief Get cumulative frequency for symbol
    [[nodiscard]] std::uint32_t getCumulative(std::size_t symbol) const noexcept {
        return cumulative_[symbol];
    }

    /// @brief Get total frequency
    [[nodiscard]] std::uint32_t getTotal() const noexcept {
        return cumulative_[numSymbols_];
    }

    /// @brief Get frequency for symbol
    [[nodiscard]] std::uint32_t getFrequency(std::size_t symbol) const noexcept {
        return frequencies_[symbol];
    }

    /// @brief Find symbol for cumulative frequency
    [[nodiscard]] std::size_t findSymbol(std::uint32_t cumFreq) const noexcept {
        // Binary search for symbol
        std::size_t low = 0;
        std::size_t high = numSymbols_;

        while (low < high) {
            std::size_t mid = (low + high) / 2;
            if (cumulative_[mid + 1] <= cumFreq) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }

        return low;
    }

    /// @brief Update model after encoding/decoding a symbol
    void update(std::size_t symbol) {
        frequencies_[symbol] += kAdaptIncrement;

        // Rescale if total exceeds maximum
        if (getTotal() + kAdaptIncrement > kMaxFrequency) {
            rescale();
        }

        updateCumulative();
    }

    /// @brief Reset model to initial state
    void reset() {
        std::fill(frequencies_.begin(), frequencies_.end(), kInitialFrequency);
        updateCumulative();
    }

private:
    void updateCumulative() {
        cumulative_[0] = 0;
        for (std::size_t i = 0; i < numSymbols_; ++i) {
            cumulative_[i + 1] = cumulative_[i] + frequencies_[i];
        }
    }

    void rescale() {
        for (auto& freq : frequencies_) {
            freq = (freq + 1) / 2;
            if (freq == 0) freq = 1;
        }
    }

    std::size_t numSymbols_;
    std::vector<std::uint32_t> frequencies_;
    std::vector<std::uint32_t> cumulative_;
};

// =============================================================================
// Arithmetic Encoder
// =============================================================================

class ArithmeticEncoder {
public:
    ArithmeticEncoder() : low_(0), high_(kTopValue), bitsToFollow_(0) {}

    /// @brief Encode a symbol
    void encode(std::size_t symbol, const AdaptiveModel& model) {
        std::uint64_t range = high_ - low_ + 1;
        std::uint32_t total = model.getTotal();
        std::uint32_t cumLow = model.getCumulative(symbol);
        std::uint32_t cumHigh = model.getCumulative(symbol + 1);

        high_ = low_ + (range * cumHigh) / total - 1;
        low_ = low_ + (range * cumLow) / total;

        normalize();
    }

    /// @brief Finish encoding and flush remaining bits
    void finish() {
        bitsToFollow_++;
        if (low_ < kFirstQuarter) {
            outputBit(0);
        } else {
            outputBit(1);
        }
    }

    /// @brief Get encoded data
    [[nodiscard]] const std::vector<std::uint8_t>& getData() const noexcept {
        return output_;
    }

    /// @brief Clear encoder state
    void clear() {
        output_.clear();
        low_ = 0;
        high_ = kTopValue;
        bitsToFollow_ = 0;
        bitBuffer_ = 0;
        bitCount_ = 0;
    }

private:
    void normalize() {
        while (true) {
            if (high_ < kHalf) {
                outputBit(0);
            } else if (low_ >= kHalf) {
                outputBit(1);
                low_ -= kHalf;
                high_ -= kHalf;
            } else if (low_ >= kFirstQuarter && high_ < kThirdQuarter) {
                bitsToFollow_++;
                low_ -= kFirstQuarter;
                high_ -= kFirstQuarter;
            } else {
                break;
            }

            low_ = 2 * low_;
            high_ = 2 * high_ + 1;
        }
    }

    void outputBit(int bit) {
        writeBit(bit);
        while (bitsToFollow_ > 0) {
            writeBit(1 - bit);
            bitsToFollow_--;
        }
    }

    void writeBit(int bit) {
        bitBuffer_ = (bitBuffer_ << 1) | bit;
        bitCount_++;

        if (bitCount_ == 8) {
            output_.push_back(static_cast<std::uint8_t>(bitBuffer_));
            bitBuffer_ = 0;
            bitCount_ = 0;
        }
    }

    std::vector<std::uint8_t> output_;
    std::uint64_t low_;
    std::uint64_t high_;
    std::uint32_t bitsToFollow_;
    std::uint8_t bitBuffer_ = 0;
    std::uint8_t bitCount_ = 0;
};

// =============================================================================
// Arithmetic Decoder
// =============================================================================

class ArithmeticDecoder {
public:
    explicit ArithmeticDecoder(std::span<const std::uint8_t> data)
        : data_(data), low_(0), high_(kTopValue), value_(0), bitPos_(0), bytePos_(0) {
        // Initialize value with first bits
        for (int i = 0; i < static_cast<int>(kCodeBits); ++i) {
            value_ = (value_ << 1) | static_cast<std::uint64_t>(readBit());
        }
    }

    /// @brief Decode a symbol
    [[nodiscard]] std::size_t decode(AdaptiveModel& model) {
        std::uint64_t range = high_ - low_ + 1;
        std::uint32_t total = model.getTotal();

        std::uint32_t cumFreq = static_cast<std::uint32_t>(
            ((value_ - low_ + 1) * total - 1) / range);

        std::size_t symbol = model.findSymbol(cumFreq);

        std::uint32_t cumLow = model.getCumulative(symbol);
        std::uint32_t cumHigh = model.getCumulative(symbol + 1);

        high_ = low_ + (range * cumHigh) / total - 1;
        low_ = low_ + (range * cumLow) / total;

        normalize();

        return symbol;
    }

private:
    void normalize() {
        while (true) {
            if (high_ < kHalf) {
                // Do nothing
            } else if (low_ >= kHalf) {
                value_ -= kHalf;
                low_ -= kHalf;
                high_ -= kHalf;
            } else if (low_ >= kFirstQuarter && high_ < kThirdQuarter) {
                value_ -= kFirstQuarter;
                low_ -= kFirstQuarter;
                high_ -= kFirstQuarter;
            } else {
                break;
            }

            low_ = 2 * low_;
            high_ = 2 * high_ + 1;
            value_ = 2 * value_ + static_cast<std::uint64_t>(readBit());
        }
    }

    [[nodiscard]] int readBit() {
        if (bytePos_ >= data_.size()) {
            return 0;  // Padding with zeros
        }

        int bit = (data_[bytePos_] >> (7 - bitPos_)) & 1;
        bitPos_++;

        if (bitPos_ == 8) {
            bitPos_ = 0;
            bytePos_++;
        }

        return bit;
    }

    std::span<const std::uint8_t> data_;
    std::uint64_t low_;
    std::uint64_t high_;
    std::uint64_t value_;
    std::size_t bitPos_;
    std::size_t bytePos_;
};

// =============================================================================
// Context Model for Quality Compression
// =============================================================================

/// @brief Context model for SCM quality compression
class QualityContextModel {
public:
    QualityContextModel(QualityContextOrder order, std::size_t numPositionBins)
        : order_(order)
        , numPositionBins_(numPositionBins)
        , numQualitySymbols_(kNumQualitySymbols) {
        initializeModels();
    }

    /// @brief Get model for given context
    [[nodiscard]] AdaptiveModel& getModel(
        std::uint8_t prevQual1,
        std::uint8_t prevQual2,
        std::size_t positionBin) {
        std::size_t contextIndex = computeContextIndex(prevQual1, prevQual2, positionBin);
        return models_[contextIndex];
    }

    /// @brief Reset all models
    void reset() {
        for (auto& model : models_) {
            model.reset();
        }
    }

private:
    void initializeModels() {
        std::size_t numContexts = computeNumContexts();
        models_.reserve(numContexts);

        for (std::size_t i = 0; i < numContexts; ++i) {
            models_.emplace_back(numQualitySymbols_);
        }
    }

    [[nodiscard]] std::size_t computeNumContexts() const {
        std::size_t qualContexts = 1;

        switch (order_) {
            case QualityContextOrder::kOrder0:
                qualContexts = 1;
                break;
            case QualityContextOrder::kOrder1:
                qualContexts = numQualitySymbols_;
                break;
            case QualityContextOrder::kOrder2:
                qualContexts = numQualitySymbols_ * numQualitySymbols_;
                break;
        }

        return qualContexts * numPositionBins_;
    }

    [[nodiscard]] std::size_t computeContextIndex(
        std::uint8_t prevQual1,
        std::uint8_t prevQual2,
        std::size_t positionBin) const {
        std::size_t qualContext = 0;

        switch (order_) {
            case QualityContextOrder::kOrder0:
                qualContext = 0;
                break;
            case QualityContextOrder::kOrder1:
                qualContext = prevQual1;
                break;
            case QualityContextOrder::kOrder2:
                qualContext = prevQual1 * numQualitySymbols_ + prevQual2;
                break;
        }

        return qualContext * numPositionBins_ + positionBin;
    }

    QualityContextOrder order_;
    std::size_t numPositionBins_;
    std::size_t numQualitySymbols_;
    std::vector<AdaptiveModel> models_;
};


// =============================================================================
// Illumina 8-bin Mapper Implementation
// =============================================================================

std::uint8_t Illumina8BinMapper::toBin(std::uint8_t quality) noexcept {
    for (std::uint8_t bin = 0; bin < 8; ++bin) {
        if (quality < kBinBoundaries[bin]) {
            return bin;
        }
    }
    return 7;
}

std::uint8_t Illumina8BinMapper::fromBin(std::uint8_t bin) noexcept {
    if (bin >= 8) bin = 7;
    return kBinRepresentatives[bin];
}

// =============================================================================
// Quality Compressor Configuration Validation
// =============================================================================

VoidResult QualityCompressorConfig::validate() const {
    if (numPositionBins == 0 || numPositionBins > 256) {
        return makeVoidError(ErrorCode::kUsageError,
                             "numPositionBins must be in range [1, 256]");
    }

    // Check if numPositionBins is a power of 2
    if ((numPositionBins & (numPositionBins - 1)) != 0) {
        return makeVoidError(ErrorCode::kUsageError,
                             "numPositionBins must be a power of 2");
    }

    if (adaptationRate < 0.0 || adaptationRate > 1.0) {
        return makeVoidError(ErrorCode::kUsageError,
                             "adaptationRate must be in range [0.0, 1.0]");
    }

    return makeVoidSuccess();
}

// =============================================================================
// Quality Compressor Implementation
// =============================================================================

class QualityCompressorImpl {
public:
    explicit QualityCompressorImpl(QualityCompressorConfig config)
        : config_(std::move(config))
        , contextModel_(config_.contextOrder, config_.numPositionBins) {}

    Result<CompressedQualityData> compress(
        std::span<const std::string_view> qualities,
        std::span<const std::string_view> sequences);

    Result<std::vector<std::string>> decompress(
        std::span<const std::uint8_t> data,
        std::span<const std::uint32_t> lengths);

    Result<std::vector<std::string>> decompress(
        std::span<const std::uint8_t> data,
        std::uint32_t numStrings,
        std::uint32_t uniformLength);

    const QualityCompressorConfig& config() const noexcept { return config_; }

    VoidResult setConfig(QualityCompressorConfig config) {
        auto result = config.validate();
        if (!result) return result;
        config_ = std::move(config);
        contextModel_ = QualityContextModel(config_.contextOrder, config_.numPositionBins);
        return makeVoidSuccess();
    }

    void reset() noexcept {
        contextModel_.reset();
    }

private:
    QualityCompressorConfig config_;
    QualityContextModel contextModel_;

    // Compress using SCM + arithmetic coding
    Result<std::vector<std::uint8_t>> compressSCM(
        std::span<const std::string_view> qualities);

    // Decompress using SCM + arithmetic coding
    Result<std::vector<std::string>> decompressSCM(
        std::span<const std::uint8_t> data,
        std::span<const std::uint32_t> lengths);

    // Fallback to Zstd for comparison/fallback
    Result<std::vector<std::uint8_t>> compressZstd(
        std::span<const std::string_view> qualities);

    Result<std::vector<std::string>> decompressZstd(
        std::span<const std::uint8_t> data,
        std::span<const std::uint32_t> lengths);
};

// =============================================================================
// SCM Compression Implementation
// =============================================================================

Result<std::vector<std::uint8_t>> QualityCompressorImpl::compressSCM(
    std::span<const std::string_view> qualities) {

    if (qualities.empty()) {
        return std::vector<std::uint8_t>{};
    }

    // Reset context model for fresh compression
    contextModel_.reset();

    ArithmeticEncoder encoder;

    for (const auto& quality : qualities) {
        std::uint8_t prevQual1 = 0;
        std::uint8_t prevQual2 = 0;
        std::size_t readLength = quality.length();

        for (std::size_t pos = 0; pos < quality.length(); ++pos) {
            std::uint8_t qualValue = qualityCharToValue(quality[pos]);

            // Apply Illumina 8-bin if configured
            if (config_.qualityMode == QualityMode::kIllumina8) {
                qualValue = Illumina8BinMapper::toBin(qualValue);
            }

            // Clamp to valid range
            if (qualValue >= kNumQualitySymbols) {
                qualValue = kNumQualitySymbols - 1;
            }

            // Compute position bin
            std::size_t positionBin = 0;
            if (config_.usePositionContext) {
                positionBin = computePositionBin(pos, readLength, config_.numPositionBins);
            }

            // Get context model and encode
            auto& model = contextModel_.getModel(prevQual1, prevQual2, positionBin);
            encoder.encode(qualValue, model);
            model.update(qualValue);

            // Update context
            prevQual2 = prevQual1;
            prevQual1 = qualValue;
        }
    }

    encoder.finish();

    // Get encoded data
    std::vector<std::uint8_t> encoded = encoder.getData();

    // Apply Zstd compression on top for better compression
    std::size_t compressBound = ZSTD_compressBound(encoded.size());
    std::vector<std::uint8_t> compressed(compressBound);

    std::size_t compressedSize = ZSTD_compress(
        compressed.data(), compressed.size(),
        encoded.data(), encoded.size(),
        3);  // Level 3 for balance

    if (ZSTD_isError(compressedSize)) {
        return makeError<std::vector<std::uint8_t>>(
            ErrorCode::kIOError,
            "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
    }

    compressed.resize(compressedSize);
    return compressed;
}

// =============================================================================
// SCM Decompression Implementation
// =============================================================================

Result<std::vector<std::string>> QualityCompressorImpl::decompressSCM(
    std::span<const std::uint8_t> data,
    std::span<const std::uint32_t> lengths) {

    if (data.empty() || lengths.empty()) {
        return std::vector<std::string>(lengths.size());
    }

    // Decompress Zstd layer first
    std::size_t decompressedSize = ZSTD_getFrameContentSize(data.data(), data.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
        decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kFormatError, "Invalid Zstd frame");
    }

    std::vector<std::uint8_t> decompressed(decompressedSize);
    std::size_t actualSize = ZSTD_decompress(
        decompressed.data(), decompressed.size(),
        data.data(), data.size());

    if (ZSTD_isError(actualSize)) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kIOError,
            "Zstd decompression failed: " + std::string(ZSTD_getErrorName(actualSize)));
    }

    // Reset context model for fresh decompression
    contextModel_.reset();

    ArithmeticDecoder decoder(decompressed);

    std::vector<std::string> qualities;
    qualities.reserve(lengths.size());

    for (std::uint32_t len : lengths) {
        std::string quality;
        quality.reserve(len);

        std::uint8_t prevQual1 = 0;
        std::uint8_t prevQual2 = 0;

        for (std::uint32_t pos = 0; pos < len; ++pos) {
            // Compute position bin
            std::size_t positionBin = 0;
            if (config_.usePositionContext) {
                positionBin = computePositionBin(pos, len, config_.numPositionBins);
            }

            // Get context model and decode
            auto& model = contextModel_.getModel(prevQual1, prevQual2, positionBin);
            std::size_t qualValue = decoder.decode(model);
            model.update(qualValue);

            // Convert back from Illumina 8-bin if needed
            std::uint8_t outputQual = static_cast<std::uint8_t>(qualValue);
            if (config_.qualityMode == QualityMode::kIllumina8) {
                outputQual = Illumina8BinMapper::fromBin(outputQual);
            }

            quality.push_back(qualityValueToChar(outputQual));

            // Update context
            prevQual2 = prevQual1;
            prevQual1 = static_cast<std::uint8_t>(qualValue);
        }

        qualities.push_back(std::move(quality));
    }

    return qualities;
}

// =============================================================================
// Zstd Fallback Implementation
// =============================================================================

Result<std::vector<std::uint8_t>> QualityCompressorImpl::compressZstd(
    std::span<const std::string_view> qualities) {

    // Concatenate all quality strings
    std::vector<std::uint8_t> buffer;
    std::size_t totalSize = 0;
    for (const auto& q : qualities) {
        totalSize += q.size();
    }
    buffer.reserve(totalSize);

    for (const auto& quality : qualities) {
        for (char c : quality) {
            std::uint8_t qualValue = qualityCharToValue(c);

            // Apply Illumina 8-bin if configured
            if (config_.qualityMode == QualityMode::kIllumina8) {
                qualValue = Illumina8BinMapper::toBin(qualValue);
            }

            buffer.push_back(qualValue);
        }
    }

    // Compress with Zstd
    std::size_t compressBound = ZSTD_compressBound(buffer.size());
    std::vector<std::uint8_t> compressed(compressBound);

    std::size_t compressedSize = ZSTD_compress(
        compressed.data(), compressed.size(),
        buffer.data(), buffer.size(),
        3);

    if (ZSTD_isError(compressedSize)) {
        return makeError<std::vector<std::uint8_t>>(
            ErrorCode::kIOError,
            "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
    }

    compressed.resize(compressedSize);
    return compressed;
}

Result<std::vector<std::string>> QualityCompressorImpl::decompressZstd(
    std::span<const std::uint8_t> data,
    std::span<const std::uint32_t> lengths) {

    if (data.empty()) {
        return std::vector<std::string>(lengths.size());
    }

    // Decompress with Zstd
    std::size_t decompressedSize = ZSTD_getFrameContentSize(data.data(), data.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
        decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kFormatError, "Invalid Zstd frame");
    }

    std::vector<std::uint8_t> buffer(decompressedSize);
    std::size_t actualSize = ZSTD_decompress(
        buffer.data(), buffer.size(),
        data.data(), data.size());

    if (ZSTD_isError(actualSize)) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kIOError,
            "Zstd decompression failed: " + std::string(ZSTD_getErrorName(actualSize)));
    }

    // Parse quality strings
    std::vector<std::string> qualities;
    qualities.reserve(lengths.size());

    const std::uint8_t* ptr = buffer.data();
    for (std::uint32_t len : lengths) {
        std::string quality;
        quality.reserve(len);

        for (std::uint32_t i = 0; i < len; ++i) {
            std::uint8_t qualValue = *ptr++;

            // Convert back from Illumina 8-bin if needed
            if (config_.qualityMode == QualityMode::kIllumina8) {
                qualValue = Illumina8BinMapper::fromBin(qualValue);
            }

            quality.push_back(qualityValueToChar(qualValue));
        }

        qualities.push_back(std::move(quality));
    }

    return qualities;
}

// =============================================================================
// Main Compression/Decompression Entry Points
// =============================================================================

Result<CompressedQualityData> QualityCompressorImpl::compress(
    std::span<const std::string_view> qualities,
    std::span<const std::string_view> /* sequences */) {

    CompressedQualityData result;
    result.numStrings = static_cast<std::uint32_t>(qualities.size());
    result.contextOrder = config_.contextOrder;
    result.qualityMode = config_.qualityMode;

    if (qualities.empty()) {
        return result;
    }

    // Calculate uncompressed size
    for (const auto& q : qualities) {
        result.uncompressedSize += q.size();
    }

    // Handle discard mode
    if (config_.qualityMode == QualityMode::kDiscard) {
        result.data.clear();
        return result;
    }

    // Use SCM compression
    auto compressResult = compressSCM(qualities);
    if (!compressResult) {
        return makeError<CompressedQualityData>(compressResult.error());
    }

    result.data = std::move(*compressResult);

    FQC_LOG_DEBUG("Quality compression: {} bytes -> {} bytes (ratio: {:.2f})",
                  result.uncompressedSize, result.data.size(),
                  result.compressionRatio());

    return result;
}

Result<std::vector<std::string>> QualityCompressorImpl::decompress(
    std::span<const std::uint8_t> data,
    std::span<const std::uint32_t> lengths) {

    // Handle discard mode
    if (config_.qualityMode == QualityMode::kDiscard) {
        std::vector<std::string> qualities;
        qualities.reserve(lengths.size());
        for (std::uint32_t len : lengths) {
            qualities.emplace_back(len, kDefaultPlaceholderQual);
        }
        return qualities;
    }

    return decompressSCM(data, lengths);
}

Result<std::vector<std::string>> QualityCompressorImpl::decompress(
    std::span<const std::uint8_t> data,
    std::uint32_t numStrings,
    std::uint32_t uniformLength) {

    std::vector<std::uint32_t> lengths(numStrings, uniformLength);
    return decompress(data, lengths);
}

// =============================================================================
// QualityCompressor Public Interface
// =============================================================================

QualityCompressor::QualityCompressor(QualityCompressorConfig config)
    : impl_(std::make_unique<QualityCompressorImpl>(std::move(config))) {}

QualityCompressor::~QualityCompressor() = default;

QualityCompressor::QualityCompressor(QualityCompressor&&) noexcept = default;
QualityCompressor& QualityCompressor::operator=(QualityCompressor&&) noexcept = default;

Result<CompressedQualityData> QualityCompressor::compress(
    std::span<const std::string_view> qualities) {
    return impl_->compress(qualities, {});
}

Result<CompressedQualityData> QualityCompressor::compress(
    std::span<const std::string_view> qualities,
    std::span<const std::string_view> sequences) {
    return impl_->compress(qualities, sequences);
}

Result<std::vector<std::string>> QualityCompressor::decompress(
    std::span<const std::uint8_t> data,
    std::span<const std::uint32_t> lengths) {
    return impl_->decompress(data, lengths);
}

Result<std::vector<std::string>> QualityCompressor::decompress(
    std::span<const std::uint8_t> data,
    std::uint32_t numStrings,
    std::uint32_t uniformLength) {
    return impl_->decompress(data, numStrings, uniformLength);
}

const QualityCompressorConfig& QualityCompressor::config() const noexcept {
    return impl_->config();
}

VoidResult QualityCompressor::setConfig(QualityCompressorConfig config) {
    return impl_->setConfig(std::move(config));
}

void QualityCompressor::reset() noexcept {
    impl_->reset();
}

// =============================================================================
// Utility Functions Implementation
// =============================================================================

std::string applyIllumina8Bin(std::string_view quality) {
    std::string result;
    result.reserve(quality.size());

    for (char c : quality) {
        std::uint8_t qualValue = qualityCharToValue(c);
        std::uint8_t binned = Illumina8BinMapper::fromBin(
            Illumina8BinMapper::toBin(qualValue));
        result.push_back(qualityValueToChar(binned));
    }

    return result;
}

std::array<std::uint64_t, kNumQualitySymbols> computeQualityHistogram(
    std::span<const std::string_view> qualities) {

    std::array<std::uint64_t, kNumQualitySymbols> histogram{};

    for (const auto& quality : qualities) {
        for (char c : quality) {
            std::uint8_t qualValue = qualityCharToValue(c);
            if (qualValue < kNumQualitySymbols) {
                histogram[qualValue]++;
            }
        }
    }

    return histogram;
}

}  // namespace fqc::algo
