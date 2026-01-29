// =============================================================================
// fq-compressor - ID Compressor Implementation
// =============================================================================
// Implements FASTQ ID compression using tokenization and delta encoding.
//
// Requirements: 1.1.2 (ID compression)
// =============================================================================

#include "fqc/algo/id_compressor.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <zstd.h>

namespace fqc::algo {

// =============================================================================
// Internal Constants
// =============================================================================

namespace {

/// @brief Magic byte for exact mode compressed data
constexpr std::uint8_t kMagicExact = 0x01;

/// @brief Magic byte for tokenize mode compressed data
constexpr std::uint8_t kMagicTokenize = 0x02;

/// @brief Magic byte for discard mode (no data)
constexpr std::uint8_t kMagicDiscard = 0x03;

/// @brief Maximum varint bytes for 64-bit integer
constexpr std::size_t kMaxVarintBytes = 10;

}  // namespace

// =============================================================================
// ParsedID Implementation
// =============================================================================

std::string ParsedID::reconstruct() const {
    std::string result;
    result.reserve(original.length());

    for (const auto& token : tokens) {
        switch (token.type) {
            case TokenType::kStatic:
            case TokenType::kDynamicString:
            case TokenType::kDelimiter:
                result += token.value;
                break;
            case TokenType::kDynamicInt:
                result += std::to_string(token.intValue);
                break;
        }
    }

    return result;
}


// =============================================================================
// IDPattern Implementation
// =============================================================================

bool IDPattern::matchesAll(std::span<const std::string_view> ids) const {
    if (!isValid() || ids.empty()) {
        return false;
    }

    IDTokenizer tokenizer;

    for (const auto& id : ids) {
        auto tokens = tokenizer.tokenize(id);
        if (tokens.size() != tokenTypes.size()) {
            return false;
        }

        std::size_t staticIdx = 0;
        std::size_t delimIdx = 0;

        for (std::size_t i = 0; i < tokens.size(); ++i) {
            // Check type matches
            if (tokens[i].type != tokenTypes[i]) {
                // Allow dynamic int to match dynamic string (flexible)
                if (!(tokenTypes[i] == TokenType::kDynamicInt &&
                      tokens[i].type == TokenType::kDynamicString)) {
                    return false;
                }
            }

            // Check static values match
            if (tokenTypes[i] == TokenType::kStatic) {
                if (staticIdx >= staticValues.size() ||
                    tokens[i].value != staticValues[staticIdx]) {
                    return false;
                }
                ++staticIdx;
            }

            // Check delimiters match
            if (tokenTypes[i] == TokenType::kDelimiter) {
                if (delimIdx >= delimiters.size() ||
                    tokens[i].value.empty() ||
                    tokens[i].value[0] != delimiters[delimIdx]) {
                    return false;
                }
                ++delimIdx;
            }
        }
    }

    return true;
}

// =============================================================================
// IDCompressorConfig Implementation
// =============================================================================

VoidResult IDCompressorConfig::validate() const {
    if (compressionLevel < kMinCompressionLevel ||
        compressionLevel > kMaxCompressionLevel) {
        return makeVoidError(ErrorCode::kInvalidArgument,
                         "Compression level must be between 1 and 9");
    }

    if (zstdLevel < 1 || zstdLevel > 22) {
        return makeVoidError(ErrorCode::kInvalidArgument,
                         "Zstd level must be between 1 and 22");
    }

    if (lzmaLevel < 0 || lzmaLevel > 9) {
        return makeVoidError(ErrorCode::kInvalidArgument,
                         "LZMA level must be between 0 and 9");
    }

    if (minPatternMatchRatio < 0.0 || minPatternMatchRatio > 1.0) {
        return makeVoidError(ErrorCode::kInvalidArgument,
                         "Pattern match ratio must be between 0.0 and 1.0");
    }

    return {};
}


// =============================================================================
// IDTokenizer Implementation
// =============================================================================

IDTokenizer::IDTokenizer(std::string_view delimiters)
    : delimiters_(delimiters) {}

bool IDTokenizer::isDelimiter(char c) const noexcept {
    return delimiters_.find(c) != std::string::npos;
}

std::optional<std::int64_t> IDTokenizer::tryParseInt(std::string_view str) {
    if (str.empty()) {
        return std::nullopt;
    }

    // Handle negative numbers
    std::size_t start = 0;
    if (str[0] == '-') {
        start = 1;
        if (str.length() == 1) {
            return std::nullopt;
        }
    }

    // Check all remaining characters are digits
    for (std::size_t i = start; i < str.length(); ++i) {
        if (str[i] < '0' || str[i] > '9') {
            return std::nullopt;
        }
    }

    // Parse the integer
    std::int64_t result = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.length(), result);

