// =============================================================================
// fq-compressor - DecompressorNode Implementation
// =============================================================================
// Implements the DecompressorNode (parallel processing stage) for decompression.
//
// Features:
// - Block decompression with optional checksum verification
// - Corrupted block handling (skip or error)
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/pipeline/decompressor_node.h"

#include "fqc/algo/block_compressor.h"
#include "fqc/common/logger.h"

#include <xxhash.h>

#include <fmt/format.h>

namespace fqc::pipeline {

// =============================================================================
// DecompressorNodeImpl
// =============================================================================

class DecompressorNodeImpl {
public:
    explicit DecompressorNodeImpl(DecompressorNodeConfig config) : config_(std::move(config)) {}

    Result<ReadChunk> decompress(CompressedBlock block, const format::GlobalHeader& globalHeader) {
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
                blockHeader, block.idStream, block.seqStream, block.qualStream, block.auxStream);

            if (!decompressResult) {
                throw FQCException(ErrorCode::kDecompressionFailed,
                                   fmt::format("Failed to decompress block {}: {}",
                                               block.blockId,
                                               decompressResult.error().message()));
            }

            // Move decompressed reads into chunk
            chunk.reads = std::move(decompressResult->reads);

            // 验证块校验和（如果启用）
            if (config_.verifyChecksums) {
                std::uint64_t calculatedChecksum = calculateDecompressedChecksum(chunk.reads);
                if (calculatedChecksum != block.checksum) {
                    if (config_.skipCorrupted) {
                        FQC_LOG_WARNING("Block {} checksum mismatch: expected {:016x}, got {:016x}",
                                        block.blockId,
                                        block.checksum,
                                        calculatedChecksum);
                        chunk.reads.clear();  // 返回空 chunk
                        state_ = NodeState::kIdle;
                        return chunk;
                    } else {
                        throw FQCException(
                            ErrorCode::kChecksumMismatch,
                            fmt::format("Block {} checksum mismatch: expected {:016x}, got {:016x}",
                                        block.blockId,
                                        block.checksum,
                                        calculatedChecksum));
                    }
                }
            }

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

    NodeState state() const noexcept {
        return state_;
    }
    std::uint32_t totalBlocksDecompressed() const noexcept {
        return totalBlocksDecompressed_;
    }

    void reset() noexcept {
        state_ = NodeState::kIdle;
        totalBlocksDecompressed_ = 0;
        cachedCompressor_.reset();
    }

    const DecompressorNodeConfig& config() const noexcept {
        return config_;
    }

private:
    /// @brief 计算解压后数据的校验和
    ///
    /// 计算顺序：ID || Comments || Seq || Qual || Aux
    /// 与 BlockCompressorImpl::computeBlockChecksum 保持一致
    [[nodiscard]] static std::uint64_t calculateDecompressedChecksum(
        const std::vector<ReadRecord>& reads) {
        XXH64_state_t* state = XXH64_createState();
        if (!state) {
            throw IOError("Failed to create xxHash64 state for block checksum");
        }

        // 使用 RAII guard 确保状态被释放
        struct XxHashStateGuard {
            XXH64_state_t* state;
            ~XxHashStateGuard() {
                XXH64_freeState(state);
            }
        } guard{state};

        XXH64_reset(state, 0);

        // Hash IDs
        for (const auto& read : reads) {
            XXH64_update(state, read.id.data(), read.id.size());
        }

        // Hash comments
        for (const auto& read : reads) {
            if (!read.comment.empty()) {
                XXH64_update(state, read.comment.data(), read.comment.size());
            }
        }

        // Hash sequences
        for (const auto& read : reads) {
            XXH64_update(state, read.sequence.data(), read.sequence.size());
        }

        // Hash quality
        for (const auto& read : reads) {
            XXH64_update(state, read.quality.data(), read.quality.size());
        }

        // Hash lengths (aux) - 逐个添加，与 BlockCompressorImpl 一致
        for (const auto& read : reads) {
            std::uint32_t len = static_cast<std::uint32_t>(read.sequence.length());
            XXH64_update(state, &len, sizeof(len));
        }

        return XXH64_digest(state);
    }

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

Result<ReadChunk> DecompressorNode::decompress(CompressedBlock block,
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
