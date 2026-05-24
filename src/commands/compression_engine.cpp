// =============================================================================
// fq-compressor - Compression Engine
// =============================================================================

#include "fqc/commands/compression_engine.h"

#include "fqc/algo/block_compressor.h"
#include "fqc/algo/global_analyzer.h"
#include "fqc/common/logger.h"
#include "fqc/format/fqc_writer.h"
#include "fqc/format/reorder_map.h"
#include "fqc/io/fastq_parser.h"
#include "fqc/pipeline/pipeline.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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

auto populateInputBytesHint(CompressionRequest& request,
                            const std::shared_ptr<io::StreamFactory>& streamFactory) -> void {
    if (request.inputBytesHint != 0 || request.input.primaryPath == "-" || !streamFactory ||
        dynamic_cast<io::FileStreamFactory*>(streamFactory.get()) == nullptr) {
        return;
    }

    request.inputBytesHint = std::filesystem::file_size(request.input.primaryPath);
    if (!request.input.secondaryPath.empty() && request.input.secondaryPath != "-") {
        request.inputBytesHint += std::filesystem::file_size(request.input.secondaryPath);
    }
}

auto executeSingleThreadCompression(const CompressionPlan& plan,
                                    const std::shared_ptr<io::StreamFactory>& streamFactory,
                                    CompressionStats& stats) -> void {
    const auto& options = plan.effectiveOptions;
    const auto& profile = plan.profile;

    FQC_LOG_INFO("Using single-threaded compression");

    auto parserFactory = streamFactory ? streamFactory : std::make_shared<io::FileStreamFactory>();
    io::FastqParser parser(options.inputPath, parserFactory);
    parser.open();

    auto allRecords = parser.readAll();
    if (allRecords.empty()) {
        throw ArgumentError("Input file contains no FASTQ records");
    }

    std::vector<ReadRecord> readRecords;
    readRecords.reserve(allRecords.size());

    std::uint64_t totalBases = 0;
    for (auto& fastqRec : allRecords) {
        totalBases += fastqRec.length();
        readRecords.emplace_back(std::move(fastqRec.id),
                                 std::move(fastqRec.comment),
                                 std::move(fastqRec.sequence),
                                 std::move(fastqRec.quality));
    }

    stats.totalReads = readRecords.size();
    stats.totalBases = totalBases;
    stats.inputBytes = estimateFastqInputBytes(readRecords);

    algo::GlobalAnalysisResult analysisResult;
    const auto& baseAnalyzerConfig = profile.globalAnalyzerConfig();

    if (baseAnalyzerConfig.enableReorder) {
        auto analyzerConfig = baseAnalyzerConfig;

        if (options.showProgress) {
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
    } else {
        analysisResult.totalReads = readRecords.size();
        analysisResult.maxReadLength = 0;
        for (const auto& rec : readRecords) {
            analysisResult.maxReadLength = std::max(analysisResult.maxReadLength, rec.length());
        }
        analysisResult.reorderingPerformed = false;
        analysisResult.lengthClass = profile.readLengthClass();

        const std::size_t blockCount =
            (readRecords.size() + options.blockSize - 1) / options.blockSize;
        for (std::uint32_t i = 0; i < blockCount; ++i) {
            algo::BlockBoundary boundary;
            boundary.blockId = i;
            boundary.archiveIdStart = static_cast<ReadId>(i * options.blockSize);
            boundary.archiveIdEnd = std::min(static_cast<ReadId>((i + 1) * options.blockSize),
                                             static_cast<ReadId>(readRecords.size()));
            analysisResult.blockBoundaries.push_back(boundary);
        }
        analysisResult.numBlocks = blockCount;
    }

    format::FQCWriter fqcWriter(options.outputPath);
    format::installSignalHandlers();
    format::registerWriterForCleanup(&fqcWriter);

    std::string inputFilename = options.inputPath.filename().string();
    const auto headerSize =
        static_cast<std::uint32_t>(format::GlobalHeader::kMinSize + inputFilename.size());

    format::GlobalHeader globalHeader;
    globalHeader.headerSize = headerSize;
    globalHeader.compressionAlgo = static_cast<std::uint8_t>(
        options.longReadMode == ReadLengthClass::kShort ? CodecFamily::kAbcV1
                                                        : CodecFamily::kZstdPlain);
    globalHeader.checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);
    globalHeader.reserved = 0;

    std::uint64_t flags = 0;
    if (options.paired || options.interleaved) {
        flags |= format::flags::kIsPaired;
        flags |= (static_cast<std::uint64_t>(options.peLayout) << format::flags::kPeLayoutShift);
    }
    if (!analysisResult.reorderingPerformed) {
        flags |= format::flags::kPreserveOrder;
    }
    if (profile.archiveHasReorderMap() && analysisResult.reorderingPerformed &&
        !analysisResult.forwardMap.empty()) {
        flags |= format::flags::kHasReorderMap;
    }
    if (options.streamingMode) {
        flags |= format::flags::kStreamingMode;
    }
    flags |= (static_cast<std::uint64_t>(options.qualityMode) << format::flags::kQualityModeShift);
    flags |= (static_cast<std::uint64_t>(options.idMode) << format::flags::kIdModeShift);
    flags |= (static_cast<std::uint64_t>(analysisResult.lengthClass)
              << format::flags::kReadLengthClassShift);

    globalHeader.flags = flags;
    globalHeader.totalReadCount = analysisResult.totalReads;

    const auto timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    fqcWriter.writeGlobalHeader(globalHeader, inputFilename, static_cast<std::uint64_t>(timestamp));

    auto compressorConfig = profile.blockCompressorConfig();
    compressorConfig.readLengthClass = analysisResult.lengthClass;
    algo::BlockCompressor blockCompressor(compressorConfig);

    std::uint64_t totalCompressedBytes = 0;

    for (const auto& blockBoundary : analysisResult.blockBoundaries) {
        std::vector<ReadRecord> blockReads;
        const auto startId = blockBoundary.archiveIdStart;
        const auto endId = blockBoundary.archiveIdEnd;

        if (analysisResult.reorderingPerformed && !analysisResult.reverseMap.empty()) {
            for (auto archiveId = startId; archiveId < endId; ++archiveId) {
                if (archiveId < analysisResult.reverseMap.size()) {
                    const auto originalId = analysisResult.reverseMap[archiveId];
                    if (originalId < readRecords.size()) {
                        blockReads.push_back(readRecords[originalId]);
                    }
                }
            }
        } else {
            for (auto archiveId = startId; archiveId < endId; ++archiveId) {
                if (archiveId < readRecords.size()) {
                    blockReads.push_back(readRecords[archiveId]);
                }
            }
        }

        if (blockReads.empty()) {
            continue;
        }

        auto compressRes = blockCompressor.compress(blockReads, blockBoundary.blockId);
        if (!compressRes) {
            throw FormatError("Failed to compress block " + std::to_string(blockBoundary.blockId) +
                              ": " + compressRes.error().message());
        }

        auto compressedBlock = std::move(compressRes.value());

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
        stats.blocksWritten++;
    }

    if (analysisResult.reorderingPerformed && !analysisResult.forwardMap.empty() &&
        !analysisResult.reverseMap.empty()) {
        format::ReorderMap mapHeader;
        mapHeader.totalReads = static_cast<std::uint64_t>(analysisResult.forwardMap.size());

        const auto compressedForward = format::deltaEncode(std::span<const ReadId>(
            analysisResult.forwardMap.data(), analysisResult.forwardMap.size()));
        const auto compressedReverse = format::deltaEncode(std::span<const ReadId>(
            analysisResult.reverseMap.data(), analysisResult.reverseMap.size()));

        fqcWriter.writeReorderMap(mapHeader, compressedForward, compressedReverse);
    }

    fqcWriter.finalize();
    stats.outputBytes = totalCompressedBytes;
    format::unregisterWriterForCleanup(&fqcWriter);
}

