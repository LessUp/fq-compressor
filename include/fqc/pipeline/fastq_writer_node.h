// =============================================================================
// fq-compressor - FASTQ Writer Node
// =============================================================================

#ifndef FQC_PIPELINE_FASTQ_WRITER_NODE_H
#define FQC_PIPELINE_FASTQ_WRITER_NODE_H

#include "fqc/pipeline/node_common.h"

#include <filesystem>
#include <memory>

namespace fqc::pipeline {

class FASTQWriterNodeImpl;

struct FASTQWriterNodeConfig {
    std::size_t bufferSize = kDefaultOutputBufferSize;
    std::size_t lineWidth = 0;

    [[nodiscard]] VoidResult validate() const;
};

class FASTQWriterNode {
public:
    explicit FASTQWriterNode(FASTQWriterNodeConfig config = {});
    ~FASTQWriterNode();

    FASTQWriterNode(const FASTQWriterNode&) = delete;
    FASTQWriterNode& operator=(const FASTQWriterNode&) = delete;
    FASTQWriterNode(FASTQWriterNode&&) noexcept;
    FASTQWriterNode& operator=(FASTQWriterNode&&) noexcept;

    [[nodiscard]] VoidResult open(const std::filesystem::path& path);
    [[nodiscard]] VoidResult openPaired(const std::filesystem::path& path1,
                                        const std::filesystem::path& path2);
    [[nodiscard]] VoidResult writeChunk(ReadChunk chunk);
    [[nodiscard]] VoidResult flush();
    [[nodiscard]] NodeState state() const noexcept;
    [[nodiscard]] std::uint64_t totalReadsWritten() const noexcept;
    [[nodiscard]] std::uint64_t totalBytesWritten() const noexcept;
    void close() noexcept;
    void reset() noexcept;
    [[nodiscard]] const FASTQWriterNodeConfig& config() const noexcept;

private:
    std::unique_ptr<FASTQWriterNodeImpl> impl_;
};

}  // namespace fqc::pipeline

#endif  // FQC_PIPELINE_FASTQ_WRITER_NODE_H
