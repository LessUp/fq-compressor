// =============================================================================
// fq-compressor - Compress Command Implementation
// =============================================================================
// Command handler implementation for FASTQ compression.
//
// Requirements: 6.2
// =============================================================================

#include "compress_command.h"

#include <chrono>
#include <iostream>

#include "fqc/common/logger.h"
#include "fqc/io/compressed_stream.h"
#include "fqc/io/fastq_parser.h"

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
    throw ArgumentError(ErrorCode::kInvalidArgument,
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
        LOG_ERROR("Compression failed: {}", e.what());
        return static_cast<int>(e.code());
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected error: {}", e.what());
        return 1;
    }
}

void CompressCommand::validateOptions() {
    // Check input exists (unless stdin)
    if (options_.inputPath != "-") {
        if (!std::filesystem::exists(options_.inputPath)) {
            throw IOError(ErrorCode::kFileNotFound,
                          "Input file not found: " + options_.inputPath.string());
        }
    }

    // Check output doesn't exist (unless force)
    if (!options_.forceOverwrite && std::filesystem::exists(options_.outputPath)) {
        throw IOError(ErrorCode::kFileExists,
                      "Output file already exists: " + options_.outputPath.string() +
                          " (use -f to overwrite)");
    }

    // Validate compression level
    if (options_.compressionLevel < 1 || options_.compressionLevel > 9) {
        throw ArgumentError(ErrorCode::kInvalidArgument,
                            "Compression level must be 1-9");
    }

    // Streaming mode implies no reordering
    if (options_.streamingMode) {
        options_.enableReordering = false;
        LOG_DEBUG("Streaming mode enabled, disabling reordering");
    }

    // stdin implies streaming mode
    if (options_.inputPath == "-" && !options_.streamingMode) {
        LOG_WARNING("stdin input detected, enabling streaming mode");
        options_.streamingMode = true;
        options_.enableReordering = false;
    }

    LOG_DEBUG("Compression options validated");
    LOG_DEBUG("  Input: {}", options_.inputPath.string());
    LOG_DEBUG("  Output: {}", options_.outputPath.string());
    LOG_DEBUG("  Level: {}", options_.compressionLevel);
    LOG_DEBUG("  Reordering: {}", options_.enableReordering);
    LOG_DEBUG("  Streaming: {}", options_.streamingMode);
    LOG_DEBUG("  Quality mode: {}", qualityModeToString(options_.qualityMode));
}

void CompressCommand::detectReadLengthClass() {
    if (options_.inputPath == "-") {
        // Cannot sample stdin
        LOG_WARNING("Cannot sample stdin for length detection, using MEDIUM strategy");
        detectedLengthClass_ = ReadLengthClass::kMedium;
        return;
    }

    if (options_.scanAllLengths) {
        LOG_INFO("Scanning all reads for length detection (--scan-all-lengths)...");
    } else {
        LOG_DEBUG("Sampling input file for read length detection...");
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
                    LOG_INFO("Scanned {} reads, max length so far: {}",
                             scannedCount, sampleStats.maxLength);
                }
            }
            LOG_INFO("Full scan complete: {} reads scanned", scannedCount);
        } else {
            // Sample records (default: 1000)
            sampleStats = parser.sampleRecords(1000);
        }

        // Detect length class
        detectedLengthClass_ = io::detectReadLengthClass(sampleStats);

        LOG_INFO("Detected read length class: {}",
                 detectedLengthClass_ == ReadLengthClass::kShort    ? "SHORT"
                 : detectedLengthClass_ == ReadLengthClass::kMedium ? "MEDIUM"
                                                                    : "LONG");
        LOG_DEBUG("  {} size: {} reads", options_.scanAllLengths ? "Scan" : "Sample",
                  sampleStats.totalRecords);
        LOG_DEBUG("  Min length: {}", sampleStats.minLength);
        LOG_DEBUG("  Max length: {}", sampleStats.maxLength);
        LOG_DEBUG("  Avg length: {:.1f}", sampleStats.averageLength());

    } catch (const std::exception& e) {
        LOG_WARNING("Failed to {} input: {}, using MEDIUM strategy",
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
            LOG_DEBUG("Using SHORT read strategy: ABC + reordering, 100K reads/block");
            break;

        case ReadLengthClass::kMedium:
            // Medium reads: Zstd, no reordering, 50K block
            options_.blockSize = 50000;
            options_.enableReordering = false;
            if (options_.maxBlockBases == 0) {
                options_.maxBlockBases = 200 * 1024 * 1024;  // 200MB
            }
            LOG_DEBUG("Using MEDIUM read strategy: Zstd, no reordering, 50K reads/block");
            break;

        case ReadLengthClass::kLong:
            // Long reads: Zstd, no reordering, 10K block
            options_.blockSize = 10000;
            options_.enableReordering = false;
            if (options_.maxBlockBases == 0) {
                options_.maxBlockBases = 50 * 1024 * 1024;  // 50MB for ultra-long
            }
            LOG_DEBUG("Using LONG read strategy: Zstd, no reordering, 10K reads/block");
            break;
    }

    // Store the effective length class
    options_.longReadMode = lengthClass;
}

void CompressCommand::runCompression() {
    // TODO: Implement actual compression pipeline
    // This is a placeholder that will be replaced in Phase 2/3

    LOG_INFO("Starting compression...");
    LOG_INFO("  Input: {}", options_.inputPath.string());
    LOG_INFO("  Output: {}", options_.outputPath.string());

    // Open input
    std::unique_ptr<std::istream> inputStream;
    if (options_.inputPath == "-") {
        inputStream = io::openInputFile("-");
    } else {
        inputStream = io::openCompressedFile(options_.inputPath);
    }

    io::FastqParser parser(std::move(inputStream));
    parser.open();

    // Count reads (placeholder - actual compression will process them)
    std::uint64_t readCount = 0;
    std::uint64_t baseCount = 0;

    while (auto record = parser.readRecord()) {
        ++readCount;
        baseCount += record->length();

        // Progress update every 100K reads
        if (options_.showProgress && readCount % 100000 == 0) {
            LOG_INFO("Processed {} reads...", readCount);
        }
    }

    // Update stats
    stats_.totalReads = readCount;
    stats_.totalBases = baseCount;
    stats_.inputBytes = parser.stats().totalBases;  // Approximate

    // TODO: Write actual compressed output
    // For now, just create an empty placeholder file
    std::ofstream outFile(options_.outputPath, std::ios::binary);
    if (!outFile) {
        throw IOError(ErrorCode::kFileOpenFailed,
                      "Failed to create output file: " + options_.outputPath.string());
    }

    // Write placeholder magic
    outFile.write("\x89FQC\r\n\x1a\n", 8);
    outFile.close();

    stats_.outputBytes = 8;  // Placeholder
    stats_.blocksWritten = 0;

    LOG_INFO("Compression complete (placeholder - actual compression not yet implemented)");
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