auto executePipelineCompression(const CompressionPlan& plan, CompressionStats& stats) -> void {
    const auto& options = plan.effectiveOptions;

    auto pipelineConfig = plan.profile.pipelineConfig();
    if (options.showProgress) {
        pipelineConfig.progressCallback = [](const pipeline::ProgressInfo& info) -> bool {
            const double elapsedSeconds =
                info.elapsedMs > 0 ? static_cast<double>(info.elapsedMs) / 1000.0 : 1.0;
            FQC_LOG_INFO("Progress: {:.1f}% ({} reads, {} blocks, {:.1f} MB/s)",
                         info.ratio() * 100.0,
                         info.readsProcessed,
                         info.currentBlock,
                         static_cast<double>(info.bytesProcessed) / (1024.0 * 1024.0) /
                             elapsedSeconds);
            return true;
        };
        pipelineConfig.progressIntervalMs = 2000;
    }

    if (auto result = pipelineConfig.validate(); !result) {
        throw FormatError("Invalid pipeline configuration: " + result.error().message());
    }

    pipeline::CompressionPipeline pipeline(pipelineConfig);

    VoidResult result;
    if (options.input2Path.empty()) {
        result = pipeline.run(options.inputPath, options.outputPath);
    } else {
        result = pipeline.runPaired(options.inputPath, options.input2Path, options.outputPath);
    }

    if (!result) {
        throw FormatError("Compression pipeline failed: " + result.error().message());
    }

    const auto& pipelineStats = pipeline.stats();
    stats.totalReads = pipelineStats.totalReads;
    stats.totalBases = pipelineStats.totalBases;
    stats.inputBytes = pipelineStats.inputBytes;
    stats.outputBytes = pipelineStats.outputBytes;
    stats.blocksWritten = pipelineStats.totalBlocks;
}

}  // namespace