    if (ec == std::errc{} && ptr == str.data() + str.length()) {
        return result;
    }

    return std::nullopt;
}

std::vector<IDToken> IDTokenizer::tokenize(std::string_view id) const {
    std::vector<IDToken> tokens;
    tokens.reserve(16);  // Typical ID has fewer than 16 tokens

    std::size_t pos = 0;
    std::size_t tokenStart = 0;

    while (pos < id.length()) {
        if (isDelimiter(id[pos])) {
            // Emit previous token if any
            if (pos > tokenStart) {
                auto tokenStr = id.substr(tokenStart, pos - tokenStart);
                auto intVal = tryParseInt(tokenStr);
                if (intVal) {
                    tokens.push_back(IDToken::makeDynamicInt(*intVal, tokenStart,
                                                             pos - tokenStart));
                } else {
                    tokens.push_back(IDToken::makeDynamicString(tokenStr, tokenStart));
                }
            }

            // Emit delimiter
            tokens.push_back(IDToken::makeDelimiter(id[pos], pos));
            ++pos;
            tokenStart = pos;
        } else {
            ++pos;
        }
    }

    // Emit final token if any
    if (pos > tokenStart) {
        auto tokenStr = id.substr(tokenStart, pos - tokenStart);
        auto intVal = tryParseInt(tokenStr);
        if (intVal) {
            tokens.push_back(IDToken::makeDynamicInt(*intVal, tokenStart,
                                                     pos - tokenStart));
        } else {
            tokens.push_back(IDToken::makeDynamicString(tokenStr, tokenStart));
        }
    }

    return tokens;
}


// =============================================================================
// Varint Encoding/Decoding
// =============================================================================

std::size_t uvarintEncode(std::uint64_t value, std::uint8_t* output) {
    std::size_t i = 0;
    while (value >= 0x80) {
        output[i++] = static_cast<std::uint8_t>((value & 0x7F) | 0x80);
        value >>= 7;
    }
    output[i++] = static_cast<std::uint8_t>(value);
    return i;
}

std::uint64_t uvarintDecode(std::span<const std::uint8_t> data, std::size_t& bytesRead) {
    std::uint64_t result = 0;
    std::size_t shift = 0;
    bytesRead = 0;

    for (std::size_t i = 0; i < data.size() && i < kMaxVarintBytes; ++i) {
        std::uint8_t byte = data[i];
        result |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        ++bytesRead;

        if ((byte & 0x80) == 0) {
            return result;
        }
        shift += 7;
    }

    // Malformed varint
    return result;
}

std::size_t varintEncode(std::int64_t value, std::uint8_t* output) {
    return uvarintEncode(zigzagEncode(value), output);
}

std::int64_t varintDecode(std::span<const std::uint8_t> data, std::size_t& bytesRead) {
    return zigzagDecode(uvarintDecode(data, bytesRead));
}

// =============================================================================
// Delta + Varint Encoding
// =============================================================================

std::vector<std::uint8_t> deltaVarintEncode(std::span<const std::int64_t> values) {
    std::vector<std::uint8_t> result;
    result.reserve(values.size() * 2);  // Estimate 2 bytes per value

    std::int64_t prev = 0;
    std::uint8_t buffer[kMaxVarintBytes];

    for (const auto& value : values) {
        std::int64_t delta = value - prev;
        std::size_t len = varintEncode(delta, buffer);
        result.insert(result.end(), buffer, buffer + len);
        prev = value;
    }

    return result;
}

Result<std::vector<std::int64_t>> deltaVarintDecode(
    std::span<const std::uint8_t> data,
    std::size_t count) {

    std::vector<std::int64_t> result;
    result.reserve(count);

    std::int64_t prev = 0;
    std::size_t offset = 0;

    for (std::size_t i = 0; i < count; ++i) {
        if (offset >= data.size()) {
            return std::unexpected(Error{ErrorCode::kCorruptedData,
                             "Unexpected end of varint data"});
        }

        std::size_t bytesRead = 0;
        std::int64_t delta = varintDecode(data.subspan(offset), bytesRead);
        offset += bytesRead;

        std::int64_t value = prev + delta;
        result.push_back(value);
        prev = value;
    }

    return result;
}


