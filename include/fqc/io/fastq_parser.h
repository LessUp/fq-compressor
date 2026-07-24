// =============================================================================
// fq-compressor - FASTQ Parser
// =============================================================================

#ifndef FQC_IO_FASTQ_PARSER_H
#define FQC_IO_FASTQ_PARSER_H

#include "fqc/common/error.h"
#include "fqc/common/types.h"

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace fqc::io {

using FastqRecord = ReadRecord;

class FastqParser {
public:
    explicit FastqParser(std::istream& stream);

    [[nodiscard]] auto readRecord() -> Result<std::optional<FastqRecord>>;

    [[nodiscard]] auto lineNumber() const noexcept -> std::uint64_t {
        return lineNumber_;
    }

    [[nodiscard]] auto recordNumber() const noexcept -> std::uint64_t {
        return recordNumber_;
    }

private:
    [[nodiscard]] auto readLine(std::string& line) -> bool;
    static void trimRight(std::string& str);

    std::istream& stream_;
    std::uint64_t lineNumber_ = 0;
    std::uint64_t recordNumber_ = 0;
    bool eof_ = false;
};

}  // namespace fqc::io

#endif  // FQC_IO_FASTQ_PARSER_H
