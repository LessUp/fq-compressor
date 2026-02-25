// =============================================================================
// fq-compressor - ReaderNode Implementation
// =============================================================================
// Implements the ReaderNode (serial input stage) for compression pipeline.
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/pipeline/pipeline_node.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>

#include <fmt/format.h>

#include "fqc/common/logger.h"
#include "fqc/io/async_io.h"
#include "fqc/io/compressed_stream.h"
#include "fqc/io/fastq_parser.h"

namespace fqc::pipeline {

// =============================================================================
// ReaderNodeImpl
// =============================================================================

class ReaderNodeImpl {
public:
    explicit ReaderNodeImpl(ReaderNodeConfig config)
        : config_(std::move(config)) {}

    VoidResult open(const std::filesystem::path& path) {
        try {
            inputPath_ = path;
            
            io::ParserOptions parserOpts;
            parserOpts.bufferSize = config_.bufferSize;
            parserOpts.collectStats = true;
            parserOpts.validateSequence = true;
            parserOpts.validateQuality = true;
            
            // Detect if file is compressed (async prefetch only for plain files)
            bool isStdin = (path == "-");
            bool isCompressed = false;
            if (!isStdin) {
                auto fmt = io::detectCompressionFormatFromExtension(path);
                isCompressed = (fmt != io::CompressionFormat::kNone);
            }
            
            bool useAsync = !isStdin && !isCompressed;
            
            if (useAsync) {
                // Phase 1: Sample with a temporary seekable parser
                {
                    io::FastqParser sampler(path, parserOpts);
                    sampler.open();
                    if (sampler.canSeek()) {
                        auto sampleStats = sampler.sampleRecords(1000);
                        estimatedTotalReads_ = estimateTotalReads(path, sampleStats);
                        detectedLengthClass_ = io::detectReadLengthClass(sampleStats);
                    }
                }  // sampler closes here
                
                // Phase 2: Create async-backed parser for actual reading
                io::AsyncReaderConfig asyncCfg;
                asyncCfg.bufferSize = io::optimalBufferSize(path);
                asyncCfg.prefetchDepth = io::optimalPrefetchDepth();
                
                asyncStream_ = io::createAsyncInputStream(path, asyncCfg);
                if (!asyncStream_) {
                    return std::unexpected(Error{ErrorCode::kIOError,
                        fmt::format("Failed to create async reader for: {}", path.string())});
                }
                parser_ = std::make_unique<io::FastqParser>(std::move(asyncStream_), parserOpts);
                // Note: stream-constructed parser is already open
                
                FQC_LOG_DEBUG("ReaderNode opened with async prefetch: path={}", path.string());
            } else {
                // Fallback: synchronous I/O (stdin or compressed files)
                parser_ = std::make_unique<io::FastqParser>(path, parserOpts);
                parser_->open();
                
                if (parser_->canSeek()) {
                    auto sampleStats = parser_->sampleRecords(1000);
                    estimatedTotalReads_ = estimateTotalReads(path, sampleStats);
                    detectedLengthClass_ = io::detectReadLengthClass(sampleStats);
                }
            }
            
            // Set effective block size from detected or configured length class
            if (estimatedTotalReads_ > 0) {
                if (config_.readLengthClass == ReadLengthClass::kShort) {
                    effectiveBlockSize_ = recommendedBlockSize(detectedLengthClass_);
                } else {
                    effectiveBlockSize_ = recommendedBlockSize(config_.readLengthClass);
                }
            } else {
                // Streaming mode - use conservative defaults
                detectedLengthClass_ = ReadLengthClass::kMedium;
                effectiveBlockSize_ = config_.blockSize;
            }
            
            state_ = NodeState::kRunning;
            chunkId_ = 0;
            totalReadsRead_ = 0;
            totalBytesRead_ = 0;
            nextReadId_ = 1;  // 1-based indexing
            
            FQC_LOG_DEBUG("ReaderNode opened: path={}, estimated_reads={}, block_size={}, async={}",
                      path.string(), estimatedTotalReads_, effectiveBlockSize_, useAsync);
            
            return {};
        } catch (const FQCException& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to open input: {}", e.what())});
        }
    }

