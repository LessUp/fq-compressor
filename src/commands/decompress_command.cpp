// =============================================================================
// fq-compressor - Decompress Command Implementation
// =============================================================================
// Command handler implementation for FQC decompression.
//
// Requirements: 6.2, 8.5
// =============================================================================

#include "decompress_command.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

#include "fqc/common/logger.h"
#include "fqc/format/fqc_reader.h"

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
            throw ArgumentError(ErrorCode::kInvalidArgument,
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
        throw ArgumentError(ErrorCode::kInvalidArgument,
                            "Invalid range format: " + std::string(str));
    }

    if (!range.isValid()) {
        throw ArgumentError(ErrorCode::kInvalidArgument,
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
        LOG_ERROR("Decompression failed: {}", e.what());
        return static_cast<int>(e.code());
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected error: {}", e.what());
        return 1;
    }
}

void DecompressCommand::validateOptions() {
    // Check input exists
    if (!std::filesystem::exists(options_.inputPath)) {
        throw IOError(ErrorCode::kFileNotFound,
                      "Input file not found: " + options_.inputPath.string());
    }

    // Check output doesn't exist (unless force or stdout)
    if (options_.outputPath != "-" && !options_.forceOverwrite &&
        std::filesystem::exists(options_.outputPath)) {
        throw IOError(ErrorCode::kFileExists,
                      "Output file already exists: " + options_.outputPath.string() +
                          " (use -f to overwrite)");
    }

    // Validate range if specified
    if (options_.range && !options_.range->isValid()) {
        throw ArgumentError(ErrorCode::kInvalidArgument, "Invalid read range");
    }

    LOG_DEBUG("Decompression options validated");
    LOG_DEBUG("  Input: {}", options_.inputPath.string());
    LOG_DEBUG("  Output: {}", options_.outputPath.string());
    LOG_DEBUG("  Header only: {}", options_.headerOnly);
    LOG_DEBUG("  Original order: {}", options_.originalOrder);
    LOG_DEBUG("  Skip corrupted: {}", options_.skipCorrupted);
}

void DecompressCommand::openArchive() {
    LOG_DEBUG("Opening archive: {}", options_.inputPath.string());

    // TODO: Actually open and validate the archive
    // This is a placeholder for Phase 2/3 implementation

    // For now, just check the file can be opened
    std::ifstream file(options_.inputPath, std::ios::binary);
    if (!file) {
        throw IOError(ErrorCode::kFileOpenFailed,
                      "Failed to open archive: " + options_.inputPath.string());
    }

    // Read and verify magic header
    char magic[8];
    file.read(magic, 8);
    if (file.gcount() < 8) {
        throw FormatError(ErrorCode::kInvalidFormat, "File too small to be a valid .fqc archive");
    }

    // Check magic bytes
    const char expectedMagic[] = "\x89FQC\r\n\x1a\n";
    if (std::memcmp(magic, expectedMagic, 8) != 0) {
        throw FormatError(ErrorCode::kInvalidFormat, "Invalid .fqc magic header");
    }

    LOG_DEBUG("Archive magic header verified");
}

void DecompressCommand::planExtraction() {
    // TODO: Determine which blocks to process based on range
    // This is a placeholder for Phase 2/3 implementation

    if (options_.range) {
        LOG_DEBUG("Planning extraction for range {}:{}", options_.range->start,
                  options_.range->end == 0 ? "end" : std::to_string(options_.range->end));
    } else {
        LOG_DEBUG("Planning full archive extraction");
    }

    // Placeholder: assume single block
    blockRange_ = {0, 1};
}

void DecompressCommand::runDecompression() {
    // TODO: Implement actual decompression pipeline
    // This is a placeholder that will be replaced in Phase 2/3

    LOG_INFO("Starting decompression...");
    LOG_INFO("  Input: {}", options_.inputPath.string());
    LOG_INFO("  Output: {}", options_.outputPath.string());

    // Open output
    std::ostream* output = nullptr;
    std::unique_ptr<std::ofstream> fileOutput;

    if (options_.outputPath == "-") {
        output = &std::cout;
    } else {
        fileOutput = std::make_unique<std::ofstream>(options_.outputPath);
        if (!fileOutput->is_open()) {
            throw IOError(ErrorCode::kFileOpenFailed,
                          "Failed to create output file: " + options_.outputPath.string());
        }
        output = fileOutput.get();
    }

    // TODO: Actually decompress blocks
    // For now, just write a placeholder message

    *output << "# Decompression not yet implemented\n";
    *output << "# Input: " << options_.inputPath.string() << "\n";

    // Update stats (placeholder values)
    stats_.totalReads = 0;
    stats_.totalBases = 0;
    stats_.blocksProcessed = 0;
    stats_.inputBytes = std::filesystem::file_size(options_.inputPath);
    stats_.outputBytes = 0;

    LOG_INFO("Decompression complete (placeholder - actual decompression not yet implemented)");
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
