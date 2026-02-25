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

// =============================================================================
// TTY Detection
// =============================================================================

/// @brief Check if stdout is a TTY.
[[nodiscard]] bool isStdoutTty() noexcept {
    return isatty(fileno(stdout)) != 0;
}

// =============================================================================
// CLI Option Structs
// =============================================================================

struct CliCompressOptions {
    std::string input;
    std::string input2;
    std::string output;
    int level = 6;
    bool reorder = true;
    bool streaming = false;
    std::string lossyQuality;
    std::string longReadMode = "auto";
    std::size_t maxBlockBases = 0;
    bool scanAllLengths = false;
    bool interleaved = false;
    std::string peLayout = "interleaved";
    bool force = false;
};

struct CliDecompressOptions {
    std::string input;
    std::string output;
    std::string range;
    bool headerOnly = false;
    bool originalOrder = false;
    bool skipCorrupted = false;
    std::string corruptedPlaceholder;
    bool splitPe = false;
};

struct CliInfoOptions {
    std::string input;
    bool json = false;
    bool detailed = false;
};

struct CliVerifyOptions {
    std::string input;
    bool failFast = false;
    bool verbose = false;
};

// =============================================================================
// Command Setup Functions (accept options by reference)
// =============================================================================

void setupCompressCommand(CLI::App& app, GlobalOptions& gOpts, CliCompressOptions& opts) {
    auto* compress = app.add_subcommand("compress", "Compress FASTQ file(s) to .fqc format");
    compress->alias("c");

    compress->add_option("-i,--input,-1", opts.input, "Input FASTQ file (or '-' for stdin)")
        ->required()
        ->check(CLI::ExistingFile | CLI::IsMember({"-"}));

    compress->add_option("-2", opts.input2, "Second input file for paired-end (R2)")
        ->check(CLI::ExistingFile);

    compress->add_option("-o,--output", opts.output, "Output .fqc file")
        ->required();

    compress->add_option("-l,--level", opts.level, "Compression level (1-9)")
        ->default_val(6)
        ->check(CLI::Range(1, 9));

    compress->add_flag("--reorder,!--no-reorder", opts.reorder,
                       "Enable/disable global read reordering")
        ->default_val(true);

    compress->add_flag("--streaming", opts.streaming,
                       "Streaming mode (disables reordering, lower compression)");

    compress->add_option("--lossy-quality", opts.lossyQuality,
                         "Lossy quality mode: none, illumina8, qvz, discard")
        ->default_val("none")
        ->check(CLI::IsMember({"none", "illumina8", "qvz", "discard"}));

    compress->add_option("--long-read-mode", opts.longReadMode,
                         "Long read handling: auto, short, medium, long")
        ->default_val("auto")
        ->check(CLI::IsMember({"auto", "short", "medium", "long"}));

    compress->add_option("--max-block-bases", opts.maxBlockBases,
                         "Maximum bases per block (for long reads)")
        ->default_val(0);

    compress->add_flag("--scan-all-lengths", opts.scanAllLengths,
                       "Scan all reads for length detection (slower but more accurate)");

    compress->add_flag("--interleaved", opts.interleaved,
                       "Input is interleaved paired-end (R1, R2, R1, R2, ...)");

    compress->add_option("--pe-layout", opts.peLayout,
                         "Paired-end storage layout: interleaved, consecutive")
        ->default_val("interleaved")
        ->check(CLI::IsMember({"interleaved", "consecutive"}));

    compress->add_flag("-f,--force", opts.force, "Overwrite existing output file");

    compress->callback([&opts]() {
        if (opts.input == "-") {
            if (!opts.streaming) {
                FQC_LOG_WARNING("stdin input detected, enabling streaming mode (no global reordering)");
                opts.streaming = true;
            }
        }
    });
}

void setupDecompressCommand(CLI::App& app, CliDecompressOptions& opts) {
    auto* decompress = app.add_subcommand("decompress", "Decompress .fqc file to FASTQ");
    decompress->alias("d");
    decompress->alias("x");

    decompress->add_option("-i,--input", opts.input, "Input .fqc file")
        ->required()
        ->check(CLI::ExistingFile);

    decompress->add_option("-o,--output", opts.output,
                           "Output FASTQ file (or '-' for stdout)")
        ->required();

    decompress->add_option("--range", opts.range,
                           "Read range to extract (e.g., '1:1000' or '100:')");

    decompress->add_flag("--header-only", opts.headerOnly,
                         "Only output read headers (IDs)");

    decompress->add_flag("--original-order", opts.originalOrder,
                         "Output reads in original order (requires reorder map)");

    decompress->add_flag("--skip-corrupted", opts.skipCorrupted,
                         "Skip corrupted blocks instead of failing");

    decompress->add_option("--corrupted-placeholder", opts.corruptedPlaceholder,
                           "Placeholder sequence for corrupted reads");

    decompress->add_flag("--split-pe", opts.splitPe,
                         "Split paired-end output to separate files");
}

void setupInfoCommand(CLI::App& app, CliInfoOptions& opts) {
    auto* info = app.add_subcommand("info", "Display archive information");
    info->alias("i");

    info->add_option("-i,--input", opts.input, "Input .fqc file")
        ->required()
        ->check(CLI::ExistingFile);

    info->add_flag("--json", opts.json, "Output as JSON");

    info->add_flag("--detailed", opts.detailed, "Show detailed block information");
}

