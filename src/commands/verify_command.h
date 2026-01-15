// =============================================================================
// fq-compressor - Verify Command
// =============================================================================
// Command handler for verifying archive integrity.
//
// This module provides:
// - VerifyCommand: Verify archive checksums and structure
// - Support for quick and full verification modes
// - Detailed error reporting
//
// Requirements: 5.1, 5.2, 5.3, 8.5
// =============================================================================

#ifndef FQC_COMMANDS_VERIFY_COMMAND_H
#define FQC_COMMANDS_VERIFY_COMMAND_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"

namespace fqc::commands {

// =============================================================================
// Verification Result
// =============================================================================

/// @brief Result of a single verification check.
struct VerificationResult {
    /// @brief Check name.
    std::string checkName;

    /// @brief Whether check passed.
    bool passed = false;

    /// @brief Error message (if failed).
    std::string errorMessage;

    /// @brief Additional details.
    std::string details;
};

/// @brief Overall verification summary.
struct VerificationSummary {
    /// @brief Total checks performed.
    std::uint32_t totalChecks = 0;

    /// @brief Checks passed.
    std::uint32_t passedChecks = 0;

    /// @brief Checks failed.
    std::uint32_t failedChecks = 0;

    /// @brief Individual results.
    std::vector<VerificationResult> results;

    /// @brief Overall pass/fail.
    [[nodiscard]] bool passed() const noexcept { return failedChecks == 0; }

    /// @brief Add a result.
    void addResult(VerificationResult result) {
        ++totalChecks;
        if (result.passed) {
            ++passedChecks;
        } else {
            ++failedChecks;
        }
        results.push_back(std::move(result));
    }
};

// =============================================================================
// Verify Options
// =============================================================================

/// @brief Configuration options for verify command.
struct VerifyOptions {
    /// @brief Input .fqc file path.
    std::filesystem::path inputPath;

    /// @brief Stop on first error.
    bool failFast = false;

    /// @brief Show detailed verification progress.
    bool verbose = false;

    /// @brief Quick verification (magic + footer only).
    bool quickMode = false;

    /// @brief Verify block checksums.
    bool verifyBlocks = true;

    /// @brief Verify global checksum.
    bool verifyGlobal = true;
};

// =============================================================================
// VerifyCommand Class
// =============================================================================

/// @brief Command handler for verifying archive integrity.
class VerifyCommand {
public:
    /// @brief Construct with options.
    explicit VerifyCommand(VerifyOptions options);

    /// @brief Destructor.
    ~VerifyCommand();

    // Non-copyable, movable
    VerifyCommand(const VerifyCommand&) = delete;
    VerifyCommand& operator=(const VerifyCommand&) = delete;
    VerifyCommand(VerifyCommand&&) noexcept;
    VerifyCommand& operator=(VerifyCommand&&) noexcept;

    /// @brief Execute the verify command.
    /// @return Exit code (0 = success, non-zero = verification failed).
    [[nodiscard]] int execute();

    /// @brief Get verification summary.
    [[nodiscard]] const VerificationSummary& summary() const noexcept { return summary_; }

    /// @brief Get the options.
    [[nodiscard]] const VerifyOptions& options() const noexcept { return options_; }

private:
    /// @brief Verify magic header.
    [[nodiscard]] VerificationResult verifyMagicHeader();

    /// @brief Verify file footer.
    [[nodiscard]] VerificationResult verifyFooter();

    /// @brief Verify global checksum.
    [[nodiscard]] VerificationResult verifyGlobalChecksum();

    /// @brief Verify block index.
    [[nodiscard]] VerificationResult verifyBlockIndex();

    /// @brief Verify individual block checksums.
    [[nodiscard]] std::vector<VerificationResult> verifyBlockChecksums();

    /// @brief Print verification summary.
    void printSummary() const;

    /// @brief Options.
    VerifyOptions options_;

    /// @brief Verification summary.
    VerificationSummary summary_;
};

// =============================================================================
// Factory Function
// =============================================================================

/// @brief Create a verify command from CLI options.
[[nodiscard]] std::unique_ptr<VerifyCommand> createVerifyCommand(
    const std::string& inputPath,
    bool failFast,
    bool verbose);

}  // namespace fqc::commands

#endif  // FQC_COMMANDS_VERIFY_COMMAND_H
