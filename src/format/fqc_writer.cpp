// =============================================================================
// fq-compressor - FQC Archive Writer Implementation
// =============================================================================
// Implementation of the FQCWriter class for writing .fqc archives.
//
// Key features:
// - Atomic write using temporary file + rename strategy
// - Signal handling for cleanup on SIGINT/SIGTERM
// - xxHash64 checksum calculation for blocks and global file
// - Block index construction and writing
//
// Requirements: 2.1, 5.1, 5.2, 8.1, 8.2
// =============================================================================

#include "fqc/format/fqc_writer.h"

#include <algorithm>
#include <bit>
#include <csignal>
#include <cstring>
#include <mutex>
#include <set>

#include <xxhash.h>

#include "fqc/common/logger.h"

namespace fqc::format {

// =============================================================================
// Signal Handler Management
// =============================================================================

namespace {

/// @brief Global mutex for signal handler state.
std::mutex gSignalMutex;

/// @brief Set of registered writers for cleanup.
std::set<FQCWriter*> gRegisteredWriters;

/// @brief Whether signal handlers have been installed.
std::atomic<bool> gSignalHandlersInstalled{false};

/// @brief Previous SIGINT handler.
void (*gPreviousSigintHandler)(int) = nullptr;

/// @brief Previous SIGTERM handler.
void (*gPreviousSigtermHandler)(int) = nullptr;

/// @brief Signal handler function.
/// @param signum Signal number.
void signalHandler(int signum) {
    // Abort all registered writers
    {
        std::lock_guard<std::mutex> lock(gSignalMutex);
        for (auto* writer : gRegisteredWriters) {
            if (writer != nullptr) {
                writer->abort();
            }
        }
        gRegisteredWriters.clear();
    }

    // Call previous handler or use default behavior
    if (signum == SIGINT && gPreviousSigintHandler != nullptr &&
        gPreviousSigintHandler != SIG_DFL && gPreviousSigintHandler != SIG_IGN) {
        gPreviousSigintHandler(signum);
    } else if (signum == SIGTERM && gPreviousSigtermHandler != nullptr &&
               gPreviousSigtermHandler != SIG_DFL && gPreviousSigtermHandler != SIG_IGN) {
        gPreviousSigtermHandler(signum);
    } else {
        // Re-raise signal with default handler
        std::signal(signum, SIG_DFL);
        std::raise(signum);
    }
}

}  // namespace

void registerWriterForCleanup(FQCWriter* writer) {
    std::lock_guard<std::mutex> lock(gSignalMutex);
    gRegisteredWriters.insert(writer);
}

void unregisterWriterForCleanup(FQCWriter* writer) {
    std::lock_guard<std::mutex> lock(gSignalMutex);
    gRegisteredWriters.erase(writer);
}

void installSignalHandlers() {
    // Use compare_exchange to ensure only one thread installs handlers
    bool expected = false;
    if (!gSignalHandlersInstalled.compare_exchange_strong(expected, true)) {
        return;  // Already installed
    }

    std::lock_guard<std::mutex> lock(gSignalMutex);

    // Install SIGINT handler
    gPreviousSigintHandler = std::signal(SIGINT, signalHandler);
    if (gPreviousSigintHandler == SIG_ERR) {
        FQC_LOG_WARNING("Failed to install SIGINT handler");
        gPreviousSigintHandler = nullptr;
    }

    // Install SIGTERM handler
    gPreviousSigtermHandler = std::signal(SIGTERM, signalHandler);
    if (gPreviousSigtermHandler == SIG_ERR) {
        FQC_LOG_WARNING("Failed to install SIGTERM handler");
        gPreviousSigtermHandler = nullptr;
    }

    FQC_LOG_DEBUG("Signal handlers installed for SIGINT and SIGTERM");
}

// =============================================================================
// xxHash64 Utility Functions
// =============================================================================

std::uint64_t calculateXxHash64(const void* data, std::size_t size, std::uint64_t seed) {
    return XXH64(data, size, seed);
}

std::uint64_t calculateXxHash64(const std::vector<std::span<const std::uint8_t>>& segments,
                                 std::uint64_t seed) {
    XXH64_state_t* state = XXH64_createState();
    if (state == nullptr) {
        throw IOError("Failed to create xxHash64 state");
    }

    XXH64_reset(state, seed);

    for (const auto& segment : segments) {
        if (!segment.empty()) {
            XXH64_update(state, segment.data(), segment.size());
        }
    }

    std::uint64_t hash = XXH64_digest(state);
    XXH64_freeState(state);

    return hash;
}

std::uint64_t calculateBlockChecksum(std::span<const std::uint8_t> idsData,
                                      std::span<const std::uint8_t> seqData,
                                      std::span<const std::uint8_t> qualData,
                                      std::span<const std::uint8_t> auxData) {
    std::vector<std::span<const std::uint8_t>> segments = {idsData, seqData, qualData, auxData};
    return calculateXxHash64(segments, 0);
}

// =============================================================================
// FQCWriter Implementation
// =============================================================================

FQCWriter::FQCWriter(std::filesystem::path outputPath)
    : outputPath_(std::move(outputPath)), tempPath_(outputPath_.string() + ".tmp") {
    // Install signal handlers (idempotent)
    installSignalHandlers();

    // Create xxHash64 state
    xxhashState_ = XXH64_createState();
    if (xxhashState_ == nullptr) {
        throw IOError("Failed to create xxHash64 state");
    }
    XXH64_reset(static_cast<XXH64_state_t*>(xxhashState_), 0);

    // Open temporary file for writing
    stream_.open(tempPath_, std::ios::binary | std::ios::trunc);
    if (!stream_.is_open()) {
        XXH64_freeState(static_cast<XXH64_state_t*>(xxhashState_));
        xxhashState_ = nullptr;
        throw IOError("Failed to create temporary file: " + tempPath_.string(),
                      ErrorContext(tempPath_.string()));
    }

    // Register for signal-based cleanup
    registerWriterForCleanup(this);

    FQC_LOG_DEBUG("FQCWriter created: output={}, temp={}", outputPath_.string(), tempPath_.string());
}

FQCWriter::~FQCWriter() {
    // Unregister from signal handler
    unregisterWriterForCleanup(this);

    // Clean up if not finalized
    if (!finalized_ && !aborted_) {
        abort();
    }

    // Free xxHash state
    if (xxhashState_ != nullptr) {
        XXH64_freeState(static_cast<XXH64_state_t*>(xxhashState_));
        xxhashState_ = nullptr;
    }
}

void FQCWriter::writeGlobalHeader(const GlobalHeader& header,
                                   std::string_view originalFilename,
                                   std::uint64_t timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (headerWritten_) {
        throw FormatError("Global header already written");
    }

    if (finalized_ || aborted_) {
        throw FormatError("Writer is finalized or aborted");
    }

    // Validate header
    if (!header.isValid()) {
        throw FormatError("Invalid global header");
    }

    // Calculate actual header size
    std::uint32_t actualHeaderSize = static_cast<std::uint32_t>(
        GlobalHeader::kMinSize + originalFilename.size());

    // Write magic header (8 bytes)
    writeBytesWithChecksum(kMagicBytes.data(), kMagicBytes.size());

    // Write version (1 byte)
    writeBytesWithChecksum(&kCurrentVersion, sizeof(kCurrentVersion));

    // Write global header fields
    writeLEWithChecksum(actualHeaderSize);
    writeLEWithChecksum(header.flags);
    writeLEWithChecksum(header.compressionAlgo);
    writeLEWithChecksum(header.checksumType);
    writeLEWithChecksum(header.reserved);
    writeLEWithChecksum(header.totalReadCount);

    // Write filename length and filename
    auto filenameLen = static_cast<std::uint16_t>(originalFilename.size());
    writeLEWithChecksum(filenameLen);

    if (!originalFilename.empty()) {
        writeBytesWithChecksum(originalFilename.data(), originalFilename.size());
    }

    // Write timestamp
    writeLEWithChecksum(timestamp);

    headerWritten_ = true;
    totalReadCount_ = header.totalReadCount;

    FQC_LOG_DEBUG("Global header written: totalReads={}, filename={}",
              header.totalReadCount, originalFilename);
}

void FQCWriter::writeBlock(BlockHeader header, const BlockPayload& payload) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!headerWritten_) {
        throw FormatError("Global header must be written before blocks");
    }

    if (finalized_ || aborted_) {
        throw FormatError("Writer is finalized or aborted");
    }

    // Update header with stream offsets and sizes
    header.offsetIds = 0;
    header.sizeIds = payload.idsData.size();

    header.offsetSeq = header.sizeIds;
    header.sizeSeq = payload.seqData.size();

    header.offsetQual = header.offsetSeq + header.sizeSeq;
    header.sizeQual = payload.qualData.size();

    header.offsetAux = header.offsetQual + header.sizeQual;
    header.sizeAux = payload.auxData.size();

    header.compressedSize = payload.totalSize();

    // Record block position for index
    auto blockOffset = static_cast<std::uint64_t>(stream_.tellp());

    // Write block header
    writeLEWithChecksum(header.headerSize);
    writeLEWithChecksum(header.blockId);
    writeLEWithChecksum(header.checksumType);
    writeLEWithChecksum(header.codecIds);
    writeLEWithChecksum(header.codecSeq);
    writeLEWithChecksum(header.codecQual);
    writeLEWithChecksum(header.codecAux);
    writeLEWithChecksum(header.reserved1);
    writeLEWithChecksum(header.reserved2);
    writeLEWithChecksum(header.blockXxhash64);
    writeLEWithChecksum(header.uncompressedCount);
    writeLEWithChecksum(header.uniformReadLength);
    writeLEWithChecksum(header.compressedSize);
    writeLEWithChecksum(header.offsetIds);
    writeLEWithChecksum(header.offsetSeq);
    writeLEWithChecksum(header.offsetQual);
    writeLEWithChecksum(header.offsetAux);
    writeLEWithChecksum(header.sizeIds);
    writeLEWithChecksum(header.sizeSeq);
    writeLEWithChecksum(header.sizeQual);
    writeLEWithChecksum(header.sizeAux);

    // Write payload streams
    if (!payload.idsData.empty()) {
        writeBytesWithChecksum(payload.idsData.data(), payload.idsData.size());
    }
    if (!payload.seqData.empty()) {
        writeBytesWithChecksum(payload.seqData.data(), payload.seqData.size());
    }
    if (!payload.qualData.empty()) {
        writeBytesWithChecksum(payload.qualData.data(), payload.qualData.size());
    }
    if (!payload.auxData.empty()) {
        writeBytesWithChecksum(payload.auxData.data(), payload.auxData.size());
    }

    // Add to index
    IndexEntry entry;
    entry.offset = blockOffset;
    entry.compressedSize = BlockHeader::kSize + header.compressedSize;
    entry.archiveIdStart = nextArchiveId_;
    entry.readCount = header.uncompressedCount;
    index_.push_back(entry);

    // Update next archive ID
    nextArchiveId_ += header.uncompressedCount;

    FQC_LOG_DEBUG("Block {} written: offset={}, reads={}, compressedSize={}",
              header.blockId, blockOffset, header.uncompressedCount, header.compressedSize);
}