    VoidResult openPaired(
        const std::filesystem::path& path1,
        const std::filesystem::path& path2) {
        try {
            inputPath_ = path1;
            inputPath2_ = path2;
            isPaired_ = true;
            
            io::ParserOptions parserOpts;
            parserOpts.bufferSize = config_.bufferSize;
            parserOpts.collectStats = true;
            parserOpts.validateSequence = true;
            parserOpts.validateQuality = true;
            
            parser_ = std::make_unique<io::FastqParser>(path1, parserOpts);
            parser_->open();
            
            parser2_ = std::make_unique<io::FastqParser>(path2, parserOpts);
            parser2_->open();
            
            // Sample from first file
            if (parser_->canSeek()) {
                auto sampleStats = parser_->sampleRecords(1000);
                estimatedTotalReads_ = estimateTotalReads(path1, sampleStats) * 2;
                detectedLengthClass_ = io::detectReadLengthClass(sampleStats);
                
                if (config_.readLengthClass == ReadLengthClass::kShort) {
                    effectiveBlockSize_ = recommendedBlockSize(detectedLengthClass_);
                } else {
                    effectiveBlockSize_ = recommendedBlockSize(config_.readLengthClass);
                }
            } else {
                estimatedTotalReads_ = 0;
                detectedLengthClass_ = ReadLengthClass::kMedium;
                effectiveBlockSize_ = config_.blockSize;
            }
            
            state_ = NodeState::kRunning;
            chunkId_ = 0;
            totalReadsRead_ = 0;
            totalBytesRead_ = 0;
            nextReadId_ = 1;
            
            FQC_LOG_DEBUG("ReaderNode opened paired: path1={}, path2={}, estimated_reads={}", 
                      path1.string(), path2.string(), estimatedTotalReads_);
            
            return {};
        } catch (const FQCException& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to open paired input: {}", e.what())});
        }
    }

    Result<std::optional<ReadChunk>> readChunk() {
        if (state_ != NodeState::kRunning) {
            return std::nullopt;
        }

        try {
            ReadChunk chunk;
            chunk.chunkId = chunkId_;
            chunk.startReadId = nextReadId_;
            
            // Determine how many reads to get based on length class
            std::size_t targetReads = effectiveBlockSize_;
            std::size_t totalBases = 0;
            
            if (isPaired_) {
                // Read interleaved from both files
                while (chunk.reads.size() < targetReads * 2) {
                    auto r1 = parser_->readRecord();
                    auto r2 = parser2_->readRecord();
                    
                    if (!r1 || !r2) {
                        break;  // One or both files exhausted
                    }
                    
                    chunk.reads.emplace_back(std::move(r1->id), std::move(r1->sequence), std::move(r1->quality));
                    chunk.reads.emplace_back(std::move(r2->id), std::move(r2->sequence), std::move(r2->quality));
                    
                    totalBases += chunk.reads[chunk.reads.size()-2].sequence.size() +
                                  chunk.reads[chunk.reads.size()-1].sequence.size();
                    
                    // Check bases limit for long reads
                    if (config_.readLengthClass == ReadLengthClass::kLong &&
                        totalBases >= config_.maxBlockBases) {
                        break;
                    }
                }
            } else {
                // Single-end reading
                while (chunk.reads.size() < targetReads) {
                    auto record = parser_->readRecord();
                    if (!record) {
                        break;
                    }
                    
                    totalBases += record->sequence.size();
                    chunk.reads.emplace_back(std::move(record->id), std::move(record->sequence), std::move(record->quality));
                    
                    // Check bases limit for long reads
                    if (config_.readLengthClass == ReadLengthClass::kLong &&
                        totalBases >= config_.maxBlockBases) {
                        break;
                    }
                }
            }

            if (chunk.reads.empty()) {
                state_ = NodeState::kFinished;
                return std::nullopt;
            }

            totalReadsRead_ += chunk.reads.size();
            nextReadId_ += chunk.reads.size();
            ++chunkId_;
            
            // Update bytes read estimate
            for (const auto& read : chunk.reads) {
                totalBytesRead_ += read.id.size() + read.sequence.size() + read.quality.size() + 10;
            }

            FQC_LOG_DEBUG("ReaderNode read chunk: id={}, reads={}, total_reads={}", 
                      chunk.chunkId, chunk.reads.size(), totalReadsRead_);
            
            return chunk;

        } catch (const FQCException& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kIOError, fmt::format("Failed to read chunk: {}", e.what())});
        }
    }

    bool hasMore() const noexcept {
        return state_ == NodeState::kRunning;
    }

    NodeState state() const noexcept { return state_; }
    std::uint64_t totalReadsRead() const noexcept { return totalReadsRead_; }
    std::uint64_t totalBytesRead() const noexcept { return totalBytesRead_; }
    std::uint64_t estimatedTotalReads() const noexcept { return estimatedTotalReads_; }

    void close() noexcept {
        if (parser_) {
            parser_->close();
            parser_.reset();
        }
        if (parser2_) {
            parser2_->close();
            parser2_.reset();
        }
        asyncStream_.reset();
        state_ = NodeState::kIdle;
    }

    void reset() noexcept {
        close();
        chunkId_ = 0;
        totalReadsRead_ = 0;
        totalBytesRead_ = 0;
        estimatedTotalReads_ = 0;
        nextReadId_ = 1;
        isPaired_ = false;
    }

    const ReaderNodeConfig& config() const noexcept { return config_; }

