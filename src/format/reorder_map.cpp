// =============================================================================
// fq-compressor - Reorder Map Implementation
// =============================================================================
// Implementation of bidirectional mapping for read reordering support.
//
// Encoding Strategy:
// - Delta encoding: First value stored as-is, subsequent values as differences
// - Varint encoding: Variable-length integer encoding (like protobuf)
// - Zigzag encoding for signed deltas: (n << 1) ^ (n >> 63)
//
// Requirements: 2.1
// =============================================================================

#include "fqc/format/reorder_map.h"

#include <algorithm>
#include <cstring>
#include <istream>
#include <numeric>
#include <ostream>

namespace fqc::format {

// =============================================================================
// Varint Encoding/Decoding Implementation
// =============================================================================

std::size_t encodeVarint(std::uint64_t value, std::uint8_t* output) noexcept {
    std::size_t bytesWritten = 0;

    // Encode 7 bits at a time, MSB indicates continuation
    while (value >= 0x80) {
        output[bytesWritten++] = static_cast<std::uint8_t>((value & 0x7F) | 0x80);
        value >>= 7;
    }
    output[bytesWritten++] = static_cast<std::uint8_t>(value);

    return bytesWritten;
}

std::size_t decodeVarint(const std::uint8_t* input, std::size_t inputSize,
                          std::uint64_t& value) noexcept {
    value = 0;
    std::size_t shift = 0;
    std::size_t bytesRead = 0;

    while (bytesRead < inputSize && bytesRead < kMaxVarintBytes) {
        const std::uint8_t byte = input[bytesRead++];
        value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;

        if ((byte & 0x80) == 0) {
            return bytesRead;  // Success
        }

        shift += 7;
        if (shift >= 64) {
            return 0;  // Overflow error
        }
    }

    return 0;  // Incomplete varint or buffer exhausted
}

std::size_t encodeSignedVarint(std::int64_t value, std::uint8_t* output) noexcept {
    // Zigzag encoding: map signed to unsigned
    // Positive values: 0 -> 0, 1 -> 2, 2 -> 4, ...
    // Negative values: -1 -> 1, -2 -> 3, -3 -> 5, ...
    const auto zigzag = static_cast<std::uint64_t>((value << 1) ^ (value >> 63));
    return encodeVarint(zigzag, output);
}

std::size_t decodeSignedVarint(const std::uint8_t* input, std::size_t inputSize,
                                std::int64_t& value) noexcept {
    std::uint64_t zigzag = 0;
    const std::size_t bytesRead = decodeVarint(input, inputSize, zigzag);

    if (bytesRead == 0) {
        return 0;  // Error
    }

    // Decode zigzag: reverse the encoding
    // Cast to avoid sign conversion warning
    value = static_cast<std::int64_t>(zigzag >> 1) ^
            -static_cast<std::int64_t>(zigzag & 1);
    return bytesRead;
}

// =============================================================================
// Delta Encoding/Decoding Implementation
// =============================================================================

std::vector<std::uint8_t> deltaEncode(std::span<const ReadId> ids) {
    if (ids.empty()) {
        return {};
    }

    // Pre-allocate with estimated size (average ~2 bytes per value)
    std::vector<std::uint8_t> result;
    result.reserve(ids.size() * 2 + kMaxVarintBytes);

    std::uint8_t buffer[kMaxVarintBytes];

    // First value stored as-is (unsigned varint)
    std::size_t bytes = encodeVarint(ids[0], buffer);
    result.insert(result.end(), buffer, buffer + bytes);

    // Subsequent values stored as signed deltas
    for (std::size_t i = 1; i < ids.size(); ++i) {
        const auto delta = static_cast<std::int64_t>(ids[i]) - static_cast<std::int64_t>(ids[i - 1]);
        bytes = encodeSignedVarint(delta, buffer);
        result.insert(result.end(), buffer, buffer + bytes);
    }

    return result;
}

Result<std::vector<ReadId>> deltaDecode(std::span<const std::uint8_t> encoded,
                                         std::uint64_t count) {
    if (count == 0) {
        return std::vector<ReadId>{};
    }

    if (encoded.empty()) {
        return makeError<std::vector<ReadId>>(ErrorCode::kFormatError,
                                               "Empty encoded data for non-zero count");
    }

    std::vector<ReadId> result;
    result.reserve(count);

    const std::uint8_t* ptr = encoded.data();
    const std::uint8_t* end = ptr + encoded.size();

    // Decode first value (unsigned varint)
    std::uint64_t firstValue = 0;
    std::size_t bytesRead = decodeVarint(ptr, static_cast<std::size_t>(end - ptr), firstValue);
    if (bytesRead == 0) {
        return makeError<std::vector<ReadId>>(ErrorCode::kFormatError,
                                               "Failed to decode first varint");
    }
    ptr += bytesRead;
    result.push_back(firstValue);

    // Decode subsequent deltas
    std::int64_t currentValue = static_cast<std::int64_t>(firstValue);
    for (std::uint64_t i = 1; i < count; ++i) {
        if (ptr >= end) {
            return makeError<std::vector<ReadId>>(
                ErrorCode::kFormatError,
                "Unexpected end of encoded data at index " + std::to_string(i));
        }

        std::int64_t delta = 0;
        bytesRead = decodeSignedVarint(ptr, static_cast<std::size_t>(end - ptr), delta);
        if (bytesRead == 0) {
            return makeError<std::vector<ReadId>>(
                ErrorCode::kFormatError,
                "Failed to decode delta varint at index " + std::to_string(i));
        }
        ptr += bytesRead;

        currentValue += delta;
        if (currentValue < 0) {
            return makeError<std::vector<ReadId>>(
                ErrorCode::kFormatError,
                "Negative read ID after delta decode at index " + std::to_string(i));
        }
        result.push_back(static_cast<ReadId>(currentValue));
    }

    return result;
}

// =============================================================================
// ReorderMapData Implementation
// =============================================================================

ReorderMapData::ReorderMapData(std::vector<ReadId> forwardMap, std::vector<ReadId> reverseMap)
    : forwardMap_(std::move(forwardMap)), reverseMap_(std::move(reverseMap)) {}

ReorderMapData::ReorderMapData(std::span<const ReadId> forwardMap,
                               std::span<const ReadId> reverseMap)
    : forwardMap_(forwardMap.begin(), forwardMap.end()),
      reverseMap_(reverseMap.begin(), reverseMap.end()) {}

ReadId ReorderMapData::getArchiveId(ReadId originalId) const noexcept {
    if (originalId >= forwardMap_.size()) {
        return originalId;  // Return as-is if out of range
    }
    return forwardMap_[originalId];
}

ReadId ReorderMapData::getOriginalId(ReadId archiveId) const noexcept {
    if (archiveId >= reverseMap_.size()) {
        return archiveId;  // Return as-is if out of range
    }
    return reverseMap_[archiveId];
}

std::uint64_t ReorderMapData::totalReads() const noexcept {
    return forwardMap_.size();
}

bool ReorderMapData::empty() const noexcept {
    return forwardMap_.empty();
}

bool ReorderMapData::isValid() const noexcept {
    return forwardMap_.size() == reverseMap_.size();
}

std::vector<std::uint8_t> ReorderMapData::serialize() const {
    // Encode both maps
    auto forwardEncoded = deltaEncode(forwardMap_);
    auto reverseEncoded = deltaEncode(reverseMap_);

    // Build header
    ReorderMap header;
    header.headerSize = ReorderMap::kHeaderSize;
    header.version = kReorderMapVersion;
    header.totalReads = forwardMap_.size();
    header.forwardMapSize = forwardEncoded.size();
    header.reverseMapSize = reverseEncoded.size();

    // Calculate total size
    const std::size_t totalSize = header.headerSize + forwardEncoded.size() + reverseEncoded.size();
    std::vector<std::uint8_t> result;
    result.reserve(totalSize);

    // Write header (little-endian)
    auto writeU32 = [&result](std::uint32_t value) {
        result.push_back(static_cast<std::uint8_t>(value & 0xFF));
        result.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        result.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
        result.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    };

    auto writeU64 = [&result](std::uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            result.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF));
        }
    };

    writeU32(header.headerSize);
    writeU32(header.version);
    writeU64(header.totalReads);
    writeU64(header.forwardMapSize);
    writeU64(header.reverseMapSize);

    // Write encoded maps
    result.insert(result.end(), forwardEncoded.begin(), forwardEncoded.end());
    result.insert(result.end(), reverseEncoded.begin(), reverseEncoded.end());

    return result;
}

