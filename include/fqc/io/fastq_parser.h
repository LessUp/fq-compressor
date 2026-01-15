// =============================================================================
// fq-compressor - FASTQ Parser
// =============================================================================
// High-performance streaming FASTQ parser with chunked reading support.
//
// This module provides:
// - FastqParser: Main class for parsing FASTQ files
// - Streaming/chunked reading for memory efficiency
// - Support for standard 4-line FASTQ format
// - Validation of FASTQ records
//
// Usage:
//   FastqParser parser("/path/to/reads.fastq");
//   parser.open();
//   while (auto chunk = parser.readChunk(10000)) {
//       for (const auto& record : *chunk) {
//           // process record...
//       }
//   }
//
// Requirements: 1.1.1
// =============================================================================

#ifndef FQC_IO_FASTQ_PARSER_H
#define FQC_IO_FASTQ_PARSER_H

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"

namespace fqc::io {

// =============================================================================
// Forward Declarations
// =============================================================================

class CompressedStream;

// =============================================================================
// FASTQ Record
// =============================================================================

/// @brief Represents a single FASTQ record.
/// @note Uses string_view for zero-copy parsing when possible.
struct FastqRecord {
    /// @brief Read identifier (without '@' prefix).
    std::string id;

    /// @brief Optional comment after ID (space-separated).
    std::string comment;

    /// @brief DNA/RNA sequence.
    std::string sequence;

    /// @brief Quality scores (Phred+33 encoded).
    std::string quality;

    /// @brief Check if record is valid.
    [[nodiscard]] bool isValid() const noexcept {
        return !id.empty() && !sequence.empty() && sequence.size() == quality.size();
    }

    /// @brief Get the read length.
    [[nodiscard]] std::size_t length() const noexcept { return sequence.size(); }

    /// @brief Clear the record.
    void clear() noexcept {
        id.clear();
        comment.clear();
        sequence.clear();
        quality.clear();
    }
};

// =============================================================================
// Parser Statistics
// =============================================================================

/// @brief Statistics collected during parsing.
struct ParserStats {
    /// @brief Total records parsed.
    std::uint64_t totalRecords = 0;

    /// @brief Total bases parsed.
    std::uint64_t totalBases = 0;

    /// @brief Minimum read length observed.
    std::uint32_t minLength = std::numeric_limits<std::uint32_t>::max();

    /// @brief Maximum read length observed.
    std::uint32_t maxLength = 0;

    /// @brief Sum of lengths for median calculation.
    std::uint64_t lengthSum = 0;

    /// @brief Number of records with N bases.
    std::uint64_t recordsWithN = 0;

    /// @brief Total N bases.
    std::uint64_t totalNBases = 0;

    /// @brief Update stats with a new record.
    void update(const FastqRecord& record) noexcept {
        ++totalRecords;
        auto len = static_cast<std::uint32_t>(record.length());
        totalBases += len;
        lengthSum += len;
        minLength = std::min(minLength, len);
        maxLength = std::max(maxLength, len);

        // Count N bases
        std::size_t nCount = 0;
        for (char c : record.sequence) {
            if (c == 'N' || c == 'n') ++nCount;
        }
        if (nCount > 0) {
            ++recordsWithN;
            totalNBases += nCount;
        }
    }

    /// @brief Get average read length.
    [[nodiscard]] double averageLength() const noexcept {
        return totalRecords > 0 ? static_cast<double>(lengthSum) / totalRecords : 0.0;
    }

    /// @brief Reset statistics.
    void reset() noexcept { *this = ParserStats{}; }
};

// =============================================================================
// Parser Options
// =============================================================================

/// @brief Configuration options for the FASTQ parser.
struct ParserOptions {
    /// @brief Buffer size for reading (default: 4MB).
    std::size_t bufferSize = 4 * 1024 * 1024;

    /// @brief Whether to validate sequence characters.
    bool validateSequence = true;

    /// @brief Whether to validate quality scores.
    bool validateQuality = true;

    /// @brief Whether to collect statistics.
    bool collectStats = true;

    /// @brief Whether to trim trailing whitespace.
    bool trimWhitespace = true;

    /// @brief Minimum quality score (Phred+33).
    char minQualityChar = '!';  // Phred 0

    /// @brief Maximum quality score (Phred+33).
    char maxQualityChar = '~';  // Phred 93

    /// @brief Valid sequence characters (ACGTN by default).
    std::string validBases = "ACGTNacgtn";
};

// =============================================================================
// Parse Error
// =============================================================================

/// @brief Error information for parsing failures.
struct ParseError {
    /// @brief Line number where error occurred (1-based).
    std::uint64_t lineNumber = 0;

    /// @brief Record number where error occurred (1-based).
    std::uint64_t recordNumber = 0;

    /// @brief Error message.
    std::string message;

    /// @brief The problematic line content (truncated).
    std::string lineContent;
};

// =============================================================================
// FastqParser Class
// =============================================================================

/// @brief High-performance streaming FASTQ parser.
///
/// Thread Safety:
/// - Not thread-safe for concurrent parsing
/// - Use separate parser instances for parallel file processing
class FastqParser {
public:
    // =========================================================================
    // Types
    // =========================================================================

    /// @brief Chunk of parsed records.
    using Chunk = std::vector<FastqRecord>;

    /// @brief Callback for record processing.
    using RecordCallback = std::function<bool(const FastqRecord&)>;

    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /// @brief Construct a parser for the specified file.
    /// @param filePath Path to the FASTQ file (or "-" for stdin).
    /// @param options Parser configuration options.
    explicit FastqParser(std::filesystem::path filePath, ParserOptions options = {});

    /// @brief Construct a parser from an input stream.
    /// @param stream Input stream to read from.
    /// @param options Parser configuration options.
    explicit FastqParser(std::unique_ptr<std::istream> stream, ParserOptions options = {});