void setupVerifyCommand(CLI::App& app, CliVerifyOptions& opts) {
    auto* verify = app.add_subcommand("verify", "Verify archive integrity");
    verify->alias("v");

    verify->add_option("-i,--input", opts.input, "Input .fqc file")
        ->required()
        ->check(CLI::ExistingFile);

    verify->add_flag("--fail-fast", opts.failFast, "Stop on first error");

    verify->add_flag("--verbose", opts.verbose, "Show detailed verification progress");
}

}  // namespace

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char* argv[]) {
    // All options are local — no global mutable state.
    GlobalOptions globalOpts;
    CliCompressOptions compressOpts;
    CliDecompressOptions decompressOpts;
    CliInfoOptions infoOpts;
    CliVerifyOptions verifyOpts;

    CLI::App app{kDescription};
    app.set_version_flag("-V,--version", kVersion);

    // Global options
    app.add_option("-t,--threads", globalOpts.threads,
                   "Number of threads (0 = auto-detect)")
        ->default_val(0)
        ->check(CLI::NonNegativeNumber);

    app.add_flag("-v,--verbose", globalOpts.verbosity,
                 "Increase verbosity (-v, -vv for debug)");

    app.add_flag("-q,--quiet", globalOpts.quiet, "Suppress non-error output");

    app.add_option("--memory-limit", globalOpts.memoryLimit,
                   "Memory limit in MB (0 = no limit)")
        ->default_val(0);

    app.add_flag("--no-progress", globalOpts.noProgress,
                 "Disable progress display");

    // Setup subcommands (pass local options by reference)
    setupCompressCommand(app, globalOpts, compressOpts);
    setupDecompressCommand(app, decompressOpts);
    setupInfoCommand(app, infoOpts);
    setupVerifyCommand(app, verifyOpts);

    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);

    // Initialize logger
    try {
        auto logLevel = fqc::log::Level::kInfo;
        if (globalOpts.quiet) {
            logLevel = fqc::log::Level::kError;
        } else if (globalOpts.verbosity >= 2) {
            logLevel = fqc::log::Level::kDebug;
        } else if (globalOpts.verbosity >= 1) {
            logLevel = fqc::log::Level::kDebug;
        }
        fqc::log::init("", logLevel);
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logger: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if (!isStdoutTty() && !globalOpts.noProgress) {
        globalOpts.noProgress = true;
    }

    // Dispatch to subcommand handlers
    using namespace fqc;
    using namespace fqc::commands;
    try {
        if (app.got_subcommand("compress")) {
            CompressOptions opts;
            opts.inputPath = compressOpts.input;
            opts.input2Path = compressOpts.input2;
            opts.outputPath = compressOpts.output;
            opts.compressionLevel = compressOpts.level;
            opts.threads = globalOpts.threads;
            opts.memoryLimitMb = globalOpts.memoryLimit;
            opts.enableReordering = compressOpts.reorder;
            opts.streamingMode = compressOpts.streaming;
            opts.qualityMode = parseQualityMode(compressOpts.lossyQuality);
            opts.maxBlockBases = compressOpts.maxBlockBases;
            opts.scanAllLengths = compressOpts.scanAllLengths;
            opts.interleaved = compressOpts.interleaved;
            opts.forceOverwrite = compressOpts.force;
            opts.showProgress = !globalOpts.noProgress;

            if (compressOpts.longReadMode == "auto") {
                opts.autoDetectLongRead = true;
            } else if (compressOpts.longReadMode == "short") {
                opts.autoDetectLongRead = false;
                opts.longReadMode = ReadLengthClass::kShort;
            } else if (compressOpts.longReadMode == "medium") {
                opts.autoDetectLongRead = false;
                opts.longReadMode = ReadLengthClass::kMedium;
            } else if (compressOpts.longReadMode == "long") {
                opts.autoDetectLongRead = false;
                opts.longReadMode = ReadLengthClass::kLong;
            }

            if (compressOpts.peLayout == "consecutive") {
                opts.peLayout = PELayout::kConsecutive;
            }

            return CompressCommand(std::move(opts)).execute();
        }
        if (app.got_subcommand("decompress")) {
            DecompressOptions opts;
            opts.inputPath = decompressOpts.input;
            opts.outputPath = decompressOpts.output;
            opts.headerOnly = decompressOpts.headerOnly;
            opts.originalOrder = decompressOpts.originalOrder;
            opts.skipCorrupted = decompressOpts.skipCorrupted;
            opts.splitPairedEnd = decompressOpts.splitPe;
            opts.threads = globalOpts.threads;
            opts.showProgress = !globalOpts.noProgress;

            if (!decompressOpts.corruptedPlaceholder.empty()) {
                opts.corruptedPlaceholder = decompressOpts.corruptedPlaceholder;
            }
            if (!decompressOpts.range.empty()) {
                opts.range = parseRange(decompressOpts.range);
            }

            return DecompressCommand(std::move(opts)).execute();
        }
        if (app.got_subcommand("info")) {
            return createInfoCommand(
                infoOpts.input,
                infoOpts.json,
                infoOpts.detailed
            )->execute();
        }
        if (app.got_subcommand("verify")) {
            VerifyOptions opts;
            opts.inputPath = verifyOpts.input;
            opts.failFast = verifyOpts.failFast;
            opts.verbose = verifyOpts.verbose;

            return VerifyCommand(std::move(opts)).execute();
        }
    } catch (const FQCException& ex) {
        FQC_LOG_ERROR("Error: {}", ex.what());
        return static_cast<int>(ex.code());
    } catch (const std::exception& ex) {
        FQC_LOG_ERROR("Unexpected error: {}", ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
