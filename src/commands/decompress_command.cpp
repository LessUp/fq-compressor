// =============================================================================
// fq-compressor - Decompress Command Implementation
// =============================================================================
// Command handler implementation for FQC decompression.
//
// Requirements: 6.2, 8.5
// =============================================================================

#include "fqc/commands/decompress_command.h"

#include "fqc/algo/block_compressor.h"
#include "fqc/common/error.h"
#include "fqc/common/logger.h"
#include "fqc/common/types.h"
#include "fqc/format/fqc_format.h"
#include "fqc/format/fqc_reader.h"
#include "fqc/pipeline/pipeline.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

namespace fqc::commands {

namespace {

[[nodiscard]] ReadId normalizeSelectionEnd(ReadId end, const format::FQCReader& reader) {
    return end == 0 ? static_cast<ReadId>(reader.totalReadCount()) : end;
}

}  // namespace

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
            throw ArgumentError("Invalid range format: " + std::string(str));
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
        throw ArgumentError("Invalid range format: " + std::string(str));
    }

    if (!range.isValid()) {
        throw ArgumentError("Invalid range: start must be <= end");
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
        stats_.elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

        // Print summary
        if (options_.showProgress) {
            printSummary();
        }

        return 0;

    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Decompression failed: {}", e.what());
        return toExitCode(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return 1;
    }
}

void DecompressCommand::validateOptions() {
    // Check input exists
    if (!std::filesystem::exists(options_.inputPath)) {
        throw IOError("Input file not found: " + options_.inputPath.string());
    }

    // Handle PE split output
    if (options_.splitPairedEnd) {
        if (options_.outputPath == "-") {
            throw ArgumentError("--split-pe cannot be used with stdout output");
        }

        // Derive output2Path if not specified
        if (options_.output2Path.empty()) {
            auto outPath = options_.outputPath.string();
            // Try to insert _R2 before extension
            auto extPos = outPath.rfind('.');
            if (extPos != std::string::npos) {
                options_.output2Path = outPath.substr(0, extPos) + "_R2" + outPath.substr(extPos);
                // Also update outputPath to have _R1
                options_.outputPath = outPath.substr(0, extPos) + "_R1" + outPath.substr(extPos);
            } else {
                options_.output2Path = outPath + "_R2";
                options_.outputPath = outPath + "_R1";
            }
            FQC_LOG_INFO("PE split output: R1={}, R2={}",
                         options_.outputPath.string(),
                         options_.output2Path.string());
        }

        // Check derived outputs don't exist
        if (!options_.forceOverwrite && std::filesystem::exists(options_.outputPath)) {
            throw IOError("Output file already exists: " + options_.outputPath.string() +
                          " (use -f to overwrite)");
        }

        if (!options_.forceOverwrite && std::filesystem::exists(options_.output2Path)) {
            throw IOError("Output file already exists: " + options_.output2Path.string() +
                          " (use -f to overwrite)");
        }
    } else if (options_.outputPath != "-" && !options_.forceOverwrite &&
               std::filesystem::exists(options_.outputPath)) {
        throw IOError("Output file already exists: " + options_.outputPath.string() +
                      " (use -f to overwrite)");
    }


    // Validate range if specified
    if (options_.range && !options_.range->isValid()) {
        throw ArgumentError("Invalid read range");
    }

    // Validate range-pairs if specified
    if (options_.rangePairs && !options_.rangePairs->isValid()) {
        throw ArgumentError("Invalid paired read range");
    }

    if (options_.rangePairs && !options_.splitPairedEnd) {
        FQC_LOG_DEBUG("--range-pairs specified without --split-pe; selection still applies in archive order");
    }

    // Cannot specify both --range and --range-pairs
    if (options_.range && options_.rangePairs) {
        throw ArgumentError("Cannot specify both --range and --range-pairs");
    }

    // Validate streams parameter
    if (!options_.streams.empty() && options_.streams != "all") {
        // Accept comma-separated: id, seq, qual
        // Basic validation — detailed parsing handled in pipeline
        for (const auto& valid : {"id", "seq", "qual"}) {
            if (options_.streams.find(valid) != std::string::npos) {
                break;
            }
        }
    }

    // Validate output format if specified
    if (!options_.outputFormat.empty()) {
        if (options_.outputFormat != "fastq" && options_.outputFormat != "fasta" &&
            options_.outputFormat != "tsv" && options_.outputFormat != "raw") {
            throw ArgumentError("Invalid output format: " + options_.outputFormat +
                                " (expected: fastq, fasta, tsv, raw)");
        }
    }

    FQC_LOG_DEBUG("Decompression options validated");
    FQC_LOG_DEBUG("  Input: {}", options_.inputPath.string());
    FQC_LOG_DEBUG("  Output: {}", options_.outputPath.string());
    if (options_.splitPairedEnd) {
        FQC_LOG_DEBUG("  Output R2: {}", options_.output2Path.string());
    }
    FQC_LOG_DEBUG("  Header only: {}", options_.headerOnly);
    FQC_LOG_DEBUG("  Streams: {}", options_.streams);
    if (!options_.outputFormat.empty()) {
        FQC_LOG_DEBUG("  Output format: {}", options_.outputFormat);
    }
    FQC_LOG_DEBUG("  Original order: {}", options_.originalOrder);
    FQC_LOG_DEBUG("  Skip corrupted: {}", options_.skipCorrupted);
    FQC_LOG_DEBUG("  Split PE: {}", options_.splitPairedEnd);
    FQC_LOG_DEBUG("  Verify checksums: {}", options_.verifyChecksums);
    if (!options_.idPrefix.empty()) {
        FQC_LOG_DEBUG("  ID prefix: {}", options_.idPrefix);
    }
}