Result<ReorderMapData> ReorderMapData::deserialize(std::span<const std::uint8_t> data) {
    // Check minimum size for header
    if (data.size() < ReorderMap::kHeaderSize) {
        return makeError<ReorderMapData>(ErrorCode::kFormatError,
                                          "Data too small for reorder map header");
    }

    // Read header (little-endian)
    auto readU32 = [](const std::uint8_t* ptr) -> std::uint32_t {
        return static_cast<std::uint32_t>(ptr[0]) |
               (static_cast<std::uint32_t>(ptr[1]) << 8) |
               (static_cast<std::uint32_t>(ptr[2]) << 16) |
               (static_cast<std::uint32_t>(ptr[3]) << 24);
    };

    auto readU64 = [](const std::uint8_t* ptr) -> std::uint64_t {
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<std::uint64_t>(ptr[i]) << (i * 8);
        }
        return value;
    };

    const std::uint8_t* ptr = data.data();

    ReorderMap header;
    header.headerSize = readU32(ptr);
    ptr += 4;
    header.version = readU32(ptr);
    ptr += 4;
    header.totalReads = readU64(ptr);
    ptr += 8;
    header.forwardMapSize = readU64(ptr);
    ptr += 8;
    header.reverseMapSize = readU64(ptr);
    ptr += 8;

    // Validate header
    auto validationResult = validateReorderMapHeader(header);
    if (!validationResult) {
        return makeError<ReorderMapData>(validationResult.error());
    }

    // Check data size
    const std::size_t expectedSize = header.headerSize + header.forwardMapSize + header.reverseMapSize;
    if (data.size() < expectedSize) {
        return makeError<ReorderMapData>(
            ErrorCode::kFormatError,
            "Data size mismatch: expected " + std::to_string(expectedSize) +
                ", got " + std::to_string(data.size()));
    }

    // Skip any extra header bytes (forward compatibility)
    ptr = data.data() + header.headerSize;

    // Decode forward map
    std::span<const std::uint8_t> forwardData(ptr, header.forwardMapSize);
    auto forwardResult = deltaDecode(forwardData, header.totalReads);
    if (!forwardResult) {
        return makeError<ReorderMapData>(forwardResult.error().code(),
                                          "Failed to decode forward map: " +
                                              forwardResult.error().message());
    }
    ptr += header.forwardMapSize;

    // Decode reverse map
    std::span<const std::uint8_t> reverseData(ptr, header.reverseMapSize);
    auto reverseResult = deltaDecode(reverseData, header.totalReads);
    if (!reverseResult) {
        return makeError<ReorderMapData>(reverseResult.error().code(),
                                          "Failed to decode reverse map: " +
                                              reverseResult.error().message());
    }

    return ReorderMapData(std::move(*forwardResult), std::move(*reverseResult));
}

