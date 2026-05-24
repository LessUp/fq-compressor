// =============================================================================
// fq-compressor - Compress Command Implementation
// =============================================================================
// Command handler implementation for FASTQ compression.
//
// Requirements: 6.2
// =============================================================================

#include "fqc/commands/compress_command.h"

#include "fqc/algo/block_compressor.h"
#include "fqc/algo/global_analyzer.h"
#include "fqc/commands/compression_profile.h"
#include "fqc/common/logger.h"
#include "fqc/format/fqc_writer.h"
#include "fqc/format/reorder_map.h"
#include "fqc/io/compressed_stream.h"
#include "fqc/io/fastq_parser.h"
#include "fqc/io/stream_factory.h"
#include "fqc/pipeline/pipeline.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace fqc::commands {

namespace {

[[nodiscard]] auto estimateFastqInputBytes(const std::vector<ReadRecord>& reads) -> std::uint64_t {
    std::uint64_t bytes = 0;
    for (const auto& read : reads) {
        bytes += 1 + static_cast<std::uint64_t>(read.id.size());
        if (!read.comment.empty()) {
            bytes += 1 + static_cast<std::uint64_t>(read.comment.size());
        }
        bytes += 1;
        bytes += static_cast<std::uint64_t>(read.sequence.size()) + 1;
        bytes += 2;
        bytes += static_cast<std::uint64_t>(read.quality.size()) + 1;
    }
    return bytes;
}

}  // namespace

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
    auto startTime = std::chrono::steady_clock::now();

    try {
        if (options_.inputBytesHint == 0 && options_.inputPath != "-") {
            options_.inputBytesHint = std::filesystem::file_size(options_.inputPath);
            if (!options_.input2Path.empty() && options_.input2Path != "-") {
                options_.inputBytesHint += std::filesystem::file_size(options_.input2Path);
            }
        }

        auto profileResult = buildCompressionProfile(options_);
        if (!profileResult) {
            profileResult.error().throwException();
        }

        auto profile = std::move(profileResult.value());
        options_ = profile.effectiveOptions();

        runCompression(profile);

        // Calculate elapsed time
        auto endTime = std::chrono::steady_clock::now();
        stats_.elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

        // Print summary
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

void CompressCommand::runCompression(const CompressionProfile& profile) {
    FQC_LOG_INFO("Starting compression...");
    FQC_LOG_INFO("  Input: {}", options_.inputPath.string());
    FQC_LOG_INFO("  Output: {}", options_.outputPath.string());
    FQC_LOG_INFO("  Compression level: {}", options_.compressionLevel);
    FQC_LOG_INFO("  Reordering: {}", options_.enableReordering ? "enabled" : "disabled");
    FQC_LOG_INFO("  Block size: {} reads", options_.blockSize);
    FQC_LOG_INFO("  Threads: {}", options_.threads > 0 ? std::to_string(options_.threads) : "auto");

    if (profile.executionMode() == CompressionExecutionMode::kPipeline) {
        FQC_LOG_INFO("Using TBB parallel pipeline for compression");
        runCompressionParallel(profile);
        return;
    }

    // =========================================================================
    // Single-threaded Path (threads == 1)
    // =========================================================================

    FQC_LOG_INFO("Using single-threaded compression");

    // =========================================================================
    // Phase 0: Open input and collect all reads
    // =========================================================================

    FQC_LOG_DEBUG("Opening input file...");
    auto factory = std::make_shared<io::FileStreamFactory>();
    io::FastqParser parser(options_.inputPath, factory);
    parser.open();

    // Read all records into memory for global analysis
    FQC_LOG_INFO("Reading input into memory...");
    auto allRecords = parser.readAll();

    if (allRecords.empty()) {
        throw ArgumentError("Input file contains no FASTQ records");
    }

    // Convert FastqRecord to ReadRecord (input type compatibility)
    std::vector<ReadRecord> readRecords;
    readRecords.reserve(allRecords.size());

    std::uint64_t totalBases = 0;
    for (auto& fastqRec : allRecords) {
        totalBases += fastqRec.length();  // read length BEFORE move
        readRecords.emplace_back(std::move(fastqRec.id),
                                 std::move(fastqRec.comment),
                                 std::move(fastqRec.sequence),
                                 std::move(fastqRec.quality));
    }

    // Update basic stats
    stats_.totalReads = readRecords.size();
    stats_.totalBases = totalBases;
    stats_.inputBytes = estimateFastqInputBytes(readRecords);

    FQC_LOG_INFO("Loaded {} reads ({} bases)", readRecords.size(), totalBases);

    // =========================================================================
    // Phase 1: Global Analysis (Reordering)
    // =========================================================================

    algo::GlobalAnalysisResult analysisResult;
    const auto& baseAnalyzerConfig = profile.globalAnalyzerConfig();

    if (baseAnalyzerConfig.enableReorder) {
        FQC_LOG_INFO("Starting global analysis (Phase 1)...");

        auto analyzerConfig = baseAnalyzerConfig;

        if (options_.showProgress) {
            analyzerConfig.progressCallback = [](double progress) {
                FQC_LOG_DEBUG("Global analysis progress: {:.1f}%", progress * 100.0);
            };
        }

        algo::GlobalAnalyzer analyzer(analyzerConfig);
        auto analysisRes = analyzer.analyze(readRecords);

        if (!analysisRes) {
            throw FormatError("Global analysis failed: " + analysisRes.error().message());
        }

        analysisResult = std::move(analysisRes.value());
        FQC_LOG_INFO("Global analysis complete");
        FQC_LOG_INFO("  Reads: {}", analysisResult.totalReads);
        FQC_LOG_INFO("  Max length: {}", analysisResult.maxReadLength);
        FQC_LOG_INFO("  Blocks: {}", analysisResult.numBlocks);
        FQC_LOG_INFO("  Reordering: {}", analysisResult.reorderingPerformed ? "yes" : "no");

    } else {
        FQC_LOG_INFO("Skipping global analysis (streaming or reordering disabled)");

        // Create simple block boundaries without reordering
        analysisResult.totalReads = readRecords.size();
        analysisResult.maxReadLength = 0;
        for (const auto& rec : readRecords) {
            analysisResult.maxReadLength = std::max(analysisResult.maxReadLength, rec.length());
        }
        analysisResult.reorderingPerformed = false;
        analysisResult.lengthClass = profile.readLengthClass();

        std::size_t blockCount = (readRecords.size() + options_.blockSize - 1) / options_.blockSize;
        for (std::uint32_t i = 0; i < blockCount; ++i) {
            algo::BlockBoundary boundary;
            boundary.blockId = i;
            boundary.archiveIdStart = static_cast<ReadId>(i * options_.blockSize);
            boundary.archiveIdEnd = std::min(static_cast<ReadId>((i + 1) * options_.blockSize),
                                             static_cast<ReadId>(readRecords.size()));
            analysisResult.blockBoundaries.push_back(boundary);
        }
        analysisResult.numBlocks = blockCount;
    }

    // =========================================================================
    // Phase 1.5: Create FQC Writer
    // =========================================================================

    FQC_LOG_DEBUG("Creating FQC writer...");
    format::FQCWriter fqcWriter(options_.outputPath);

    // Register for signal handling cleanup
    format::installSignalHandlers();
    format::registerWriterForCleanup(&fqcWriter);

    // =========================================================================
    // Phase 1.5: Write Global Header
    // =========================================================================

    FQC_LOG_DEBUG("Writing global header...");

    // Calculate header size (will be recalculated by writer, but must be valid for isValid())
    std::string inputFilename = options_.inputPath.filename().string();
    std::uint32_t headerSize =
        static_cast<std::uint32_t>(format::GlobalHeader::kMinSize + inputFilename.size());

    format::GlobalHeader globalHeader;
    globalHeader.headerSize = headerSize;
    globalHeader.compressionAlgo = static_cast<std::uint8_t>(
        options_.longReadMode == ReadLengthClass::kShort ? CodecFamily::kAbcV1
                                                         : CodecFamily::kZstdPlain);
    globalHeader.checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);
    globalHeader.reserved = 0;  // Explicitly set to 0 (required by isValid)

    // Set flags
    std::uint64_t flags = 0;
    if (options_.paired || options_.interleaved) {
        flags |= format::flags::kIsPaired;
        flags |= (static_cast<std::uint64_t>(options_.peLayout) << format::flags::kPeLayoutShift);
    }
    if (!analysisResult.reorderingPerformed) {
        flags |= format::flags::kPreserveOrder;
    }
    if (profile.archiveHasReorderMap() && analysisResult.reorderingPerformed &&
        !analysisResult.forwardMap.empty()) {
        flags |= format::flags::kHasReorderMap;
    }
    if (options_.streamingMode) {
        flags |= format::flags::kStreamingMode;
    }

    // Set quality mode
    flags |= (static_cast<std::uint64_t>(options_.qualityMode) << format::flags::kQualityModeShift);

    // Set ID mode
    flags |= (static_cast<std::uint64_t>(options_.idMode) << format::flags::kIdModeShift);

    // Set read length class
    flags |= (static_cast<std::uint64_t>(analysisResult.lengthClass)
              << format::flags::kReadLengthClassShift);

    globalHeader.flags = flags;
    globalHeader.totalReadCount = analysisResult.totalReads;

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);

    fqcWriter.writeGlobalHeader(globalHeader, inputFilename, static_cast<std::uint64_t>(timestamp));

    // =========================================================================
    // Phase 2: Block Compression
    // =========================================================================

    FQC_LOG_INFO("Starting block compression (Phase 2)...");

    auto compressorConfig = profile.blockCompressorConfig();
    compressorConfig.readLengthClass = analysisResult.lengthClass;
    algo::BlockCompressor blockCompressor(compressorConfig);

    std::uint64_t totalCompressedBytes = 0;

    // Process blocks
    for (const auto& blockBoundary : analysisResult.blockBoundaries) {
        if (options_.showProgress) {
            FQC_LOG_INFO("Processing block {} of {}...",
                         blockBoundary.blockId + 1,
                         analysisResult.numBlocks);
        }

        // Extract reads for this block
        std::vector<ReadRecord> blockReads;
        const auto& startId = blockBoundary.archiveIdStart;
        const auto& endId = blockBoundary.archiveIdEnd;

        if (analysisResult.reorderingPerformed && !analysisResult.reverseMap.empty()) {
            // Reordered: use reverse map to get original read order
            for (auto archiveId = startId; archiveId < endId; ++archiveId) {
                if (archiveId < analysisResult.reverseMap.size()) {
                    auto originalId = analysisResult.reverseMap[archiveId];
                    if (originalId < readRecords.size()) {
                        blockReads.push_back(readRecords[originalId]);
                    }
                }
            }
        } else {
            // No reordering: use original order
            for (auto archiveId = startId; archiveId < endId; ++archiveId) {
                if (archiveId < readRecords.size()) {
                    blockReads.push_back(readRecords[archiveId]);
                }
            }
        }

        if (blockReads.empty()) {
            FQC_LOG_WARNING("Block {} has no reads, skipping", blockBoundary.blockId);
            continue;
        }

        // Compress the block
        auto compressRes = blockCompressor.compress(blockReads, blockBoundary.blockId);

        if (!compressRes) {
            throw FormatError("Failed to compress block " + std::to_string(blockBoundary.blockId) +
                              ": " + compressRes.error().message());
        }

        auto compressedBlock = std::move(compressRes.value());

        // Write block to FQC file
        format::BlockHeader blockHeader;
        blockHeader.blockId = compressedBlock.blockId;
        blockHeader.uncompressedCount = compressedBlock.readCount;
        blockHeader.uniformReadLength = compressedBlock.uniformReadLength;
        blockHeader.blockXxhash64 = compressedBlock.blockChecksum;
        blockHeader.codecIds = compressedBlock.codecIds;
        blockHeader.codecSeq = compressedBlock.codecSeq;
        blockHeader.codecQual = compressedBlock.codecQual;
        blockHeader.codecAux = compressedBlock.codecAux;

        format::BlockPayload payload;
        payload.idsData = compressedBlock.idStream;
        payload.seqData = compressedBlock.seqStream;
        payload.qualData = compressedBlock.qualStream;
        payload.auxData = compressedBlock.auxStream;

        fqcWriter.writeBlock(blockHeader, payload);

        totalCompressedBytes += compressedBlock.totalCompressedSize();
        stats_.blocksWritten++;

        if (options_.showProgress) {
            FQC_LOG_DEBUG(
                "Block {} compressed: {} bytes -> {} bytes ({:.1f}%)",
                blockBoundary.blockId,
                blockReads.size() * 100,  // Rough estimate
                compressedBlock.totalCompressedSize(),
                blockReads.empty()
                    ? 0.0
                    : (100.0 * static_cast<double>(compressedBlock.totalCompressedSize()) /
                       static_cast<double>(blockReads.size() * 100)));
        }
    }

    // =========================================================================
    // Phase 2.5: Write Reorder Map (if applicable)
    // =========================================================================

    if (analysisResult.reorderingPerformed && !analysisResult.forwardMap.empty() &&
        !analysisResult.reverseMap.empty()) {
        FQC_LOG_DEBUG("Writing reorder map...");

        // Prepare ReorderMap header
        format::ReorderMap mapHeader;
        mapHeader.totalReads = static_cast<std::uint64_t>(analysisResult.forwardMap.size());
        // forwardMapSize / reverseMapSize are filled from actual span sizes by
        // FQCWriter::writeReorderMap

        // Compress maps (Delta + Varint encoding)
        auto compressedForward = format::deltaEncode(std::span<const ReadId>(
            analysisResult.forwardMap.data(), analysisResult.forwardMap.size()));
        auto compressedReverse = format::deltaEncode(std::span<const ReadId>(
            analysisResult.reverseMap.data(), analysisResult.reverseMap.size()));

        // Write to archive
        fqcWriter.writeReorderMap(mapHeader, compressedForward, compressedReverse);

        FQC_LOG_INFO("Reorder map written: {} reads, forward={} bytes, reverse={} bytes",
                     mapHeader.totalReads,
                     compressedForward.size(),
                     compressedReverse.size());
    }

    // =========================================================================
    // Phase 3: Finalize
    // =========================================================================

    FQC_LOG_DEBUG("Finalizing FQC archive...");
    fqcWriter.finalize();

    stats_.outputBytes = totalCompressedBytes;

    // Unregister from signal handlers
    format::unregisterWriterForCleanup(&fqcWriter);

    FQC_LOG_INFO("Compression complete!");
    FQC_LOG_INFO("  Blocks written: {}", stats_.blocksWritten);
    FQC_LOG_INFO("  Compression ratio: {:.2f}x", stats_.compressionRatio());
    FQC_LOG_INFO("  Bits per base: {:.3f}", stats_.bitsPerBase());
}