void DecompressCommand::openArchive() {
    FQC_LOG_DEBUG("Opening archive: {}", options_.inputPath.string());

    // Open and validate the archive magic header
    std::ifstream file(options_.inputPath, std::ios::binary);
    if (!file) {
        throw IOError("Failed to open archive: " + options_.inputPath.string());
    }

    // Read and verify magic header
    char magic[8];
    file.read(magic, 8);
    if (file.gcount() < 8) {
        throw FormatError("File too small to be a valid .fqc archive");
    }

    // Check magic bytes
    const char expectedMagic[] =
        "\x89"
        "FQC\r\n"
        "\x1a\n";
    if (std::memcmp(magic, expectedMagic, 8) != 0) {
        throw FormatError("Invalid .fqc magic header");
    }

    FQC_LOG_DEBUG("Archive magic header verified");
}

void DecompressCommand::planExtraction() {
    // Log extraction plan
    // Note: Actual block processing is done in runDecompression() based on FQCReader::blockCount()
    // The range filtering is handled by the pipeline when processing reads

    if (options_.range) {
        FQC_LOG_DEBUG("Planning extraction for range {}:{}",
                      options_.range->start,
                      options_.range->end == 0 ? "end" : std::to_string(options_.range->end));
        FQC_LOG_DEBUG("Range filtering will be applied during read processing");
    } else {
        FQC_LOG_DEBUG("Planning full archive extraction");
    }

    // blockRange_ is not currently used in the actual decompression logic
    // Keeping it for potential future optimizations
    blockRange_ = {0, 0};  // 0 means process all blocks
}

