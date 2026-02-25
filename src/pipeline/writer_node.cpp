// =============================================================================
// fq-compressor - WriterNode Implementation
// =============================================================================
// Implements the WriterNode (serial output stage) for compression pipeline.
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/pipeline/pipeline_node.h"

#include <chrono>
#include <cstring>
#include <map>

#include <fmt/format.h>

#include "fqc/common/logger.h"
#include "fqc/format/fqc_writer.h"
#include "fqc/format/reorder_map.h"

namespace fqc::pipeline {

// =============================================================================
// WriterNodeImpl
// =============================================================================

class WriterNodeImpl {
public:
    explicit WriterNodeImpl(WriterNodeConfig config)
        : config_(std::move(config)) {}

    VoidResult open(
        const std::filesystem::path& path,
        const format::GlobalHeader& globalHeader) {
        try {
            outputPath_ = path;
            globalHeader_ = globalHeader;
            
            writer_ = std::make_unique<format::FQCWriter>(path);
            
            std::string filename = path.filename().string();
            auto timestamp = static_cast<std::uint64_t>(
                std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            
            writer_->writeGlobalHeader(globalHeader, filename, timestamp);
            
            state_ = NodeState::kRunning;
            totalBlocksWritten_ = 0;
            totalBytesWritten_ = 0;
            nextExpectedBlockId_ = 0;
            
            FQC_LOG_DEBUG("WriterNode opened: path={}", path.string());
            
            return {};
        } catch (const FQCException& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to open output: {}", e.what())});
        }
    }

    VoidResult writeBlock(CompressedBlock block) {
        if (state_ != NodeState::kRunning) {
            return makeVoidError(ErrorCode::kInvalidState, "Writer not open");
        }

        try {
            if (block.blockId != nextExpectedBlockId_) {
                pendingBlocks_[block.blockId] = std::move(block);
                return {};
            }
            
            auto result = writeBlockInternal(block);
            if (!result) {
                return result;
            }
            
            while (true) {
                auto it = pendingBlocks_.find(nextExpectedBlockId_);
                if (it == pendingBlocks_.end()) {
                    break;
                }
                
                result = writeBlockInternal(it->second);
                if (!result) {
                    return result;
                }
                
                pendingBlocks_.erase(it);
            }
            
            return {};
            
        } catch (const FQCException& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to write block: {}", e.what())});
        }
    }

    VoidResult finalize(std::optional<std::span<const std::uint8_t>> reorderMap) {
        if (state_ != NodeState::kRunning) {
            return makeVoidError(ErrorCode::kInvalidState, "Writer not open");
        }

        try {
            if (!pendingBlocks_.empty()) {
                FQC_LOG_WARNING("WriterNode finalize with {} pending blocks", pendingBlocks_.size());
                while (!pendingBlocks_.empty()) {
                    auto it = pendingBlocks_.begin();
                    auto result = writeBlockInternal(it->second);
                    if (!result) {
                        return result;
                    }
                    pendingBlocks_.erase(it);
                }
            }
            
            if (reorderMap.has_value() && !reorderMap->empty()) {
                const auto& mapData = reorderMap.value();

                fqc::format::ReorderMap mapHeader;
                mapHeader.totalReads = static_cast<std::uint32_t>(mapData.size());
                mapHeader.forwardMapSize = 0;
                mapHeader.reverseMapSize = 0;

                std::vector<fqc::ReadId> ids;
                ids.reserve(mapData.size() / sizeof(fqc::ReadId));
                for (std::size_t i = 0; i + sizeof(fqc::ReadId) <= mapData.size(); i += sizeof(fqc::ReadId)) {
                    fqc::ReadId id;
                    std::memcpy(&id, mapData.data() + i, sizeof(fqc::ReadId));
                    ids.push_back(id);
                }

                auto compressedForward = fqc::format::deltaEncode(std::span<const fqc::ReadId>(ids.data(), ids.size()));
                auto compressedReverse = fqc::format::deltaEncode(std::span<const fqc::ReadId>(ids.data(), ids.size()));

                writer_->writeReorderMap(mapHeader, compressedForward, compressedReverse);

                FQC_LOG_DEBUG("WriterNode: Reorder map written ({} reads)", mapHeader.totalReads);
            }
            
            writer_->finalize();
            
            state_ = NodeState::kFinished;
            
            FQC_LOG_DEBUG("WriterNode finalized: blocks={}, bytes={}",
                      totalBlocksWritten_, totalBytesWritten_);
            
            return {};
            
        } catch (const FQCException& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to finalize: {}", e.what())});
        }
    }

    NodeState state() const noexcept { return state_; }
    std::uint32_t totalBlocksWritten() const noexcept { return totalBlocksWritten_; }
    std::uint64_t totalBytesWritten() const noexcept { return totalBytesWritten_; }

    void close() noexcept {
        if (writer_ && !writer_->isFinalized()) {
            writer_->abort();
        }
        writer_.reset();
        state_ = NodeState::kIdle;
    }

    void reset() noexcept {
        close();
        totalBlocksWritten_ = 0;
        totalBytesWritten_ = 0;
        nextExpectedBlockId_ = 0;
        pendingBlocks_.clear();
    }

    const WriterNodeConfig& config() const noexcept { return config_; }

private:
    VoidResult writeBlockInternal(const CompressedBlock& block) {
        format::BlockHeader header;
        header.headerSize = format::BlockHeader::kSize;
        header.blockId = block.blockId;
        header.checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);
        header.codecIds = block.codecIds;
        header.codecSeq = block.codecSeq;
        header.codecQual = block.codecQual;
        header.codecAux = block.codecAux;
        header.blockXxhash64 = block.checksum;
        header.uncompressedCount = block.readCount;
        header.uniformReadLength = block.uniformReadLength;
        
        header.offsetIds = 0;
        header.sizeIds = block.idStream.size();
        header.offsetSeq = header.sizeIds;
        header.sizeSeq = block.seqStream.size();
        header.offsetQual = header.offsetSeq + header.sizeSeq;
        header.sizeQual = block.qualStream.size();
        header.offsetAux = header.offsetQual + header.sizeQual;
        header.sizeAux = block.auxStream.size();
        header.compressedSize = block.totalSize();
        
        format::BlockPayload payload;
        payload.idsData = block.idStream;
        payload.seqData = block.seqStream;
        payload.qualData = block.qualStream;
        payload.auxData = block.auxStream;
        
        writer_->writeBlock(header, payload);
        
        ++totalBlocksWritten_;
        totalBytesWritten_ += format::BlockHeader::kSize + block.totalSize();
        ++nextExpectedBlockId_;
        
        FQC_LOG_DEBUG("WriterNode wrote block: id={}, size={}", block.blockId, block.totalSize());
        
        return {};
    }

