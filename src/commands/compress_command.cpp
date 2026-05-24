// =============================================================================
// fq-compressor - Compress Command Implementation
// =============================================================================
// Command handler implementation for FASTQ compression.
//
// Requirements: 6.2
// =============================================================================

#include "fqc/commands/compress_command.h"

#include "fqc/commands/compression_engine.h"
#include "fqc/common/logger.h"

#include <iomanip>
#include <iostream>

namespace fqc::commands {

// =============================================================================
// Quality Mode Parsing
// =============================================================================

QualityMode parseQualityMode(std::string_view str) {
    if (str == "none" || str == "lossless") {
        return QualityMode::kLossless;
    }
    if (str == "illumina8") {
        return QualityMode::kIllumina8;
    }
    if (str == "qvz") {
        throw ArgumentError(
            "QVZ lossy compression is not yet implemented. "
            "Please use 'illumina8' for lossy or 'discard' to drop quality values.");
    }
    if (str == "discard") {
        return QualityMode::kDiscard;
    }
    throw ArgumentError("Invalid quality mode: " + std::string(str));
}

IDMode parseIdMode(std::string_view str) {
    if (str == "exact") {
        return IDMode::kExact;
    }
    if (str == "tokenize") {
        return IDMode::kTokenize;
    }
    if (str == "discard") {
        return IDMode::kDiscard;
    }
    throw ArgumentError("Invalid ID mode: " + std::string(str));
}

CompressionRequest toCompressionRequest(const CompressOptions& options) {
    CompressionRequest request;

    request.input.primaryPath = options.inputPath;
    request.input.secondaryPath = options.input2Path;
    request.input.archiveLayout = options.peLayout;
    request.paired = options.paired;
    request.outputPath = options.outputPath;
    request.compressionLevel = options.compressionLevel;
    request.threads = options.threads;
    request.memoryLimitMb = options.memoryLimitMb;
    request.enableReordering = options.enableReordering;
    request.saveReorderMap = options.saveReorderMap;
    request.forceOverwrite = options.forceOverwrite;
    request.showProgress = options.showProgress;
    request.qualityMode = options.qualityMode;
    request.idMode = options.idMode;
    request.requestedLengthClass = options.longReadMode;
    request.autoDetectLongRead = options.autoDetectLongRead;
    request.scanAllLengths = options.scanAllLengths;
    request.blockSize = options.blockSize;
    request.blockSizeExplicit = options.blockSizeExplicit;
    request.maxBlockBases = options.maxBlockBases;
    request.checksumType = options.checksumType;
    request.inputBytesHint = options.inputBytesHint;

    const bool streamingInput = options.streamingMode || options.inputPath == "-";

    if (options.inputPath == "-") {
        request.mode = CompressionMode::kStreaming;
        request.input.kind = CompressionInputKind::kStdin;
    } else if (!options.input2Path.empty()) {
        request.input.kind = CompressionInputKind::kPairedFiles;
    } else if (options.interleaved) {
        request.input.kind = CompressionInputKind::kInterleavedFile;
    } else {
        request.input.kind = CompressionInputKind::kSingleFile;
    }

    if (request.input.kind == CompressionInputKind::kPairedFiles ||
        request.input.kind == CompressionInputKind::kInterleavedFile) {
        request.paired = true;
    } else if (request.input.kind == CompressionInputKind::kStdin) {
        request.paired = options.interleaved;
    } else {
        request.paired = false;
    }

    if (streamingInput) {
        request.mode = CompressionMode::kStreaming;
        request.enableReordering = false;
        request.saveReorderMap = false;
    }

    if (!request.enableReordering) {
        request.saveReorderMap = false;
    }

    return request;
}

// =============================================================================
// CompressOptions::toCompressorConfig
// =============================================================================

algo::BlockCompressorConfig CompressOptions::toCompressorConfig() const noexcept {
    algo::BlockCompressorConfig config;
    config.readLengthClass = longReadMode;
    config.qualityMode = qualityMode;
    config.idMode = idMode;
    config.compressionLevel = static_cast<CompressionLevel>(compressionLevel);
    config.zstdLevel = 3;
    config.numThreads = static_cast<std::size_t>(threads);
    return config;
}

// =============================================================================
// CompressCommand Implementation
// =============================================================================

CompressCommand::CompressCommand(CompressOptions options) : options_(std::move(options)) {}

CompressCommand::~CompressCommand() = default;

CompressCommand::CompressCommand(CompressCommand&&) noexcept = default;
CompressCommand& CompressCommand::operator=(CompressCommand&&) noexcept = default;

int CompressCommand::execute() {
    try {
        CompressionEngine engine;
        auto planResult = engine.plan(toCompressionRequest(options_));
        if (!planResult) {
            planResult.error().throwException();
        }

        auto plan = std::move(planResult.value());
        options_ = plan.effectiveOptions;

        auto statsResult = engine.execute(plan);
        if (!statsResult) {
            statsResult.error().throwException();
        }
        stats_ = std::move(statsResult.value());

        if (options_.showProgress) {
            printSummary();
        }

        return 0;

    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Compression failed: {}", e.what());
        return toExitCode(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return 1;
    }
}

void CompressCommand::printSummary() const {
    std::cout << "\n=== Compression Summary ===" << std::endl;
    std::cout << "  Total reads:      " << stats_.totalReads << std::endl;
    std::cout << "  Total bases:      " << stats_.totalBases << std::endl;
    std::cout << "  Input size:       " << stats_.inputBytes << " bytes" << std::endl;
    std::cout << "  Output size:      " << stats_.outputBytes << " bytes" << std::endl;
    std::cout << "  Compression ratio: " << std::fixed << std::setprecision(2)
              << stats_.compressionRatio() << "x" << std::endl;
    std::cout << "  Bits per base:    " << std::fixed << std::setprecision(3)
              << stats_.bitsPerBase() << std::endl;
    std::cout << "  Elapsed time:     " << std::fixed << std::setprecision(2)
              << stats_.elapsedSeconds << " s" << std::endl;
    std::cout << "  Throughput:       " << std::fixed << std::setprecision(2)
              << stats_.throughputMbps() << " MB/s" << std::endl;
    std::cout << "===========================" << std::endl;
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<CompressCommand> createCompressCommand(const std::string& inputPath,
                                                       const std::string& outputPath,
                                                       int level,
                                                       bool reorder,
                                                       bool streaming,
                                                       const std::string& qualityMode,
                                                       const std::string& longReadMode,
                                                       int threads,
                                                       std::size_t memoryLimit,
                                                       bool force) {
    CompressOptions opts;
    opts.inputPath = inputPath;
    opts.outputPath = outputPath;
    opts.compressionLevel = level;
    opts.enableReordering = reorder;
    opts.streamingMode = streaming;
    opts.qualityMode = parseQualityMode(qualityMode);
    opts.threads = threads;
    opts.memoryLimitMb = memoryLimit;
    opts.forceOverwrite = force;

    // Parse long read mode
    if (longReadMode == "auto") {
        opts.autoDetectLongRead = true;
    } else if (longReadMode == "short") {
        opts.autoDetectLongRead = false;
        opts.longReadMode = ReadLengthClass::kShort;
    } else if (longReadMode == "medium") {
        opts.autoDetectLongRead = false;
        opts.longReadMode = ReadLengthClass::kMedium;
    } else if (longReadMode == "long") {
        opts.autoDetectLongRead = false;
        opts.longReadMode = ReadLengthClass::kLong;
    }

    return std::make_unique<CompressCommand>(std::move(opts));
}

}  // namespace fqc::commands
