// =============================================================================
// fq-compressor - FASTQWriterNode Implementation
// =============================================================================
// Implements the FASTQWriterNode (serial output stage) for decompression.
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/pipeline/pipeline_node.h"

#include <fstream>
#include <iostream>

#include <fmt/format.h>

#include "fqc/common/logger.h"

namespace fqc::pipeline {

// =============================================================================
// FASTQWriterNodeImpl
// =============================================================================

class FASTQWriterNodeImpl {
public:
    explicit FASTQWriterNodeImpl(FASTQWriterNodeConfig config)
        : config_(std::move(config)) {}

    VoidResult open(const std::filesystem::path& path) {
        try {
            outputPath_ = path;
            isPaired_ = false;
            
            if (path == "-") {
                useStdout_ = true;
            } else {
                stream1_.open(path, std::ios::binary | std::ios::trunc);
                if (!stream1_.is_open()) {
                    return std::unexpected(Error{ErrorCode::kIOError, 
                                     fmt::format("Failed to open output file: {}", path.string())});
                }
                useStdout_ = false;
            }
            
            state_ = NodeState::kRunning;
            totalReadsWritten_ = 0;
            totalBytesWritten_ = 0;
            
            return {};
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to open output: {}", e.what())});
        }
    }

    VoidResult openPaired(
        const std::filesystem::path& path1,
        const std::filesystem::path& path2) {
        try {
            outputPath_ = path1;
            outputPath2_ = path2;
            isPaired_ = true;
            useStdout_ = false;
            
            stream1_.open(path1, std::ios::binary | std::ios::trunc);
            if (!stream1_.is_open()) {
                return std::unexpected(Error{ErrorCode::kIOError, 
                                 fmt::format("Failed to open R1 output file: {}", path1.string())});
            }
            
            stream2_.open(path2, std::ios::binary | std::ios::trunc);
            if (!stream2_.is_open()) {
                stream1_.close();
                return std::unexpected(Error{ErrorCode::kIOError, 
                                 fmt::format("Failed to open R2 output file: {}", path2.string())});
            }
            
            state_ = NodeState::kRunning;
            totalReadsWritten_ = 0;
            totalBytesWritten_ = 0;
            
            return {};
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to open paired output: {}", e.what())});
        }
    }

    VoidResult writeChunk(ReadChunk chunk) {
        if (state_ != NodeState::kRunning) {
            return makeVoidError(ErrorCode::kInvalidState, "Writer not open");
        }

        try {
            if (isPaired_) {
                for (std::size_t i = 0; i < chunk.reads.size(); ++i) {
                    const auto& record = chunk.reads[i];
                    std::string fastqRecord = formatFastqRecord(record);
                    
                    if (i % 2 == 0) {
                        stream1_.write(fastqRecord.data(), 
                                       static_cast<std::streamsize>(fastqRecord.size()));
                    } else {
                        stream2_.write(fastqRecord.data(), 
                                       static_cast<std::streamsize>(fastqRecord.size()));
                    }
                    
                    totalBytesWritten_ += fastqRecord.size();
                }
            } else {
                for (const auto& record : chunk.reads) {
                    std::string fastqRecord = formatFastqRecord(record);
                    
                    if (useStdout_) {
                        std::cout.write(fastqRecord.data(), 
                                        static_cast<std::streamsize>(fastqRecord.size()));
                    } else {
                        stream1_.write(fastqRecord.data(), 
                                       static_cast<std::streamsize>(fastqRecord.size()));
                    }
                    
                    totalBytesWritten_ += fastqRecord.size();
                }
            }
            
            totalReadsWritten_ += chunk.reads.size();
            return {};
            
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to write FASTQ: {}", e.what())});
        }
    }

    VoidResult flush() {
        try {
            if (!useStdout_) {
                if (stream1_.is_open()) {
                    stream1_.flush();
                }
                if (isPaired_ && stream2_.is_open()) {
                    stream2_.flush();
                }
            } else {
                std::cout.flush();
            }
            return {};
        } catch (const std::exception& e) {
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to flush output: {}", e.what())});
        }
    }

