// =============================================================================
// fq-compressor - Reader Node
// =============================================================================

#ifndef FQC_PIPELINE_READER_NODE_H
#define FQC_PIPELINE_READER_NODE_H

#include "fqc/pipeline/node_common.h"

#include <filesystem>
#include <memory>
#include <optional>

namespace fqc::pipeline {

class ReaderNodeImpl;

struct ReaderNodeConfig {
    std::size_t blockSize = kDefaultBlockSizeShort;
    std::size_t bufferSize = kDefaultInputBufferSize;
    ReadLengthClass readLengthClass = ReadLengthClass::kShort;
    std::size_t maxBlockBases = kDefaultMaxBlockBasesLong;

    [[nodiscard]] VoidResult validate() const;
};

class ReaderNode {
public:
    explicit ReaderNode(ReaderNodeConfig config = {});
    ~ReaderNode();

    ReaderNode(const ReaderNode&) = delete;
    ReaderNode& operator=(const ReaderNode&) = delete;
    ReaderNode(ReaderNode&&) noexcept;
    ReaderNode& operator=(ReaderNode&&) noexcept;

    [[nodiscard]] VoidResult open(const std::filesystem::path& path);
    [[nodiscard]] VoidResult openPaired(const std::filesystem::path& path1,
                                        const std::filesystem::path& path2);
    [[nodiscard]] Result<std::optional<ReadChunk>> readChunk();
    [[nodiscard]] bool hasMore() const noexcept;
    [[nodiscard]] NodeState state() const noexcept;
    [[nodiscard]] std::uint64_t totalReadsRead() const noexcept;
    [[nodiscard]] std::uint64_t totalBytesRead() const noexcept;
    [[nodiscard]] std::uint64_t estimatedTotalReads() const noexcept;
    void close() noexcept;
    void reset() noexcept;
    [[nodiscard]] const ReaderNodeConfig& config() const noexcept;

private:
    std::unique_ptr<ReaderNodeImpl> impl_;
};

}  // namespace fqc::pipeline

#endif  // FQC_PIPELINE_READER_NODE_H
