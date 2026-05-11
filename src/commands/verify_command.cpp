// =============================================================================
// fq-compressor - Verify Command Implementation
// =============================================================================
// Command handler implementation for verifying archive integrity.
//
// Verification levels:
// 1. Structure check: Magic header, footer, block index integrity
// 2. Block header check: Each block header is valid and consistent with index
// 3. Payload check: Each block is decompressed and checksum verified
//
// Requirements: 5.1, 5.2, 5.3, 8.5
// =============================================================================

#include "fqc/commands/verify_command.h"

#include "fqc/algo/block_compressor.h"
#include "fqc/common/logger.h"
#include "fqc/format/fqc_format.h"
#include "fqc/format/fqc_reader.h"

#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include <xxhash.h>

#include <fmt/format.h>

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
            throw IOError("Input file not found: " + options_.inputPath.string());
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
                // Save values before move
                bool passed = result.passed;
                std::string checkName = result.checkName;
                std::string errorMessage = result.errorMessage;
                summary_.addResult(std::move(result));
                if (options_.verbose && !passed) {
                    std::cout << "[FAIL] " << checkName << ": " << errorMessage << std::endl;
                }
                if (!passed && options_.failFast) {
                    break;
                }
            }
        }

        // Print summary
        printSummary();

        return summary_.passed() ? 0 : toExitCode(ErrorCode::kChecksumMismatch);

    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Verification failed: {}", e.what());
        return toExitCode(e.code());
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

    const char expectedMagic[] =
        "\x89"
        "FQC\r\n"
        "\x1a\n";
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
    if (std::memcmp(footer.magicEnd.data(), format::kMagicEnd.data(), format::kMagicEnd.size()) !=
        0) {
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
        if (bytesRead == 0)
            break;
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
    std::uint64_t expectedArchiveId = 1;
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
            file.seekg(
                static_cast<std::streamoff>(indexHeader.entrySize - format::IndexEntry::kSize),
                std::ios::cur);
        }
    }

    result.passed = true;
    result.details = std::to_string(indexHeader.numBlocks) + " blocks indexed";
    return result;
}

std::vector<VerificationResult> VerifyCommand::verifyBlockChecksums() {
    std::vector<VerificationResult> results;

    try {
        // 使用 FQCReader 来读取和解压数据
        format::FQCReader reader(options_.inputPath);
        reader.open();

        // 读取全局头信息以配置解压器
        const auto& globalHeader = reader.globalHeader();

        // 配置解压器
        algo::BlockCompressorConfig config;
        config.readLengthClass = format::getReadLengthClass(globalHeader.flags);
        config.qualityMode = format::getQualityMode(globalHeader.flags);
        config.idMode = format::getIdMode(globalHeader.flags);
        config.numThreads = 1;  // 单线程验证

        algo::BlockCompressor compressor(config);

        std::uint32_t corruptedBlocks = 0;
        std::uint32_t checksumMismatches = 0;
        std::uint64_t totalBlocks = reader.blockCount();

        for (BlockId blockId = 0; blockId < totalBlocks; ++blockId) {
            VerificationResult blockResult;
            blockResult.checkName = "Block " + std::to_string(blockId);

            try {
                // 读取块数据
                auto blockData = reader.readBlock(blockId);
                const auto& header = blockData.header;

                // 创建块头用于解压
                format::BlockHeader decompressHeader;
                decompressHeader.blockId = header.blockId;
                decompressHeader.uncompressedCount = header.uncompressedCount;
                decompressHeader.uniformReadLength = header.uniformReadLength;
                decompressHeader.codecIds = header.codecIds;
                decompressHeader.codecSeq = header.codecSeq;
                decompressHeader.codecQual = header.codecQual;
                decompressHeader.codecAux = header.codecAux;

                // 解压块数据
                auto decompressResult = compressor.decompress(decompressHeader,
                                                              blockData.idsData,
                                                              blockData.seqData,
                                                              blockData.qualData,
                                                              blockData.auxData);

                if (!decompressResult) {
                    blockResult.passed = false;
                    blockResult.errorMessage = "解压失败: " + decompressResult.error().message();
                    ++corruptedBlocks;
                    results.push_back(blockResult);
                    if (options_.failFast) {
                        break;
                    }
                    continue;
                }

                // 计算解压后数据的校验和
                // 需要将解压后的 reads 转换为字节流来计算校验和
                std::uint64_t calculatedChecksum =
                    calculateDecompressedChecksum(decompressResult.value().reads);

                // 验证校验和
                if (calculatedChecksum != header.blockXxhash64) {
                    blockResult.passed = false;
                    blockResult.errorMessage = "校验和不匹配: 期望 0x" +
                        fmt::format("{:016x}", header.blockXxhash64) + ", 实际 0x" +
                        fmt::format("{:016x}", calculatedChecksum);
                    ++checksumMismatches;
                    results.push_back(blockResult);
                    if (options_.failFast) {
                        break;
                    }
                    continue;
                }

                blockResult.passed = true;
                blockResult.details =
                    "验证通过 (" + std::to_string(header.uncompressedCount) + " reads)";
                results.push_back(blockResult);

            } catch (const std::exception& e) {
                blockResult.passed = false;
                blockResult.errorMessage = "验证异常: " + std::string(e.what());
                ++corruptedBlocks;
                results.push_back(blockResult);
                if (options_.failFast) {
                    break;
                }
            }
        }

        // 添加汇总结果
        VerificationResult summary;
        summary.checkName = "Block Payload Verification";
        if (corruptedBlocks == 0 && checksumMismatches == 0) {
            summary.passed = true;
            summary.details = fmt::format("{} blocks verified successfully", totalBlocks);
        } else {
            summary.passed = false;
            std::string msg;
            if (corruptedBlocks > 0) {
                msg += fmt::format("{} corrupted blocks", corruptedBlocks);
            }
            if (checksumMismatches > 0) {
                if (!msg.empty())
                    msg += ", ";
                msg += fmt::format("{} checksum mismatches", checksumMismatches);
            }
            summary.errorMessage = msg + fmt::format(" (out of {} total)", totalBlocks);
        }
        results.insert(results.begin(), summary);

    } catch (const std::exception& e) {
        VerificationResult err;
        err.checkName = "Block Payload Verification";
        err.passed = false;
        err.errorMessage = "验证失败: " + std::string(e.what());
        results.push_back(err);
    }

    return results;
}

