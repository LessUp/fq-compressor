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
#include <vector>

#include <xxhash.h>

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

    const char expectedMagic[] = "\x89" "FQC\r\n" "\x1a\n";
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

    std::ifstream file(options_.inputPath, std::ios::binary);
    if (!file) {
        result.passed = false;
        result.errorMessage = "Failed to open file";
        return result;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    if (fileSize < static_cast<std::streamoff>(format::kFileFooterSize)) {
        result.passed = false;
        result.errorMessage = "File too small";
        return result;
    }

    // Read footer to get stored checksum
    file.seekg(-static_cast<std::streamoff>(format::kFileFooterSize), std::ios::end);
    format::FileFooter footer;
    file.read(reinterpret_cast<char*>(&footer), sizeof(footer));

    // Compute checksum of everything except footer
    file.seekg(0, std::ios::beg);
    auto dataSize = static_cast<std::size_t>(fileSize) - format::kFileFooterSize;

    XXH64_state_t* state = XXH64_createState();
    if (!state) {
        result.passed = false;
        result.errorMessage = "Failed to create hash state";
        return result;
    }
    XXH64_reset(state, 0);

    constexpr std::size_t kBufferSize = 1024 * 1024;  // 1MB buffer
    std::vector<char> buffer(kBufferSize);
    std::size_t remaining = dataSize;

    while (remaining > 0) {
        auto toRead = std::min(remaining, kBufferSize);
        file.read(buffer.data(), static_cast<std::streamsize>(toRead));
        auto bytesRead = static_cast<std::size_t>(file.gcount());
        if (bytesRead == 0) break;
        XXH64_update(state, buffer.data(), bytesRead);
        remaining -= bytesRead;
    }

    auto computed = XXH64_digest(state);
    XXH64_freeState(state);

    if (computed != footer.globalChecksum) {
        result.passed = false;
        result.errorMessage = "Checksum mismatch: expected 0x" + 
            std::to_string(footer.globalChecksum) + ", got 0x" + std::to_string(computed);
        return result;
    }

    result.passed = true;
    result.details = "Global checksum valid (xxHash64: 0x" + std::to_string(computed) + ")";
    return result;
}

