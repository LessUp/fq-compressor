// =============================================================================
// fq-compressor - FASTQ Parser Implementation
// =============================================================================
// High-performance streaming FASTQ parser implementation.
//
// Requirements: 1.1.1
// =============================================================================

#include "fqc/io/fastq_parser.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

#include "fqc/common/logger.h"

namespace fqc::io {

// =============================================================================
// FastqParser Implementation
// =============================================================================

FastqParser::FastqParser(std::filesystem::path filePath, ParserOptions options)
    : filePath_(std::move(filePath)), options_(std::move(options)), isStdin_(filePath_ == "-") {}

FastqParser::FastqParser(std::unique_ptr<std::istream> stream, ParserOptions options)
    : filePath_("<stream>"),
      options_(std::move(options)),
      stream_(std::move(stream)),
      isStdin_(false) {
    if (stream_) {
        isOpen_ = true;
    }
}

FastqParser::~FastqParser() { close(); }

FastqParser::FastqParser(FastqParser&&) noexcept = default;
FastqParser& FastqParser::operator=(FastqParser&&) noexcept = default;

void FastqParser::open() {
    if (isOpen_) {
        return;
    }

    if (isStdin_) {
        // Use cin for stdin
        stream_ = std::make_unique<std::istream>(std::cin.rdbuf());
        LOG_DEBUG("Opening stdin for FASTQ parsing");
    } else {
        auto fileStream = std::make_unique<std::ifstream>(filePath_, std::ios::binary);
        if (!fileStream->is_open()) {
            throw IOError(ErrorCode::kFileOpenFailed,
                          "Failed to open FASTQ file: " + filePath_.string());
        }
        stream_ = std::move(fileStream);
        LOG_DEBUG("Opened FASTQ file: {}", filePath_.string());
    }

    isOpen_ = true;
    eof_ = false;
    lineNumber_ = 0;
    recordNumber_ = 0;
    stats_.reset();
    lastError_.reset();

    // Reserve buffer space
    buffer_.reserve(options_.bufferSize);
    lineBuffer_.reserve(1024);
}

void FastqParser::close() noexcept {
    if (!isOpen_) {
        return;
    }

    stream_.reset();
    isOpen_ = false;
    eof_ = true;
}

std::optional<FastqRecord> FastqParser::readRecord() {
    if (!isOpen_ || eof_) {
        return std::nullopt;
    }

    FastqRecord record;
    if (!parseRecord(record)) {
        return std::nullopt;
    }

    ++recordNumber_;
    if (options_.collectStats) {
        stats_.update(record);
    }

    return record;
}

std::optional<FastqParser::Chunk> FastqParser::readChunk(std::size_t maxRecords) {
    if (!isOpen_ || eof_) {
        return std::nullopt;
    }

    Chunk chunk;
    chunk.reserve(maxRecords);

    for (std::size_t i = 0; i < maxRecords && !eof_; ++i) {
        FastqRecord record;
        if (!parseRecord(record)) {
            break;
        }

        ++recordNumber_;
        if (options_.collectStats) {
            stats_.update(record);
        }

        chunk.push_back(std::move(record));
    }

    if (chunk.empty()) {
        return std::nullopt;
    }

    return chunk;
}

std::uint64_t FastqParser::forEach(RecordCallback callback) {
    if (!isOpen_) {
        return 0;
    }

    std::uint64_t count = 0;
    while (!eof_) {
        FastqRecord record;
        if (!parseRecord(record)) {
            break;
        }

        ++recordNumber_;
        if (options_.collectStats) {
            stats_.update(record);
        }

        ++count;
        if (!callback(record)) {
            break;
        }
    }

    return count;
}

std::vector<FastqRecord> FastqParser::readAll() {
    std::vector<FastqRecord> records;

    while (!eof_) {
        FastqRecord record;
        if (!parseRecord(record)) {
            break;
        }

        ++recordNumber_;
        if (options_.collectStats) {
            stats_.update(record);
        }

        records.push_back(std::move(record));
    }

    return records;
}

ParserStats FastqParser::sampleRecords(std::size_t maxSamples) {
    if (!isOpen_) {
        return {};
    }

    // Save current state
    auto savedLineNumber = lineNumber_;
    auto savedRecordNumber = recordNumber_;
    auto savedStats = stats_;

    // Reset for sampling
    if (canSeek()) {
        reset();
    }

    ParserStats sampleStats;
    std::size_t sampled = 0;

    while (sampled < maxSamples && !eof_) {
        FastqRecord record;
        if (!parseRecord(record)) {
            break;
        }

        sampleStats.update(record);
        ++sampled;
    }

    // Restore state if possible
    if (canSeek()) {
        reset();
        lineNumber_ = savedLineNumber;
        recordNumber_ = savedRecordNumber;
        stats_ = savedStats;
    }

    return sampleStats;
}

void FastqParser::reset() {
    if (!canSeek()) {
        throw IOError(ErrorCode::kSeekFailed, "Cannot seek on stdin input");
    }

    if (stream_) {
        stream_->clear();
        stream_->seekg(0, std::ios::beg);
    }

    eof_ = false;
    lineNumber_ = 0;
    recordNumber_ = 0;
    stats_.reset();
    lastError_.reset();
}

bool FastqParser::readLine(std::string& line) {
    if (!stream_ || !*stream_) {
        eof_ = true;
        return false;
    }

    if (!std::getline(*stream_, line)) {
        eof_ = true;
        return false;
    }

    ++lineNumber_;

    // Trim trailing whitespace (CR/LF)
    if (options_.trimWhitespace) {
        trimRight(line);
    }

    return true;
}

bool FastqParser::parseRecord(FastqRecord& record) {
    record.clear();

    // Line 1: ID line (starts with '@')
    std::string idLine;
    if (!readLine(idLine)) {
        return false;
    }

    // Skip empty lines at the beginning
    while (idLine.empty() && !eof_) {
        if (!readLine(idLine)) {
            return false;
        }
    }

    if (idLine.empty()) {
        return false;
    }

    if (idLine[0] != '@') {
        setError("Expected '@' at start of ID line", idLine);
        throw FormatError(ErrorCode::kInvalidFormat,
                          "Invalid FASTQ format at line " + std::to_string(lineNumber_) +
                              ": expected '@' at start of ID line");
    }

    // Parse ID and optional comment
    std::string_view idView(idLine);
    idView.remove_prefix(1);  // Remove '@'

    auto spacePos = idView.find(' ');
    if (spacePos != std::string_view::npos) {
        record.id = std::string(idView.substr(0, spacePos));
        record.comment = std::string(idView.substr(spacePos + 1));
    } else {
        record.id = std::string(idView);
    }

    if (record.id.empty()) {
        setError("Empty read ID", idLine);
        throw FormatError(ErrorCode::kInvalidFormat,
                          "Invalid FASTQ format at line " + std::to_string(lineNumber_) +
                              ": empty read ID");
    }

    // Line 2: Sequence
    if (!readLine(record.sequence)) {
        setError("Unexpected EOF: missing sequence line");
        throw FormatError(ErrorCode::kInvalidFormat,
                          "Invalid FASTQ format: unexpected EOF at line " +
                              std::to_string(lineNumber_));
    }

    if (record.sequence.empty()) {
        setError("Empty sequence", record.sequence);
        throw FormatError(ErrorCode::kInvalidFormat,
                          "Invalid FASTQ format at line " + std::to_string(lineNumber_) +
                              ": empty sequence");
    }

    // Validate sequence if enabled
    if (options_.validateSequence && !validateSequence(record.sequence)) {
        throw FormatError(ErrorCode::kInvalidFormat,
                          "Invalid FASTQ format at line " + std::to_string(lineNumber_) +
                              ": invalid sequence characters");
    }

    // Line 3: Plus line (starts with '+')
    std::string plusLine;
    if (!readLine(plusLine)) {
        setError("Unexpected EOF: missing '+' line");
        throw FormatError(ErrorCode::kInvalidFormat,
                          "Invalid FASTQ format: unexpected EOF at line " +
                              std::to_string(lineNumber_));
    }

    if (plusLine.empty() || plusLine[0] != '+') {
        setError("Expected '+' at start of separator line", plusLine);
        throw FormatError(ErrorCode::kInvalidFormat,
                          "Invalid FASTQ format at line " + std::to_string(lineNumber_) +
                              ": expected '+' at start of separator line");
    }

    // Line 4: Quality scores
    if (!readLine(record.quality)) {
        setError("Unexpected EOF: missing quality line");
        throw FormatError(ErrorCode::kInvalidFormat,
                          "Invalid FASTQ format: unexpected EOF at line " +
                              std::to_string(lineNumber_));
    }

    // Validate quality length matches sequence length
    if (record.quality.size() != record.sequence.size()) {
        setError("Quality length (" + std::to_string(record.quality.size()) +
                     ") does not match sequence length (" + std::to_string(record.sequence.size()) +
                     ")",
                 record.quality);
        throw FormatError(ErrorCode::kInvalidFormat,
                          "Invalid FASTQ format at line " + std::to_string(lineNumber_) +
                              ": quality length does not match sequence length");
    }

    // Validate quality scores if enabled
    if (options_.validateQuality && !validateQuality(record.quality)) {
        throw FormatError(ErrorCode::kInvalidFormat,
                          "Invalid FASTQ format at line " + std::to_string(lineNumber_) +
                              ": invalid quality characters");
    }

    return true;
}

bool FastqParser::validateSequence(std::string_view seq) const {
    for (char c : seq) {
        if (!isValidBase(c)) {
            return false;
        }
    }
    return true;
}

bool FastqParser::validateQuality(std::string_view qual) const {
    for (char c : qual) {
        if (c < options_.minQualityChar || c > options_.maxQualityChar) {
            return false;
        }
    }
    return true;
}

void FastqParser::setError(std::string message, std::string_view lineContent) {
    lastError_ = ParseError{
        .lineNumber = lineNumber_,
        .recordNumber = recordNumber_,
        .message = std::move(message),
        .lineContent = std::string(lineContent.substr(0, 100))  // Truncate long lines
    };
}

void FastqParser::trimRight(std::string& str) {
    auto it = std::find_if(str.rbegin(), str.rend(),
                           [](unsigned char c) { return !std::isspace(c); });
    str.erase(it.base(), str.end());
}

// =============================================================================
// Utility Functions
// =============================================================================

ReadLengthClass detectReadLengthClass(const ParserStats& stats) noexcept {
    // Priority-based classification (from design doc):
    // 1. max >= 100KB → LONG (Ultra-long)
    // 2. max >= 10KB → LONG
    // 3. max > 511 → MEDIUM (Spring compatibility protection)
    // 4. median >= 1KB → MEDIUM
    // 5. Otherwise → SHORT

    constexpr std::uint32_t kUltraLongThreshold = 100 * 1024;  // 100KB
    constexpr std::uint32_t kLongThreshold = 10 * 1024;        // 10KB
    constexpr std::uint32_t kSpringMaxLen = 511;               // Spring ABC limit
    constexpr std::uint32_t kMediumMedianThreshold = 1024;     // 1KB

    if (stats.maxLength >= kUltraLongThreshold) {
        return ReadLengthClass::kLong;  // Ultra-long (still LONG enum)
    }

    if (stats.maxLength >= kLongThreshold) {
        return ReadLengthClass::kLong;
    }

    if (stats.maxLength > kSpringMaxLen) {
        return ReadLengthClass::kMedium;  // Spring compatibility protection
    }

    // Calculate approximate median (using average as proxy)
    auto avgLength = static_cast<std::uint32_t>(stats.averageLength());
    if (avgLength >= kMediumMedianThreshold) {
        return ReadLengthClass::kMedium;
    }

    return ReadLengthClass::kShort;
}

}  // namespace fqc::io