std::uint64_t VerifyCommand::calculateDecompressedChecksum(const std::vector<ReadRecord>& reads) {
    XXH64_state_t* state = XXH64_createState();
    if (!state) {
        throw IOError("Failed to create xxHash64 state for block checksum");
    }

    // 使用 RAII guard 确保状态被释放
    struct XxHashStateGuard {
        XXH64_state_t* state;
        ~XxHashStateGuard() {
            XXH64_freeState(state);
        }
    } guard{state};

    XXH64_reset(state, 0);

    // 按顺序计算：ID || Comments || Seq || Qual || Aux (与 BlockCompressorImpl 一致)
    // Hash IDs
    for (const auto& read : reads) {
        XXH64_update(state, read.id.data(), read.id.size());
    }

    // Hash comments
    for (const auto& read : reads) {
        if (!read.comment.empty()) {
            XXH64_update(state, read.comment.data(), read.comment.size());
        }
    }

    // Hash sequences
    for (const auto& read : reads) {
        XXH64_update(state, read.sequence.data(), read.sequence.size());
    }

    // Hash quality
    for (const auto& read : reads) {
        XXH64_update(state, read.quality.data(), read.quality.size());
    }

    // Hash lengths (aux) - 逐个添加，与 BlockCompressorImpl 一致
    for (const auto& read : reads) {
        std::uint32_t len = static_cast<std::uint32_t>(read.sequence.length());
        XXH64_update(state, &len, sizeof(len));
    }

    return XXH64_digest(state);
}

void VerifyCommand::printSummary() const {
    if (options_.jsonOutput) {
        // JSON output
        std::cout << "{" << std::endl;
        std::cout << "  \"file\": \"" << options_.inputPath.string() << "\"," << std::endl;
        std::cout << "  \"status\": \"" << (summary_.passed() ? "ok" : "failed") << "\","
                  << std::endl;
        std::cout << "  \"total_checks\": " << summary_.totalChecks << "," << std::endl;
        std::cout << "  \"passed_checks\": " << summary_.passedChecks << "," << std::endl;
        std::cout << "  \"failed_checks\": " << summary_.failedChecks << "," << std::endl;
        std::cout << "  \"mode\": \"" << (options_.quickMode ? "quick" : "full") << "\","
                  << std::endl;
        std::cout << "  \"results\": [" << std::endl;
        for (std::size_t i = 0; i < summary_.results.size(); ++i) {
            const auto& result = summary_.results[i];
            std::cout << "    {\"check\": \"" << result.checkName
                      << "\", \"passed\": " << (result.passed ? "true" : "false");
            if (!result.passed && !result.errorMessage.empty()) {
                std::cout << ", \"error\": \"" << result.errorMessage << "\"";
            }
            if (!result.details.empty()) {
                std::cout << ", \"details\": \"" << result.details << "\"";
            }
            std::cout << "}" << (i + 1 < summary_.results.size() ? "," : "") << std::endl;
        }
        std::cout << "  ]" << std::endl;
        std::cout << "}" << std::endl;
        return;
    }

    // Text output
    std::cout << std::endl;
    std::cout << "=== Verification Summary ===" << std::endl;
    std::cout << "File:    " << options_.inputPath.string() << std::endl;
    std::cout << "Mode:    " << (options_.quickMode ? "quick" : "full") << std::endl;
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

std::unique_ptr<VerifyCommand> createVerifyCommand(const std::string& inputPath,
                                                   bool failFast,
                                                   bool verbose) {
    VerifyOptions opts;
    opts.inputPath = inputPath;
    opts.failFast = failFast;
    opts.verbose = verbose;

    return std::make_unique<VerifyCommand>(std::move(opts));
}

}  // namespace fqc::commands
