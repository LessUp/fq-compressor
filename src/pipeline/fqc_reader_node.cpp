// =============================================================================
// fq-compressor - FQCReaderNode Implementation
// =============================================================================
// Implements the FQCReaderNode (serial input stage) for decompression pipeline.
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/pipeline/pipeline_node.h"

#include <fmt/format.h>

#include "fqc/common/logger.h"
#include "fqc/format/fqc_reader.h"

namespace fqc::pipeline {

// =============================================================================
// FQCReaderNodeImpl
// =============================================================================

class FQCReaderNodeImpl {
public:
    explicit FQCReaderNodeImpl(FQCReaderNodeConfig config)
        : config_(std::move(config)) {}

    VoidResult open(const std::filesystem::path& path) {
        try {
            inputPath_ = path;
            reader_ = std::make_unique<format::FQCReader>(path);
            reader_->open();
            
            globalHeader_ = reader_->globalHeader();
            
            if (reader_->hasReorderMap()) {
                reader_->loadReorderMap();
            }
            
            totalBlocks_ = static_cast<std::uint32_t>(reader_->blockCount());
            
            if (config_.rangeStart > 0 || config_.rangeEnd > 0) {
                if (config_.rangeStart > 0) {
                    startBlockId_ = reader_->findBlockForRead(config_.rangeStart);
                    if (startBlockId_ == kInvalidBlockId) {
                        startBlockId_ = 0;
                    }
                }
                if (config_.rangeEnd > 0) {
                    endBlockId_ = reader_->findBlockForRead(config_.rangeEnd);
                    if (endBlockId_ == kInvalidBlockId) {
                        endBlockId_ = totalBlocks_;
                    } else {
                        ++endBlockId_;
                    }
                } else {
                    endBlockId_ = totalBlocks_;
                }
            } else {
                startBlockId_ = 0;
                endBlockId_ = totalBlocks_;
            }
            
            currentBlockId_ = startBlockId_;
            state_ = NodeState::kRunning;
            totalBlocksRead_ = 0;
            
            return {};
        } catch (const FQCException& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to open FQC file: {}", e.what())});
        }
    }

    Result<std::optional<CompressedBlock>> readBlock() {
        if (state_ != NodeState::kRunning) {
            return std::nullopt;
        }

        if (currentBlockId_ >= endBlockId_) {
            state_ = NodeState::kFinished;
            return std::nullopt;
        }

        try {
            auto blockData = reader_->readBlock(currentBlockId_);
            
            CompressedBlock block;
            block.blockId = currentBlockId_;
            block.idStream = std::move(blockData.idsData);
            block.seqStream = std::move(blockData.seqData);
            block.qualStream = std::move(blockData.qualData);
            block.auxStream = std::move(blockData.auxData);
            block.readCount = blockData.header.uncompressedCount;
            block.uniformReadLength = blockData.header.uniformReadLength;
            block.checksum = blockData.header.blockXxhash64;
            block.codecIds = blockData.header.codecIds;
            block.codecSeq = blockData.header.codecSeq;
            block.codecQual = blockData.header.codecQual;
            block.codecAux = blockData.header.codecAux;
            
            auto indexEntry = reader_->getIndexEntry(currentBlockId_);
            if (indexEntry) {
                block.startReadId = indexEntry->archiveIdStart;
            } else {
                block.startReadId = 1;
            }
            
            block.isLast = (currentBlockId_ + 1 >= endBlockId_);
            
            ++currentBlockId_;
            ++totalBlocksRead_;
            
            return block;
        } catch (const FQCException& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to read block: {}", e.what())});
        }
    }

    bool hasMore() const noexcept {
        return state_ == NodeState::kRunning && currentBlockId_ < endBlockId_;
    }

    const format::GlobalHeader& globalHeader() const noexcept {
        return globalHeader_;
    }

    std::optional<std::span<const std::uint8_t>> reorderMap() const noexcept {
        if (reorderMapData_.empty()) {
            return std::nullopt;
        }
        return std::span<const std::uint8_t>(reorderMapData_);
    }

    NodeState state() const noexcept { return state_; }
    std::uint32_t totalBlocksRead() const noexcept { return totalBlocksRead_; }

    void close() noexcept {
        if (reader_) {
            reader_->close();
            reader_.reset();
        }
        state_ = NodeState::kIdle;
    }

    void reset() noexcept {
        close();
        totalBlocksRead_ = 0;
        currentBlockId_ = 0;
        reorderMapData_.clear();
    }

    const FQCReaderNodeConfig& config() const noexcept { return config_; }

private:
    FQCReaderNodeConfig config_;
    std::filesystem::path inputPath_;
    std::unique_ptr<format::FQCReader> reader_;
    format::GlobalHeader globalHeader_;
    std::vector<std::uint8_t> reorderMapData_;
    NodeState state_ = NodeState::kIdle;
    std::uint32_t totalBlocksRead_ = 0;
    std::uint32_t currentBlockId_ = 0;
    std::uint32_t startBlockId_ = 0;
    std::uint32_t endBlockId_ = 0;
    std::uint32_t totalBlocks_ = 0;
};

// =============================================================================
// FQCReaderNode Public Interface
// =============================================================================

FQCReaderNode::FQCReaderNode(FQCReaderNodeConfig config)
    : impl_(std::make_unique<FQCReaderNodeImpl>(std::move(config))) {}

FQCReaderNode::~FQCReaderNode() = default;

FQCReaderNode::FQCReaderNode(FQCReaderNode&&) noexcept = default;
FQCReaderNode& FQCReaderNode::operator=(FQCReaderNode&&) noexcept = default;

VoidResult FQCReaderNode::open(const std::filesystem::path& path) {
    return impl_->open(path);
}

Result<std::optional<CompressedBlock>> FQCReaderNode::readBlock() {
    return impl_->readBlock();
}

bool FQCReaderNode::hasMore() const noexcept {
    return impl_->hasMore();
}

const format::GlobalHeader& FQCReaderNode::globalHeader() const noexcept {
    return impl_->globalHeader();
}

std::optional<std::span<const std::uint8_t>> FQCReaderNode::reorderMap() const noexcept {
    return impl_->reorderMap();
}

NodeState FQCReaderNode::state() const noexcept {
    return impl_->state();
}

std::uint32_t FQCReaderNode::totalBlocksRead() const noexcept {
    return impl_->totalBlocksRead();
}

void FQCReaderNode::close() noexcept {
    impl_->close();
}

void FQCReaderNode::reset() noexcept {
    impl_->reset();
}

const FQCReaderNodeConfig& FQCReaderNode::config() const noexcept {
    return impl_->config();
}

}  // namespace fqc::pipeline
