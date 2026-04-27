// =============================================================================
// fq-compressor - Info Command Implementation
// =============================================================================
// Command handler implementation for displaying archive information.
//
// Requirements: 5.3, 6.2
// =============================================================================

#include "fqc/commands/info_command.h"

#include "fqc/common/logger.h"
#include "fqc/format/fqc_format.h"
#include "fqc/format/fqc_reader.h"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace fqc::commands {

// =============================================================================
// Helper: Codec family to human-readable string
// =============================================================================

namespace {

[[nodiscard]] std::string_view codecFamilyToString(CodecFamily family) noexcept {
    switch (family) {
        case CodecFamily::kRaw:
            return "Raw";
        case CodecFamily::kAbcV1:
            return "ABC-v1";
        case CodecFamily::kScmV1:
            return "SCM-v1";
        case CodecFamily::kDeltaLzma:
            return "Delta+LZMA";
        case CodecFamily::kDeltaZstd:
            return "Delta+Zstd";
        case CodecFamily::kDeltaVarint:
            return "Delta+Varint";
        case CodecFamily::kOverlapV1:
            return "Overlap-v1";
        case CodecFamily::kZstdPlain:
            return "Zstd";
        case CodecFamily::kScmOrder1:
            return "SCM-Order1";
        case CodecFamily::kExternal:
            return "External";
        case CodecFamily::kReserved:
            return "Reserved";
    }
    return "unknown";
}

[[nodiscard]] std::string codecByteToString(std::uint8_t codec) {
    auto family = format::decodeCodecFamily(codec);
    auto ver = format::decodeCodecVersion(codec);
    std::ostringstream oss;
    oss << codecFamilyToString(family);
    if (ver > 0)
        oss << "." << static_cast<int>(ver);
    return oss.str();
}

[[nodiscard]] std::string formatBytes(std::uint64_t bytes) {
    std::ostringstream oss;
    if (bytes >= 1024ULL * 1024 * 1024) {
        oss << std::fixed << std::setprecision(2)
            << static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0) << " GiB";
    } else if (bytes >= 1024ULL * 1024) {
        oss << std::fixed << std::setprecision(2) << static_cast<double>(bytes) / (1024.0 * 1024.0)
            << " MiB";
    } else if (bytes >= 1024) {
        oss << std::fixed << std::setprecision(2) << static_cast<double>(bytes) / 1024.0 << " KiB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

[[nodiscard]] std::string formatTimestamp(std::uint64_t ts) {
    if (ts == 0)
        return "(not set)";
    auto t = static_cast<std::time_t>(ts);
    std::tm tmBuf{};
#ifdef _WIN32
    gmtime_s(&tmBuf, &t);
#else
    gmtime_r(&t, &tmBuf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tmBuf);
    return std::string(buf);
}

// Escape a string for JSON output (minimal: backslash, quotes, control chars).
[[nodiscard]] std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

}  // namespace

// =============================================================================
// InfoCommand Implementation
// =============================================================================

InfoCommand::InfoCommand(InfoOptions options) : options_(std::move(options)) {}

InfoCommand::~InfoCommand() = default;

InfoCommand::InfoCommand(InfoCommand&&) noexcept = default;
InfoCommand& InfoCommand::operator=(InfoCommand&&) noexcept = default;

int InfoCommand::execute() {
    try {
        if (!std::filesystem::exists(options_.inputPath)) {
            throw IOError("Input file not found: " + options_.inputPath.string());
        }

        if (options_.jsonOutput) {
            printJsonInfo();
        } else {
            printTextInfo();
        }

        return 0;

    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Info command failed: {}", e.what());
        return toExitCode(e.code());
    } catch (const std::exception& e) {
        FQC_LOG_ERROR("Unexpected error: {}", e.what());
        return 1;
    }
}

void InfoCommand::printTextInfo() {
    // Open archive via FQCReader
    format::FQCReader reader(options_.inputPath);
    reader.open();

    auto fileSize = std::filesystem::file_size(options_.inputPath);
    const auto& gh = reader.globalHeader();
    const auto& ft = reader.footer();
    auto version = reader.version();

    std::cout << "=== FQC Archive Information ===" << std::endl;
    std::cout << std::endl;
    std::cout << "File:             " << options_.inputPath.string() << std::endl;
    std::cout << "Size:             " << fileSize << " bytes (" << formatBytes(fileSize) << ")"
              << std::endl;
    std::cout << "Magic:            Valid" << std::endl;
    std::cout << "Version:          " << static_cast<int>(format::decodeMajorVersion(version))
              << "." << static_cast<int>(format::decodeMinorVersion(version)) << std::endl;

    // --- Global Header ---
    std::cout << std::endl;
    std::cout << "--- Global Header ---" << std::endl;
    std::cout << "Total reads:      " << gh.totalReadCount << std::endl;
    std::cout << "Compression algo: "
              << codecFamilyToString(static_cast<CodecFamily>(gh.compressionAlgo)) << std::endl;
    std::cout << "Checksum type:    "
              << checksumTypeToString(static_cast<ChecksumType>(gh.checksumType)) << std::endl;
    std::cout << "Quality mode:     " << qualityModeToString(format::getQualityMode(gh.flags))
              << std::endl;
    std::cout << "ID mode:          " << idModeToString(format::getIdMode(gh.flags)) << std::endl;
    std::cout << "Read length class: "
              << readLengthClassToString(format::getReadLengthClass(gh.flags)) << std::endl;
    std::cout << "Paired-end:       " << (format::isPaired(gh.flags) ? "yes" : "no") << std::endl;
    if (format::isPaired(gh.flags)) {
        std::cout << "PE layout:        " << peLayoutToString(format::getPeLayout(gh.flags))
                  << std::endl;
    }
    std::cout << "Preserve order:   " << (format::isPreserveOrder(gh.flags) ? "yes" : "no")
              << std::endl;
    std::cout << "Has reorder map:  " << (format::hasReorderMap(gh.flags) ? "yes" : "no")
              << std::endl;
    std::cout << "Streaming mode:   " << (format::isStreamingMode(gh.flags) ? "yes" : "no")
              << std::endl;

    if (!reader.originalFilename().empty()) {
        std::cout << "Original file:    " << reader.originalFilename() << std::endl;
    }
    std::cout << "Timestamp:        " << formatTimestamp(reader.timestamp()) << std::endl;

    // --- Index Summary ---
    if (options_.showIndex || options_.detailed) {
        const auto& index = reader.blockIndex();
        std::cout << std::endl;
        std::cout << "--- Block Index (" << index.size() << " blocks) ---" << std::endl;
        std::cout << "Index offset:     " << ft.indexOffset << std::endl;
        if (ft.hasReorderMap()) {
            std::cout << "Reorder map at:   " << ft.reorderMapOffset << std::endl;
        }
        std::cout << "Global checksum:  0x" << std::hex << ft.globalChecksum << std::dec
                  << std::endl;

        for (std::size_t i = 0; i < index.size(); ++i) {
            const auto& e = index[i];
            std::cout << "  Block " << std::setw(4) << i << "  offset=" << e.offset
                      << "  size=" << e.compressedSize << "  reads=" << e.readCount << "  range=["
                      << e.archiveIdStart << "," << e.archiveIdEnd() << ")" << std::endl;
        }
    }

    // --- Block Details ---
    if (options_.detailed) {
        printBlockDetails();
    }

    // --- Codec Statistics ---
    if (options_.showCodecs) {
        printCodecStats(reader);
    }

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
}

void InfoCommand::printJsonInfo() {
    format::FQCReader reader(options_.inputPath);
    reader.open();

    auto fileSize = std::filesystem::file_size(options_.inputPath);
    const auto& gh = reader.globalHeader();
    const auto& ft = reader.footer();
    auto version = reader.version();
    const auto& index = reader.blockIndex();

    std::cout << "{" << std::endl;
    std::cout << "  \"file\": \"" << jsonEscape(options_.inputPath.string()) << "\"," << std::endl;
    std::cout << "  \"size\": " << fileSize << "," << std::endl;
    std::cout << "  \"valid_magic\": true," << std::endl;
    std::cout << "  \"version\": {" << std::endl;
    std::cout << "    \"major\": " << static_cast<int>(format::decodeMajorVersion(version)) << ","
              << std::endl;
    std::cout << "    \"minor\": " << static_cast<int>(format::decodeMinorVersion(version))
              << std::endl;
    std::cout << "  }," << std::endl;

    // Global header
    std::cout << "  \"global_header\": {" << std::endl;
    std::cout << "    \"total_reads\": " << gh.totalReadCount << "," << std::endl;
    std::cout << "    \"compression_algo\": \""
              << codecFamilyToString(static_cast<CodecFamily>(gh.compressionAlgo)) << "\","
              << std::endl;
    std::cout << "    \"checksum_type\": \""
              << checksumTypeToString(static_cast<ChecksumType>(gh.checksumType)) << "\","
              << std::endl;
    std::cout << "    \"quality_mode\": \"" << qualityModeToString(format::getQualityMode(gh.flags))
              << "\"," << std::endl;
    std::cout << "    \"id_mode\": \"" << idModeToString(format::getIdMode(gh.flags)) << "\","
              << std::endl;
    std::cout << "    \"read_length_class\": \""
              << readLengthClassToString(format::getReadLengthClass(gh.flags)) << "\","
              << std::endl;
    std::cout << "    \"paired\": " << (format::isPaired(gh.flags) ? "true" : "false") << ","
              << std::endl;
    if (format::isPaired(gh.flags)) {
        std::cout << "    \"pe_layout\": \"" << peLayoutToString(format::getPeLayout(gh.flags))
                  << "\"," << std::endl;
    }
    std::cout << "    \"preserve_order\": "
              << (format::isPreserveOrder(gh.flags) ? "true" : "false") << "," << std::endl;
    std::cout << "    \"has_reorder_map\": " << (reader.hasReorderMap() ? "true" : "false") << ","
              << std::endl;
    std::cout << "    \"streaming_mode\": "
              << (format::isStreamingMode(gh.flags) ? "true" : "false") << "," << std::endl;
    std::cout << "    \"original_filename\": \"" << jsonEscape(reader.originalFilename()) << "\","
              << std::endl;
    std::cout << "    \"timestamp\": " << reader.timestamp() << std::endl;
    std::cout << "  }," << std::endl;

    // Footer
    std::cout << "  \"footer\": {" << std::endl;
    std::cout << "    \"index_offset\": " << ft.indexOffset << "," << std::endl;
    std::cout << "    \"reorder_map_offset\": " << ft.reorderMapOffset << "," << std::endl;
    std::cout << "    \"global_checksum\": \"0x" << std::hex << ft.globalChecksum << std::dec
              << "\"" << std::endl;
    std::cout << "  }," << std::endl;

    // Block index
    std::cout << "  \"block_count\": " << index.size() << "," << std::endl;
    std::cout << "  \"blocks\": [" << std::endl;
    for (std::size_t i = 0; i < index.size(); ++i) {
        const auto& e = index[i];
        std::cout << "    {\"id\": " << i << ", \"offset\": " << e.offset
                  << ", \"compressed_size\": " << e.compressedSize
                  << ", \"read_count\": " << e.readCount
                  << ", \"archive_id_start\": " << e.archiveIdStart << "}"
                  << (i + 1 < index.size() ? "," : "") << std::endl;
    }
    std::cout << "  ]" << std::endl;
    std::cout << "}" << std::endl;
}

void InfoCommand::printBlockDetails() {
    format::FQCReader reader(options_.inputPath);
    reader.open();

    auto blockCount = reader.blockCount();
    std::cout << std::endl;
    std::cout << "--- Block Details ---" << std::endl;

    for (BlockId bid = 0; bid < static_cast<BlockId>(blockCount); ++bid) {
        auto bh = reader.readBlockHeader(bid);
        std::cout << "Block " << bid << ":" << std::endl;
        std::cout << "  Reads:          " << bh.uncompressedCount << std::endl;
        std::cout << "  Uniform length: "
                  << (bh.hasUniformLength() ? std::to_string(bh.uniformReadLength) : "variable")
                  << std::endl;
        std::cout << "  Compressed:     " << bh.compressedSize << " bytes ("
                  << formatBytes(bh.compressedSize) << ")" << std::endl;
        std::cout << "  Checksum:       0x" << std::hex << bh.blockXxhash64 << std::dec
                  << std::endl;
        std::cout << "  Codecs:         IDs=" << codecByteToString(bh.codecIds)
                  << "  Seq=" << codecByteToString(bh.codecSeq)
                  << "  Qual=" << codecByteToString(bh.codecQual)
                  << "  Aux=" << codecByteToString(bh.codecAux) << std::endl;
        std::cout << "  Stream sizes:   IDs=" << bh.sizeIds << "  Seq=" << bh.sizeSeq
                  << "  Qual=" << bh.sizeQual << "  Aux=" << bh.sizeAux << std::endl;
        if (bh.isQualityDiscarded()) {
            std::cout << "  (quality discarded)" << std::endl;
        }
    }
}

void InfoCommand::printCodecStats(format::FQCReader& reader) {
    auto blockCount = reader.blockCount();
    if (blockCount == 0)
        return;

    // Tally codec usage per stream
    std::map<std::string, std::uint64_t> idsCodecs, seqCodecs, qualCodecs, auxCodecs;
    std::uint64_t totalIds = 0, totalSeq = 0, totalQual = 0, totalAux = 0;

    for (BlockId bid = 0; bid < static_cast<BlockId>(blockCount); ++bid) {
        auto bh = reader.readBlockHeader(bid);
        idsCodecs[codecByteToString(bh.codecIds)] += bh.sizeIds;
        seqCodecs[codecByteToString(bh.codecSeq)] += bh.sizeSeq;
        qualCodecs[codecByteToString(bh.codecQual)] += bh.sizeQual;
        auxCodecs[codecByteToString(bh.codecAux)] += bh.sizeAux;
        totalIds += bh.sizeIds;
        totalSeq += bh.sizeSeq;
        totalQual += bh.sizeQual;
        totalAux += bh.sizeAux;
    }

    auto printStreamCodecs = [](const char* name,
                                const std::map<std::string, std::uint64_t>& codecs,
                                std::uint64_t total) {
        std::cout << "  " << name << " (" << formatBytes(total) << " total):" << std::endl;
        for (const auto& [codec, bytes] : codecs) {
            double pct =
                total > 0 ? 100.0 * static_cast<double>(bytes) / static_cast<double>(total) : 0.0;
            std::cout << "    " << codec << ": " << formatBytes(bytes) << " (" << std::fixed
                      << std::setprecision(1) << pct << "%)" << std::endl;
        }
    };

    std::cout << std::endl;
    std::cout << "--- Codec Statistics ---" << std::endl;
    printStreamCodecs("IDs", idsCodecs, totalIds);
    printStreamCodecs("Sequence", seqCodecs, totalSeq);
    printStreamCodecs("Quality", qualCodecs, totalQual);
    printStreamCodecs("Aux", auxCodecs, totalAux);

    auto totalAll = totalIds + totalSeq + totalQual + totalAux;
    std::cout << "  Total compressed payload: " << formatBytes(totalAll) << std::endl;
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<InfoCommand> createInfoCommand(
    const std::string& inputPath, bool jsonOutput, bool detailed, bool showIndex, bool showCodecs) {
    InfoOptions opts;
    opts.inputPath = inputPath;
    opts.jsonOutput = jsonOutput;
    opts.detailed = detailed;
    opts.showIndex = showIndex;
    opts.showCodecs = showCodecs;

    return std::make_unique<InfoCommand>(std::move(opts));
}

}  // namespace fqc::commands
