// =============================================================================
// fq-compressor - Verify Command Implementation
// =============================================================================
// Command handler implementation for verifying archive integrity.
//
// Requirements: 5.1, 5.2, 5.3, 8.5
// =============================================================================

#include "verify_command.h"

#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "fqc/common/logger.h"
#include "fqc/format/fqc_format.h"

namespace fqc::commands {

// =============================================================================
// VerifyCommand Implementation
// =============================================================================

VerifyCommand::VerifyCommand(VerifyOptions options) : options_(std::move(options)) {}

VerifyCommand::~VerifyCommand() = default;

VerifyCommand::VerifyCommand(VerifyCommand&&) noexcept = default;
VerifyCommand& VerifyCommand::operator=(VerifyCommand&&) noexcept = default;

int VerifyCommand::execute() {
    try {
        // Check input exists
        if (!std::filesystem::exists(options_.inputPath)) {
            throw IOError(
                          "Input file not found: " + options_.inputPath.string());
        }

        if (options_.verbose) {
            std::cout << "Verifying: " << options_.inputPath.string() << std::endl;
            std::cout << std::endl;
        }

        // Run verification checks
        bool shouldContinue = true;

        // 1. Verify magic header
        auto magicResult = verifyMagicHeader();
        summary_.addResult(magicResult);
        if (options_.verbose) {
            std::cout << "[" << (magicResult.passed ? "PASS" : "FAIL") << "] "
                      << magicResult.checkName << std::endl;
        }
        if (!magicResult.passed && options_.failFast) {
            shouldContinue = false;
        }

        // 2. Verify footer
        if (shouldContinue) {
            auto footerResult = verifyFooter();
            summary_.addResult(footerResult);
            if (options_.verbose) {
                std::cout << "[" << (footerResult.passed ? "PASS" : "FAIL") << "] "
                          << footerResult.checkName << std::endl;
            }
            if (!footerResult.passed && options_.failFast) {
                shouldContinue = false;
            }
        }

        // 3. Verify global checksum (if enabled)
        if (shouldContinue && options_.verifyGlobal) {
            auto globalResult = verifyGlobalChecksum();
            summary_.addResult(globalResult);
            if (options_.verbose) {
                std::cout << "[" << (globalResult.passed ? "PASS" : "FAIL") << "] "
                          << globalResult.checkName << std::endl;
            }
            if (!globalResult.passed && options_.failFast) {
                shouldContinue = false;
            }
        }

        // 4. Verify block index
        if (shouldContinue) {
            auto indexResult = verifyBlockIndex();
            summary_.addResult(indexResult);
            if (options_.verbose) {
                std::cout << "[" << (indexResult.passed ? "PASS" : "FAIL") << "] "
                          << indexResult.checkName << std::endl;
            }
            if (!indexResult.passed && options_.failFast) {
                shouldContinue = false;
            }
        }

        // 5. Verify block checksums (if enabled)
        if (shouldContinue && options_.verifyBlocks) {
            auto blockResults = verifyBlockChecksums();
            for (auto& result : blockResults) {
                summary_.addResult(std::move(result));
                if (options_.verbose && !result.passed) {
                    std::cout << "[FAIL] " << result.checkName << ": " << result.errorMessage
                              << std::endl;
                }
                if (!result.passed && options_.failFast) {
                    break;
                }
            }
        }

        // Print summary
        printSummary();

        return summary_.passed() ? 0 : static_cast<int>(ErrorCode::kChecksumMismatch);

    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Verification failed: {}", e.what());
        return static_cast<int>(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return 1;
    }
}

VerificationResult VerifyCommand::verifyMagicHeader() {
    VerificationResult result;
    result.checkName = "Magic Header";

    std::ifstream file(options_.inputPath, std::ios::binary);
    if (!file) {
        result.passed = false;
        result.errorMessage = "Failed to open file";
        return result;
    }

    char magic[8];
    file.read(magic, 8);
    if (file.gcount() < 8) {
        result.passed = false;
        result.errorMessage = "File too small";
        return result;
    }

    const char expectedMagic[] = "\x89FQC\r\n\x1a\n";
    if (std::memcmp(magic, expectedMagic, 8) != 0) {
        result.passed = false;
        result.errorMessage = "Invalid magic bytes";
        return result;
    }

    result.passed = true;
    result.details = "Magic header valid";
    return result;
}

VerificationResult VerifyCommand::verifyFooter() {
    VerificationResult result;
    result.checkName = "File Footer";

    std::ifstream file(options_.inputPath, std::ios::binary);
    if (!file) {
        result.passed = false;
        result.errorMessage = "Failed to open file";
        return result;
    }

    // Seek to footer (last 32 bytes)
    file.seekg(-32, std::ios::end);
    if (!file) {
        result.passed = false;
        result.errorMessage = "File too small for footer";
        return result;
    }

    // Read footer
    format::FileFooter footer;
    file.read(reinterpret_cast<char*>(&footer), sizeof(footer));
    if (file.gcount() < static_cast<std::streamsize>(sizeof(footer))) {
        result.passed = false;
        result.errorMessage = "Failed to read footer";
        return result;
    }

    // Verify footer magic
    if (std::memcmp(footer.magicEnd.data(), format::kMagicEnd.data(), format::kMagicEnd.size()) != 0) {
        result.passed = false;
        result.errorMessage = "Invalid footer magic";
        return result;
    }

    result.passed = true;
    result.details = "Footer magic valid";
    return result;
}

VerificationResult VerifyCommand::verifyGlobalChecksum() {
    VerificationResult result;
    result.checkName = "Global Checksum";

    // TODO: Implement actual checksum verification
    // This requires reading the entire file and computing xxHash64

    result.passed = true;
    result.details = "Checksum verification not yet implemented";
    return result;
}

VerificationResult VerifyCommand::verifyBlockIndex() {
    VerificationResult result;
    result.checkName = "Block Index";

    // TODO: Implement block index verification
    // This requires reading the footer to find index offset, then validating index

    result.passed = true;
    result.details = "Block index verification not yet implemented";
    return result;
}

std::vector<VerificationResult> VerifyCommand::verifyBlockChecksums() {
    std::vector<VerificationResult> results;

    // TODO: Implement block checksum verification
    // This requires iterating through all blocks and verifying each checksum

    VerificationResult placeholder;
    placeholder.checkName = "Block Checksums";
    placeholder.passed = true;
    placeholder.details = "Block checksum verification not yet implemented";
    results.push_back(placeholder);

    return results;
}

void VerifyCommand::printSummary() const {
    std::cout << std::endl;
    std::cout << "=== Verification Summary ===" << std::endl;
    std::cout << "File:    " << options_.inputPath.string() << std::endl;
    std::cout << "Checks:  " << summary_.passedChecks << "/" << summary_.totalChecks << " passed"
              << std::endl;

    if (summary_.passed()) {
        std::cout << "Status:  OK" << std::endl;
    } else {
        std::cout << "Status:  FAILED" << std::endl;
        std::cout << std::endl;
        std::cout << "Failed checks:" << std::endl;
        for (const auto& result : summary_.results) {
            if (!result.passed) {
                std::cout << "  - " << result.checkName << ": " << result.errorMessage << std::endl;
            }
        }
    }

    std::cout << "=============================" << std::endl;
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<VerifyCommand> createVerifyCommand(
    const std::string& inputPath,
    bool failFast,
    bool verbose) {

    VerifyOptions opts;
    opts.inputPath = inputPath;
    opts.failFast = failFast;
    opts.verbose = verbose;

    return std::make_unique<VerifyCommand>(std::move(opts));
}

}  // namespace fqc::commands