// =============================================================================
// Utility Functions
// =============================================================================

std::string generateDiscardId(
    ReadId archiveId,
    bool isPaired,
    PELayout peLayout,
    std::string_view prefix) {

    std::string result;
    if (!prefix.empty()) {
        result = std::string(prefix);
    }

    if (isPaired && peLayout == PELayout::kInterleaved) {
        // Interleaved PE: @prefix{pair_id}/{read_num}
        // archiveId 1,2 -> pair 1, reads 1,2
        // archiveId 3,4 -> pair 2, reads 1,2
        ReadId pairId = (archiveId + 1) / 2;
        int readNum = ((archiveId - 1) % 2) + 1;
        result += std::to_string(pairId);
        result += '/';
        result += std::to_string(readNum);
    } else {
        // SE or PE Consecutive: @prefix{archive_id}
        result += std::to_string(archiveId);
    }

    return result;
}

// =============================================================================
// IDCompressor Implementation
// =============================================================================

/// @brief Internal implementation class
class IDCompressorImpl {
public:
    explicit IDCompressorImpl(IDCompressorConfig config)
        : config_(std::move(config))
        , tokenizer_(config_.delimiters) {}

    Result<CompressedIDData> compress(std::span<const std::string_view> ids);
    Result<std::vector<std::string>> decompress(
        std::span<const std::uint8_t> data,
        std::uint32_t numIds,
        IDMode mode);

    std::optional<IDPattern> detectPattern(std::span<const std::string_view> ids) const;
    ParsedID parseId(std::string_view id) const;

    const IDCompressorConfig& config() const noexcept { return config_; }
    VoidResult setConfig(IDCompressorConfig config);
    void reset() noexcept;

private:
    Result<CompressedIDData> compressExact(std::span<const std::string_view> ids);
    Result<CompressedIDData> compressTokenize(std::span<const std::string_view> ids);
    Result<CompressedIDData> compressDiscard(std::span<const std::string_view> ids);

    Result<std::vector<std::string>> decompressExact(
        std::span<const std::uint8_t> data,
        std::uint32_t numIds);
    Result<std::vector<std::string>> decompressTokenize(
        std::span<const std::uint8_t> data,
        std::uint32_t numIds);
    Result<std::vector<std::string>> decompressDiscard(std::uint32_t numIds);

    std::vector<std::uint8_t> compressWithZstd(std::span<const std::uint8_t> data);
    Result<std::vector<std::uint8_t>> decompressWithZstd(
        std::span<const std::uint8_t> data,
        std::size_t uncompressedSize);

    IDCompressorConfig config_;
    IDTokenizer tokenizer_;
};


// =============================================================================
// IDCompressorImpl - Pattern Detection
// =============================================================================

std::optional<IDPattern> IDCompressorImpl::detectPattern(
    std::span<const std::string_view> ids) const {

    if (ids.empty()) {
        return std::nullopt;
    }

    // Tokenize first ID to establish pattern
    auto firstTokens = tokenizer_.tokenize(ids[0]);
    if (firstTokens.empty()) {
        return std::nullopt;
    }

    IDPattern pattern;
    pattern.tokenTypes.reserve(firstTokens.size());

    for (const auto& token : firstTokens) {
        pattern.tokenTypes.push_back(token.type);

        if (token.type == TokenType::kStatic) {
            pattern.staticValues.push_back(token.value);
        } else if (token.type == TokenType::kDelimiter) {
            pattern.delimiters.push_back(token.value.empty() ? ':' : token.value[0]);
        } else if (token.type == TokenType::kDynamicInt) {
            ++pattern.numDynamicInts;
        } else if (token.type == TokenType::kDynamicString) {
            ++pattern.numDynamicStrings;
        }
    }

    // Check how many IDs match this pattern
    std::size_t matchCount = 0;
    for (const auto& id : ids) {
        auto tokens = tokenizer_.tokenize(id);
        if (tokens.size() != pattern.tokenTypes.size()) {
            continue;
        }

        bool matches = true;
        std::size_t staticIdx = 0;
        std::size_t delimIdx = 0;

        for (std::size_t i = 0; i < tokens.size() && matches; ++i) {
            if (tokens[i].type != pattern.tokenTypes[i]) {
                // Allow int/string flexibility
                if (!(pattern.tokenTypes[i] == TokenType::kDynamicInt &&
                      tokens[i].type == TokenType::kDynamicString) &&
                    !(pattern.tokenTypes[i] == TokenType::kDynamicString &&
                      tokens[i].type == TokenType::kDynamicInt)) {
                    matches = false;
                }
            }

            if (pattern.tokenTypes[i] == TokenType::kStatic) {
                if (staticIdx >= pattern.staticValues.size() ||
                    tokens[i].value != pattern.staticValues[staticIdx]) {
                    matches = false;
                }
                ++staticIdx;
            }

            if (pattern.tokenTypes[i] == TokenType::kDelimiter) {
                if (delimIdx >= pattern.delimiters.size() ||
                    tokens[i].value.empty() ||
                    tokens[i].value[0] != pattern.delimiters[delimIdx]) {
                    matches = false;
                }
                ++delimIdx;
            }
        }

        if (matches) {
            ++matchCount;
        }
    }

    // Check if enough IDs match the pattern
    double matchRatio = static_cast<double>(matchCount) / static_cast<double>(ids.size());
    if (matchRatio < config_.minPatternMatchRatio) {
        return std::nullopt;
    }

    return pattern;
}

