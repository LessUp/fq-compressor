// =============================================================================
// fq-compressor - Info Command Implementation
// =============================================================================
// Command handler implementation for displaying archive information.
//
// Requirements: 5.3, 6.2
// =============================================================================

#include "info_command.h"

#include <fstream>
#include <iomanip>
#include <iostream>

#include "fqc/common/logger.h"
#include "fqc/format/fqc_format.h"

namespace fqc::commands {

// =============================================================================
// InfoCommand Implementation
// =============================================================================

InfoCommand::InfoCommand(InfoOptions options) : options_(std::move(options)) {}

InfoCommand::~InfoCommand() = default;

InfoCommand::InfoCommand(InfoCommand&&) noexcept = default;
InfoCommand& InfoCommand::operator=(InfoCommand&&) noexcept = default;

int InfoCommand::execute() {
    try {
        // Check input exists
        if (!std::filesystem::exists(options_.inputPath)) {
            throw IOError(
                          "Input file not found: " + options_.inputPath.string());
        }

        if (options_.jsonOutput) {
            printJsonInfo();
        } else {
            printTextInfo();
        }

        return 0;

    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Info command failed: {}", e.what());
        return static_cast<int>(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return 1;
    }
}

void InfoCommand::printTextInfo() {
    // Open file and read basic info
    std::ifstream file(options_.inputPath, std::ios::binary);
    if (!file) {
        throw IOError(
                      "Failed to open file: " + options_.inputPath.string());
    }

    // Get file size
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read magic header
    char magic[9] = {0};
    file.read(magic, 8);

    // Verify magic
    const char expectedMagic[] = "\x89" "FQC\r\n" "\x1a\n";
    bool validMagic = (std::memcmp(magic, expectedMagic, 8) == 0);

    // Read version byte
    std::uint8_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 1);

    std::cout << "=== FQC Archive Information ===" << std::endl;
    std::cout << std::endl;
    std::cout << "File:           " << options_.inputPath.string() << std::endl;
    std::cout << "Size:           " << fileSize << " bytes" << std::endl;
    std::cout << "Magic:          " << (validMagic ? "Valid" : "INVALID") << std::endl;
    std::cout << "Version:        " << static_cast<int>(version >> 4) << "."
              << static_cast<int>(version & 0x0F) << std::endl;

    if (!validMagic) {
        std::cout << std::endl;
        std::cout << "WARNING: Invalid magic header - file may be corrupted" << std::endl;
        return;
    }

    // TODO: Read and display GlobalHeader, BlockIndex, etc.
    // This is a placeholder for Phase 2/3 implementation

    std::cout << std::endl;
    std::cout << "--- Global Header ---" << std::endl;
    std::cout << "(Detailed header parsing not yet implemented)" << std::endl;

    if (options_.detailed) {
        printBlockDetails();
    }

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
}

void InfoCommand::printJsonInfo() {
    // Open file and read basic info
    std::ifstream file(options_.inputPath, std::ios::binary);
    if (!file) {
        throw IOError(
                      "Failed to open file: " + options_.inputPath.string());
    }

    // Get file size
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read magic header
    char magic[9] = {0};
    file.read(magic, 8);

    // Verify magic
    const char expectedMagic[] = "\x89" "FQC\r\n" "\x1a\n";
    bool validMagic = (std::memcmp(magic, expectedMagic, 8) == 0);

    // Read version byte
    std::uint8_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 1);

    // Output JSON
    std::cout << "{" << std::endl;
    std::cout << "  \"file\": \"" << options_.inputPath.string() << "\"," << std::endl;
    std::cout << "  \"size\": " << fileSize << "," << std::endl;
    std::cout << "  \"valid_magic\": " << (validMagic ? "true" : "false") << "," << std::endl;
    std::cout << "  \"version\": {" << std::endl;
    std::cout << "    \"major\": " << static_cast<int>(version >> 4) << "," << std::endl;
    std::cout << "    \"minor\": " << static_cast<int>(version & 0x0F) << std::endl;
    std::cout << "  }" << std::endl;

    // TODO: Add more fields as they become available

    std::cout << "}" << std::endl;
}

void InfoCommand::printBlockDetails() {
    std::cout << std::endl;
    std::cout << "--- Block Details ---" << std::endl;
    std::cout << "(Block details not yet implemented)" << std::endl;

    // TODO: Iterate through blocks and display info
    // This is a placeholder for Phase 2/3 implementation
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<InfoCommand> createInfoCommand(
    const std::string& inputPath,
    bool jsonOutput,
    bool detailed) {

    InfoOptions opts;
    opts.inputPath = inputPath;
    opts.jsonOutput = jsonOutput;
    opts.detailed = detailed;

    return std::make_unique<InfoCommand>(std::move(opts));
}

}  // namespace fqc::commands
