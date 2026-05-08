// =============================================================================
// fq-compressor - FQC Reader Node
// =============================================================================

#ifndef FQC_PIPELINE_FQC_READER_NODE_H
#define FQC_PIPELINE_FQC_READER_NODE_H

#include "fqc/format/fqc_format.h"
#include "fqc/pipeline/node_common.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <span>

namespace fqc::pipeline {

class FQCReaderNodeImpl;

struct FQCReaderNodeConfig {
    ReadId rangeStart = 0;
    ReadId rangeEnd = 0;
    bool verifyChecksums = true;

    [[nodiscard]] VoidResult validate() const;
};

class FQCReaderNode {
public:
    explicit FQCReaderNode(FQCReaderNodeConfig config = {});
    ~FQCReaderNode();

    FQCReaderNode(const FQCReaderNode&) = delete;
    FQCReaderNode& operator=(const FQCReaderNode&) = delete;
    FQCReaderNode(FQCReaderNode&&) noexcept;
    FQCReaderNode& operator=(FQCReaderNode&&) noexcept;

    [[nodiscard]] VoidResult open(const std::filesystem::path& path);
    [[nodiscard]] Result<std::optional<CompressedBlock>> readBlock();
    [[nodiscard]] bool hasMore() const noexcept;
    [[nodiscard]] const format::GlobalHeader& globalHeader() const noexcept;
    [[nodiscard]] std::optional<std::span<const std::uint8_t>> reorderMap() const noexcept;
    [[nodiscard]] NodeState state() const noexcept;
    [[nodiscard]] std::uint32_t totalBlocksRead() const noexcept;
    void close() noexcept;
    void reset() noexcept;
    [[nodiscard]] const FQCReaderNodeConfig& config() const noexcept;

private:
    std::unique_ptr<FQCReaderNodeImpl> impl_;
};

}  // namespace fqc::pipeline

#endif  // FQC_PIPELINE_FQC_READER_NODE_H
