// =============================================================================
// fq-compressor - Paired-End FASTQ Parser
// =============================================================================
// Parser for paired-end FASTQ files with support for:
// - Dual file input (R1.fastq + R2.fastq)
// - Interleaved format (R1, R2, R1, R2, ...)
//
// Requirements: 1.1.3
// =============================================================================

#ifndef FQC_IO_PAIRED_END_PARSER_H
#define FQC_IO_PAIRED_END_PARSER_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"
#include "fqc/io/fastq_parser.h"

namespace fqc::io {

// =============================================================================
// Paired-End Record
// =============================================================================

/// @brief Represents a paired-end read pair (R1 + R2).
struct PairedEndRecord {
    /// @brief Read 1 (forward read).
    FastqRecord read1;

    /// @brief Read 2 (reverse read).
    FastqRecord read2;

    /// @brief Check if pair is valid (both reads non-empty with matching lengths).
    [[nodiscard]] bool isValid() const noexcept {
        return read1.isValid() && read2.isValid();
    }

    /// @brief Get combined length of both reads.
    [[nodiscard]] std::size_t totalLength() const noexcept {
        return read1.length() + read2.length();
    }

    /// @brief Clear both reads.
    void clear() noexcept {
        read1.clear();
        read2.clear();
    }
};

// =============================================================================
// Paired-End Parser Configuration
// =============================================================================

/// @brief Input mode for paired-end data.
enum class PEInputMode : std::uint8_t {
    /// @brief Two separate files (R1.fastq + R2.fastq).
    kDualFile = 0,

    /// @brief Single interleaved file (R1, R2, R1, R2, ...).
    kInterleaved = 1
};

/// @brief Configuration options for the paired-end parser.
struct PairedEndParserOptions {
    /// @brief Input mode.
    PEInputMode inputMode = PEInputMode::kDualFile;

    /// @brief Validate that R1/R2 IDs match (after stripping /1, /2 suffix).
    bool validatePairing = true;

    /// @brief Base parser options.
    ParserOptions baseOptions = {};
};

// =============================================================================
// Paired-End Parser Statistics
// =============================================================================

/// @brief Statistics collected during PE parsing.
struct PairedEndStats {
    /// @brief Total pairs parsed.
    std::uint64_t totalPairs = 0;

    /// @brief Statistics for R1 reads.
    ParserStats r1Stats;

    /// @brief Statistics for R2 reads.
    ParserStats r2Stats;

    /// @brief Number of pairs with mismatched IDs (if validation enabled).
    std::uint64_t mismatchedPairs = 0;

    /// @brief Reset statistics.
    void reset() noexcept { *this = PairedEndStats{}; }
};

// =============================================================================
// PairedEndParser Class
// =============================================================================

/// @brief Parser for paired-end FASTQ files.
///
/// Supports:
/// - Dual file input: Separate R1 and R2 files
/// - Interleaved input: Single file with alternating R1/R2 reads
///
/// Thread Safety:
/// - Not thread-safe for concurrent parsing
/// - Use separate parser instances for parallel processing
class PairedEndParser {
public:
    // =========================================================================
    // Types
    // =========================================================================

    /// @brief Chunk of parsed pairs.
    using Chunk = std::vector<PairedEndRecord>;

    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /// @brief Construct for dual file input.
    /// @param r1Path Path to R1 file.
    /// @param r2Path Path to R2 file.
    /// @param options Parser options.
    PairedEndParser(std::filesystem::path r1Path,
                    std::filesystem::path r2Path,
                    PairedEndParserOptions options = {});

    /// @brief Construct for interleaved input.
    /// @param interleavedPath Path to interleaved file.
    /// @param options Parser options (inputMode should be kInterleaved).
    PairedEndParser(std::filesystem::path interleavedPath,
                    PairedEndParserOptions options = {});

    /// @brief Construct from streams (dual file mode).
    /// @param r1Stream R1 input stream.
    /// @param r2Stream R2 input stream.
    /// @param options Parser options.
    PairedEndParser(std::unique_ptr<std::istream> r1Stream,
                    std::unique_ptr<std::istream> r2Stream,
                    PairedEndParserOptions options = {});

