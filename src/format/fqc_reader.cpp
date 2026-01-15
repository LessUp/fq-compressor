// =============================================================================
// fq-compressor - FQC Archive Reader Implementation
// =============================================================================
// Implementation of the FQCReader class for reading .fqc archives.
//
// Key features:
// - Random access via Block Index
// - Checksum verification (optional)
// - Header-only mode support
// - Selective stream decoding
//
// Requirements: 2.1, 2.2, 2.3, 5.1, 5.2
// =============================================================================

#include "fqc/format/fqc_reader.h"

#include <algorithm>
#include <bit>
#include <cstring>

#include <xxhash.h>

#include "fqc/common/logger.h"
#include "fqc/format/fqc_writer.h"  // For calculateXxHash64

namespace fqc::format {

// =============================================================================
// FQCReader Implementation
// =============================================================================

FQCReader::FQCReader(std::filesystem::path archivePath)
    : archivePath_(std::move(archivePath)) {}

FQCReader::~FQCReader() {
    close();
}

FQCReader::FQCReader(FQCReader&& other) noexcept
    : archivePath_(std::move(other.archivePath_)),
      stream_(std::move(other.stream_)),
      isOpen_(other.isOpen_),
      version_(other.version_),
      globalHeader_(other.globalHeader_),
      originalFilename_(std::move(other.originalFilename_)),
      timestamp_(other.timestamp_),
      footer_(other.footer_),
      blockIndex_(std::move(other.blockIndex_)),
      reorderMap_(std::move(other.reorderMap_)),
      fileSize_(other.fileSize_) {
    other.isOpen_ = false;
}

FQCReader& FQCReader::operator=(FQCReader&& other) noexcept {
    if (this != &other) {
        close();
        archivePath_ = std::move(other.archivePath_);
        stream_ = std::move(other.stream_);
        isOpen_ = other.isOpen_;
        version_ = other.version_;
        globalHeader_ = other.globalHeader_;
        originalFilename_ = std::move(other.originalFilename_);
        timestamp_ = other.timestamp_;
        footer_ = other.footer_;
        blockIndex_ = std::move(other.blockIndex_);
        reorderMap_ = std::move(other.reorderMap_);
        fileSize_ = other.fileSize_;
        other.isOpen_ = false;
    }
    return *this;
}

void FQCReader::open() {
    if (isOpen_) {
        return;
    }

    // Open file
    stream_.open(archivePath_, std::ios::binary);
    if (!stream_.is_open()) {
        throw IOError("Failed to open archive file: " + archivePath_.string(),
                      ErrorContext(archivePath_.string()));
    }

    // Get file size
    stream_.seekg(0, std::ios::end);
    fileSize_ = static_cast<std::uint64_t>(stream_.tellg());
    stream_.seekg(0, std::ios::beg);

    // Validate minimum file size
    if (fileSize_ < kMagicHeaderSize + GlobalHeader::kMinSize + FileFooter::kSize) {
        throw FormatError("File too small to be a valid FQC archive",
                          ErrorContext(archivePath_.string()));
    }

    // Read and validate components
    readMagicHeader();
    readGlobalHeader();
    readFileFooter();
    readBlockIndex();

    isOpen_ = true;

    FQC_LOG_DEBUG("FQCReader opened: {}, version={}.{}, blocks={}, reads={}",
              archivePath_.string(),
              decodeMajorVersion(version_),
              decodeMinorVersion(version_),
              blockIndex_.size(),
              globalHeader_.totalReadCount);
}

void FQCReader::close() noexcept {
    if (stream_.is_open()) {
        stream_.close();
    }
    isOpen_ = false;
    blockIndex_.clear();
    reorderMap_.reset();
}

const GlobalHeader& FQCReader::globalHeader() const {
    if (!isOpen_) {
        throw FormatError("Archive is not open");
    }
    return globalHeader_;
}

const FileFooter& FQCReader::footer() const {
    if (!isOpen_) {
        throw FormatError("Archive is not open");
    }
    return footer_;
}

const std::vector<IndexEntry>& FQCReader::blockIndex() const {
    if (!isOpen_) {
        throw FormatError("Archive is not open");
    }
    return blockIndex_;
}

std::uint64_t FQCReader::totalReadCount() const noexcept {
    return globalHeader_.totalReadCount;
}

BlockData FQCReader::readBlock(BlockId blockId, StreamSelection selection) {
    if (!isOpen_) {
        throw FormatError("Archive is not open");
    }

    if (blockId >= blockIndex_.size()) {
        throw FormatError("Invalid block ID: " + std::to_string(blockId),
                          ErrorContext(archivePath_.string()).withBlock(blockId));
    }

    BlockData result;

    // Seek to block
    seekToBlock(blockId);

    // Read block header
    result.header = readBlockHeader(blockId);

    // Calculate payload start position
    auto payloadStart = blockIndex_[blockId].offset + BlockHeader::kSize;

    // Read selected streams
    if (hasStream(selection, StreamSelection::kIds) && result.header.sizeIds > 0) {
        seekTo(payloadStart + result.header.offsetIds);
        result.idsData.resize(result.header.sizeIds);
        readBytes(result.idsData.data(), result.header.sizeIds);
    }

    if (hasStream(selection, StreamSelection::kSequence) && result.header.sizeSeq > 0) {
        seekTo(payloadStart + result.header.offsetSeq);
        result.seqData.resize(result.header.sizeSeq);
        readBytes(result.seqData.data(), result.header.sizeSeq);
    }

    if (hasStream(selection, StreamSelection::kQuality) && result.header.sizeQual > 0) {
        seekTo(payloadStart + result.header.offsetQual);
        result.qualData.resize(result.header.sizeQual);
        readBytes(result.qualData.data(), result.header.sizeQual);
    }

    if (hasStream(selection, StreamSelection::kAux) && result.header.sizeAux > 0) {
        seekTo(payloadStart + result.header.offsetAux);
        result.auxData.resize(result.header.sizeAux);
        readBytes(result.auxData.data(), result.header.sizeAux);
    }

    return result;
}

BlockHeader FQCReader::readBlockHeader(BlockId blockId) {
    if (!isOpen_) {
        throw FormatError("Archive is not open");
    }

    if (blockId >= blockIndex_.size()) {
        throw FormatError("Invalid block ID: " + std::to_string(blockId));
    }

    // Seek to block start
    seekTo(blockIndex_[blockId].offset);

    // Read header fields
    BlockHeader header;
    header.headerSize = readLE<std::uint32_t>();
    header.blockId = readLE<std::uint32_t>();
    header.checksumType = readLE<std::uint8_t>();
    header.codecIds = readLE<std::uint8_t>();
    header.codecSeq = readLE<std::uint8_t>();
    header.codecQual = readLE<std::uint8_t>();
    header.codecAux = readLE<std::uint8_t>();
    header.reserved1 = readLE<std::uint8_t>();
    header.reserved2 = readLE<std::uint16_t>();
    header.blockXxhash64 = readLE<std::uint64_t>();
    header.uncompressedCount = readLE<std::uint32_t>();
    header.uniformReadLength = readLE<std::uint32_t>();
    header.compressedSize = readLE<std::uint64_t>();
    header.offsetIds = readLE<std::uint64_t>();
    header.offsetSeq = readLE<std::uint64_t>();
    header.offsetQual = readLE<std::uint64_t>();
    header.offsetAux = readLE<std::uint64_t>();
    header.sizeIds = readLE<std::uint64_t>();
    header.sizeSeq = readLE<std::uint64_t>();
    header.sizeQual = readLE<std::uint64_t>();
    header.sizeAux = readLE<std::uint64_t>();

    // Validate header
    if (!header.isValid()) {
        throw FormatError("Invalid block header",
                          ErrorContext(archivePath_.string()).withBlock(blockId));
    }

    return header;
}

void FQCReader::seekToBlock(BlockId blockId) {
    if (!isOpen_) {
        throw FormatError("Archive is not open");
    }

    if (blockId >= blockIndex_.size()) {
        throw FormatError("Invalid block ID: " + std::to_string(blockId));
    }

    seekTo(blockIndex_[blockId].offset);
}

BlockId FQCReader::findBlockForRead(ReadId archiveId) const noexcept {
    if (blockIndex_.empty() || archiveId == 0) {
        return kInvalidBlockId;
    }

    // Binary search for the block containing the read
    auto it = std::upper_bound(
        blockIndex_.begin(), blockIndex_.end(), archiveId,
        [](ReadId id, const IndexEntry& entry) {
            return id < entry.archiveIdStart;
        });

    if (it == blockIndex_.begin()) {
        // archiveId is before the first block
        return kInvalidBlockId;
    }

    --it;  // Move to the block that might contain the read

    // Check if the read is actually in this block
    if (archiveId >= it->archiveIdStart && archiveId < it->archiveIdStart + it->readCount) {
        return static_cast<BlockId>(std::distance(blockIndex_.begin(), it));
    }

    return kInvalidBlockId;
}

std::optional<IndexEntry> FQCReader::getIndexEntry(BlockId blockId) const noexcept {
    if (blockId >= blockIndex_.size()) {
        return std::nullopt;
    }
    return blockIndex_[blockId];
}

std::pair<ReadId, ReadId> FQCReader::getBlockReadRange(BlockId blockId) const noexcept {
    if (blockId >= blockIndex_.size()) {
        return {0, 0};
    }
    const auto& entry = blockIndex_[blockId];
    return {entry.archiveIdStart, entry.archiveIdStart + entry.readCount};
}

bool FQCReader::hasReorderMap() const noexcept {
    return isOpen_ && footer_.hasReorderMap();
}

void FQCReader::loadReorderMap() {
    if (!isOpen_) {
        throw FormatError("Archive is not open");
    }

    if (!hasReorderMap()) {
        throw FormatError("Archive does not contain a reorder map");
    }

    // Seek to reorder map
    seekTo(footer_.reorderMapOffset);

    // Read header
    ReorderMapData mapData;
    mapData.header.headerSize = readLE<std::uint32_t>();
    mapData.header.version = readLE<std::uint32_t>();
    mapData.header.totalReads = readLE<std::uint64_t>();
    mapData.header.forwardMapSize = readLE<std::uint64_t>();
    mapData.header.reverseMapSize = readLE<std::uint64_t>();

    // Validate header
    if (!mapData.header.isValid()) {
        throw FormatError("Invalid reorder map header");
    }

    // Read compressed map data
    std::vector<std::uint8_t> forwardCompressed(mapData.header.forwardMapSize);
    std::vector<std::uint8_t> reverseCompressed(mapData.header.reverseMapSize);

    if (mapData.header.forwardMapSize > 0) {
        readBytes(forwardCompressed.data(), mapData.header.forwardMapSize);
    }
    if (mapData.header.reverseMapSize > 0) {
        readBytes(reverseCompressed.data(), mapData.header.reverseMapSize);
    }

    // TODO: Decompress maps (Delta + Varint decoding)
    // For now, store placeholder - actual decompression will be implemented
    // when the compression module is ready
    mapData.forwardMap.resize(mapData.header.totalReads);
    mapData.reverseMap.resize(mapData.header.totalReads);

    // Placeholder: identity mapping
    for (std::uint64_t i = 0; i < mapData.header.totalReads; ++i) {
        mapData.forwardMap[i] = i + 1;  // 1-based
        mapData.reverseMap[i] = i + 1;
    }

    reorderMap_ = std::move(mapData);

    FQC_LOG_DEBUG("Reorder map loaded: totalReads={}", reorderMap_->header.totalReads);
}

ReadId FQCReader::lookupArchiveId(ReadId originalId) const noexcept {
    if (!reorderMap_.has_value()) {
        return kInvalidReadId;
    }
    return reorderMap_->lookupArchiveId(originalId);
}

ReadId FQCReader::lookupOriginalId(ReadId archiveId) const noexcept {
    if (!reorderMap_.has_value()) {
        return kInvalidReadId;
    }
    return reorderMap_->lookupOriginalId(archiveId);
}

bool FQCReader::verifyGlobalChecksum() {
    if (!isOpen_) {
        throw FormatError("Archive is not open");
    }

    // Calculate checksum of entire file except footer
    seekTo(0);

    XXH64_state_t* state = XXH64_createState();
    if (state == nullptr) {
        throw IOError("Failed to create xxHash64 state");
    }
    XXH64_reset(state, 0);

    // Read and hash in chunks
    constexpr std::size_t kChunkSize = 64 * 1024;  // 64KB chunks
    std::vector<std::uint8_t> buffer(kChunkSize);

    std::uint64_t bytesToHash = footer_.indexOffset + 
        BlockIndex::kHeaderSize + (blockIndex_.size() * IndexEntry::kSize);

    std::uint64_t bytesHashed = 0;
    while (bytesHashed < bytesToHash) {
        std::size_t toRead = std::min(kChunkSize, 
            static_cast<std::size_t>(bytesToHash - bytesHashed));
        readBytes(buffer.data(), toRead);
        XXH64_update(state, buffer.data(), toRead);
        bytesHashed += toRead;
    }

    std::uint64_t calculatedChecksum = XXH64_digest(state);
    XXH64_freeState(state);

    bool matches = (calculatedChecksum == footer_.globalChecksum);

    if (!matches) {
        FQC_LOG_WARNING("Global checksum mismatch: expected {:016x}, got {:016x}",
                    footer_.globalChecksum, calculatedChecksum);
    }

    return matches;
}

bool FQCReader::verifyBlockChecksum(BlockId blockId,
                                     std::span<const std::uint8_t> idsData,
                                     std::span<const std::uint8_t> seqData,
                                     std::span<const std::uint8_t> qualData,
                                     std::span<const std::uint8_t> auxData) {
    if (!isOpen_) {
        throw FormatError("Archive is not open");
    }

    if (blockId >= blockIndex_.size()) {
        throw FormatError("Invalid block ID: " + std::to_string(blockId));
    }

    // Read block header to get expected checksum
    auto header = readBlockHeader(blockId);

    // Calculate checksum of uncompressed data
    std::uint64_t calculatedChecksum = calculateBlockChecksum(idsData, seqData, qualData, auxData);

    bool matches = (calculatedChecksum == header.blockXxhash64);

    if (!matches) {
        FQC_LOG_WARNING("Block {} checksum mismatch: expected {:016x}, got {:016x}",
                    blockId, header.blockXxhash64, calculatedChecksum);
    }

    return matches;
}

bool FQCReader::verifyQuick() {
    if (!isOpen_) {
        return false;
    }

    // Check footer magic
    return footer_.isValid();
}

// =============================================================================
// Private Methods
// =============================================================================

void FQCReader::readMagicHeader() {
    seekTo(0);

    // Read magic bytes
    std::array<std::uint8_t, 8> magic{};
    readBytes(magic.data(), magic.size());

    if (!validateMagic(magic)) {
        throw FormatError("Invalid magic header - not an FQC archive",
                          ErrorContext(archivePath_.string()));
    }

    // Read version
    version_ = readLE<std::uint8_t>();

    // Check version compatibility
    if (!isVersionCompatible(version_)) {
        throw FormatError(
            "Incompatible format version: " + 
            std::to_string(decodeMajorVersion(version_)) + "." +
            std::to_string(decodeMinorVersion(version_)),
            ErrorContext(archivePath_.string()));
    }

    if (isVersionNewer(version_)) {
        FQC_LOG_WARNING("Archive version {}.{} is newer than reader version {}.{}",
                    decodeMajorVersion(version_), decodeMinorVersion(version_),
                    kFormatVersionMajor, kFormatVersionMinor);
    }
}

void FQCReader::readGlobalHeader() {
    // Position should be right after magic header
    globalHeader_.headerSize = readLE<std::uint32_t>();
    globalHeader_.flags = readLE<std::uint64_t>();
    globalHeader_.compressionAlgo = readLE<std::uint8_t>();
    globalHeader_.checksumType = readLE<std::uint8_t>();
    globalHeader_.reserved = readLE<std::uint16_t>();
    globalHeader_.totalReadCount = readLE<std::uint64_t>();
    globalHeader_.originalFilenameLen = readLE<std::uint16_t>();

    // Read filename
    if (globalHeader_.originalFilenameLen > 0) {
        originalFilename_.resize(globalHeader_.originalFilenameLen);
        readBytes(originalFilename_.data(), globalHeader_.originalFilenameLen);
    }

    // Read timestamp
    timestamp_ = readLE<std::uint64_t>();

    // Validate header
    if (!globalHeader_.isValid()) {
        throw FormatError("Invalid global header", ErrorContext(archivePath_.string()));
    }

    // Skip any unknown extension fields
    auto currentPos = currentPosition();
    auto expectedEnd = kMagicHeaderSize + globalHeader_.headerSize;
    if (currentPos < expectedEnd) {
        seekTo(expectedEnd);
    }
}

void FQCReader::readFileFooter() {
    // Seek to footer (last 32 bytes)
    seekTo(fileSize_ - FileFooter::kSize);

    footer_.indexOffset = readLE<std::uint64_t>();
    footer_.reorderMapOffset = readLE<std::uint64_t>();
    footer_.globalChecksum = readLE<std::uint64_t>();
    readBytes(footer_.magicEnd.data(), footer_.magicEnd.size());

    // Validate footer
    if (!footer_.isValid()) {
        throw FormatError("Invalid file footer - file may be corrupted",
                          ErrorContext(archivePath_.string()));
    }
}

void FQCReader::readBlockIndex() {
    // Seek to index
    seekTo(footer_.indexOffset);

    // Read index header
    BlockIndex indexHeader;
    indexHeader.headerSize = readLE<std::uint32_t>();
    indexHeader.entrySize = readLE<std::uint32_t>();
    indexHeader.numBlocks = readLE<std::uint64_t>();

    // Validate index header
    if (!indexHeader.isValid()) {
        throw FormatError("Invalid block index header", ErrorContext(archivePath_.string()));
    }

    // Read index entries
    blockIndex_.clear();
    blockIndex_.reserve(indexHeader.numBlocks);

    for (std::uint64_t i = 0; i < indexHeader.numBlocks; ++i) {
        IndexEntry entry;
        entry.offset = readLE<std::uint64_t>();
        entry.compressedSize = readLE<std::uint64_t>();
        entry.archiveIdStart = readLE<std::uint64_t>();
        entry.readCount = readLE<std::uint32_t>();

        blockIndex_.push_back(entry);

        // Skip any extension fields in entry
        if (indexHeader.entrySize > IndexEntry::kSize) {
            auto toSkip = indexHeader.entrySize - IndexEntry::kSize;
            seekTo(currentPosition() + toSkip);
        }
    }

    FQC_LOG_DEBUG("Block index loaded: {} blocks", blockIndex_.size());
}

void FQCReader::readBytes(void* buffer, std::size_t size) {
    stream_.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
    if (!stream_.good()) {
        throw IOError("Failed to read from file", ErrorContext(archivePath_.string()));
    }
}

template <typename T>
T FQCReader::readLE() {
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

    T value;
    readBytes(&value, sizeof(T));

    // Convert from little-endian if necessary
    if constexpr (std::endian::native == std::endian::big) {
        if constexpr (sizeof(T) == 2) {
            value = static_cast<T>(__builtin_bswap16(static_cast<std::uint16_t>(value)));
        } else if constexpr (sizeof(T) == 4) {
            value = static_cast<T>(__builtin_bswap32(static_cast<std::uint32_t>(value)));
        } else if constexpr (sizeof(T) == 8) {
            value = static_cast<T>(__builtin_bswap64(static_cast<std::uint64_t>(value)));
        }
    }

    return value;
}

void FQCReader::seekTo(std::uint64_t position) {
    stream_.seekg(static_cast<std::streamoff>(position), std::ios::beg);
    if (!stream_.good()) {
        throw IOError("Failed to seek in file", ErrorContext(archivePath_.string()));
    }
}

std::uint64_t FQCReader::currentPosition() const {
    return static_cast<std::uint64_t>(const_cast<std::ifstream&>(stream_).tellg());
}

}  // namespace fqc::format