VerificationResult VerifyCommand::verifyBlockIndex() {
    VerificationResult result;
    result.checkName = "Block Index";

    std::ifstream file(options_.inputPath, std::ios::binary);
    if (!file) {
        result.passed = false;
        result.errorMessage = "Failed to open file";
        return result;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();

    // Read footer to get index offset
    file.seekg(-static_cast<std::streamoff>(format::kFileFooterSize), std::ios::end);
    format::FileFooter footer;
    file.read(reinterpret_cast<char*>(&footer), sizeof(footer));

    // Validate index offset
    if (footer.indexOffset == 0 || footer.indexOffset >= static_cast<std::uint64_t>(fileSize)) {
        result.passed = false;
        result.errorMessage = "Invalid index offset: " + std::to_string(footer.indexOffset);
        return result;
    }

    // Seek to block index
    file.seekg(static_cast<std::streamoff>(footer.indexOffset), std::ios::beg);
    if (!file) {
        result.passed = false;
        result.errorMessage = "Failed to seek to index";
        return result;
    }

    // Read block index header
    format::BlockIndex indexHeader;
    file.read(reinterpret_cast<char*>(&indexHeader), sizeof(indexHeader));
    if (file.gcount() < static_cast<std::streamsize>(sizeof(indexHeader))) {
        result.passed = false;
        result.errorMessage = "Failed to read index header";
        return result;
    }

    // Validate index header
    if (!indexHeader.isValid()) {
        result.passed = false;
        result.errorMessage = "Invalid index header";
        return result;
    }

    // Validate index entries fit within file
    auto indexEndOffset = footer.indexOffset + indexHeader.totalSize();
    auto footerOffset = static_cast<std::uint64_t>(fileSize) - format::kFileFooterSize;
    if (indexEndOffset > footerOffset) {
        result.passed = false;
        result.errorMessage = "Index extends beyond footer";
        return result;
    }

    // Read and validate each index entry
    std::uint64_t expectedArchiveId = 0;
    for (std::uint64_t i = 0; i < indexHeader.numBlocks; ++i) {
        format::IndexEntry entry;
        file.read(reinterpret_cast<char*>(&entry), sizeof(entry));
        if (file.gcount() < static_cast<std::streamsize>(sizeof(entry))) {
            result.passed = false;
            result.errorMessage = "Failed to read index entry " + std::to_string(i);
            return result;
        }

        // Validate entry offset
        if (entry.offset >= footer.indexOffset) {
            result.passed = false;
            result.errorMessage = "Block " + std::to_string(i) + " offset beyond index";
            return result;
        }

        // Validate archive ID continuity
        if (entry.archiveIdStart != expectedArchiveId) {
            result.passed = false;
            result.errorMessage = "Archive ID discontinuity at block " + std::to_string(i);
            return result;
        }
        expectedArchiveId += entry.readCount;

        // Skip any extension fields
        if (indexHeader.entrySize > format::IndexEntry::kSize) {
            file.seekg(static_cast<std::streamoff>(indexHeader.entrySize - format::IndexEntry::kSize), 
                       std::ios::cur);
        }
    }

    result.passed = true;
    result.details = std::to_string(indexHeader.numBlocks) + " blocks indexed";
    return result;
}

std::vector<VerificationResult> VerifyCommand::verifyBlockChecksums() {
    std::vector<VerificationResult> results;

    std::ifstream file(options_.inputPath, std::ios::binary);
    if (!file) {
        VerificationResult err;
        err.checkName = "Block Checksums";
        err.passed = false;
        err.errorMessage = "Failed to open file";
        results.push_back(err);
        return results;
    }

    // Get file size and read footer
    file.seekg(0, std::ios::end);
    [[maybe_unused]] auto fileSize = file.tellg();
    file.seekg(-static_cast<std::streamoff>(format::kFileFooterSize), std::ios::end);
    
    format::FileFooter footer;
    file.read(reinterpret_cast<char*>(&footer), sizeof(footer));

    // Read block index
    file.seekg(static_cast<std::streamoff>(footer.indexOffset), std::ios::beg);
    format::BlockIndex indexHeader;
    file.read(reinterpret_cast<char*>(&indexHeader), sizeof(indexHeader));

    if (!indexHeader.isValid()) {
        VerificationResult err;
        err.checkName = "Block Checksums";
        err.passed = false;
        err.errorMessage = "Invalid index header";
        results.push_back(err);
        return results;
    }

    // Read all index entries
    std::vector<format::IndexEntry> entries(indexHeader.numBlocks);
    for (std::uint64_t i = 0; i < indexHeader.numBlocks; ++i) {
        file.read(reinterpret_cast<char*>(&entries[i]), sizeof(format::IndexEntry));
        if (indexHeader.entrySize > format::IndexEntry::kSize) {
            file.seekg(static_cast<std::streamoff>(indexHeader.entrySize - format::IndexEntry::kSize),
                       std::ios::cur);
        }
    }

    // Verify each block's header
    std::uint32_t corruptedBlocks = 0;
    for (std::uint64_t i = 0; i < indexHeader.numBlocks; ++i) {
        const auto& entry = entries[i];
        
        // Seek to block
        file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
        if (!file) {
            VerificationResult blockResult;
            blockResult.checkName = "Block " + std::to_string(i);
            blockResult.passed = false;
            blockResult.errorMessage = "Failed to seek to block";
            results.push_back(blockResult);
            ++corruptedBlocks;
            if (options_.failFast) break;
            continue;
        }

        // Read block header
        format::BlockHeader blockHeader;
        file.read(reinterpret_cast<char*>(&blockHeader), sizeof(blockHeader));
        if (file.gcount() < static_cast<std::streamsize>(sizeof(blockHeader))) {
            VerificationResult blockResult;
            blockResult.checkName = "Block " + std::to_string(i);
            blockResult.passed = false;
            blockResult.errorMessage = "Failed to read block header";
            results.push_back(blockResult);
            ++corruptedBlocks;
            if (options_.failFast) break;
            continue;
        }

        // Validate block header
        if (!blockHeader.isValid()) {
            VerificationResult blockResult;
            blockResult.checkName = "Block " + std::to_string(i);
            blockResult.passed = false;
            blockResult.errorMessage = "Invalid block header";
            results.push_back(blockResult);
            ++corruptedBlocks;
            if (options_.failFast) break;
            continue;
        }

        // Verify block ID matches index
        if (blockHeader.blockId != i) {
            VerificationResult blockResult;
            blockResult.checkName = "Block " + std::to_string(i);
            blockResult.passed = false;
            blockResult.errorMessage = "Block ID mismatch: expected " + std::to_string(i) +
                                       ", got " + std::to_string(blockHeader.blockId);
            results.push_back(blockResult);
            ++corruptedBlocks;
            if (options_.failFast) break;
            continue;
        }

        // Verify read count matches index
        if (blockHeader.uncompressedCount != entry.readCount) {
            VerificationResult blockResult;
            blockResult.checkName = "Block " + std::to_string(i);
            blockResult.passed = false;
            blockResult.errorMessage = "Read count mismatch";
            results.push_back(blockResult);
            ++corruptedBlocks;
            if (options_.failFast) break;
            continue;
        }
    }

    // Add summary result
    VerificationResult summary;
    summary.checkName = "Block Checksums";
    if (corruptedBlocks == 0) {
        summary.passed = true;
        summary.details = std::to_string(indexHeader.numBlocks) + " blocks verified";
    } else {
        summary.passed = false;
        summary.errorMessage = std::to_string(corruptedBlocks) + " of " + 
                               std::to_string(indexHeader.numBlocks) + " blocks corrupted";
    }
    results.insert(results.begin(), summary);

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
