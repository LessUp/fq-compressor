// =============================================================================
// fq-compressor - FASTQ Parser Implementation
// =============================================================================

#include "fqc/io/fastq_parser.h"

#include <istream>
#include <string>

namespace fqc::io {

FastqParser::FastqParser(std::istream& stream) : stream_(stream) {}

auto FastqParser::readRecord() -> Result<std::optional<FastqRecord>> {
    if (eof_) {
        return std::optional<FastqRecord>{};
    }

    std::string idLine;
    if (!readLine(idLine)) {
        return std::optional<FastqRecord>{};
    }

    while (idLine.empty()) {
        if (!readLine(idLine)) {
            return std::optional<FastqRecord>{};
        }
    }

    if (idLine[0] != '@') {
        return makeError<std::optional<FastqRecord>>(
            ErrorCode::kFormatError,
            "invalid FASTQ at line " + std::to_string(lineNumber_) + ": expected '@'");
    }

    FastqRecord record;
    std::string_view idView(idLine);
    idView.remove_prefix(1);

    auto spacePos = idView.find(' ');
    if (spacePos != std::string_view::npos) {
        record.id = std::string(idView.substr(0, spacePos));
        record.comment = std::string(idView.substr(spacePos + 1));
    } else {
        record.id = std::string(idView);
    }

    if (record.id.empty()) {
        return makeError<std::optional<FastqRecord>>(
            ErrorCode::kFormatError,
            "invalid FASTQ at line " + std::to_string(lineNumber_) + ": empty read ID");
    }

    if (!readLine(record.sequence)) {
        return makeError<std::optional<FastqRecord>>(ErrorCode::kFormatError,
                                                     "invalid FASTQ: unexpected EOF at line " +
                                                         std::to_string(lineNumber_));
    }

    if (record.sequence.empty()) {
        return makeError<std::optional<FastqRecord>>(
            ErrorCode::kFormatError,
            "invalid FASTQ at line " + std::to_string(lineNumber_) + ": empty sequence");
    }

    std::string plusLine;
    if (!readLine(plusLine)) {
        return makeError<std::optional<FastqRecord>>(ErrorCode::kFormatError,
                                                     "invalid FASTQ: unexpected EOF at line " +
                                                         std::to_string(lineNumber_));
    }

    if (plusLine.empty() || plusLine[0] != '+') {
        return makeError<std::optional<FastqRecord>>(
            ErrorCode::kFormatError,
            "invalid FASTQ at line " + std::to_string(lineNumber_) + ": expected '+'");
    }

    if (!readLine(record.quality)) {
        return makeError<std::optional<FastqRecord>>(ErrorCode::kFormatError,
                                                     "invalid FASTQ: unexpected EOF at line " +
                                                         std::to_string(lineNumber_));
    }

    if (record.quality.size() != record.sequence.size()) {
        return makeError<std::optional<FastqRecord>>(
            ErrorCode::kFormatError,
            "invalid FASTQ at line " + std::to_string(lineNumber_) +
                ": quality length does not match sequence length");
    }

    ++recordNumber_;
    return std::optional<FastqRecord>(std::move(record));
}

auto FastqParser::readLine(std::string& line) -> bool {
    if (eof_ || !std::getline(stream_, line)) {
        eof_ = true;
        return false;
    }
    ++lineNumber_;
    trimRight(line);
    return true;
}

void FastqParser::trimRight(std::string& str) {
    while (!str.empty() && str.back() == '\r') {
        str.pop_back();
    }
}

}  // namespace fqc::io