    /// @brief Construct from stream (interleaved mode).
    /// @param interleavedStream Interleaved input stream.
    /// @param options Parser options.
    PairedEndParser(std::unique_ptr<std::istream> interleavedStream,
                    PairedEndParserOptions options = {});

    /// @brief Destructor.
    ~PairedEndParser();

    // Non-copyable, movable
    PairedEndParser(const PairedEndParser&) = delete;
    PairedEndParser& operator=(const PairedEndParser&) = delete;
    PairedEndParser(PairedEndParser&&) noexcept;
    PairedEndParser& operator=(PairedEndParser&&) noexcept;

    // =========================================================================
    // Opening and Closing
    // =========================================================================

    /// @brief Open for parsing.
    /// @throws IOError if files cannot be opened.
    void open();

    /// @brief Close the parser.
    void close() noexcept;

    /// @brief Check if the parser is open.
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    /// @brief Check if end of file has been reached.
    [[nodiscard]] bool eof() const noexcept { return eof_; }

    // =========================================================================
    // Parsing Methods
    // =========================================================================

    /// @brief Read a single pair.
    /// @return The parsed pair, or nullopt if EOF.
    /// @throws FormatError on parse error.
    [[nodiscard]] std::optional<PairedEndRecord> readPair();

    /// @brief Read a chunk of pairs.
    /// @param maxPairs Maximum number of pairs to read.
    /// @return Vector of parsed pairs.
    /// @throws FormatError on parse error.
    [[nodiscard]] std::optional<Chunk> readChunk(std::size_t maxPairs);

    /// @brief Read all pairs into memory.
    /// @return Vector of all pairs.
    /// @warning May consume large amounts of memory.
    [[nodiscard]] std::vector<PairedEndRecord> readAll();

    // =========================================================================
    // Statistics and State
    // =========================================================================

    /// @brief Get current parsing statistics.
    [[nodiscard]] const PairedEndStats& stats() const noexcept { return stats_; }

    /// @brief Get the input mode.
    [[nodiscard]] PEInputMode inputMode() const noexcept { return options_.inputMode; }

    /// @brief Check if input is from stdin.
    [[nodiscard]] bool isStdin() const noexcept;

    // =========================================================================
    // Seeking (File Input Only)
    // =========================================================================

    /// @brief Reset to the beginning.
    /// @throws IOError if seeking fails or input is stdin.
    void reset();

    /// @brief Check if seeking is supported.
    [[nodiscard]] bool canSeek() const noexcept;

private:
    /// @brief Validate that R1/R2 IDs match.
    [[nodiscard]] bool validatePairIds(const FastqRecord& r1, const FastqRecord& r2) const;

    /// @brief Strip /1, /2 or .1, .2 suffix from read ID.
    [[nodiscard]] static std::string_view stripPairSuffix(std::string_view id);

    // Member Variables
    PairedEndParserOptions options_;
    std::unique_ptr<FastqParser> r1Parser_;
    std::unique_ptr<FastqParser> r2Parser_;  // nullptr for interleaved mode
    std::filesystem::path r1Path_;
    std::filesystem::path r2Path_;           // empty for interleaved mode
    PairedEndStats stats_;
    bool isOpen_ = false;
    bool eof_ = false;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Check if a FASTQ file appears to be interleaved PE.
/// @param filePath Path to the file.
/// @param sampleCount Number of records to check.
/// @return true if the file appears to be interleaved.
[[nodiscard]] bool detectInterleavedFormat(const std::filesystem::path& filePath,
                                           std::size_t sampleCount = 10);

/// @brief Extract base read ID (without /1, /2, etc.).
/// @param id Full read ID.
/// @return Base ID with pair suffix removed.
[[nodiscard]] std::string_view extractBaseReadId(std::string_view id);

/// @brief Check if two read IDs are from the same pair.
/// @param id1 First read ID.
/// @param id2 Second read ID.
/// @return true if IDs match after stripping pair suffixes.
[[nodiscard]] bool arePairedIds(std::string_view id1, std::string_view id2);

}  // namespace fqc::io

#endif  // FQC_IO_PAIRED_END_PARSER_H
