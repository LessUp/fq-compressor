// =============================================================================
// fq-compressor - Compression Request
// =============================================================================
// Normalized command-layer compression request.
// =============================================================================

#ifndef FQC_COMMANDS_COMPRESSION_REQUEST_H
#define FQC_COMMANDS_COMPRESSION_REQUEST_H

#include "fqc/common/types.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace fqc::commands {

struct CompressOptions;

enum class CompressionMode : std::uint8_t {
    kArchive = 0,
    kPipeline = 1,
    kStreaming = 2,
};

enum class CompressionInputKind : std::uint8_t {
    kSingleFile = 0,
    kPairedFiles = 1,
    kInterleavedFile = 2,
    kStdin = 3,
};

struct CompressionInput {
    CompressionInputKind kind = CompressionInputKind::kSingleFile;
    std::filesystem::path primaryPath;
    std::filesystem::path secondaryPath;
    PELayout archiveLayout = PELayout::kInterleaved;
};

struct CompressionRequest {
    CompressionMode mode = CompressionMode::kArchive;
    CompressionInput input;
    std::filesystem::path outputPath;
    int compressionLevel = 6;
    int threads = 0;
    std::size_t memoryLimitMb = 0;
    bool enableReordering = true;
    bool saveReorderMap = true;
    bool forceOverwrite = false;
    bool showProgress = true;
    QualityMode qualityMode = QualityMode::kLossless;
    IDMode idMode = IDMode::kExact;
    ReadLengthClass requestedLengthClass = ReadLengthClass::kShort;
    bool autoDetectLongRead = true;
    bool scanAllLengths = false;
    std::size_t blockSize = 0;
    bool blockSizeExplicit = false;
    std::size_t maxBlockBases = 0;
    ChecksumType checksumType = ChecksumType::kXxHash64;
    std::uint64_t inputBytesHint = 0;
};

[[nodiscard]] auto toCompressionRequest(const CompressOptions& options) -> CompressionRequest;

}  // namespace fqc::commands

#endif  // FQC_COMMANDS_COMPRESSION_REQUEST_H