private:
    /// @brief Estimate total reads based on file size and sample statistics
    std::uint64_t estimateTotalReads(const std::filesystem::path& path,
                                      const io::ParserStats& stats) const {
        if (stats.totalRecords == 0) {
            return 0;
        }
        
        try {
            auto fileSize = std::filesystem::file_size(path);
            // Estimate bytes per record from sample
            double avgRecordSize = static_cast<double>(stats.totalBases) / static_cast<double>(stats.totalRecords);
            // FASTQ overhead: @ID\n + SEQ\n + +\n + QUAL\n ≈ 4 + ID_len + 2*seq_len
            avgRecordSize = static_cast<double>(avgRecordSize) * 2 + 20;  // Rough estimate
            
            return static_cast<std::uint64_t>(static_cast<double>(fileSize) / avgRecordSize);
        } catch (...) {
            return 0;
        }
    }

    ReaderNodeConfig config_;
    std::filesystem::path inputPath_;
    std::filesystem::path inputPath2_;  // For paired-end
    std::unique_ptr<io::FastqParser> parser_;
    std::unique_ptr<io::FastqParser> parser2_;  // For paired-end
    std::unique_ptr<std::istream> asyncStream_;  // Async prefetch stream (kept alive for parser)
    NodeState state_ = NodeState::kIdle;
    std::uint32_t chunkId_ = 0;
    std::uint64_t totalReadsRead_ = 0;
    std::uint64_t totalBytesRead_ = 0;
    std::uint64_t estimatedTotalReads_ = 0;
    ReadId nextReadId_ = 1;
    std::size_t effectiveBlockSize_ = kDefaultBlockSizeShort;
    ReadLengthClass detectedLengthClass_ = ReadLengthClass::kShort;
    bool isPaired_ = false;
};

// =============================================================================
// ReaderNode Public Interface
// =============================================================================

ReaderNode::ReaderNode(ReaderNodeConfig config)
    : impl_(std::make_unique<ReaderNodeImpl>(std::move(config))) {}

ReaderNode::~ReaderNode() = default;

ReaderNode::ReaderNode(ReaderNode&&) noexcept = default;
ReaderNode& ReaderNode::operator=(ReaderNode&&) noexcept = default;

VoidResult ReaderNode::open(const std::filesystem::path& path) {
    return impl_->open(path);
}

VoidResult ReaderNode::openPaired(
    const std::filesystem::path& path1,
    const std::filesystem::path& path2) {
    return impl_->openPaired(path1, path2);
}

Result<std::optional<ReadChunk>> ReaderNode::readChunk() {
    return impl_->readChunk();
}

bool ReaderNode::hasMore() const noexcept {
    return impl_->hasMore();
}

NodeState ReaderNode::state() const noexcept {
    return impl_->state();
}

std::uint64_t ReaderNode::totalReadsRead() const noexcept {
    return impl_->totalReadsRead();
}

std::uint64_t ReaderNode::totalBytesRead() const noexcept {
    return impl_->totalBytesRead();
}

std::uint64_t ReaderNode::estimatedTotalReads() const noexcept {
    return impl_->estimatedTotalReads();
}

void ReaderNode::close() noexcept {
    impl_->close();
}

void ReaderNode::reset() noexcept {
    impl_->reset();
}

const ReaderNodeConfig& ReaderNode::config() const noexcept {
    return impl_->config();
}

}  // namespace fqc::pipeline