std::size_t ReorderMapData::estimateSerializedSize() const noexcept {
    // Header size + estimated compressed map sizes
    // Average ~2 bytes per read per map
    return ReorderMap::kHeaderSize + forwardMap_.size() * 2 + reverseMap_.size() * 2;
}

void ReorderMapData::appendChunk(const ReorderMapData& other, ReadId archiveIdOffset,
                                  ReadId originalIdOffset) {
    const std::size_t currentSize = forwardMap_.size();

    // Reserve space
    forwardMap_.reserve(currentSize + other.forwardMap_.size());
    reverseMap_.reserve(currentSize + other.reverseMap_.size());

    // Append forward map with archive ID offset
    for (ReadId archiveId : other.forwardMap_) {
        forwardMap_.push_back(archiveId + archiveIdOffset);
    }

    // Append reverse map with original ID offset
    for (ReadId originalId : other.reverseMap_) {
        reverseMap_.push_back(originalId + originalIdOffset);
    }
}

ReorderMapData ReorderMapData::combineChunks(std::span<const ReorderMapData> chunks,
                                              std::span<const std::uint64_t> chunkSizes) {
    if (chunks.empty()) {
        return ReorderMapData{};
    }

    // Calculate total size
    std::uint64_t totalReads = 0;
    for (const auto& chunk : chunks) {
        totalReads += chunk.totalReads();
    }

    // Pre-allocate
    std::vector<ReadId> combinedForward;
    std::vector<ReadId> combinedReverse;
    combinedForward.reserve(totalReads);
    combinedReverse.reserve(totalReads);

    // Combine chunks with offset accumulation
    ReadId archiveIdOffset = 0;
    ReadId originalIdOffset = 0;

    for (std::size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];

        // Append forward map with archive ID offset
        for (ReadId archiveId : chunk.forwardMap()) {
            combinedForward.push_back(archiveId + archiveIdOffset);
        }

        // Append reverse map with original ID offset
        for (ReadId originalId : chunk.reverseMap()) {
            combinedReverse.push_back(originalId + originalIdOffset);
        }

        // Update offsets for next chunk
        archiveIdOffset += chunk.totalReads();
        if (i < chunkSizes.size()) {
            originalIdOffset += chunkSizes[i];
        } else {
            originalIdOffset += chunk.totalReads();
        }
    }

    return ReorderMapData(std::move(combinedForward), std::move(combinedReverse));
}

