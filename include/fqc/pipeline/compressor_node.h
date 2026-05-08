// =============================================================================
// fq-compressor - Compressor Node
// =============================================================================

#ifndef FQC_PIPELINE_COMPRESSOR_NODE_H
#define FQC_PIPELINE_COMPRESSOR_NODE_H

#include "fqc/algo/block_compressor.h"
#include "fqc/pipeline/node_common.h"

#include <memory>

namespace fqc::pipeline {

class CompressorNodeImpl;

class CompressorNode {
public:
    explicit CompressorNode(algo::BlockCompressorConfig config = {});
    ~CompressorNode();

    CompressorNode(const CompressorNode&) = delete;
    CompressorNode& operator=(const CompressorNode&) = delete;
    CompressorNode(CompressorNode&&) noexcept;
    CompressorNode& operator=(CompressorNode&&) noexcept;

    [[nodiscard]] Result<CompressedBlock> compress(ReadChunk chunk);
    [[nodiscard]] NodeState state() const noexcept;
    [[nodiscard]] std::uint32_t totalBlocksCompressed() const noexcept;
    void reset() noexcept;
    [[nodiscard]] const algo::BlockCompressorConfig& config() const noexcept;

private:
    std::unique_ptr<CompressorNodeImpl> impl_;
};

}  // namespace fqc::pipeline

#endif  // FQC_PIPELINE_COMPRESSOR_NODE_H
