// =============================================================================
// fq-compressor - Decompressor Node
// =============================================================================

#ifndef FQC_PIPELINE_DECOMPRESSOR_NODE_H
#define FQC_PIPELINE_DECOMPRESSOR_NODE_H

#include "fqc/format/fqc_format.h"
#include "fqc/pipeline/node_common.h"

#include <memory>
#include <string>

namespace fqc::pipeline {

class DecompressorNodeImpl;

struct DecompressorNodeConfig {
    bool skipCorrupted = false;
    char placeholderQual = kDefaultPlaceholderQual;
    std::string idPrefix;

    [[nodiscard]] VoidResult validate() const;
};

class DecompressorNode {
public:
    explicit DecompressorNode(DecompressorNodeConfig config = {});
    ~DecompressorNode();

    DecompressorNode(const DecompressorNode&) = delete;
    DecompressorNode& operator=(const DecompressorNode&) = delete;
    DecompressorNode(DecompressorNode&&) noexcept;
    DecompressorNode& operator=(DecompressorNode&&) noexcept;

    [[nodiscard]] Result<ReadChunk> decompress(CompressedBlock block,
                                               const format::GlobalHeader& globalHeader);
    [[nodiscard]] NodeState state() const noexcept;
    [[nodiscard]] std::uint32_t totalBlocksDecompressed() const noexcept;
    void reset() noexcept;
    [[nodiscard]] const DecompressorNodeConfig& config() const noexcept;

private:
    std::unique_ptr<DecompressorNodeImpl> impl_;
};

}  // namespace fqc::pipeline

#endif  // FQC_PIPELINE_DECOMPRESSOR_NODE_H