ReorderMapData::CompressionStats ReorderMapData::getCompressionStats() const {
    auto forwardEncoded = deltaEncode(forwardMap_);
    auto reverseEncoded = deltaEncode(reverseMap_);

    CompressionStats stats;
    stats.totalReads = forwardMap_.size();
    stats.forwardMapCompressedSize = forwardEncoded.size();
    stats.reverseMapCompressedSize = reverseEncoded.size();
    stats.totalCompressedSize = ReorderMap::kHeaderSize + forwardEncoded.size() + reverseEncoded.size();

    if (stats.totalReads > 0) {
        stats.bytesPerRead = static_cast<double>(stats.totalCompressedSize) /
                             static_cast<double>(stats.totalReads);
    } else {
        stats.bytesPerRead = 0.0;
    }

    // Uncompressed size: 2 maps * 8 bytes per ReadId * totalReads
    const std::size_t uncompressedSize = 2 * sizeof(ReadId) * stats.totalReads;
    if (stats.totalCompressedSize > 0) {
        stats.compressionRatio = static_cast<double>(uncompressedSize) /
                                 static_cast<double>(stats.totalCompressedSize);
    } else {
        stats.compressionRatio = 1.0;
    }

    return stats;
}

// =============================================================================
// ReorderMapIO Implementation
// =============================================================================

VoidResult ReorderMapIO::write(std::ostream& stream, const ReorderMapData& mapData) {
    auto bytes = mapData.serialize();

    if (!stream.write(reinterpret_cast<const char*>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()))) {
        return makeVoidError(ErrorCode::kIOError, "Failed to write reorder map to stream");
    }

    return makeVoidSuccess();
}