void CompressCommand::runCompressionParallel(const CompressionProfile& profile) {
    FQC_LOG_INFO("Initializing parallel compression pipeline...");

    auto pipelineConfig = profile.pipelineConfig();

    if (options_.showProgress) {
        pipelineConfig.progressCallback = [](const pipeline::ProgressInfo& info) -> bool {
            double progress = info.ratio() * 100.0;
            FQC_LOG_INFO(
                "Progress: {:.1f}% ({} reads, {} blocks, {:.1f} MB/s)",
                progress,
                info.readsProcessed,
                info.currentBlock,
                static_cast<double>(info.bytesProcessed) / (1024.0 * 1024.0) /
                    (info.elapsedMs > 0 ? static_cast<double>(info.elapsedMs) / 1000.0 : 1.0));
            return true;
        };
        pipelineConfig.progressIntervalMs = 2000;
    }

    if (auto result = pipelineConfig.validate(); !result) {
        throw FormatError("Invalid pipeline configuration: " + result.error().message());
    }

    FQC_LOG_INFO("Pipeline configured:");
    FQC_LOG_INFO("  Threads: {}", pipelineConfig.effectiveThreads());
    FQC_LOG_INFO("  Block size: {}", pipelineConfig.effectiveBlockSize());
    FQC_LOG_INFO(
        "  Read length class: {}",
        pipelineConfig.compressorConfig.readLengthClass == ReadLengthClass::kShort        ? "SHORT"
            : pipelineConfig.compressorConfig.readLengthClass == ReadLengthClass::kMedium ? "MEDIUM"
                                                                                          : "LONG");
    FQC_LOG_INFO("  Quality mode: {}", qualityModeToString(options_.qualityMode));
    FQC_LOG_INFO("  Reordering: {}", pipelineConfig.enableReorder ? "enabled" : "disabled");

    pipeline::CompressionPipeline pipeline(pipelineConfig);

    VoidResult result;
    if (options_.input2Path.empty()) {
        result = pipeline.run(options_.inputPath, options_.outputPath);
    } else {
        result = pipeline.runPaired(options_.inputPath, options_.input2Path, options_.outputPath);
    }

    if (!result) {
        throw FormatError("Compression pipeline failed: " + result.error().message());
    }

    // =========================================================================
    // Update Statistics
    // =========================================================================

    const auto& pipelineStats = pipeline.stats();
    stats_.totalReads = pipelineStats.totalReads;
    stats_.totalBases = pipelineStats.totalBases;
    stats_.inputBytes = pipelineStats.inputBytes;
    stats_.outputBytes = pipelineStats.outputBytes;
    stats_.blocksWritten = pipelineStats.totalBlocks;

    FQC_LOG_INFO("Parallel compression complete!");
    FQC_LOG_INFO("  Blocks written: {}", stats_.blocksWritten);
    FQC_LOG_INFO("  Compression ratio: {:.2f}x", stats_.compressionRatio());
    FQC_LOG_INFO("  Throughput: {:.2f} MB/s", pipelineStats.throughputMBps());
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