ParsedID IDCompressorImpl::parseId(std::string_view id) const {
    ParsedID parsed;
    parsed.original = std::string(id);
    parsed.tokens = tokenizer_.tokenize(id);
    return parsed;
}


// =============================================================================
// IDCompressorImpl - Compression
// =============================================================================

Result<CompressedIDData> IDCompressorImpl::compress(std::span<const std::string_view> ids) {
    switch (config_.idMode) {
        case IDMode::kExact:
            return compressExact(ids);
        case IDMode::kTokenize:
            return compressTokenize(ids);
        case IDMode::kDiscard:
            return compressDiscard(ids);
    }
    return std::unexpected(Error{ErrorCode::kInvalidArgument, "Unknown ID mode"});
}

Result<CompressedIDData> IDCompressorImpl::compressExact(std::span<const std::string_view> ids) {
    CompressedIDData result;
    result.idMode = IDMode::kExact;
    result.numIds = static_cast<std::uint32_t>(ids.size());

    if (ids.empty()) {
        result.data.push_back(kMagicExact);
        return result;
    }

    // Calculate total uncompressed size
    result.uncompressedSize = 0;
    for (const auto& id : ids) {
        result.uncompressedSize += id.length() + 1;  // +1 for length prefix or newline
    }

    // Build uncompressed buffer: [len1][id1][len2][id2]...
    std::vector<std::uint8_t> uncompressed;
    uncompressed.reserve(result.uncompressedSize + ids.size() * 4);

    std::uint8_t lenBuf[kMaxVarintBytes];
    for (const auto& id : ids) {
        // Write length as varint
        std::size_t lenBytes = uvarintEncode(id.length(), lenBuf);
        uncompressed.insert(uncompressed.end(), lenBuf, lenBuf + lenBytes);

        // Write ID bytes
        uncompressed.insert(uncompressed.end(), id.begin(), id.end());
    }

    // Compress with Zstd
    auto compressed = compressWithZstd(uncompressed);

    // Build output: [magic][uncompressed_size][compressed_data]
    result.data.reserve(1 + kMaxVarintBytes + compressed.size());
    result.data.push_back(kMagicExact);

    std::size_t sizeBytes = uvarintEncode(uncompressed.size(), lenBuf);
    result.data.insert(result.data.end(), lenBuf, lenBuf + sizeBytes);
    result.data.insert(result.data.end(), compressed.begin(), compressed.end());

    return result;
}