Result<ReorderMapData> ReorderMapIO::read(std::istream& stream) {
    // Read header first
    std::array<std::uint8_t, ReorderMap::kHeaderSize> headerBytes{};
    if (!stream.read(reinterpret_cast<char*>(headerBytes.data()), ReorderMap::kHeaderSize)) {
        return makeError<ReorderMapData>(ErrorCode::kIOError,
                                          "Failed to read reorder map header");
    }

    // Parse header
    auto readU32 = [](const std::uint8_t* ptr) -> std::uint32_t {
        return static_cast<std::uint32_t>(ptr[0]) |
               (static_cast<std::uint32_t>(ptr[1]) << 8) |
               (static_cast<std::uint32_t>(ptr[2]) << 16) |
               (static_cast<std::uint32_t>(ptr[3]) << 24);
    };

    auto readU64 = [](const std::uint8_t* ptr) -> std::uint64_t {
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<std::uint64_t>(ptr[i]) << (i * 8);
        }
        return value;
    };

    const std::uint8_t* ptr = headerBytes.data();

    ReorderMap header;
    header.headerSize = readU32(ptr);
    ptr += 4;
    header.version = readU32(ptr);
    ptr += 4;
    header.totalReads = readU64(ptr);
    ptr += 8;
    header.forwardMapSize = readU64(ptr);
    ptr += 8;
    header.reverseMapSize = readU64(ptr);

    // Validate header
    auto validationResult = validateReorderMapHeader(header);
    if (!validationResult) {
        return makeError<ReorderMapData>(validationResult.error());
    }

    // Skip extra header bytes if present (forward compatibility)
    if (header.headerSize > ReorderMap::kHeaderSize) {
        stream.seekg(static_cast<std::streamoff>(header.headerSize - ReorderMap::kHeaderSize),
                     std::ios::cur);
        if (!stream) {
            return makeError<ReorderMapData>(ErrorCode::kIOError,
                                              "Failed to skip extra header bytes");
        }
    }

    // Read map data
    const std::size_t mapDataSize = header.forwardMapSize + header.reverseMapSize;
    std::vector<std::uint8_t> mapData(mapDataSize);
    if (!stream.read(reinterpret_cast<char*>(mapData.data()),
                     static_cast<std::streamsize>(mapDataSize))) {
        return makeError<ReorderMapData>(ErrorCode::kIOError,
                                          "Failed to read reorder map data");
    }

    // Decode forward map
    std::span<const std::uint8_t> forwardData(mapData.data(), header.forwardMapSize);
    auto forwardResult = deltaDecode(forwardData, header.totalReads);
    if (!forwardResult) {
        return makeError<ReorderMapData>(forwardResult.error().code(),
                                          "Failed to decode forward map: " +
                                              forwardResult.error().message());
    }

    // Decode reverse map
    std::span<const std::uint8_t> reverseData(mapData.data() + header.forwardMapSize,
                                               header.reverseMapSize);
    auto reverseResult = deltaDecode(reverseData, header.totalReads);
    if (!reverseResult) {
        return makeError<ReorderMapData>(reverseResult.error().code(),
                                          "Failed to decode reverse map: " +
                                              reverseResult.error().message());
    }

    return ReorderMapData(std::move(*forwardResult), std::move(*reverseResult));
}

Result<ReorderMap> ReorderMapIO::readHeader(std::istream& stream) {
    std::array<std::uint8_t, ReorderMap::kHeaderSize> headerBytes{};
    if (!stream.read(reinterpret_cast<char*>(headerBytes.data()), ReorderMap::kHeaderSize)) {
        return makeError<ReorderMap>(ErrorCode::kIOError,
                                      "Failed to read reorder map header");
    }

    auto readU32 = [](const std::uint8_t* ptr) -> std::uint32_t {
        return static_cast<std::uint32_t>(ptr[0]) |
               (static_cast<std::uint32_t>(ptr[1]) << 8) |
               (static_cast<std::uint32_t>(ptr[2]) << 16) |
               (static_cast<std::uint32_t>(ptr[3]) << 24);
    };

    auto readU64 = [](const std::uint8_t* ptr) -> std::uint64_t {
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<std::uint64_t>(ptr[i]) << (i * 8);
        }
        return value;
    };

    const std::uint8_t* ptr = headerBytes.data();

    ReorderMap header;
    header.headerSize = readU32(ptr);
    ptr += 4;
    header.version = readU32(ptr);
    ptr += 4;
    header.totalReads = readU64(ptr);
    ptr += 8;
    header.forwardMapSize = readU64(ptr);
    ptr += 8;
    header.reverseMapSize = readU64(ptr);

    auto validationResult = validateReorderMapHeader(header);
    if (!validationResult) {
        return makeError<ReorderMap>(validationResult.error());
    }

    return header;
}

std::vector<std::uint8_t> ReorderMapIO::toBytes(const ReorderMapData& mapData) {
    return mapData.serialize();
}

Result<ReorderMapData> ReorderMapIO::fromBytes(std::span<const std::uint8_t> data) {
    return ReorderMapData::deserialize(data);
}

// =============================================================================
// Validation Functions Implementation
// =============================================================================