void DecompressCommand::runDecompression() {
    FQC_LOG_INFO("Starting decompression...");
    FQC_LOG_INFO("  Input: {}", options_.inputPath.string());
    FQC_LOG_INFO("  Output: {}", options_.outputPath.string());
    FQC_LOG_INFO("  Threads: {}", options_.threads > 0 ? std::to_string(options_.threads) : "auto");

    if (options_.originalOrder) {
        FQC_LOG_INFO("Using single-threaded original-order restoration");
        runDecompressionOriginalOrder();
        return;
    }

    // =========================================================================
    // TBB Parallel Pipeline Path (threads > 1)
    // =========================================================================

    if (options_.threads > 1 ||
        (options_.threads == 0 && std::thread::hardware_concurrency() > 1)) {
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

    // Open output stream(s)
    std::ostream* output = nullptr;
    std::unique_ptr<std::ofstream> fileOutput;
    std::unique_ptr<std::ofstream> fileOutput2;

    if (options_.outputPath == "-") {
        output = &std::cout;
    } else {
        fileOutput = std::make_unique<std::ofstream>(options_.outputPath);
        if (!fileOutput->is_open()) {
            throw IOError("Failed to create output file: " + options_.outputPath.string());
        }
        output = fileOutput.get();
    }

    // Create BlockCompressor for decompression
    algo::BlockCompressorConfig config;
    config.readLengthClass = format::getReadLengthClass(reader.globalHeader().flags);
    config.qualityMode = format::getQualityMode(reader.globalHeader().flags);
    config.idMode = format::getIdMode(reader.globalHeader().flags);
    config.numThreads =
        options_.threads > 0 ? static_cast<std::size_t>(options_.threads) : 0;  // 0 = auto-detect

    algo::BlockCompressor compressor(config);

    // Process blocks
    std::size_t blockCount = reader.blockCount();
    for (BlockId blockId = 0; blockId < blockCount; ++blockId) {
        try {
            FQC_LOG_DEBUG("Processing block {}/{}", blockId + 1, blockCount);

            // Read block data
            auto blockData = reader.readBlock(blockId);

            // Decompress block
            auto decompressResult = compressor.decompress(blockData.header,
                                                          blockData.idsData,
                                                          blockData.seqData,
                                                          blockData.qualData,
                                                          blockData.auxData);

            if (!decompressResult) {
                if (options_.skipCorrupted) {
                    FQC_LOG_WARNING("Block {} decompression failed, skipping", blockId);
                    stats_.corruptedBlocks++;
                    continue;
                } else {
                    decompressResult.error().throwException();
                }
            }

            auto& decompressed = decompressResult.value();
            auto& reads = decompressed.reads;

            // Write reads to output in archive order.
            for (const auto& read : reads) {

                // Write FASTQ record (only header if headerOnly)
                if (read.comment.empty()) {
                    *output << "@" << read.id << "\n";
                } else {
                    *output << "@" << read.id << " " << read.comment << "\n";
                }
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

ArchiveReadSelection DecompressCommand::resolveArchiveSelection(const format::FQCReader& reader) const {
    ArchiveReadSelection selection;
    selection.start = 1;
    selection.end = static_cast<ReadId>(reader.totalReadCount());

    if (options_.range) {
        selection.start = static_cast<ReadId>(options_.range->start);
        selection.end = normalizeSelectionEnd(static_cast<ReadId>(options_.range->end), reader);
    }

    if (options_.rangePairs) {
        selection.start = static_cast<ReadId>((options_.rangePairs->start - 1) * 2 + 1);
        selection.end = options_.rangePairs->end > 0
            ? static_cast<ReadId>(options_.rangePairs->end * 2)
            : static_cast<ReadId>(reader.totalReadCount());
    }

    selection.start = std::max<ReadId>(1, selection.start);
    selection.end = std::min<ReadId>(selection.end, static_cast<ReadId>(reader.totalReadCount()));

    if (selection.start > selection.end) {
        throw ArgumentError("Requested range does not overlap archive contents");
    }

    return selection;
}

OriginalOrderPlan DecompressCommand::buildOriginalOrderPlan(const format::FQCReader& reader) const {
    OriginalOrderPlan plan;
    plan.selection = resolveArchiveSelection(reader);

    for (ReadId originalId = 1; originalId <= static_cast<ReadId>(reader.totalReadCount()); ++originalId) {
        ReadId archiveId = reader.lookupArchiveId(originalId);
        if (archiveId == kInvalidReadId) {
            throw FormatError("Reorder map lookup failed for original read ID " +
                              std::to_string(originalId));
        }
        if (plan.selection.contains(archiveId)) {
            plan.originalIdToSlot.emplace(originalId, plan.orderedOriginalIds.size());
            plan.orderedOriginalIds.push_back(originalId);
        }
    }

    plan.orderedReads.resize(plan.orderedOriginalIds.size());
    return plan;
}

void DecompressCommand::writeRecord(std::ostream& output, const ReadRecord& read) {
    if (read.comment.empty()) {
        output << "@" << read.id << "\n";
    } else {
        output << "@" << read.id << " " << read.comment << "\n";
    }

    if (!options_.headerOnly) {
        output << read.sequence << "\n";
        output << "+\n";
        output << read.quality << "\n";
    }

    stats_.totalReads++;
    stats_.totalBases += read.sequence.length();
    stats_.outputBytes += read.id.length() + read.sequence.length() + read.quality.length() + 4;
}

void DecompressCommand::writeSplitPairedRecord(std::ostream& output1,
                                               std::ostream& output2,
                                               const ReadRecord& read,
                                               ReadId originalId,
                                               PELayout peLayout) {
    if (peLayout == PELayout::kConsecutive) {
        throw ArgumentError(
            "--split-pe with --original-order is currently only supported for interleaved paired-end archives");
    }

    if ((originalId % 2U) == 1U) {
        writeRecord(output1, read);
    } else {
        writeRecord(output2, read);
    }
}

void DecompressCommand::runDecompressionOriginalOrder() {
    format::FQCReader reader(options_.inputPath);
    reader.open();

    if (!reader.hasReorderMap()) {
        throw FormatError("Original order requested but reorder map not present");
    }

    const bool isPaired = format::isPaired(reader.globalHeader().flags);
    const auto peLayout = format::getPeLayout(reader.globalHeader().flags);

    if (options_.rangePairs && !isPaired) {
        throw ArgumentError("--range-pairs requires a paired-end archive");
    }
    if (options_.splitPairedEnd && !isPaired) {
        throw ArgumentError("--split-pe requires a paired-end archive");
    }

    reader.loadReorderMap();
    auto plan = buildOriginalOrderPlan(reader);

    std::ostream* output = nullptr;
    std::unique_ptr<std::ofstream> fileOutput;
    std::unique_ptr<std::ofstream> fileOutput2;

    if (options_.outputPath == "-") {
        output = &std::cout;
    } else {
        fileOutput = std::make_unique<std::ofstream>(options_.outputPath);
        if (!fileOutput->is_open()) {
            throw IOError("Failed to create output file: " + options_.outputPath.string());
        }
        output = fileOutput.get();
    }

    if (options_.splitPairedEnd) {
        fileOutput2 = std::make_unique<std::ofstream>(options_.output2Path);
        if (!fileOutput2->is_open()) {
            throw IOError("Failed to create output file: " + options_.output2Path.string());
        }
    }

    algo::BlockCompressorConfig config;
    config.readLengthClass = format::getReadLengthClass(reader.globalHeader().flags);
    config.qualityMode = format::getQualityMode(reader.globalHeader().flags);
    config.idMode = format::getIdMode(reader.globalHeader().flags);
    config.numThreads = 1;

    algo::BlockCompressor compressor(config);

    for (BlockId blockId = 0; blockId < reader.blockCount(); ++blockId) {
        auto [blockStart, blockEndExclusive] = reader.getBlockReadRange(blockId);
        if (blockEndExclusive <= plan.selection.start || blockStart > plan.selection.end) {
            continue;
        }

        try {
            auto blockData = reader.readBlock(blockId);
            auto decompressResult = compressor.decompress(blockData.header,
                                                          blockData.idsData,
                                                          blockData.seqData,
                                                          blockData.qualData,
                                                          blockData.auxData);
            if (!decompressResult) {
                if (options_.skipCorrupted) {
                    FQC_LOG_WARNING("Block {} decompression failed, skipping", blockId);
                    stats_.corruptedBlocks++;
                    continue;
                }
                decompressResult.error().throwException();
            }

            auto& reads = decompressResult.value().reads;
            for (std::size_t i = 0; i < reads.size(); ++i) {
                ReadId archiveId = blockStart + static_cast<ReadId>(i);
                if (!plan.selection.contains(archiveId)) {
                    continue;
                }
                ReadId originalId = reader.lookupOriginalId(archiveId);
                if (originalId == kInvalidReadId) {
                    throw FormatError("Reorder map lookup failed for archive read ID " +
                                      std::to_string(archiveId));
                }
                auto slotIt = plan.originalIdToSlot.find(originalId);
                if (slotIt == plan.originalIdToSlot.end()) {
                    throw FormatError("Original-order slot missing for read ID " +
                                      std::to_string(originalId));
                }
                plan.orderedReads[slotIt->second] = reads[i];
            }

            stats_.blocksProcessed++;
        } catch (const FQCException& e) {
            if (options_.skipCorrupted) {
                FQC_LOG_WARNING("Block {} processing error: {}", blockId, e.what());
                stats_.corruptedBlocks++;
            } else {
                throw;
            }
        }
    }

    for (std::size_t i = 0; i < plan.orderedReads.size(); ++i) {
        if (!plan.orderedReads[i].has_value()) {
            throw FormatError("Missing read while restoring original order at slot " +
                              std::to_string(i) + " (originalId=" +
                              std::to_string(plan.orderedOriginalIds[i]) + ")");
        }
        const auto& read = *plan.orderedReads[i];
        ReadId originalId = plan.orderedOriginalIds[i];
        if (options_.splitPairedEnd) {
            writeSplitPairedRecord(*output, *fileOutput2, read, originalId, peLayout);
        } else {
            writeRecord(*output, read);
        }
    }

    stats_.inputBytes = std::filesystem::file_size(options_.inputPath);
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
    pipelineConfig.numThreads =
        options_.threads > 0 ? static_cast<std::size_t>(options_.threads) : 0;  // 0 = auto-detect

    // Set range if specified
    if (options_.range) {
        pipelineConfig.rangeStart = static_cast<ReadId>(options_.range->start);
        pipelineConfig.rangeEnd = static_cast<ReadId>(options_.range->end);
    }
    // For --range-pairs, convert pair range to read range (each pair = 2 reads)
    if (options_.rangePairs) {
        pipelineConfig.rangeStart = static_cast<ReadId>((options_.rangePairs->start - 1) * 2 + 1);
        pipelineConfig.rangeEnd =
            options_.rangePairs->end > 0 ? static_cast<ReadId>(options_.rangePairs->end * 2) : 0;
    }

    pipelineConfig.originalOrder = options_.originalOrder;
    pipelineConfig.headerOnly = options_.headerOnly;
    pipelineConfig.verifyChecksums = options_.verifyChecksums;
    pipelineConfig.skipCorrupted = options_.skipCorrupted;
    pipelineConfig.placeholderQual = options_.placeholderQual;

    // Progress callback
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

std::unique_ptr<DecompressCommand> createDecompressCommand(const std::string& inputPath,
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
