// =============================================================================
// fq-compressor - Info Command
// =============================================================================
// Command handler for displaying archive information.
//
// This module provides:
// - InfoCommand: Display archive metadata and statistics
// - Support for JSON output format
// - Detailed block-level information
//
// Requirements: 5.3, 6.2
// =============================================================================

#ifndef FQC_COMMANDS_INFO_COMMAND_H
#define FQC_COMMANDS_INFO_COMMAND_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "fqc/common/error.h"

namespace fqc::commands {

// =============================================================================
// Info Options
// =============================================================================

/// @brief Configuration options for info command.
struct InfoOptions {
    /// @brief Input .fqc file path.
    std::filesystem::path inputPath;

    /// @brief Output as JSON.
    bool jsonOutput = false;

    /// @brief Show detailed block information.
    bool detailed = false;

    /// @brief Show codec information.
    bool showCodecs = false;
};

// =============================================================================
// InfoCommand Class
// =============================================================================

/// @brief Command handler for displaying archive information.
class InfoCommand {
public:
    /// @brief Construct with options.
    explicit InfoCommand(InfoOptions options);

    /// @brief Destructor.
    ~InfoCommand();

    // Non-copyable, movable
    InfoCommand(const InfoCommand&) = delete;
    InfoCommand& operator=(const InfoCommand&) = delete;
    InfoCommand(InfoCommand&&) noexcept;
    InfoCommand& operator=(InfoCommand&&) noexcept;

    /// @brief Execute the info command.
    /// @return Exit code (0 = success).
    [[nodiscard]] int execute();

    /// @brief Get the options.
    [[nodiscard]] const InfoOptions& options() const noexcept { return options_; }

private:
    /// @brief Print info in text format.
    void printTextInfo();

    /// @brief Print info in JSON format.
    void printJsonInfo();

    /// @brief Print detailed block information.
    void printBlockDetails();

    /// @brief Options.
    InfoOptions options_;
};

// =============================================================================
// Factory Function
// =============================================================================

/// @brief Create an info command from CLI options.
[[nodiscard]] std::unique_ptr<InfoCommand> createInfoCommand(
    const std::string& inputPath,
    bool jsonOutput,
    bool detailed);

}  // namespace fqc::commands

#endif  // FQC_COMMANDS_INFO_COMMAND_H
