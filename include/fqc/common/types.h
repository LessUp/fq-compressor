// =============================================================================
// fq-compressor - Shared FASTQ Record Type
// =============================================================================

#ifndef FQC_COMMON_TYPES_H
#define FQC_COMMON_TYPES_H

#include <cstddef>
#include <string>
#include <utility>

namespace fqc {

/// @brief Owning representation of the four logical fields preserved by FQC v2.
struct ReadRecord {
    /// @brief Identifier without the leading '@'.
    std::string id;

    /// @brief Optional text following the first space on the header line.
    std::string comment;

    /// @brief IUPAC nucleotide sequence; case is significant and preserved.
    std::string sequence;

    /// @brief Phred+33 quality string; length must equal the sequence length.
    std::string quality;

    ReadRecord() = default;

    ReadRecord(std::string idValue, std::string sequenceValue, std::string qualityValue)
        : id(std::move(idValue)),
          sequence(std::move(sequenceValue)),
          quality(std::move(qualityValue)) {}

    ReadRecord(std::string idValue,
               std::string commentValue,
               std::string sequenceValue,
               std::string qualityValue)
        : id(std::move(idValue)),
          comment(std::move(commentValue)),
          sequence(std::move(sequenceValue)),
          quality(std::move(qualityValue)) {}

    [[nodiscard]] auto isValid() const noexcept -> bool {
        return !id.empty() && !sequence.empty() && sequence.size() == quality.size();
    }

    [[nodiscard]] auto length() const noexcept -> std::size_t {
        return sequence.size();
    }

    void clear() noexcept {
        id.clear();
        comment.clear();
        sequence.clear();
        quality.clear();
    }

    [[nodiscard]] auto operator==(const ReadRecord& other) const noexcept -> bool = default;
};

}  // namespace fqc

#endif  // FQC_COMMON_TYPES_H