Result<CompressedIDData> IDCompressorImpl::compressTokenize(
    std::span<const std::string_view> ids) {

    // Try to detect a pattern
    auto pattern = detectPattern(ids);
    if (!pattern || pattern->numDynamicInts == 0) {
        // Fall back to exact mode if no good pattern found
        auto result = compressExact(ids);
        if (result) {
            result->idMode = IDMode::kExact;  // Mark as exact
        }
        return result;
    }

    CompressedIDData result;
    result.idMode = IDMode::kTokenize;
    result.numIds = static_cast<std::uint32_t>(ids.size());
    result.pattern = *pattern;

    // Calculate uncompressed size
    result.uncompressedSize = 0;
    for (const auto& id : ids) {
        result.uncompressedSize += id.length();
    }

    // Extract dynamic integer values for delta encoding
    std::vector<std::vector<std::int64_t>> intColumns(pattern->numDynamicInts);
    for (auto& col : intColumns) {
        col.reserve(ids.size());
    }

    // Extract dynamic string values
    std::vector<std::vector<std::string>> strColumns(pattern->numDynamicStrings);
    for (auto& col : strColumns) {
        col.reserve(ids.size());
    }

    for (const auto& id : ids) {
        auto tokens = tokenizer_.tokenize(id);
        std::size_t intIdx = 0;
        std::size_t strIdx = 0;

        for (std::size_t i = 0; i < tokens.size() && i < pattern->tokenTypes.size(); ++i) {
            if (pattern->tokenTypes[i] == TokenType::kDynamicInt) {
                if (tokens[i].type == TokenType::kDynamicInt) {
                    intColumns[intIdx].push_back(tokens[i].intValue);
                } else {
                    // Token didn't parse as int, use 0
                    intColumns[intIdx].push_back(0);
                }
                ++intIdx;
            } else if (pattern->tokenTypes[i] == TokenType::kDynamicString) {
                strColumns[strIdx].push_back(tokens[i].value);
                ++strIdx;
            }
        }
    }

    // Build output buffer
    std::vector<std::uint8_t> uncompressed;
    std::uint8_t lenBuf[kMaxVarintBytes];

    // Write pattern header
    // [num_token_types][token_types...][num_static][static_values...][num_delims][delims...]
    std::size_t bytes = uvarintEncode(pattern->tokenTypes.size(), lenBuf);
    uncompressed.insert(uncompressed.end(), lenBuf, lenBuf + bytes);

    for (auto type : pattern->tokenTypes) {
        uncompressed.push_back(static_cast<std::uint8_t>(type));
    }

    bytes = uvarintEncode(pattern->staticValues.size(), lenBuf);
    uncompressed.insert(uncompressed.end(), lenBuf, lenBuf + bytes);

    for (const auto& sv : pattern->staticValues) {
        bytes = uvarintEncode(sv.length(), lenBuf);
        uncompressed.insert(uncompressed.end(), lenBuf, lenBuf + bytes);
        uncompressed.insert(uncompressed.end(), sv.begin(), sv.end());
    }

    bytes = uvarintEncode(pattern->delimiters.size(), lenBuf);
    uncompressed.insert(uncompressed.end(), lenBuf, lenBuf + bytes);
    uncompressed.insert(uncompressed.end(), pattern->delimiters.begin(),
                        pattern->delimiters.end());

    // Write delta-encoded integer columns
    bytes = uvarintEncode(intColumns.size(), lenBuf);
    uncompressed.insert(uncompressed.end(), lenBuf, lenBuf + bytes);

    for (const auto& col : intColumns) {
        auto encoded = deltaVarintEncode(col);
        bytes = uvarintEncode(encoded.size(), lenBuf);
        uncompressed.insert(uncompressed.end(), lenBuf, lenBuf + bytes);
        uncompressed.insert(uncompressed.end(), encoded.begin(), encoded.end());
    }

    // Write string columns (length-prefixed)
    bytes = uvarintEncode(strColumns.size(), lenBuf);
    uncompressed.insert(uncompressed.end(), lenBuf, lenBuf + bytes);

    for (const auto& col : strColumns) {
        for (const auto& s : col) {
            bytes = uvarintEncode(s.length(), lenBuf);
            uncompressed.insert(uncompressed.end(), lenBuf, lenBuf + bytes);
            uncompressed.insert(uncompressed.end(), s.begin(), s.end());
        }
    }

    // Compress with Zstd
    auto compressed = compressWithZstd(uncompressed);

    // Build final output
    result.data.reserve(1 + kMaxVarintBytes + compressed.size());
    result.data.push_back(kMagicTokenize);

    bytes = uvarintEncode(uncompressed.size(), lenBuf);
    result.data.insert(result.data.end(), lenBuf, lenBuf + bytes);
    result.data.insert(result.data.end(), compressed.begin(), compressed.end());

    return result;
}

Result<CompressedIDData> IDCompressorImpl::compressDiscard(
    std::span<const std::string_view> ids) {

    CompressedIDData result;
    result.idMode = IDMode::kDiscard;
    result.numIds = static_cast<std::uint32_t>(ids.size());

    // Calculate original size for statistics
    result.uncompressedSize = 0;
    for (const auto& id : ids) {
        result.uncompressedSize += id.length();
    }

    // Discard mode stores nothing except magic byte
    result.data.push_back(kMagicDiscard);

    return result;
}


// =============================================================================
// IDCompressorImpl - Decompression
// =============================================================================

