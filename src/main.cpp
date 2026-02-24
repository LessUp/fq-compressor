// =============================================================================
// fq-compressor - High Performance FASTQ Compressor
// =============================================================================
// Main entry point for the fqc command-line tool.
//
// This file implements the CLI framework using CLI11, providing:
// - Subcommands: compress, decompress, info, verify
// - Global options: threads, verbose, memory-limit
// - TTY detection for progress display
// - stdin detection for streaming mode
//
// Requirements: 6.1, 6.2, 6.3
// =============================================================================

#include <CLI/CLI.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "fqc/common/error.h"
#include "fqc/common/logger.h"
#include "fqc/common/types.h"

// Command implementations
#include "commands/compress_command.h"
#include "commands/decompress_command.h"
#include "commands/info_command.h"
#include "commands/verify_command.h"

// Forward declarations for command handlers
namespace fqc::commands {
int runCompress(CLI::App* app);
int runDecompress(CLI::App* app);
int runInfo(CLI::App* app);
int runVerify(CLI::App* app);
}  // namespace fqc::commands

namespace {

// =============================================================================
// Version Information
// =============================================================================

constexpr const char* kVersion = "0.1.0";
constexpr const char* kDescription =
    "fq-compressor: High-performance FASTQ compressor with random access support\n"
    "Combines Spring's ABC algorithm with modern C++20 and TBB parallelism.\n\n"
    "Note: The .fqc format is already highly compressed (0.4-0.6 bits/base).\n"
    "External compression (gzip/xz) provides minimal benefit and breaks random access.\n"
    "For distribution, wrap with 'xz' externally and unwrap before use.";

// =============================================================================
// Global Options
// =============================================================================

struct GlobalOptions {
    int threads = 0;           // 0 = auto-detect
    int verbosity = 0;         // 0 = normal, 1 = verbose, 2 = debug
    std::size_t memoryLimit = 0;  // 0 = no limit (in MB)
    bool quiet = false;
    bool noProgress = false;
};

GlobalOptions gOptions;

// =============================================================================
// TTY Detection
// =============================================================================

/// @brief Check if stdout is a TTY.
[[nodiscard]] bool isStdoutTty() noexcept {
    return isatty(fileno(stdout)) != 0;
}

// =============================================================================
// Compress Command Options
// =============================================================================

struct CliCompressOptions {
    std::string input;
    std::string input2;               // Second input for PE (R2)
    std::string output;
    int level = 6;                    // Compression level (1-9)
    bool reorder = true;              // Enable global reordering
    bool streaming = false;           // Streaming mode (no reordering)
    std::string lossyQuality;         // Lossy quality mode
    std::string longReadMode = "auto";  // auto, short, medium, long
    std::size_t maxBlockBases = 0;    // Max bases per block (0 = default)
    bool scanAllLengths = false;      // Scan all reads for length detection
    bool interleaved = false;         // Input is interleaved PE
    std::string peLayout = "interleaved";  // PE storage layout
    bool force = false;               // Overwrite existing output
};

CliCompressOptions gCompressOpts;

// =============================================================================
// Decompress Command Options
// =============================================================================

struct CliDecompressOptions {
    std::string input;
    std::string output;
    std::string range;           // Read range (e.g., "1:1000")
    bool headerOnly = false;     // Only output headers
    bool originalOrder = false;  // Output in original order
    bool skipCorrupted = false;  // Skip corrupted blocks
    std::string corruptedPlaceholder;  // Placeholder for corrupted reads
    bool splitPe = false;        // Split PE output
};

CliDecompressOptions gDecompressOpts;

// =============================================================================
// Info Command Options
// =============================================================================

struct CliInfoOptions {
    std::string input;
    bool json = false;  // Output as JSON
    bool detailed = false;  // Show detailed block info
};

CliInfoOptions gInfoOpts;

// =============================================================================
// Verify Command Options
// =============================================================================

struct CliVerifyOptions {
    std::string input;
    bool failFast = false;  // Stop on first error
    bool verbose = false;   // Show detailed verification
};

CliVerifyOptions gVerifyOpts;

// =============================================================================
// Command Setup Functions
// =============================================================================

void setupCompressCommand(CLI::App& app) {
    auto* compress = app.add_subcommand("compress", "Compress FASTQ file(s) to .fqc format");
    compress->alias("c");

    // Required options
    compress->add_option("-i,--input,-1", gCompressOpts.input, "Input FASTQ file (or '-' for stdin)")
        ->required()
        ->check(CLI::ExistingFile | CLI::IsMember({"-"}));

    compress->add_option("-2", gCompressOpts.input2, "Second input file for paired-end (R2)")
        ->check(CLI::ExistingFile);

    compress->add_option("-o,--output", gCompressOpts.output, "Output .fqc file")
        ->required();

    // Compression options
    compress->add_option("-l,--level", gCompressOpts.level, "Compression level (1-9)")
        ->default_val(6)
        ->check(CLI::Range(1, 9));

    compress->add_flag("--reorder,!--no-reorder", gCompressOpts.reorder,
                       "Enable/disable global read reordering")
        ->default_val(true);

    compress->add_flag("--streaming", gCompressOpts.streaming,
                       "Streaming mode (disables reordering, lower compression)");

    compress->add_option("--lossy-quality", gCompressOpts.lossyQuality,
                         "Lossy quality mode: none, illumina8, qvz, discard")
        ->default_val("none")
        ->check(CLI::IsMember({"none", "illumina8", "qvz", "discard"}));

    compress->add_option("--long-read-mode", gCompressOpts.longReadMode,
                         "Long read handling: auto, short, medium, long")
        ->default_val("auto")
        ->check(CLI::IsMember({"auto", "short", "medium", "long"}));

    compress->add_option("--max-block-bases", gCompressOpts.maxBlockBases,
                         "Maximum bases per block (for long reads)")
        ->default_val(0);

    compress->add_flag("--scan-all-lengths", gCompressOpts.scanAllLengths,
                       "Scan all reads for length detection (slower but more accurate)");

    compress->add_flag("--interleaved", gCompressOpts.interleaved,
                       "Input is interleaved paired-end (R1, R2, R1, R2, ...)");

    compress->add_option("--pe-layout", gCompressOpts.peLayout,
                         "Paired-end storage layout: interleaved, consecutive")
        ->default_val("interleaved")
        ->check(CLI::IsMember({"interleaved", "consecutive"}));

    compress->add_flag("-f,--force", gCompressOpts.force, "Overwrite existing output file");

    compress->callback([]() {
        // Check for stdin input
        if (gCompressOpts.input == "-") {
            if (!gCompressOpts.streaming) {
                FQC_LOG_WARNING("stdin input detected, enabling streaming mode (no global reordering)");
                gCompressOpts.streaming = true;
            }
        }
    });
}

void setupDecompressCommand(CLI::App& app) {
    auto* decompress = app.add_subcommand("decompress", "Decompress .fqc file to FASTQ");
    decompress->alias("d");
    decompress->alias("x");

    // Required options
    decompress->add_option("-i,--input", gDecompressOpts.input, "Input .fqc file")
        ->required()
        ->check(CLI::ExistingFile);

    decompress->add_option("-o,--output", gDecompressOpts.output,
                           "Output FASTQ file (or '-' for stdout)")
        ->required();

    // Decompression options
    decompress->add_option("--range", gDecompressOpts.range,
                           "Read range to extract (e.g., '1:1000' or '100:')");

    decompress->add_flag("--header-only", gDecompressOpts.headerOnly,
                         "Only output read headers (IDs)");

    decompress->add_flag("--original-order", gDecompressOpts.originalOrder,
                         "Output reads in original order (requires reorder map)");

    decompress->add_flag("--skip-corrupted", gDecompressOpts.skipCorrupted,
                         "Skip corrupted blocks instead of failing");

    decompress->add_option("--corrupted-placeholder", gDecompressOpts.corruptedPlaceholder,
                           "Placeholder sequence for corrupted reads");

    decompress->add_flag("--split-pe", gDecompressOpts.splitPe,
                         "Split paired-end output to separate files");
}

void setupInfoCommand(CLI::App& app) {
    auto* info = app.add_subcommand("info", "Display archive information");
    info->alias("i");

    info->add_option("-i,--input", gInfoOpts.input, "Input .fqc file")
        ->required()
        ->check(CLI::ExistingFile);

    info->add_flag("--json", gInfoOpts.json, "Output as JSON");

    info->add_flag("--detailed", gInfoOpts.detailed, "Show detailed block information");
}

void setupVerifyCommand(CLI::App& app) {
    auto* verify = app.add_subcommand("verify", "Verify archive integrity");
    verify->alias("v");

    verify->add_option("-i,--input", gVerifyOpts.input, "Input .fqc file")
        ->required()
        ->check(CLI::ExistingFile);

    verify->add_flag("--fail-fast", gVerifyOpts.failFast, "Stop on first error");

    verify->add_flag("--verbose", gVerifyOpts.verbose, "Show detailed verification progress");
}

}  // namespace

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char* argv[]) {
    CLI::App app{kDescription};
    app.set_version_flag("-V,--version", kVersion);

    // Global options
    app.add_option("-t,--threads", gOptions.threads,
                   "Number of threads (0 = auto-detect)")
        ->default_val(0)
        ->check(CLI::NonNegativeNumber);

    app.add_flag("-v,--verbose", gOptions.verbosity,
                 "Increase verbosity (-v, -vv for debug)");

    app.add_flag("-q,--quiet", gOptions.quiet, "Suppress non-error output");

    app.add_option("--memory-limit", gOptions.memoryLimit,
                   "Memory limit in MB (0 = no limit)")
        ->default_val(0);

    app.add_flag("--no-progress", gOptions.noProgress,
                 "Disable progress display");

    // Setup subcommands
    setupCompressCommand(app);
    setupDecompressCommand(app);
    setupInfoCommand(app);
    setupVerifyCommand(app);

    // Require a subcommand
    app.require_subcommand(1);

    // Parse arguments
    CLI11_PARSE(app, argc, argv);

    // Initialize logger
    try {
        auto logLevel = fqc::log::Level::kInfo;
        if (gOptions.quiet) {
            logLevel = fqc::log::Level::kError;
        } else if (gOptions.verbosity >= 2) {
            logLevel = fqc::log::Level::kDebug;
        } else if (gOptions.verbosity >= 1) {
            logLevel = fqc::log::Level::kDebug;
        }
        fqc::log::init("", logLevel);
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logger: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Auto-disable progress on non-TTY
    if (!isStdoutTty() && !gOptions.noProgress) {
        gOptions.noProgress = true;
        // FQC_LOG_DEBUG("stdout is not a TTY, disabling progress display");
    }

    // Dispatch to subcommand handlers
    try {
        if (app.got_subcommand("compress")) {
            return fqc::commands::runCompress(app.get_subcommand("compress"));
        }
        if (app.got_subcommand("decompress")) {
            return fqc::commands::runDecompress(app.get_subcommand("decompress"));
        }
        if (app.got_subcommand("info")) {
            return fqc::commands::runInfo(app.get_subcommand("info"));
        }
        if (app.got_subcommand("verify")) {
            return fqc::commands::runVerify(app.get_subcommand("verify"));
        }
    } catch (const fqc::FQCException& ex) {
        FQC_LOG_ERROR("Error: {}", ex.what());
        return static_cast<int>(ex.code());
    } catch (const std::exception& ex) {
        FQC_LOG_ERROR("Unexpected error: {}", ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// =============================================================================
// Command Implementations
// =============================================================================

namespace fqc::commands {

int runCompress([[maybe_unused]] CLI::App* app) {
    try {
        CompressOptions opts;
        opts.inputPath = gCompressOpts.input;
        opts.input2Path = gCompressOpts.input2;
        opts.outputPath = gCompressOpts.output;
        opts.compressionLevel = gCompressOpts.level;
        opts.threads = gOptions.threads;
        opts.memoryLimitMb = gOptions.memoryLimit;
        opts.enableReordering = gCompressOpts.reorder;
        opts.streamingMode = gCompressOpts.streaming;
        opts.qualityMode = parseQualityMode(gCompressOpts.lossyQuality);
        opts.maxBlockBases = gCompressOpts.maxBlockBases;
        opts.scanAllLengths = gCompressOpts.scanAllLengths;
        opts.interleaved = gCompressOpts.interleaved;
        opts.forceOverwrite = gCompressOpts.force;
        opts.showProgress = !gOptions.noProgress;

        // Parse long read mode
        if (gCompressOpts.longReadMode == "auto") {
            opts.autoDetectLongRead = true;
        } else if (gCompressOpts.longReadMode == "short") {
            opts.autoDetectLongRead = false;
            opts.longReadMode = ReadLengthClass::kShort;
        } else if (gCompressOpts.longReadMode == "medium") {
            opts.autoDetectLongRead = false;
            opts.longReadMode = ReadLengthClass::kMedium;
        } else if (gCompressOpts.longReadMode == "long") {
            opts.autoDetectLongRead = false;
            opts.longReadMode = ReadLengthClass::kLong;
        }

        // Parse PE layout
        if (gCompressOpts.peLayout == "consecutive") {
            opts.peLayout = PELayout::kConsecutive;
        }

        auto cmd = std::make_unique<CompressCommand>(std::move(opts));
        return cmd->execute();
    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Compression failed: {}", e.what());
        return static_cast<int>(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return EXIT_FAILURE;
    }
}

int runDecompress([[maybe_unused]] CLI::App* app) {
    try {
        DecompressOptions opts;
        opts.inputPath = gDecompressOpts.input;
        opts.outputPath = gDecompressOpts.output;
        opts.headerOnly = gDecompressOpts.headerOnly;
        opts.originalOrder = gDecompressOpts.originalOrder;
        opts.skipCorrupted = gDecompressOpts.skipCorrupted;
        opts.splitPairedEnd = gDecompressOpts.splitPe;
        opts.threads = gOptions.threads;
        opts.showProgress = !gOptions.noProgress;

        if (!gDecompressOpts.corruptedPlaceholder.empty()) {
            opts.corruptedPlaceholder = gDecompressOpts.corruptedPlaceholder;
        }

        if (!gDecompressOpts.range.empty()) {
            opts.range = parseRange(gDecompressOpts.range);
        }

        auto cmd = std::make_unique<DecompressCommand>(std::move(opts));
        return cmd->execute();
    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Decompression failed: {}", e.what());
        return static_cast<int>(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return EXIT_FAILURE;
    }
}

int runInfo([[maybe_unused]] CLI::App* app) {
    try {
        auto cmd = createInfoCommand(
            gInfoOpts.input,
            gInfoOpts.json,
            gInfoOpts.detailed
        );
        return cmd->execute();
    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Info command failed: {}", e.what());
        return static_cast<int>(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return EXIT_FAILURE;
    }
}

int runVerify([[maybe_unused]] CLI::App* app) {
    try {
        VerifyOptions opts;
        opts.inputPath = gVerifyOpts.input;
        opts.failFast = gVerifyOpts.failFast;
        opts.verbose = gVerifyOpts.verbose;

        auto cmd = std::make_unique<VerifyCommand>(std::move(opts));
        return cmd->execute();
    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Verification failed: {}", e.what());
        return static_cast<int>(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return EXIT_FAILURE;
    }
}

}  // namespace fqc::commands