void FQCWriter::writeBlock(const BlockHeader& header, std::span<const std::uint8_t> payloadData) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!headerWritten_) {
        throw FormatError("Global header must be written before blocks");
    }

    if (finalized_ || aborted_) {
        throw FormatError("Writer is finalized or aborted");
    }

    // Record block position for index
    auto blockOffset = static_cast<std::uint64_t>(stream_.tellp());

    // Write block header
    writeLEWithChecksum(header.headerSize);
    writeLEWithChecksum(header.blockId);
    writeLEWithChecksum(header.checksumType);
    writeLEWithChecksum(header.codecIds);
    writeLEWithChecksum(header.codecSeq);
    writeLEWithChecksum(header.codecQual);
    writeLEWithChecksum(header.codecAux);
    writeLEWithChecksum(header.reserved1);
    writeLEWithChecksum(header.reserved2);
    writeLEWithChecksum(header.blockXxhash64);
    writeLEWithChecksum(header.uncompressedCount);
    writeLEWithChecksum(header.uniformReadLength);
    writeLEWithChecksum(header.compressedSize);
    writeLEWithChecksum(header.offsetIds);
    writeLEWithChecksum(header.offsetSeq);
    writeLEWithChecksum(header.offsetQual);
    writeLEWithChecksum(header.offsetAux);
    writeLEWithChecksum(header.sizeIds);
    writeLEWithChecksum(header.sizeSeq);
    writeLEWithChecksum(header.sizeQual);
    writeLEWithChecksum(header.sizeAux);

    // Write raw payload
    if (!payloadData.empty()) {
        writeBytesWithChecksum(payloadData.data(), payloadData.size());
    }

    // Add to index
    IndexEntry entry;
    entry.offset = blockOffset;
    entry.compressedSize = BlockHeader::kSize + payloadData.size();
    entry.archiveIdStart = nextArchiveId_;
    entry.readCount = header.uncompressedCount;
    index_.push_back(entry);

    // Update next archive ID
    nextArchiveId_ += header.uncompressedCount;

    FQC_LOG_DEBUG("Block {} written (raw): offset={}, reads={}, payloadSize={}",
              header.blockId, blockOffset, header.uncompressedCount, payloadData.size());
}