Result<std::vector<std::string>> IDCompressorImpl::decompress(
    std::span<const std::uint8_t> data,
    std::uint32_t numIds,
    IDMode mode) {

    if (data.empty()) {
        if (numIds == 0) {
            return std::vector<std::string>{};
        }
        return std::unexpected(Error{ErrorCode::kCorruptedData, "Empty compressed data"});
    }

    std::uint8_t magic = data[0];

    switch (magic) {
        case kMagicExact:
            return decompressExact(data.subspan(1), numIds);
        case kMagicTokenize:
            return decompressTokenize(data.subspan(1), numIds);
        case kMagicDiscard:
            return decompressDiscard(numIds);
        default:
            return std::unexpected(Error{ErrorCode::kCorruptedData,
                             "Unknown ID compression magic byte"});
    }
}

Result<std::vector<std::string>> IDCompressorImpl::decompressExact(
    std::span<const std::uint8_t> data,
    std::uint32_t numIds) {

    if (numIds == 0) {
        return std::vector<std::string>{};
    }

    if (data.empty()) {
        return std::unexpected(Error{ErrorCode::kCorruptedData, "Missing uncompressed size"});
    }

    // Read uncompressed size
    std::size_t bytesRead = 0;
    std::uint64_t uncompressedSize = uvarintDecode(data, bytesRead);
    data = data.subspan(bytesRead);

    // Decompress with Zstd
    auto uncompressedResult = decompressWithZstd(data, uncompressedSize);
    if (!uncompressedResult) {
        return std::unexpected(uncompressedResult.error());
    }

    const auto& uncompressed = *uncompressedResult;

    // Parse IDs: [len1][id1][len2][id2]...
    std::vector<std::string> result;
    result.reserve(numIds);

    std::size_t offset = 0;
    for (std::uint32_t i = 0; i < numIds; ++i) {
        if (offset >= uncompressed.size()) {
            return std::unexpected(Error{ErrorCode::kCorruptedData,
                             "Unexpected end of ID data"});
        }

        std::size_t lenBytes = 0;
        std::uint64_t idLen = uvarintDecode(
            std::span<const std::uint8_t>(uncompressed.data() + offset,
                                          uncompressed.size() - offset),
            lenBytes);
        offset += lenBytes;

        if (offset + idLen > uncompressed.size()) {
            return std::unexpected(Error{ErrorCode::kCorruptedData,
                             "ID length exceeds data size"});
        }

        result.emplace_back(
            reinterpret_cast<const char*>(uncompressed.data() + offset),
            idLen);
        offset += idLen;
    }

    return result;
}