    /// @brief Destructor.
    ~FastqParser();

    // Non-copyable, movable
    FastqParser(const FastqParser&) = delete;
    FastqParser& operator=(const FastqParser&) = delete;
    FastqParser(FastqParser&&) noexcept;
    FastqParser& operator=(FastqParser&&) noexcept;

    // =========================================================================
    // Opening and Closing
    // =========================================================================

    /// @brief Open the file for parsing.
    /// @throws IOError if file cannot be opened.
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

    /// @brief Read a single record.
    /// @return The parsed record, or nullopt if EOF.
    /// @throws FormatError on parse error.
    [[nodiscard]] std::optional<FastqRecord> readRecord();

    /// @brief Read a chunk of records.
    /// @param maxRecords Maximum number of records to read.
    /// @return Vector of parsed records (may be smaller than maxRecords at EOF).
    /// @throws FormatError on parse error.
    [[nodiscard]] std::optional<Chunk> readChunk(std::size_t maxRecords);

    /// @brief Process all records with a callback.
    /// @param callback Function called for each record. Return false to stop.
    /// @return Number of records processed.
    /// @throws FormatError on parse error.
    std::uint64_t forEach(RecordCallback callback);

    /// @brief Read all records into memory.
    /// @return Vector of all records.
    /// @throws FormatError on parse error.
    /// @warning May consume large amounts of memory for big files.
    [[nodiscard]] std::vector<FastqRecord> readAll();

    // =========================================================================
    // Sampling Methods
    // =========================================================================

    /// @brief Sample records for length statistics.
    /// @param maxSamples Maximum number of records to sample.
    /// @return Statistics from sampled records.
    /// @note Resets file position after sampling.
    [[nodiscard]] ParserStats sampleRecords(std::size_t maxSamples = 1000);

    // =========================================================================
    // Statistics and State
    // =========================================================================

    /// @brief Get current parsing statistics.
    [[nodiscard]] const ParserStats& stats() const noexcept { return stats_; }

    /// @brief Get the last parse error (if any).
    [[nodiscard]] const std::optional<ParseError>& lastError() const noexcept { return lastError_; }

    /// @brief Get current line number.
    [[nodiscard]] std::uint64_t lineNumber() const noexcept { return lineNumber_; }

    /// @brief Get current record number.
    [[nodiscard]] std::uint64_t recordNumber() const noexcept { return recordNumber_; }

    /// @brief Get the file path.
    [[nodiscard]] const std::filesystem::path& filePath() const noexcept { return filePath_; }

    /// @brief Check if input is from stdin.
    [[nodiscard]] bool isStdin() const noexcept { return isStdin_; }

    // =========================================================================
    // Seeking (File Input Only)
    // =========================================================================

    /// @brief Reset to the beginning of the file.
    /// @throws IOError if seeking fails or input is stdin.
    void reset();

    /// @brief Check if seeking is supported.
    [[nodiscard]] bool canSeek() const noexcept { return !isStdin_; }

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /// @brief Read a line from the input.
    [[nodiscard]] bool readLine(std::string& line);

    /// @brief Parse a single record from buffered lines.
    [[nodiscard]] bool parseRecord(FastqRecord& record);

    /// @brief Validate a sequence string.
    [[nodiscard]] bool validateSequence(std::string_view seq) const;

    /// @brief Validate a quality string.
    [[nodiscard]] bool validateQuality(std::string_view qual) const;

    /// @brief Set parse error.
    void setError(std::string message, std::string_view lineContent = {});

    /// @brief Trim trailing whitespace from a string.
    static void trimRight(std::string& str);

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// @brief File path (or "-" for stdin).
    std::filesystem::path filePath_;

    /// @brief Parser options.
    ParserOptions options_;

    /// @brief Input stream.
    std::unique_ptr<std::istream> stream_;

    /// @brief Whether parser is open.
    bool isOpen_ = false;

    /// @brief Whether EOF has been reached.
    bool eof_ = false;

    /// @brief Whether input is stdin.
    bool isStdin_ = false;

    /// @brief Current line number (1-based).
    std::uint64_t lineNumber_ = 0;

    /// @brief Current record number (1-based).
    std::uint64_t recordNumber_ = 0;

    /// @brief Parsing statistics.
    ParserStats stats_;

    /// @brief Last parse error.
    std::optional<ParseError> lastError_;

    /// @brief Read buffer.
    std::string buffer_;

    /// @brief Line buffer for parsing.
    std::string lineBuffer_;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Detect read length class from statistics.
/// @param stats Parser statistics.
/// @return Detected read length class.
[[nodiscard]] ReadLengthClass detectReadLengthClass(const ParserStats& stats) noexcept;

/// @brief Check if a character is a valid DNA base.
[[nodiscard]] constexpr bool isValidBase(char c) noexcept {
    return c == 'A' || c == 'C' || c == 'G' || c == 'T' || c == 'N' || c == 'a' || c == 'c' ||
           c == 'g' || c == 't' || c == 'n';
}

/// @brief Check if a character is a valid quality score.
[[nodiscard]] constexpr bool isValidQuality(char c) noexcept {
    return c >= '!' && c <= '~';  // Phred+33: 0-93
}

/// @brief Convert quality character to Phred score.
[[nodiscard]] constexpr std::uint8_t qualityToPhred(char c) noexcept {
    return static_cast<std::uint8_t>(c - '!');
}

/// @brief Convert Phred score to quality character.
[[nodiscard]] constexpr char phredToQuality(std::uint8_t phred) noexcept {
    return static_cast<char>('!' + phred);
}

}  // namespace fqc::io

#endif  // FQC_IO_FASTQ_PARSER_H