void FQCWriter::writeReorderMap(const ReorderMap& mapHeader,
                                 std::span<const std::uint8_t> forwardMapData,
                                 std::span<const std::uint8_t> reverseMapData) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!headerWritten_) {
        throw FormatError("Global header must be written before reorder map");
    }

    if (finalized_ || aborted_) {
        throw FormatError("Writer is finalized or aborted");
    }

    if (reorderMapOffset_ != 0) {
        throw FormatError("Reorder map already written");
    }

    // Record reorder map position
    reorderMapOffset_ = static_cast<std::uint64_t>(stream_.tellp());

    // Write reorder map header
    writeLEWithChecksum(mapHeader.headerSize);
    writeLEWithChecksum(mapHeader.version);
    writeLEWithChecksum(mapHeader.totalReads);
    writeLEWithChecksum(mapHeader.forwardMapSize);
    writeLEWithChecksum(mapHeader.reverseMapSize);

    // Write forward map data
    if (!forwardMapData.empty()) {
        writeBytesWithChecksum(forwardMapData.data(), forwardMapData.size());
    }

    // Write reverse map data
    if (!reverseMapData.empty()) {
        writeBytesWithChecksum(reverseMapData.data(), reverseMapData.size());
    }

    FQC_LOG_DEBUG("Reorder map written: offset={}, totalReads={}, forwardSize={}, reverseSize={}",
              reorderMapOffset_, mapHeader.totalReads,
              forwardMapData.size(), reverseMapData.size());
}

