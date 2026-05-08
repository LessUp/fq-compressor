// =============================================================================
// fq-compressor - Writer Node
// =============================================================================

#ifndef FQC_PIPELINE_WRITER_NODE_H
#define FQC_PIPELINE_WRITER_NODE_H

#include "fqc/format/fqc_format.h"
#include "fqc/pipeline/node_common.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace fqc::pipeline {

class WriterNodeImpl;

struct WriterNodeConfig {
    std::size_t bufferSize = kDefaultOutputBufferSize;
    bool atomicWrite = true;

    [[nodiscard]] VoidResult validate() const;
};

class WriterNode {
public:
    explicit WriterNode(WriterNodeConfig config = {});
    ~WriterNode();

    WriterNode(const WriterNode&) = delete;
    WriterNode& operator=(const WriterNode&) = delete;
    WriterNode(WriterNode&&) noexcept;
    WriterNode& operator=(WriterNode&&) noexcept;

    [[nodiscard]] VoidResult open(const std::filesystem::path& path,
                                  const format::GlobalHeader& globalHeader,
                                  std::string_view originalFilename = {});
    [[nodiscard]] VoidResult writeBlock(CompressedBlock block);
    [[nodiscard]] VoidResult finalize(
        std::optional<std::span<const std::uint8_t>> reorderMap = std::nullopt);
    [[nodiscard]] NodeState state() const noexcept;
    [[nodiscard]] std::uint32_t totalBlocksWritten() const noexcept;
    [[nodiscard]] std::uint64_t totalBytesWritten() const noexcept;
    [[nodiscard]] VoidResult updateTotalReadCount(std::uint64_t totalReadCount);
    void close() noexcept;
    void reset() noexcept;
    [[nodiscard]] const WriterNodeConfig& config() const noexcept;

private:
    std::unique_ptr<WriterNodeImpl> impl_;
};

}  // namespace fqc::pipeline

#endif  // FQC_PIPELINE_WRITER_NODE_H