    NodeState state() const noexcept { return state_; }
    std::uint64_t totalReadsWritten() const noexcept { return totalReadsWritten_; }
    std::uint64_t totalBytesWritten() const noexcept { return totalBytesWritten_; }

    void close() noexcept {
        if (stream1_.is_open()) {
            stream1_.close();
        }
        if (stream2_.is_open()) {
            stream2_.close();
        }
        state_ = NodeState::kIdle;
    }

    void reset() noexcept {
        close();
        totalReadsWritten_ = 0;
        totalBytesWritten_ = 0;
    }

    const FASTQWriterNodeConfig& config() const noexcept { return config_; }

private:
    std::string formatFastqRecord(const ReadRecord& record) const {
        std::string result;
        result.reserve(record.id.size() + record.sequence.size() + 
                       record.quality.size() + 10);
        
        result += '@';
        result += record.id;
        result += '\n';
        
        if (config_.lineWidth > 0 && record.sequence.size() > config_.lineWidth) {
            for (std::size_t i = 0; i < record.sequence.size(); i += config_.lineWidth) {
                result += record.sequence.substr(i, config_.lineWidth);
                result += '\n';
            }
        } else {
            result += record.sequence;
            result += '\n';
        }
        
        result += "+\n";
        
        if (config_.lineWidth > 0 && record.quality.size() > config_.lineWidth) {
            for (std::size_t i = 0; i < record.quality.size(); i += config_.lineWidth) {
                result += record.quality.substr(i, config_.lineWidth);
                result += '\n';
            }
        } else {
            result += record.quality;
            result += '\n';
        }
        
        return result;
    }

    FASTQWriterNodeConfig config_;
    std::filesystem::path outputPath_;
    std::filesystem::path outputPath2_;
    std::ofstream stream1_;
    std::ofstream stream2_;
    bool isPaired_ = false;
    bool useStdout_ = false;
    NodeState state_ = NodeState::kIdle;
    std::uint64_t totalReadsWritten_ = 0;
    std::uint64_t totalBytesWritten_ = 0;
};

// =============================================================================
// FASTQWriterNode Public Interface
// =============================================================================

FASTQWriterNode::FASTQWriterNode(FASTQWriterNodeConfig config)
    : impl_(std::make_unique<FASTQWriterNodeImpl>(std::move(config))) {}

FASTQWriterNode::~FASTQWriterNode() = default;

FASTQWriterNode::FASTQWriterNode(FASTQWriterNode&&) noexcept = default;
FASTQWriterNode& FASTQWriterNode::operator=(FASTQWriterNode&&) noexcept = default;

VoidResult FASTQWriterNode::open(const std::filesystem::path& path) {
    return impl_->open(path);
}

VoidResult FASTQWriterNode::openPaired(
    const std::filesystem::path& path1,
    const std::filesystem::path& path2) {
    return impl_->openPaired(path1, path2);
}

VoidResult FASTQWriterNode::writeChunk(ReadChunk chunk) {
    return impl_->writeChunk(std::move(chunk));
}

VoidResult FASTQWriterNode::flush() {
    return impl_->flush();
}

NodeState FASTQWriterNode::state() const noexcept {
    return impl_->state();
}

std::uint64_t FASTQWriterNode::totalReadsWritten() const noexcept {
    return impl_->totalReadsWritten();
}

std::uint64_t FASTQWriterNode::totalBytesWritten() const noexcept {
    return impl_->totalBytesWritten();
}

void FASTQWriterNode::close() noexcept {
    impl_->close();
}

void FASTQWriterNode::reset() noexcept {
    impl_->reset();
}

const FASTQWriterNodeConfig& FASTQWriterNode::config() const noexcept {
    return impl_->config();
}

}  // namespace fqc::pipeline