void FQCWriter::finalize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (finalized_) {
        return;  // Already finalized
    }

    if (aborted_) {
        throw FormatError("Cannot finalize aborted writer");
    }

    if (!headerWritten_) {
        throw FormatError("Cannot finalize without writing global header");
    }

    // Write block index
    writeBlockIndex();

    // Write file footer
    writeFileFooter();

    // Flush and close stream
    stream_.flush();
    if (!stream_.good()) {
        throw IOError("Failed to flush output file", ErrorContext(tempPath_.string()));
    }
    stream_.close();

    // Atomic rename: temp -> final
    std::error_code ec;
    std::filesystem::rename(tempPath_, outputPath_, ec);
    if (ec) {
        throw IOError("Failed to rename temporary file to final output",
                      ec, ErrorContext(outputPath_.string()));
    }

    finalized_ = true;

    FQC_LOG_INFO("FQC archive finalized: {}, blocks={}, reads={}",
             outputPath_.string(), index_.size(), totalReadCount_);
}

void FQCWriter::abort() noexcept {
    // Use atomic flag to prevent double abort
    bool expected = false;
    if (!aborted_.compare_exchange_strong(expected, true)) {
        return;  // Already aborted
    }

    cleanupTempFile();

    FQC_LOG_DEBUG("FQCWriter aborted: {}", tempPath_.string());
}

std::uint64_t FQCWriter::currentPosition() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<std::uint64_t>(const_cast<std::ofstream&>(stream_).tellp());
}

std::uint64_t FQCWriter::currentGlobalChecksum() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (xxhashState_ == nullptr) {
        return 0;
    }
    // Create a copy of the state to get digest without affecting the running state
    XXH64_state_t* stateCopy = XXH64_createState();
    if (stateCopy == nullptr) {
        return 0;
    }
    XXH64_copyState(stateCopy, static_cast<const XXH64_state_t*>(xxhashState_));
    std::uint64_t hash = XXH64_digest(stateCopy);
    XXH64_freeState(stateCopy);
    return hash;
}

// =============================================================================
// Private Methods
// =============================================================================

void FQCWriter::writeBytes(const void* data, std::size_t size) {
    stream_.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    if (!stream_.good()) {
        throw IOError("Failed to write to file", ErrorContext(tempPath_.string()));
    }
}

