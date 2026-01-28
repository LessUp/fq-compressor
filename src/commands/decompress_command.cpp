// =============================================================================
// fq-compressor - Decompress Command Implementation
// =============================================================================
// Command handler implementation for FQC decompression.
//
// Requirements: 6.2, 8.5
// =============================================================================

#include "decompress_command.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>

#include "fqc/common/error.h"
#include "fqc/common/logger.h"
#include "fqc/common/types.h"
#include "fqc/algo/block_compressor.h"
#include "fqc/format/fqc_format.h"
#include "fqc/format/fqc_reader.h"
#include "fqc/pipeline/pipeline.h"

namespace fqc::commands {

// =============================================================================
// Range Parsing
// =============================================================================

ReadRange parseRange(std::string_view str) {
    ReadRange range;

    if (str.empty()) {
        return range;  // Default: all reads
    }

    auto colonPos = str.find(':');
    if (colonPos == std::string_view::npos) {
        // Single number - just that read
        try {
            range.start = std::stoull(std::string(str));
            range.end = range.start;
        } catch (const std::exception&) {
            throw ArgumentError(
                                "Invalid range format: " + std::string(str));
        }
        return range;
    }

    // Parse start:end format
    auto startStr = str.substr(0, colonPos);
    auto endStr = str.substr(colonPos + 1);

    try {
        if (!startStr.empty()) {
            range.start = std::stoull(std::string(startStr));
        }
        if (!endStr.empty()) {
            range.end = std::stoull(std::string(endStr));
        }
    } catch (const std::exception&) {
        throw ArgumentError(
                            "Invalid range format: " + std::string(str));
    }

    if (!range.isValid()) {
        throw ArgumentError(
                            "Invalid range: start must be <= end");
    }

    return range;
}

// =============================================================================
// DecompressCommand Implementation
// =============================================================================

DecompressCommand::DecompressCommand(DecompressOptions options)
    : options_(std::move(options)), blockRange_{0, 0} {}

DecompressCommand::~DecompressCommand() = default;

DecompressCommand::DecompressCommand(DecompressCommand&&) noexcept = default;
DecompressCommand& DecompressCommand::operator=(DecompressCommand&&) noexcept = default;

int DecompressCommand::execute() {
    auto startTime = std::chrono::steady_clock::now();

    try {
        // Validate options
        validateOptions();

        // Open archive
        openArchive();

        // Plan extraction
        planExtraction();

        // Run decompression
        runDecompression();

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
        FQC_LOG_ERROR("Decompression failed: {}", e.what());
        return static_cast<int>(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return 1;
    }
}

void DecompressCommand::validateOptions() {
    // Check input exists
    if (!std::filesystem::exists(options_.inputPath)) {
        throw IOError(
                      "Input file not found: " + options_.inputPath.string());
    }

    // Check output doesn't exist (unless force or stdout)
    if (options_.outputPath != "-" && !options_.forceOverwrite &&
        std::filesystem::exists(options_.outputPath)) {
        throw IOError(
                      "Output file already exists: " + options_.outputPath.string() +
                          " (use -f to overwrite)");
    }

    // Handle PE split output
    if (options_.splitPairedEnd) {
        if (options_.outputPath == "-") {
            throw ArgumentError(
                                "--split-pe cannot be used with stdout output");
        }

        // Derive output2Path if not specified
        if (options_.output2Path.empty()) {
            auto outPath = options_.outputPath.string();
            // Try to insert _R2 before extension
            auto extPos = outPath.rfind('.');
            if (extPos != std::string::npos) {
                options_.output2Path = outPath.substr(0, extPos) + "_R2" +
                                       outPath.substr(extPos);
                // Also update outputPath to have _R1
                options_.outputPath = outPath.substr(0, extPos) + "_R1" +
                                      outPath.substr(extPos);
            } else {
                options_.output2Path = outPath + "_R2";
                options_.outputPath = outPath + "_R1";
            }
            FQC_LOG_INFO("PE split output: R1={}, R2={}",
                     options_.outputPath.string(), options_.output2Path.string());
        }

        // Check output2 doesn't exist
        if (!options_.forceOverwrite && std::filesystem::exists(options_.output2Path)) {
            throw IOError(
                          "Output file already exists: " + options_.output2Path.string() +
                              " (use -f to overwrite)");
        }
    }

    // Validate range if specified
    if (options_.range && !options_.range->isValid()) {
        throw ArgumentError( "Invalid read range");
    }

    FQC_LOG_DEBUG("Decompression options validated");
    FQC_LOG_DEBUG("  Input: {}", options_.inputPath.string());
    FQC_LOG_DEBUG("  Output: {}", options_.outputPath.string());
    if (options_.splitPairedEnd) {
        FQC_LOG_DEBUG("  Output R2: {}", options_.output2Path.string());
    }
    FQC_LOG_DEBUG("  Header only: {}", options_.headerOnly);
    FQC_LOG_DEBUG("  Original order: {}", options_.originalOrder);
    FQC_LOG_DEBUG("  Skip corrupted: {}", options_.skipCorrupted);
    FQC_LOG_DEBUG("  Split PE: {}", options_.splitPairedEnd);
}

void DecompressCommand::openArchive() {
    FQC_LOG_DEBUG("Opening archive: {}", options_.inputPath.string());

    // TODO: Actually open and validate the archive
    // This is a placeholder for Phase 2/3 implementation

    // For now, just check the file can be opened
    std::ifstream file(options_.inputPath, std::ios::binary);
    if (!file) {
        throw IOError(
                      "Failed to open archive: " + options_.inputPath.string());
    }

    // Read and verify magic header
    char magic[8];
    file.read(magic, 8);
    if (file.gcount() < 8) {
        throw FormatError( "File too small to be a valid .fqc archive");
    }

    // Check magic bytes
    const char expectedMagic[] = "\x89" "FQC\r\n" "\x1a\n";
    if (std::memcmp(magic, expectedMagic, 8) != 0) {
        throw FormatError( "Invalid .fqc magic header");
    }

    FQC_LOG_DEBUG("Archive magic header verified");
}

void DecompressCommand::planExtraction() {
    // TODO: Determine which blocks to process based on range
    // This is a placeholder for Phase 2/3 implementation

    if (options_.range) {
        FQC_LOG_DEBUG("Planning extraction for range {}:{}", options_.range->start,
                  options_.range->end == 0 ? "end" : std::to_string(options_.range->end));
    } else {
        FQC_LOG_DEBUG("Planning full archive extraction");
    }

    // Placeholder: assume single block
    blockRange_ = {0, 1};
}

void DecompressCommand::runDecompression() {
    FQC_LOG_INFO("Starting decompression...");
    FQC_LOG_INFO("  Input: {}", options_.inputPath.string());
    FQC_LOG_INFO("  Output: {}", options_.outputPath.string());
    FQC_LOG_INFO("  Threads: {}", options_.threads > 0 ? std::to_string(options_.threads) : "auto");

    // =========================================================================
    // TBB Parallel Pipeline Path (threads > 1)
    // =========================================================================

    if (options_.threads > 1 || (options_.threads == 0 && std::thread::hardware_concurrency() > 1)) {
        FQC_LOG_INFO("Using TBB parallel pipeline for decompression");
        runDecompressionParallel();
        return;
    }

    // =========================================================================
    // Single-threaded Path (threads == 1)
    // =========================================================================

    FQC_LOG_INFO("Using single-threaded decompression");

    // Open FQC reader
    format::FQCReader reader(options_.inputPath);
    reader.open();

    // Load reorder map if needed
    if (options_.originalOrder) {
        if (!reader.hasReorderMap()) {
            throw FormatError("Original order requested but reorder map not present");
        }
        FQC_LOG_DEBUG("Loading reorder map...");
        reader.loadReorderMap();
    }

    // Open output stream(s)
    std::ostream* output = nullptr;
    std::unique_ptr<std::ofstream> fileOutput;
    std::unique_ptr<std::ofstream> fileOutput2;

    if (options_.outputPath == "-") {
        output = &std::cout;
    } else {
        fileOutput = std::make_unique<std::ofstream>(options_.outputPath);
        if (!fileOutput->is_open()) {
            throw IOError(
                          "Failed to create output file: " + options_.outputPath.string());
        }
        output = fileOutput.get();
    }

    // Create BlockCompressor for decompression
    algo::BlockCompressorConfig config;
    config.readLengthClass = format::getReadLengthClass(reader.globalHeader().flags);
    config.qualityMode = format::getQualityMode(reader.globalHeader().flags);
    config.idMode = format::getIdMode(reader.globalHeader().flags);
    config.numThreads = options_.threads > 0 ? static_cast<std::size_t>(options_.threads) : 0;  // 0 = auto-detect

    algo::BlockCompressor compressor(config);

    // Process blocks
    std::size_t blockCount = reader.blockCount();
    for (BlockId blockId = 0; blockId < blockCount; ++blockId) {
        try {
            FQC_LOG_DEBUG("Processing block {}/{}", blockId + 1, blockCount);

            // Read block data
            auto blockData = reader.readBlock(blockId);

            // Decompress block
            auto decompressResult = compressor.decompress(
                blockData.header,
                blockData.idsData,
                blockData.seqData,
                blockData.qualData,
                blockData.auxData
            );

            if (!decompressResult) {
                if (options_.skipCorrupted) {
                    FQC_LOG_WARNING("Block {} decompression failed, skipping", blockId);
                    stats_.corruptedBlocks++;
                    continue;
                } else {
                    throw decompressResult.error();
                }
            }

            auto& decompressed = decompressResult.value();
            auto& reads = decompressed.reads;

            // Write reads to output
            for (const auto& read : reads) {
                // Handle original order if needed
                // Note: Original order reordering is currently a placeholder
                // Full implementation requires buffering and sorting by original ID
                if (options_.originalOrder && reader.reorderMap()) {
                    ReadId archiveId = stats_.totalReads + 1;
                    ReadId originalId = reader.lookupOriginalId(archiveId);
                    (void)originalId;  // Suppress unused warning - will be used for sorting
                }

                // Write FASTQ record (only header if headerOnly)
                *output << "@" << read.id << "\n";
                if (!options_.headerOnly) {
                    *output << read.sequence << "\n";
                    *output << "+\n";
                    *output << read.quality << "\n";
                }

                stats_.totalReads++;
                stats_.totalBases += read.sequence.length();
                stats_.outputBytes += read.id.length() + read.sequence.length() +
                                    read.quality.length() + 4;  // +4 for @, +, \n chars
            }

            stats_.blocksProcessed++;

        } catch (const FQCException& e) {
            if (options_.skipCorrupted) {
                FQC_LOG_WARNING("Block {} processing error: {}", blockId, e.what());
                stats_.corruptedBlocks++;
            } else {
                throw;
            }
        } catch (const std::exception& e) {
            if (options_.skipCorrupted) {
                FQC_LOG_WARNING("Block {} processing error: {}", blockId, e.what());
                stats_.corruptedBlocks++;
            } else {
                throw;
            }
        }
    }

    // Update final stats
    stats_.inputBytes = std::filesystem::file_size(options_.inputPath);

    FQC_LOG_INFO("Decompression complete");
    FQC_LOG_INFO("  Total reads: {}", stats_.totalReads);
    FQC_LOG_INFO("  Total bases: {}", stats_.totalBases);
    FQC_LOG_INFO("  Blocks processed: {}", stats_.blocksProcessed);
    if (stats_.corruptedBlocks > 0) {
        FQC_LOG_WARNING("  Corrupted blocks: {}", stats_.corruptedBlocks);
    }
}

void DecompressCommand::printSummary() const {
    std::cout << "\n=== Decompression Summary ===" << std::endl;
    std::cout << "  Total reads:       " << stats_.totalReads << std::endl;
    std::cout << "  Total bases:       " << stats_.totalBases << std::endl;
    std::cout << "  Blocks processed:  " << stats_.blocksProcessed << std::endl;
    if (stats_.corruptedBlocks > 0) {
        std::cout << "  Corrupted blocks:  " << stats_.corruptedBlocks << std::endl;
    }
    if (stats_.checksumFailures > 0) {
        std::cout << "  Checksum failures: " << stats_.checksumFailures << std::endl;
    }
    std::cout << "  Input size:        " << stats_.inputBytes << " bytes" << std::endl;
    std::cout << "  Output size:       " << stats_.outputBytes << " bytes" << std::endl;
    std::cout << "  Elapsed time:      " << std::fixed << std::setprecision(2)
              << stats_.elapsedSeconds << " s" << std::endl;
    std::cout << "  Throughput:        " << std::fixed << std::setprecision(2)
              << stats_.throughputMbps() << " MB/s" << std::endl;
    std::cout << "==============================" << std::endl;
}

void DecompressCommand::runDecompressionParallel() {
    FQC_LOG_INFO("Initializing parallel decompression pipeline...");

    // =========================================================================
    // Configure Pipeline
    // =========================================================================

    pipeline::DecompressionPipelineConfig pipelineConfig;
    pipelineConfig.numThreads = options_.threads > 0
        ? static_cast<std::size_t>(options_.threads)
        : 0;  // 0 = auto-detect

    // Set range if specified
    if (options_.range) {
        pipelineConfig.rangeStart = static_cast<ReadId>(options_.range->start);
        pipelineConfig.rangeEnd = static_cast<ReadId>(options_.range->end);
    }

    pipelineConfig.originalOrder = options_.originalOrder;
    pipelineConfig.headerOnly = options_.headerOnly;
    pipelineConfig.verifyChecksums = options_.verifyChecksums;
    pipelineConfig.skipCorrupted = options_.skipCorrupted;

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
    FQC_LOG_INFO("  Original order: {}", pipelineConfig.originalOrder ? "yes" : "no");
    FQC_LOG_INFO("  Verify checksums: {}", pipelineConfig.verifyChecksums ? "yes" : "no");
    FQC_LOG_INFO("  Skip corrupted: {}", pipelineConfig.skipCorrupted ? "yes" : "no");

    // =========================================================================
    // Execute Pipeline
    // =========================================================================

    pipeline::DecompressionPipeline pipeline(pipelineConfig);

    VoidResult result;
    if (options_.splitPairedEnd) {
        // Paired-end split mode
        result = pipeline.runPaired(options_.inputPath, options_.outputPath, options_.output2Path);
    } else {
        // Single output mode
        result = pipeline.run(options_.inputPath, options_.outputPath);
    }

    if (!result) {
        throw FormatError("Decompression pipeline failed: " + result.error().message());
    }

    // =========================================================================
    // Update Statistics
    // =========================================================================

    const auto& pipelineStats = pipeline.stats();
    stats_.totalReads = pipelineStats.totalReads;
    stats_.totalBases = pipelineStats.inputBytes;  // Approximate
    stats_.blocksProcessed = pipelineStats.totalBlocks;
    stats_.inputBytes = pipelineStats.inputBytes;
    stats_.outputBytes = pipelineStats.outputBytes;

    FQC_LOG_INFO("Parallel decompression complete!");
    FQC_LOG_INFO("  Blocks processed: {}", stats_.blocksProcessed);
    FQC_LOG_INFO("  Total reads: {}", stats_.totalReads);
    FQC_LOG_INFO("  Throughput: {:.2f} MB/s", pipelineStats.throughputMBps());
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<DecompressCommand> createDecompressCommand(
    const std::string& inputPath,
    const std::string& outputPath,
    const std::string& range,
    bool headerOnly,
    bool originalOrder,
    bool skipCorrupted,
    const std::string& corruptedPlaceholder,
    bool splitPe,
    int threads,
    bool force) {

    DecompressOptions opts;
    opts.inputPath = inputPath;
    opts.outputPath = outputPath;
    opts.headerOnly = headerOnly;
    opts.originalOrder = originalOrder;
    opts.skipCorrupted = skipCorrupted;
    opts.splitPairedEnd = splitPe;
    opts.threads = threads;
    opts.forceOverwrite = force;

    if (!corruptedPlaceholder.empty()) {
        opts.corruptedPlaceholder = corruptedPlaceholder;
    }

    if (!range.empty()) {
        opts.range = parseRange(range);
    }

    return std::make_unique<DecompressCommand>(std::move(opts));
}

}  // namespace fqc::commands