    WriterNodeConfig config_;
    std::filesystem::path outputPath_;
    format::GlobalHeader globalHeader_;
    std::unique_ptr<format::FQCWriter> writer_;
    NodeState state_ = NodeState::kIdle;
    std::uint32_t totalBlocksWritten_ = 0;
    std::uint64_t totalBytesWritten_ = 0;
    BlockId nextExpectedBlockId_ = 0;
    std::map<BlockId, CompressedBlock> pendingBlocks_;
};

// =============================================================================
// WriterNode Public Interface
// =============================================================================

WriterNode::WriterNode(WriterNodeConfig config)
    : impl_(std::make_unique<WriterNodeImpl>(std::move(config))) {}

WriterNode::~WriterNode() = default;

WriterNode::WriterNode(WriterNode&&) noexcept = default;
WriterNode& WriterNode::operator=(WriterNode&&) noexcept = default;

VoidResult WriterNode::open(
    const std::filesystem::path& path,
    const format::GlobalHeader& globalHeader) {
    return impl_->open(path, globalHeader);
}

VoidResult WriterNode::writeBlock(CompressedBlock block) {
    return impl_->writeBlock(std::move(block));
}

VoidResult WriterNode::finalize(std::optional<std::span<const std::uint8_t>> reorderMap) {
    return impl_->finalize(reorderMap);
}

NodeState WriterNode::state() const noexcept {
    return impl_->state();
}

std::uint32_t WriterNode::totalBlocksWritten() const noexcept {
    return impl_->totalBlocksWritten();
}

std::uint64_t WriterNode::totalBytesWritten() const noexcept {
    return impl_->totalBytesWritten();
}

void WriterNode::close() noexcept {
    impl_->close();
}

void WriterNode::reset() noexcept {
    impl_->reset();
}

const WriterNodeConfig& WriterNode::config() const noexcept {
    return impl_->config();
}

}  // namespace fqc::pipeline
