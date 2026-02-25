// =============================================================================
// fq-compressor - DecompressorNode Implementation
// =============================================================================
// Implements the DecompressorNode (parallel processing stage) for decompression.
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/pipeline/pipeline_node.h"

#include <fmt/format.h>

#include "fqc/common/logger.h"
#include "fqc/algo/block_compressor.h"

namespace fqc::pipeline {

// =============================================================================
// DecompressorNodeImpl
// =============================================================================

class DecompressorNodeImpl {
public:
    explicit DecompressorNodeImpl(DecompressorNodeConfig config)
        : config_(std::move(config)) {}

    Result<ReadChunk> decompress(
        CompressedBlock block,
        const format::GlobalHeader& globalHeader) {
        state_ = NodeState::kRunning;

        ReadChunk chunk;
        chunk.chunkId = block.blockId;
        chunk.startReadId = block.startReadId;
        chunk.isLast = block.isLast;

        try {
            // Lazily initialize cached compressor on first call
            if (!cachedCompressor_) {
                algo::BlockCompressorConfig compressorConfig;
                compressorConfig.readLengthClass = format::getReadLengthClass(globalHeader.flags);
                compressorConfig.qualityMode = format::getQualityMode(globalHeader.flags);
                compressorConfig.idMode = format::getIdMode(globalHeader.flags);
                compressorConfig.numThreads = 1;  // Single-threaded per block
                cachedCompressor_ = std::make_unique<algo::BlockCompressor>(compressorConfig);
            }

            // Create block header from compressed block metadata
            format::BlockHeader blockHeader;
            blockHeader.blockId = block.blockId;
            blockHeader.uncompressedCount = block.readCount;
            blockHeader.uniformReadLength = block.uniformReadLength;
            blockHeader.codecIds = block.codecIds;
            blockHeader.codecSeq = block.codecSeq;
            blockHeader.codecQual = block.codecQual;
            blockHeader.codecAux = block.codecAux;

            // Reuse cached compressor for decompression
            auto decompressResult = cachedCompressor_->decompress(
                blockHeader,
                block.idStream,
                block.seqStream,
                block.qualStream,
                block.auxStream
            );

            if (!decompressResult) {
                throw std::runtime_error(
                    fmt::format("Failed to decompress block {}: {}",
                               block.blockId, decompressResult.error().message()));
            }

            // Move decompressed reads into chunk
            chunk.reads = std::move(decompressResult->reads);

            ++totalBlocksDecompressed_;
            state_ = NodeState::kIdle;
            return chunk;

        } catch (const FQCException& e) {
            if (config_.skipCorrupted) {
                FQC_LOG_WARNING("Skipping corrupted block {}: {}", block.blockId, e.what());
                state_ = NodeState::kIdle;
                return chunk;  // Return empty chunk
            }
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            if (config_.skipCorrupted) {
                FQC_LOG_WARNING("Skipping corrupted block {}: {}", block.blockId, e.what());
                state_ = NodeState::kIdle;
                return chunk;
            }
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kDecompressionError, 
                             fmt::format("Failed to decompress block: {}", e.what())});
        }
    }

    NodeState state() const noexcept { return state_; }
    std::uint32_t totalBlocksDecompressed() const noexcept { return totalBlocksDecompressed_; }

    void reset() noexcept {
        state_ = NodeState::kIdle;
        totalBlocksDecompressed_ = 0;
        cachedCompressor_.reset();
    }

    const DecompressorNodeConfig& config() const noexcept { return config_; }

private:
    DecompressorNodeConfig config_;
    NodeState state_ = NodeState::kIdle;
    std::atomic<std::uint32_t> totalBlocksDecompressed_{0};
    std::unique_ptr<algo::BlockCompressor> cachedCompressor_;
};

// =============================================================================
// DecompressorNode Public Interface
// =============================================================================

DecompressorNode::DecompressorNode(DecompressorNodeConfig config)
    : impl_(std::make_unique<DecompressorNodeImpl>(std::move(config))) {}

DecompressorNode::~DecompressorNode() = default;

DecompressorNode::DecompressorNode(DecompressorNode&&) noexcept = default;
DecompressorNode& DecompressorNode::operator=(DecompressorNode&&) noexcept = default;

Result<ReadChunk> DecompressorNode::decompress(
    CompressedBlock block,
    const format::GlobalHeader& globalHeader) {
    return impl_->decompress(std::move(block), globalHeader);
}

NodeState DecompressorNode::state() const noexcept {
    return impl_->state();
}

std::uint32_t DecompressorNode::totalBlocksDecompressed() const noexcept {
    return impl_->totalBlocksDecompressed();
}

void DecompressorNode::reset() noexcept {
    impl_->reset();
}

const DecompressorNodeConfig& DecompressorNode::config() const noexcept {
    return impl_->config();
}

}  // namespace fqc::pipeline