VoidResult validateReorderMapHeader(const ReorderMap& header) {
    // Check header size
    if (header.headerSize < ReorderMap::kHeaderSize) {
        return makeVoidError(ErrorCode::kFormatError,
                             "Invalid reorder map header size: " +
                                 std::to_string(header.headerSize) +
                                 " (minimum: " + std::to_string(ReorderMap::kHeaderSize) + ")");
    }

    // Check version
    if (header.version > kReorderMapVersion) {
        return makeVoidError(ErrorCode::kFormatError,
                             "Unsupported reorder map version: " +
                                 std::to_string(header.version) +
                                 " (maximum supported: " + std::to_string(kReorderMapVersion) + ")");
    }

    // Check for reasonable map sizes (sanity check)
    // Each map should be at least 1 byte per read (varint minimum)
    // and at most ~10 bytes per read (varint maximum)
    if (header.totalReads > 0) {
        if (header.forwardMapSize == 0 || header.reverseMapSize == 0) {
            return makeVoidError(ErrorCode::kFormatError,
                                 "Empty map data for non-zero read count");
        }

        // Sanity check: map size should be reasonable
        const std::size_t maxMapSize = header.totalReads * kMaxVarintBytes;
        if (header.forwardMapSize > maxMapSize || header.reverseMapSize > maxMapSize) {
            return makeVoidError(ErrorCode::kFormatError,
                                 "Map size exceeds maximum expected size");
        }
    }

    return makeVoidSuccess();
}

VoidResult validateReorderMapData(const ReorderMapData& mapData) {
    if (!mapData.isValid()) {
        return makeVoidError(ErrorCode::kFormatError,
                             "Forward and reverse map sizes do not match");
    }

    // Verify map consistency
    return verifyMapConsistency(mapData.forwardMap(), mapData.reverseMap());
}

VoidResult verifyMapConsistency(std::span<const ReadId> forwardMap,
                                 std::span<const ReadId> reverseMap) {
    if (forwardMap.size() != reverseMap.size()) {
        return makeVoidError(ErrorCode::kFormatError,
                             "Forward and reverse map sizes do not match: " +
                                 std::to_string(forwardMap.size()) + " vs " +
                                 std::to_string(reverseMap.size()));
    }

    const std::size_t size = forwardMap.size();
    if (size == 0) {
        return makeVoidSuccess();  // Empty maps are valid
    }

    // Verify that forward and reverse maps are consistent inverses
    // For each original_id: reverse[forward[original_id]] == original_id
    for (std::size_t originalId = 0; originalId < size; ++originalId) {
        const ReadId archiveId = forwardMap[originalId];
        if (archiveId >= size) {
            return makeVoidError(ErrorCode::kFormatError,
                                 "Forward map contains out-of-range archive ID: " +
                                     std::to_string(archiveId) + " at original ID " +
                                     std::to_string(originalId));
        }

        const ReadId recoveredOriginalId = reverseMap[archiveId];
        if (recoveredOriginalId != originalId) {
            return makeVoidError(ErrorCode::kFormatError,
                                 "Map consistency check failed: reverse[forward[" +
                                     std::to_string(originalId) + "]] = " +
                                     std::to_string(recoveredOriginalId) +
                                     " (expected " + std::to_string(originalId) + ")");
        }
    }

    // Also verify the reverse direction
    // For each archive_id: forward[reverse[archive_id]] == archive_id
    for (std::size_t archiveId = 0; archiveId < size; ++archiveId) {
        const ReadId originalId = reverseMap[archiveId];
        if (originalId >= size) {
            return makeVoidError(ErrorCode::kFormatError,
                                 "Reverse map contains out-of-range original ID: " +
                                     std::to_string(originalId) + " at archive ID " +
                                     std::to_string(archiveId));
        }

        const ReadId recoveredArchiveId = forwardMap[originalId];
        if (recoveredArchiveId != archiveId) {
            return makeVoidError(ErrorCode::kFormatError,
                                 "Map consistency check failed: forward[reverse[" +
                                     std::to_string(archiveId) + "]] = " +
                                     std::to_string(recoveredArchiveId) +
                                     " (expected " + std::to_string(archiveId) + ")");
        }
    }

    return makeVoidSuccess();
}

}  // namespace fqc::format