void FQCWriter::writeBytesWithChecksum(const void* data, std::size_t size) {
    writeBytes(data, size);
    XXH64_update(static_cast<XXH64_state_t*>(xxhashState_), data, size);
}

template <typename T>
void FQCWriter::writeLE(T value) {
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

    // Convert to little-endian if necessary
    if constexpr (std::endian::native == std::endian::big) {
        if constexpr (sizeof(T) == 2) {
            value = static_cast<T>(__builtin_bswap16(static_cast<std::uint16_t>(value)));
        } else if constexpr (sizeof(T) == 4) {
            value = static_cast<T>(__builtin_bswap32(static_cast<std::uint32_t>(value)));
        } else if constexpr (sizeof(T) == 8) {
            value = static_cast<T>(__builtin_bswap64(static_cast<std::uint64_t>(value)));
        }
    }

    writeBytes(&value, sizeof(T));
}

template <typename T>
void FQCWriter::writeLEWithChecksum(T value) {
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

    // Convert to little-endian if necessary
    if constexpr (std::endian::native == std::endian::big) {
        if constexpr (sizeof(T) == 2) {
            value = static_cast<T>(__builtin_bswap16(static_cast<std::uint16_t>(value)));
        } else if constexpr (sizeof(T) == 4) {
            value = static_cast<T>(__builtin_bswap32(static_cast<std::uint32_t>(value)));
        } else if constexpr (sizeof(T) == 8) {
            value = static_cast<T>(__builtin_bswap64(static_cast<std::uint64_t>(value)));
        }
    }

    writeBytesWithChecksum(&value, sizeof(T));
}

void FQCWriter::writeBlockIndex() {
    // Record index offset
    auto indexOffset = static_cast<std::uint64_t>(stream_.tellp());

    // Write block index header
    BlockIndex indexHeader;
    indexHeader.headerSize = BlockIndex::kHeaderSize;
    indexHeader.entrySize = IndexEntry::kSize;
    indexHeader.numBlocks = index_.size();

    writeLEWithChecksum(indexHeader.headerSize);
    writeLEWithChecksum(indexHeader.entrySize);
    writeLEWithChecksum(indexHeader.numBlocks);

    // Write index entries
    for (const auto& entry : index_) {
        writeLEWithChecksum(entry.offset);
        writeLEWithChecksum(entry.compressedSize);
        writeLEWithChecksum(entry.archiveIdStart);
        writeLEWithChecksum(entry.readCount);
    }

    FQC_LOG_DEBUG("Block index written: offset={}, numBlocks={}", indexOffset, index_.size());
}

void FQCWriter::writeFileFooter() {
    // Get index offset (already recorded during writeBlockIndex)
    // We need to calculate it from current position minus index size
    auto currentPos = static_cast<std::uint64_t>(stream_.tellp());
    auto indexSize = BlockIndex::kHeaderSize + (index_.size() * IndexEntry::kSize);
    auto indexOffset = currentPos - indexSize;

    // Get final global checksum (before writing footer)
    auto globalChecksum = XXH64_digest(static_cast<XXH64_state_t*>(xxhashState_));

    // Write footer (NOT included in checksum)
    FileFooter footer;
    footer.indexOffset = indexOffset;
    footer.reorderMapOffset = reorderMapOffset_;
    footer.globalChecksum = globalChecksum;
    footer.magicEnd = kMagicEnd;

    writeLE(footer.indexOffset);
    writeLE(footer.reorderMapOffset);
    writeLE(footer.globalChecksum);
    writeBytes(footer.magicEnd.data(), footer.magicEnd.size());

    FQC_LOG_DEBUG("File footer written: indexOffset={}, reorderMapOffset={}, checksum={:016x}",
              footer.indexOffset, footer.reorderMapOffset, footer.globalChecksum);
}

void FQCWriter::cleanupTempFile() noexcept {
    // Close stream if open
    if (stream_.is_open()) {
        stream_.close();
    }

    // Remove temporary file
    std::error_code ec;
    if (std::filesystem::exists(tempPath_, ec)) {
        std::filesystem::remove(tempPath_, ec);
        if (ec) {
            // Log but don't throw - this is called from destructor/signal handler
            FQC_LOG_WARNING("Failed to remove temporary file: {}", tempPath_.string());
        }
    }
}

}  // namespace fqc::format