CompressionEngine::CompressionEngine(std::shared_ptr<io::StreamFactory> streamFactory)
    : streamFactory_(std::move(streamFactory)) {}

auto CompressionEngine::plan(const CompressionRequest& request) const -> Result<CompressionPlan> {
    try {
        auto requestForPlanning = request;
        populateInputBytesHint(requestForPlanning, streamFactory_);
        return buildCompressionProfilePlan(requestForPlanning, streamFactory_);
    } catch (const FQCException& ex) {
        return makeError<CompressionPlan>(ex);
    } catch (const std::exception& ex) {
        return makeError<CompressionPlan>(ErrorCode::kInternalError, ex.what());
    }
}

auto CompressionEngine::execute(const CompressionRequest& request) const
    -> Result<CompressionStats> {
    auto planResult = plan(request);
    if (!planResult) {
        return makeError<CompressionStats>(planResult.error());
    }
    return execute(planResult.value());
}

auto CompressionEngine::execute(const CompressionPlan& plan) const -> Result<CompressionStats> {
    try {
        CompressionStats stats;
        const auto startTime = std::chrono::steady_clock::now();

        const auto& options = plan.effectiveOptions;
        FQC_LOG_INFO("Starting compression...");
        FQC_LOG_INFO("  Input: {}", options.inputPath.string());
        FQC_LOG_INFO("  Output: {}", options.outputPath.string());
        FQC_LOG_INFO("  Compression level: {}", options.compressionLevel);
        FQC_LOG_INFO("  Reordering: {}", options.enableReordering ? "enabled" : "disabled");
        FQC_LOG_INFO("  Block size: {} reads", options.blockSize);
        FQC_LOG_INFO("  Threads: {}",
                     options.threads > 0 ? std::to_string(options.threads) : "auto");

        if (plan.profile.executionMode() == CompressionExecutionMode::kPipeline) {
            executePipelineCompression(plan, stats);
        } else {
            executeSingleThreadCompression(plan, streamFactory_, stats);
        }

        stats.elapsedSeconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
        return stats;
    } catch (const FQCException& ex) {
        return makeError<CompressionStats>(ex);
    } catch (const std::exception& ex) {
        return makeError<CompressionStats>(ErrorCode::kInternalError, ex.what());
    }
}

}  // namespace fqc::commands
