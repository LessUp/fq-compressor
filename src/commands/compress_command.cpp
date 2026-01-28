// =============================================================================
// fq-compressor - Compress Command Implementation
// =============================================================================
// Command handler implementation for FASTQ compression.
//
// Requirements: 6.2
// =============================================================================

#include "compress_command.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "fqc/algo/block_compressor.h"
#include "fqc/algo/global_analyzer.h"
#include "fqc/common/logger.h"
#include "fqc/format/fqc_writer.h"
#include "fqc/io/compressed_stream.h"
#include "fqc/io/fastq_parser.h"
#include "fqc/pipeline/pipeline.h"

namespace fqc::commands {

// =============================================================================
// Quality Mode Parsing
// =============================================================================

QualityCompressionMode parseQualityMode(std::string_view str) {
    if (str == "none" || str == "lossless") {
        return QualityCompressionMode::kLossless;
    }
    if (str == "illumina8") {
        return QualityCompressionMode::kIllumina8;
    }
    if (str == "qvz") {
        return QualityCompressionMode::kQvz;
    }
    if (str == "discard") {
        return QualityCompressionMode::kDiscard;
    }
    throw ArgumentError(
                        "Invalid quality mode: " + std::string(str));
}

std::string_view qualityModeToString(QualityCompressionMode mode) {
    switch (mode) {
        case QualityCompressionMode::kLossless:
            return "lossless";
        case QualityCompressionMode::kIllumina8:
            return "illumina8";
        case QualityCompressionMode::kQvz:
            return "qvz";
        case QualityCompressionMode::kDiscard:
            return "discard";
        default:
            return "unknown";
    }
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
        // Validate options
        validateOptions();

        // Detect read length class if auto
        if (options_.autoDetectLongRead && !options_.streamingMode) {
            detectReadLengthClass();
        }

        // Setup compression parameters
        setupCompressionParams();

        // Run compression
        runCompression();

        // Calculate elapsed time
        auto endTime = std::chrono::steady_clock::now();
        stats_.elapsedSeconds =
            std::chrono::duration<double>(endTime - startTime).count();

        // Print summary
        if (options_.showProgress) {
            printSummary();
        }

        return 0;

    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Compression failed: {}", e.what());
        return static_cast<int>(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return 1;
    }
}

void CompressCommand::validateOptions() {
    // Check input exists (unless stdin)
    if (options_.inputPath != "-") {
        if (!std::filesystem::exists(options_.inputPath)) {
            throw IOError(
                          "Input file not found: " + options_.inputPath.string());
        }
    }

    // Check output doesn't exist (unless force)
    if (!options_.forceOverwrite && std::filesystem::exists(options_.outputPath)) {
        throw IOError(
                      "Output file already exists: " + options_.outputPath.string() +
                          " (use -f to overwrite)");
    }

    // Validate compression level
    if (options_.compressionLevel < 1 || options_.compressionLevel > 9) {
        throw ArgumentError(
                            "Compression level must be 1-9");
    }

    // Streaming mode implies no reordering
    if (options_.streamingMode) {
        options_.enableReordering = false;
        FQC_LOG_DEBUG("Streaming mode enabled, disabling reordering");
    }

    // stdin implies streaming mode
    if (options_.inputPath == "-" && !options_.streamingMode) {
        FQC_LOG_WARNING("stdin input detected, enabling streaming mode");
        options_.streamingMode = true;
        options_.enableReordering = false;
    }

    FQC_LOG_DEBUG("Compression options validated");
    FQC_LOG_DEBUG("  Input: {}", options_.inputPath.string());
    FQC_LOG_DEBUG("  Output: {}", options_.outputPath.string());
    FQC_LOG_DEBUG("  Level: {}", options_.compressionLevel);
    FQC_LOG_DEBUG("  Reordering: {}", options_.enableReordering);
    FQC_LOG_DEBUG("  Streaming: {}", options_.streamingMode);
    FQC_LOG_DEBUG("  Quality mode: {}", qualityModeToString(options_.qualityMode));
}

void CompressCommand::detectReadLengthClass() {
    if (options_.inputPath == "-") {
        // Cannot sample stdin
        FQC_LOG_WARNING("Cannot sample stdin for length detection, using MEDIUM strategy");
        detectedLengthClass_ = ReadLengthClass::kMedium;
        return;
    }

    if (options_.scanAllLengths) {
        FQC_LOG_INFO("Scanning all reads for length detection (--scan-all-lengths)...");
    } else {
        FQC_LOG_DEBUG("Sampling input file for read length detection...");
    }

    try {
        // Open input file
        auto stream = io::openCompressedFile(options_.inputPath);
        io::FastqParser parser(std::move(stream));
        parser.open();

        io::ParserStats sampleStats;

        if (options_.scanAllLengths) {
            // Full file scan for accurate max length detection
            std::uint64_t scannedCount = 0;
            while (auto record = parser.readRecord()) {
                sampleStats.update(*record);
                ++scannedCount;

                // Progress report every 1M reads
                if (options_.showProgress && scannedCount % 1000000 == 0) {
                    FQC_LOG_INFO("Scanned {} reads, max length so far: {}",
                             scannedCount, sampleStats.maxLength);
                }
            }
            FQC_LOG_INFO("Full scan complete: {} reads scanned", scannedCount);
        } else {
            // Sample records (default: 1000)
            sampleStats = parser.sampleRecords(1000);
        }

        // Detect length class
        detectedLengthClass_ = io::detectReadLengthClass(sampleStats);

        FQC_LOG_INFO("Detected read length class: {}",
                 detectedLengthClass_ == ReadLengthClass::kShort    ? "SHORT"
                 : detectedLengthClass_ == ReadLengthClass::kMedium ? "MEDIUM"
                                                                    : "LONG");
        FQC_LOG_DEBUG("  {} size: {} reads", options_.scanAllLengths ? "Scan" : "Sample",
                  sampleStats.totalRecords);
        FQC_LOG_DEBUG("  Min length: {}", sampleStats.minLength);
        FQC_LOG_DEBUG("  Max length: {}", sampleStats.maxLength);
        FQC_LOG_DEBUG("  Avg length: {:.1f}", sampleStats.averageLength());

    } catch (const std::exception& e) {
        FQC_LOG_WARNING("Failed to {} input: {}, using MEDIUM strategy",
                    options_.scanAllLengths ? "scan" : "sample", e.what());
        detectedLengthClass_ = ReadLengthClass::kMedium;
    }
}

void CompressCommand::setupCompressionParams() {
    // Use detected or specified length class
    ReadLengthClass lengthClass = detectedLengthClass_.value_or(options_.longReadMode);

    // Adjust parameters based on length class
    switch (lengthClass) {
        case ReadLengthClass::kShort:
            // Short reads: ABC + reordering, 100K block
            options_.blockSize = 100000;
            if (options_.maxBlockBases == 0) {
                options_.maxBlockBases = 0;  // No limit for short reads
            }
            FQC_LOG_DEBUG("Using SHORT read strategy: ABC + reordering, 100K reads/block");
            break;

        case ReadLengthClass::kMedium:
            // Medium reads: Zstd, no reordering, 50K block
            options_.blockSize = 50000;
            options_.enableReordering = false;
            if (options_.maxBlockBases == 0) {
                options_.maxBlockBases = 200 * 1024 * 1024;  // 200MB
            }
            FQC_LOG_DEBUG("Using MEDIUM read strategy: Zstd, no reordering, 50K reads/block");
            break;

        case ReadLengthClass::kLong:
            // Long reads: Zstd, no reordering, 10K block
            options_.blockSize = 10000;
            options_.enableReordering = false;
            if (options_.maxBlockBases == 0) {
                options_.maxBlockBases = 50 * 1024 * 1024;  // 50MB for ultra-long
            }
            FQC_LOG_DEBUG("Using LONG read strategy: Zstd, no reordering, 10K reads/block");
            break;
    }

    // Store the effective length class
    options_.longReadMode = lengthClass;
}

void CompressCommand::runCompression() {
    FQC_LOG_INFO("Starting compression...");
    FQC_LOG_INFO("  Input: {}", options_.inputPath.string());
    FQC_LOG_INFO("  Output: {}", options_.outputPath.string());
    FQC_LOG_INFO("  Compression level: {}", options_.compressionLevel);
    FQC_LOG_INFO("  Reordering: {}", options_.enableReordering ? "enabled" : "disabled");
    FQC_LOG_INFO("  Block size: {} reads", options_.blockSize);
    FQC_LOG_INFO("  Threads: {}", options_.threads > 0 ? std::to_string(options_.threads) : "auto");

    // =========================================================================
    // TBB Parallel Pipeline Path (threads > 1)
    // =========================================================================

    if (options_.threads > 1 || (options_.threads == 0 && std::thread::hardware_concurrency() > 1)) {
        FQC_LOG_INFO("Using TBB parallel pipeline for compression");
        runCompressionParallel();
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
    std::unique_ptr<std::istream> inputStream;
    if (options_.inputPath == "-") {
        inputStream = io::openInputFile("-");
    } else {
        inputStream = io::openCompressedFile(options_.inputPath);
    }

    io::FastqParser parser(std::move(inputStream));
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
        readRecords.emplace_back(
            std::move(fastqRec.id),
            std::move(fastqRec.sequence),
            std::move(fastqRec.quality)
        );
        totalBases += fastqRec.length();
    }

    // Update basic stats
    stats_.totalReads = readRecords.size();
    stats_.totalBases = totalBases;
    stats_.inputBytes = totalBases;  // Approximate - actual byte count would be larger

    FQC_LOG_INFO("Loaded {} reads ({} bases)", readRecords.size(), totalBases);

    // =========================================================================
    // Phase 1: Global Analysis (Reordering)
    // =========================================================================

    algo::GlobalAnalysisResult analysisResult;

    if (options_.enableReordering && !options_.streamingMode) {
        FQC_LOG_INFO("Starting global analysis (Phase 1)...");

        algo::GlobalAnalyzerConfig analyzerConfig;
        analyzerConfig.readsPerBlock = options_.blockSize;
        analyzerConfig.enableReorder = true;
        analyzerConfig.numThreads = options_.threads > 0 ? options_.threads : 0;
        analyzerConfig.memoryLimit = options_.memoryLimitMb > 0
            ? options_.memoryLimitMb * 1024 * 1024 : 0;

        // Progress callback
        if (options_.showProgress) {
            analyzerConfig.progressCallback = [this](double progress) {
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
        analysisResult.lengthClass = options_.longReadMode;

        // Divide into blocks
        std::size_t blockCount = (readRecords.size() + options_.blockSize - 1) / options_.blockSize;
        for (std::uint32_t i = 0; i < blockCount; ++i) {
            algo::BlockBoundary boundary;
            boundary.blockId = i;
            boundary.archiveIdStart = static_cast<ReadId>(i * options_.blockSize);
            boundary.archiveIdEnd = std::min(
                static_cast<ReadId>((i + 1) * options_.blockSize),
                static_cast<ReadId>(readRecords.size())
            );
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
    std::uint32_t headerSize = static_cast<std::uint32_t>(
        format::GlobalHeader::kMinSize + inputFilename.size());

    format::GlobalHeader globalHeader;
    globalHeader.headerSize = headerSize;
    globalHeader.compressionAlgo = static_cast<std::uint8_t>(CodecFamily::kAbcV1);
    globalHeader.checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);
    globalHeader.reserved = 0;  // Explicitly set to 0 (required by isValid)

    // Set flags
    std::uint64_t flags = 0;
    if (options_.interleaved) {
        flags |= format::flags::kIsPaired;
        flags |= (static_cast<std::uint64_t>(options_.peLayout) << format::flags::kPeLayoutShift);
    }
    if (!analysisResult.reorderingPerformed) {
        flags |= format::flags::kPreserveOrder;
    }
    if (analysisResult.reorderingPerformed && analysisResult.forwardMap.size() > 0) {
        flags |= format::flags::kHasReorderMap;
    }
    if (options_.streamingMode) {
        flags |= format::flags::kStreamingMode;
    }

    // Set quality mode
    {
        QualityMode qualMode = QualityMode::kLossless;
        switch (options_.qualityMode) {
            case QualityCompressionMode::kLossless:
                qualMode = QualityMode::kLossless;
                break;
            case QualityCompressionMode::kIllumina8:
                qualMode = QualityMode::kIllumina8;
                break;
            case QualityCompressionMode::kQvz:
                qualMode = QualityMode::kQvz;
                break;
            case QualityCompressionMode::kDiscard:
                qualMode = QualityMode::kDiscard;
                break;
        }
        flags |= (static_cast<std::uint64_t>(qualMode) << format::flags::kQualityModeShift);
    }

    // Set read length class
    flags |= (static_cast<std::uint64_t>(analysisResult.lengthClass) << format::flags::kReadLengthClassShift);

    globalHeader.flags = flags;
    globalHeader.totalReadCount = analysisResult.totalReads;

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);

    fqcWriter.writeGlobalHeader(globalHeader, inputFilename, timestamp);

    // =========================================================================
    // Phase 2: Block Compression
    // =========================================================================

    FQC_LOG_INFO("Starting block compression (Phase 2)...");

    algo::BlockCompressorConfig compressorConfig;
    compressorConfig.readLengthClass = analysisResult.lengthClass;
    compressorConfig.compressionLevel = options_.compressionLevel;
    compressorConfig.numThreads = options_.threads > 0 ? options_.threads : 0;

    // Convert quality mode
    {
        QualityMode qualMode = QualityMode::kLossless;
        switch (options_.qualityMode) {
            case QualityCompressionMode::kLossless:
                qualMode = QualityMode::kLossless;
                break;
            case QualityCompressionMode::kIllumina8:
                qualMode = QualityMode::kIllumina8;
                break;
            case QualityCompressionMode::kQvz:
                qualMode = QualityMode::kQvz;
                break;
            case QualityCompressionMode::kDiscard:
                qualMode = QualityMode::kDiscard;
                break;
        }
        compressorConfig.qualityMode = qualMode;
    }

    algo::BlockCompressor blockCompressor(compressorConfig);

    std::uint64_t totalCompressedBytes = 0;

    // Process blocks
    for (const auto& blockBoundary : analysisResult.blockBoundaries) {
        if (options_.showProgress) {
            FQC_LOG_INFO("Processing block {} of {}...",
                        blockBoundary.blockId + 1, analysisResult.numBlocks);
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
            throw FormatError(
                "Failed to compress block " + std::to_string(blockBoundary.blockId) +
                ": " + compressRes.error().message()
            );
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
            FQC_LOG_DEBUG("Block {} compressed: {} bytes -> {} bytes ({:.1f}%)",
                         blockBoundary.blockId,
                         blockReads.size() * 100,  // Rough estimate
                         compressedBlock.totalCompressedSize(),
                         blockReads.empty() ? 0.0 :
                            (100.0 * compressedBlock.totalCompressedSize() /
                             (blockReads.size() * 100)));
        }
    }

    // =========================================================================
    // Phase 2.5: Write Reorder Map (if applicable)
    // =========================================================================

    if (analysisResult.reorderingPerformed &&
        !analysisResult.forwardMap.empty() && !analysisResult.reverseMap.empty()) {
        FQC_LOG_DEBUG("Writing reorder map...");
        // TODO: Compress and write reorder maps
        // For now, we skip this as the FQCWriter interface needs map compression
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

void CompressCommand::runCompressionParallel() {
    FQC_LOG_INFO("Initializing parallel compression pipeline...");

    // =========================================================================
    // Configure Pipeline
    // =========================================================================

    pipeline::CompressionPipelineConfig pipelineConfig;
    pipelineConfig.numThreads = options_.threads > 0
        ? static_cast<std::size_t>(options_.threads)
        : 0;  // 0 = auto-detect
    pipelineConfig.blockSize = options_.blockSize;
    pipelineConfig.readLengthClass = options_.longReadMode;
    pipelineConfig.compressionLevel = static_cast<CompressionLevel>(options_.compressionLevel);
    pipelineConfig.enableReorder = options_.enableReordering && !options_.streamingMode;
    pipelineConfig.streamingMode = options_.streamingMode;
    pipelineConfig.memoryLimitMB = options_.memoryLimitMb;

    // Convert quality mode
    QualityMode qualMode = QualityMode::kLossless;
    switch (options_.qualityMode) {
        case QualityCompressionMode::kLossless:
            qualMode = QualityMode::kLossless;
            break;
        case QualityCompressionMode::kIllumina8:
            qualMode = QualityMode::kIllumina8;
            break;
        case QualityCompressionMode::kQvz:
            qualMode = QualityMode::kQvz;
            break;
        case QualityCompressionMode::kDiscard:
            qualMode = QualityMode::kDiscard;
            break;
    }
    pipelineConfig.qualityMode = qualMode;
    pipelineConfig.idMode = IDMode::kExact;  // TODO: Make configurable

    // Progress callback
    if (options_.showProgress) {
        pipelineConfig.progressCallback = [this](const pipeline::ProgressInfo& info) -> bool {
            double progress = info.ratio() * 100.0;
            FQC_LOG_INFO("Progress: {:.1f}% ({} reads, {} blocks, {:.1f} MB/s)",
                        progress,
                        info.readsProcessed,
                        info.currentBlock,
                        info.bytesProcessed / (1024.0 * 1024.0) /
                        (info.elapsedMs > 0 ? info.elapsedMs / 1000.0 : 1.0));
            return true;  // Continue
        };
        pipelineConfig.progressIntervalMs = 2000;  // Report every 2 seconds
    }

    // Validate configuration
    if (auto result = pipelineConfig.validate(); !result) {
        throw FormatError("Invalid pipeline configuration: " + result.error().message());
    }

    FQC_LOG_INFO("Pipeline configured:");
    FQC_LOG_INFO("  Threads: {}", pipelineConfig.effectiveThreads());
    FQC_LOG_INFO("  Block size: {}", pipelineConfig.effectiveBlockSize());
    FQC_LOG_INFO("  Read length class: {}",
                pipelineConfig.readLengthClass == ReadLengthClass::kShort ? "SHORT" :
                pipelineConfig.readLengthClass == ReadLengthClass::kMedium ? "MEDIUM" : "LONG");
    FQC_LOG_INFO("  Quality mode: {}", qualityModeToString(options_.qualityMode));
    FQC_LOG_INFO("  Reordering: {}", pipelineConfig.enableReorder ? "enabled" : "disabled");

    // =========================================================================
    // Execute Pipeline
    // =========================================================================

    pipeline::CompressionPipeline pipeline(pipelineConfig);

    VoidResult result;
    if (options_.input2Path.empty()) {
        // Single-end mode
        result = pipeline.run(options_.inputPath, options_.outputPath);
    } else {
        // Paired-end mode
        result = pipeline.runPaired(options_.input2Path, options_.input2Path, options_.outputPath);
    }

    if (!result) {
        throw FormatError("Compression pipeline failed: " + result.error().message());
    }

    // =========================================================================
    // Update Statistics
    // =========================================================================

    const auto& pipelineStats = pipeline.stats();
    stats_.totalReads = pipelineStats.totalReads;
    stats_.totalBases = pipelineStats.inputBytes;  // Approximate
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

std::unique_ptr<CompressCommand> createCompressCommand(
    const std::string& inputPath,
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