Result<std::vector<std::string>> IDCompressorImpl::decompressTokenize(
    std::span<const std::uint8_t> data,
    std::uint32_t numIds) {

    if (numIds == 0) {
        return std::vector<std::string>{};
    }

    if (data.empty()) {
        return std::unexpected(Error{ErrorCode::kCorruptedData, "Missing uncompressed size"});
    }

    // Read uncompressed size
    std::size_t bytesRead = 0;
    std::uint64_t uncompressedSize = uvarintDecode(data, bytesRead);
    data = data.subspan(bytesRead);

    // Decompress with Zstd
    auto uncompressedResult = decompressWithZstd(data, uncompressedSize);
    if (!uncompressedResult) {
        return std::unexpected(uncompressedResult.error());
    }

    const auto& uncompressed = *uncompressedResult;
    std::size_t offset = 0;

    auto readVarint = [&]() -> std::uint64_t {
        std::size_t bytes = 0;
        auto val = uvarintDecode(
            std::span<const std::uint8_t>(uncompressed.data() + offset,
                                          uncompressed.size() - offset),
            bytes);
        offset += bytes;
        return val;
    };

    // Read pattern
    IDPattern pattern;
    std::size_t numTokenTypes = readVarint();
    pattern.tokenTypes.resize(numTokenTypes);

    for (std::size_t i = 0; i < numTokenTypes; ++i) {
        pattern.tokenTypes[i] = static_cast<TokenType>(uncompressed[offset++]);
        if (pattern.tokenTypes[i] == TokenType::kDynamicInt) {
            ++pattern.numDynamicInts;
        } else if (pattern.tokenTypes[i] == TokenType::kDynamicString) {
            ++pattern.numDynamicStrings;
        }
    }

    // Read static values
    std::size_t numStatic = readVarint();
    pattern.staticValues.resize(numStatic);
    for (std::size_t i = 0; i < numStatic; ++i) {
        std::size_t len = readVarint();
        pattern.staticValues[i] = std::string(
            reinterpret_cast<const char*>(uncompressed.data() + offset), len);
        offset += len;
    }

    // Read delimiters
    std::size_t numDelims = readVarint();
    pattern.delimiters.resize(numDelims);
    for (std::size_t i = 0; i < numDelims; ++i) {
        pattern.delimiters[i] = static_cast<char>(uncompressed[offset++]);
    }

    // Read integer columns
    std::size_t numIntCols = readVarint();
    std::vector<std::vector<std::int64_t>> intColumns(numIntCols);

    for (std::size_t col = 0; col < numIntCols; ++col) {
        std::size_t encodedLen = readVarint();
        auto decodeResult = deltaVarintDecode(
            std::span<const std::uint8_t>(uncompressed.data() + offset, encodedLen),
            numIds);
        if (!decodeResult) {
            return std::unexpected(decodeResult.error());
        }
        intColumns[col] = std::move(*decodeResult);
        offset += encodedLen;
    }

    // Read string columns
    std::size_t numStrCols = readVarint();
    std::vector<std::vector<std::string>> strColumns(numStrCols);

    for (std::size_t col = 0; col < numStrCols; ++col) {
        strColumns[col].reserve(numIds);
        for (std::uint32_t i = 0; i < numIds; ++i) {
            std::size_t len = readVarint();
            strColumns[col].emplace_back(
                reinterpret_cast<const char*>(uncompressed.data() + offset), len);
            offset += len;
        }
    }

    // Reconstruct IDs
    std::vector<std::string> result;
    result.reserve(numIds);

    for (std::uint32_t i = 0; i < numIds; ++i) {
        std::string id;
        std::size_t staticIdx = 0;
        std::size_t delimIdx = 0;
        std::size_t intIdx = 0;
        std::size_t strIdx = 0;

        for (auto type : pattern.tokenTypes) {
            switch (type) {
                case TokenType::kStatic:
                    if (staticIdx < pattern.staticValues.size()) {
                        id += pattern.staticValues[staticIdx++];
                    }
                    break;
                case TokenType::kDelimiter:
                    if (delimIdx < pattern.delimiters.size()) {
                        id += pattern.delimiters[delimIdx++];
                    }
                    break;
                case TokenType::kDynamicInt:
                    if (intIdx < intColumns.size() && i < intColumns[intIdx].size()) {
                        id += std::to_string(intColumns[intIdx][i]);
                    }
                    ++intIdx;
                    break;
                case TokenType::kDynamicString:
                    if (strIdx < strColumns.size() && i < strColumns[strIdx].size()) {
                        id += strColumns[strIdx][i];
                    }
                    ++strIdx;
                    break;
            }
        }

        result.push_back(std::move(id));
    }

    return result;
}

Result<std::vector<std::string>> IDCompressorImpl::decompressDiscard(std::uint32_t numIds) {
    std::vector<std::string> result;
    result.reserve(numIds);

    // Generate sequential IDs
    // Note: In real usage, the caller should use generateDiscardId with proper PE info
    for (std::uint32_t i = 1; i <= numIds; ++i) {
        result.push_back(config_.idPrefix + std::to_string(i));
    }

    return result;
}


// =============================================================================
// IDCompressorImpl - Zstd Compression
// =============================================================================

std::vector<std::uint8_t> IDCompressorImpl::compressWithZstd(
    std::span<const std::uint8_t> data) {

    // Handle empty input
    if (data.empty()) {
        return {};
    }

    // Estimate compressed size (use Zstd's bound function)
    std::size_t const cBuffSize = ZSTD_compressBound(data.size());
    std::vector<std::uint8_t> compressed(cBuffSize);

    // Compress with level 3 (balance between speed and compression ratio)
    std::size_t const cSize = ZSTD_compress(
        compressed.data(), compressed.size(),
        data.data(), data.size(),
        3  // compression level
    );

    // Check for errors
    if (ZSTD_isError(cSize)) {
        throw std::runtime_error(
            "Zstd compression failed: " + std::string(ZSTD_getErrorName(cSize))
        );
    }

    // Resize to actual compressed size
    compressed.resize(cSize);
    return compressed;
}

Result<std::vector<std::uint8_t>> IDCompressorImpl::decompressWithZstd(
    std::span<const std::uint8_t> data,
    std::size_t uncompressedSize) {

    // Handle empty input
    if (data.empty()) {
        if (uncompressedSize == 0) {
            return std::vector<std::uint8_t>{};
        }
        return makeError<std::vector<std::uint8_t>>(
            ErrorCode::kDecompressionError,
            "Empty compressed data but non-zero uncompressed size"
        );
    }

    // Get frame content size from compressed data
    unsigned long long const rSize = ZSTD_getFrameContentSize(
        data.data(), data.size()
    );

    if (rSize == ZSTD_CONTENTSIZE_ERROR) {
        return makeError<std::vector<std::uint8_t>>(
            ErrorCode::kDecompressionError,
            "Invalid Zstd frame"
        );
    }
    if (rSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        // Frame size not stored in header, use provided uncompressedSize
        if (uncompressedSize == 0) {
            return makeError<std::vector<std::uint8_t>>(
                ErrorCode::kDecompressionError,
                "Zstd content size unknown and uncompressedSize not provided"
            );
        }
    } else {
        // Verify size matches if both are known
        if (uncompressedSize != 0 && rSize != uncompressedSize) {
            return makeError<std::vector<std::uint8_t>>(
                ErrorCode::kDecompressionError,
                "Zstd frame size mismatch: expected " + std::to_string(uncompressedSize) +
                ", got " + std::to_string(rSize)
            );
        }
    }

    // Use frame size if available, otherwise use provided size
    std::size_t const decompSize = (rSize != ZSTD_CONTENTSIZE_UNKNOWN) ? rSize : uncompressedSize;
    std::vector<std::uint8_t> decompressed(decompSize);

    // Decompress
    std::size_t const dSize = ZSTD_decompress(
        decompressed.data(), decompressed.size(),
        data.data(), data.size()
    );

    // Check for errors
    if (ZSTD_isError(dSize)) {
        return makeError<std::vector<std::uint8_t>>(
            ErrorCode::kDecompressionError,
            "Zstd decompression failed: " + std::string(ZSTD_getErrorName(dSize))
        );
    }

    // Verify decompressed size matches expected
    if (dSize != decompSize) {
        return makeError<std::vector<std::uint8_t>>(
            ErrorCode::kDecompressionError,
            "Zstd decompressed size mismatch: expected " + std::to_string(decompSize) +
            ", got " + std::to_string(dSize)
        );
    }
    return decompressed;
}

VoidResult IDCompressorImpl::setConfig(IDCompressorConfig config) {
    auto result = config.validate();
    if (!result) {
        return result;
    }
    config_ = std::move(config);
    tokenizer_ = IDTokenizer(config_.delimiters);
    return {};
}

void IDCompressorImpl::reset() noexcept {
    // Nothing to reset in current implementation
}

// =============================================================================
// IDCompressor Public Interface
// =============================================================================

IDCompressor::IDCompressor(IDCompressorConfig config)
    : impl_(std::make_unique<IDCompressorImpl>(std::move(config))) {}

IDCompressor::~IDCompressor() = default;

IDCompressor::IDCompressor(IDCompressor&&) noexcept = default;
IDCompressor& IDCompressor::operator=(IDCompressor&&) noexcept = default;

Result<CompressedIDData> IDCompressor::compress(std::span<const std::string_view> ids) {
    return impl_->compress(ids);
}

Result<CompressedIDData> IDCompressor::compress(std::span<const std::string> ids) {
    std::vector<std::string_view> views;
    views.reserve(ids.size());
    for (const auto& id : ids) {
        views.emplace_back(id);
    }
    return impl_->compress(views);
}

Result<std::vector<std::string>> IDCompressor::decompress(
    std::span<const std::uint8_t> data,
    std::uint32_t numIds) {
    return impl_->decompress(data, numIds, impl_->config().idMode);
}

Result<std::vector<std::string>> IDCompressor::decompress(
    std::span<const std::uint8_t> data,
    std::uint32_t numIds,
    IDMode mode) {
    return impl_->decompress(data, numIds, mode);
}

const IDCompressorConfig& IDCompressor::config() const noexcept {
    return impl_->config();
}

VoidResult IDCompressor::setConfig(IDCompressorConfig config) {
    return impl_->setConfig(std::move(config));
}

void IDCompressor::reset() noexcept {
    impl_->reset();
}

std::optional<IDPattern> IDCompressor::detectPattern(
    std::span<const std::string_view> ids) const {
    return impl_->detectPattern(ids);
}

ParsedID IDCompressor::parseId(std::string_view id) const {
    return impl_->parseId(id);
}

}  // namespace fqc::algo
