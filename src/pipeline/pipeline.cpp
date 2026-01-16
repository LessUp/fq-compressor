// =============================================================================
// fq-compressor - Pipeline Implementation
// =============================================================================
// Implements the TBB-based parallel compression/decompression pipeline.
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/pipeline/pipeline.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>

// TBB headers (will be available when TBB is linked)
// #include <tbb/parallel_pipeline.h>
// #include <tbb/task_arena.h>

#include "fqc/common/logger.h"
#include "fqc/format/fqc_format.h"
#include "fqc/pipeline/pipeline_node.h"

namespace fqc::pipeline {

// =============================================================================
// Utility Function Implementations
// =============================================================================

std::size_t recommendedThreadCount() noexcept {
    auto hwThreads = std::thread::hardware_concurrency();
    if (hwThreads == 0) {
        return 4;  // Fallback default
    }
    // Use all available threads, but cap at reasonable maximum
    return std::min(hwThreads, 32u);
}

std::size_t recommendedBlockSize(ReadLengthClass lengthClass) noexcept {
    switch (lengthClass) {
        case ReadLengthClass::kShort:
            return kDefaultBlockSizeShort;
        case ReadLengthClass::kMedium:
            return kDefaultBlockSizeMedium;
        case ReadLengthClass::kLong:
            return kDefaultBlockSizeLong;
    }
    return kDefaultBlockSizeShort;
}

std::size_t estimateMemoryUsage(
    const CompressionPipelineConfig& config,
    std::size_t estimatedReads) {
    // Phase 1 memory: ~24 bytes/read for minimizer index + reorder map
    constexpr std::size_t kPhase1BytesPerRead = 24;

    // Phase 2 memory: ~50 bytes/read per block in flight
    constexpr std::size_t kPhase2BytesPerRead = 50;

    std::size_t phase1Memory = 0;
    if (!config.streamingMode && config.enableReorder) {
        phase1Memory = estimatedReads * kPhase1BytesPerRead;
    }

    std::size_t blockSize = config.blockSize > 0 ? config.blockSize
                                                  : recommendedBlockSize(config.readLengthClass);
    std::size_t phase2Memory = blockSize * kPhase2BytesPerRead * config.maxInFlightBlocks;

    // Buffer memory
    std::size_t bufferMemory = config.inputBufferSize + config.outputBufferSize;

    return phase1Memory + phase2Memory + bufferMemory;
}

bool hasEnoughMemory(
    const CompressionPipelineConfig& config,
    std::size_t estimatedReads) {
    if (config.memoryLimitMB == 0) {
        return true;  // No limit
    }

    std::size_t estimated = estimateMemoryUsage(config, estimatedReads);
    std::size_t limitBytes = config.memoryLimitMB * 1024 * 1024;

    return estimated <= limitBytes;
}

// =============================================================================
// CompressionPipelineConfig Implementation
// =============================================================================

VoidResult CompressionPipelineConfig::validate() const {
    if (blockSize > 0 && (blockSize < kMinBlockSize || blockSize > kMaxBlockSize)) {
        return makeError(
            ErrorCode::kInvalidArgument,
            "Block size must be between {} and {}",
            kMinBlockSize, kMaxBlockSize);
    }

    if (compressionLevel < kMinCompressionLevel || compressionLevel > kMaxCompressionLevel) {
        return makeError(
            ErrorCode::kInvalidArgument,
            "Compression level must be between {} and {}",
            kMinCompressionLevel, kMaxCompressionLevel);
    }

    if (maxInFlightBlocks == 0) {
        return makeError(ErrorCode::kInvalidArgument, "Max in-flight blocks must be > 0");
    }

    return {};
}

std::size_t CompressionPipelineConfig::effectiveThreads() const noexcept {
    return numThreads > 0 ? numThreads : recommendedThreadCount();
}

std::size_t CompressionPipelineConfig::effectiveBlockSize() const noexcept {
    return blockSize > 0 ? blockSize : recommendedBlockSize(readLengthClass);
}

// =============================================================================
// DecompressionPipelineConfig Implementation
// =============================================================================

VoidResult DecompressionPipelineConfig::validate() const {
    if (rangeStart > 0 && rangeEnd > 0 && rangeStart > rangeEnd) {
        return makeError(
            ErrorCode::kInvalidArgument,
            "Range start ({}) must be <= range end ({})",
            rangeStart, rangeEnd);
    }

    if (maxInFlightBlocks == 0) {
        return makeError(ErrorCode::kInvalidArgument, "Max in-flight blocks must be > 0");
    }

    return {};
}

std::size_t DecompressionPipelineConfig::effectiveThreads() const noexcept {
    return numThreads > 0 ? numThreads : recommendedThreadCount();
}

// =============================================================================
// CompressionPipelineImpl
// =============================================================================

class CompressionPipelineImpl {
public:
    explicit CompressionPipelineImpl(CompressionPipelineConfig config)
        : config_(std::move(config)) {}

    VoidResult run(
        const std::filesystem::path& inputPath,
        const std::filesystem::path& outputPath) {
        // Validate configuration
        if (auto result = config_.validate(); !result) {
            return result;
        }

        running_.store(true);
        cancelled_.store(false);

        auto startTime = std::chrono::steady_clock::now();

        // Reset statistics
        stats_ = PipelineStats{};
        stats_.threadsUsed = config_.effectiveThreads();

        try {
            // Initialize pipeline nodes
            ReaderNodeConfig readerConfig;
            readerConfig.blockSize = config_.effectiveBlockSize();
            readerConfig.bufferSize = config_.inputBufferSize;
            readerConfig.readLengthClass = config_.readLengthClass;
            readerConfig.maxBlockBases = kDefaultMaxBlockBasesLong;
            
            ReaderNode reader(readerConfig);
            auto openResult = reader.open(inputPath);
            if (!openResult) {
                running_.store(false);
                return openResult;
            }

            CompressorNodeConfig compressorConfig;
            compressorConfig.readLengthClass = config_.readLengthClass;
            compressorConfig.qualityMode = config_.qualityMode;
            compressorConfig.idMode = config_.idMode;
            compressorConfig.compressionLevel = config_.compressionLevel;
            
            WriterNodeConfig writerConfig;
            writerConfig.bufferSize = config_.outputBufferSize;
            writerConfig.atomicWrite = true;
            
            WriterNode writer(writerConfig);
            
            // Build global header
            format::GlobalHeader globalHeader;
            globalHeader.headerSize = format::GlobalHeader::kMinSize;
            globalHeader.flags = format::buildFlags(
                false,  // isPaired
                !config_.enableReorder || config_.streamingMode,  // preserveOrder
                config_.qualityMode,
                config_.idMode,
                config_.saveReorderMap && config_.enableReorder && !config_.streamingMode,
                PELayout::kInterleaved,
                config_.readLengthClass,
                config_.streamingMode
            );
            globalHeader.compressionAlgo = static_cast<std::uint8_t>(
                config_.readLengthClass == ReadLengthClass::kShort 
                    ? CodecFamily::kAbcV1 
                    : CodecFamily::kZstdPlain
            );
            globalHeader.checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);
            globalHeader.totalReadCount = reader.estimatedTotalReads();
            
            auto writerOpenResult = writer.open(outputPath, globalHeader);
            if (!writerOpenResult) {
                running_.store(false);
                return writerOpenResult;
            }

            // Create thread-local compressors for parallel processing
            std::vector<std::unique_ptr<CompressorNode>> compressors;
            for (std::size_t i = 0; i < config_.effectiveThreads(); ++i) {
                compressors.push_back(std::make_unique<CompressorNode>(compressorConfig));
            }

            // Backpressure controller
            BackpressureController backpressure(config_.maxInFlightBlocks);
            
            // Ordered queue for maintaining block order
            OrderedQueue<CompressedBlock> outputQueue(0);
            
            // Progress tracking
            std::atomic<std::uint64_t> readsProcessed{0};
            std::atomic<std::uint32_t> blocksProcessed{0};
            auto lastProgressTime = std::chrono::steady_clock::now();
            
            // Simple sequential pipeline (TBB integration placeholder)
            // In production, this would use tbb::parallel_pipeline
            // For now, implement a simple sequential version
            
            std::uint32_t blockId = 0;
            while (!cancelled_.load()) {
                // Read chunk
                auto chunkResult = reader.readChunk();
                if (!chunkResult) {
                    running_.store(false);
                    return chunkResult.error();
                }
                
                if (!chunkResult->has_value()) {
                    // EOF
                    break;
                }
                
                auto& chunk = chunkResult->value();
                chunk.chunkId = blockId++;
                
                // Compress (using first compressor in sequential mode)
                auto compressResult = compressors[0]->compress(std::move(chunk));
                if (!compressResult) {
                    running_.store(false);
                    return compressResult.error();
                }
                
                // Write
                auto writeResult = writer.writeBlock(std::move(*compressResult));
                if (!writeResult) {
                    running_.store(false);
                    return writeResult;
                }
                
                // Update progress
                readsProcessed.fetch_add(chunk.reads.size());
                blocksProcessed.fetch_add(1);
                
                // Report progress
                if (config_.progressCallback) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - lastProgressTime).count();
                    
                    if (elapsed >= config_.progressIntervalMs) {
                        ProgressInfo info;
                        info.readsProcessed = readsProcessed.load();
                        info.totalReads = reader.estimatedTotalReads();
                        info.bytesProcessed = reader.totalBytesRead();
                        info.currentBlock = blocksProcessed.load();
                        info.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - startTime).count();
                        
                        if (!config_.progressCallback(info)) {
                            cancelled_.store(true);
                            break;
                        }
                        
                        lastProgressTime = now;
                    }
                }
            }
            
            // Finalize
            if (!cancelled_.load()) {
                auto finalizeResult = writer.finalize(std::nullopt);
                if (!finalizeResult) {
                    running_.store(false);
                    return finalizeResult;
                }
            }
            
            // Update statistics
            stats_.totalReads = readsProcessed.load();
            stats_.totalBlocks = blocksProcessed.load();
            stats_.inputBytes = reader.totalBytesRead();
            stats_.outputBytes = writer.totalBytesWritten();
            
        } catch (const FQCException& e) {
            running_.store(false);
            return makeError(e.code(), e.what());
        } catch (const std::exception& e) {
            running_.store(false);
            return makeError(ErrorCode::kInternalError, "Pipeline error: {}", e.what());
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();

        running_.store(false);

        if (cancelled_.load()) {
            return makeError(ErrorCode::kCancelled, "Pipeline cancelled");
        }

        LOG_INFO("Compression complete: {} reads, {} blocks, {:.2f}x ratio, {:.1f} MB/s",
                 stats_.totalReads, stats_.totalBlocks, 
                 stats_.compressionRatio(), stats_.throughputMBps());

        return {};
    }

    VoidResult runPaired(
        const std::filesystem::path& input1Path,
        const std::filesystem::path& input2Path,
        const std::filesystem::path& outputPath) {
        // Validate configuration
        if (auto result = config_.validate(); !result) {
            return result;
        }

        running_.store(true);
        cancelled_.store(false);

        auto startTime = std::chrono::steady_clock::now();

        // Reset statistics
        stats_ = PipelineStats{};
        stats_.threadsUsed = config_.effectiveThreads();

        try {
            // Initialize reader for paired-end
            ReaderNodeConfig readerConfig;
            readerConfig.blockSize = config_.effectiveBlockSize();
            readerConfig.bufferSize = config_.inputBufferSize;
            readerConfig.readLengthClass = config_.readLengthClass;
            
            ReaderNode reader(readerConfig);
            auto openResult = reader.openPaired(input1Path, input2Path);
            if (!openResult) {
                running_.store(false);
                return openResult;
            }

            CompressorNodeConfig compressorConfig;
            compressorConfig.readLengthClass = config_.readLengthClass;
            compressorConfig.qualityMode = config_.qualityMode;
            compressorConfig.idMode = config_.idMode;
            compressorConfig.compressionLevel = config_.compressionLevel;
            
            CompressorNode compressor(compressorConfig);
            
            WriterNodeConfig writerConfig;
            writerConfig.bufferSize = config_.outputBufferSize;
            writerConfig.atomicWrite = true;
            
            WriterNode writer(writerConfig);
            
            // Build global header for paired-end
            format::GlobalHeader globalHeader;
            globalHeader.headerSize = format::GlobalHeader::kMinSize;
            globalHeader.flags = format::buildFlags(
                true,  // isPaired
                !config_.enableReorder || config_.streamingMode,
                config_.qualityMode,
                config_.idMode,
                config_.saveReorderMap && config_.enableReorder && !config_.streamingMode,
                PELayout::kInterleaved,
                config_.readLengthClass,
                config_.streamingMode
            );
            globalHeader.compressionAlgo = static_cast<std::uint8_t>(
                config_.readLengthClass == ReadLengthClass::kShort 
                    ? CodecFamily::kAbcV1 
                    : CodecFamily::kZstdPlain
            );
            globalHeader.checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);
            globalHeader.totalReadCount = reader.estimatedTotalReads();
            
            auto writerOpenResult = writer.open(outputPath, globalHeader);
            if (!writerOpenResult) {
                running_.store(false);
                return writerOpenResult;
            }

            // Sequential pipeline for paired-end
            std::uint32_t blockId = 0;
            while (!cancelled_.load()) {
                auto chunkResult = reader.readChunk();
                if (!chunkResult) {
                    running_.store(false);
                    return chunkResult.error();
                }
                
                if (!chunkResult->has_value()) {
                    break;
                }
                
                auto& chunk = chunkResult->value();
                chunk.chunkId = blockId++;
                
                auto compressResult = compressor.compress(std::move(chunk));
                if (!compressResult) {
                    running_.store(false);
                    return compressResult.error();
                }
                
                auto writeResult = writer.writeBlock(std::move(*compressResult));
                if (!writeResult) {
                    running_.store(false);
                    return writeResult;
                }
                
                stats_.totalReads += chunk.reads.size();
                ++stats_.totalBlocks;
            }
            
            if (!cancelled_.load()) {
                auto finalizeResult = writer.finalize(std::nullopt);
                if (!finalizeResult) {
                    running_.store(false);
                    return finalizeResult;
                }
            }
            
            stats_.inputBytes = reader.totalBytesRead();
            stats_.outputBytes = writer.totalBytesWritten();
            
        } catch (const FQCException& e) {
            running_.store(false);
            return makeError(e.code(), e.what());
        } catch (const std::exception& e) {
            running_.store(false);
            return makeError(ErrorCode::kInternalError, "Pipeline error: {}", e.what());
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();

        running_.store(false);

        if (cancelled_.load()) {
            return makeError(ErrorCode::kCancelled, "Pipeline cancelled");
        }

        return {};
    }

    void cancel() noexcept {
        cancelled_.store(true);
    }

    bool isRunning() const noexcept {
        return running_.load();
    }

    bool isCancelled() const noexcept {
        return cancelled_.load();
    }

    const PipelineStats& stats() const noexcept {
        return stats_;
    }

    const CompressionPipelineConfig& config() const noexcept {
        return config_;
    }

    VoidResult setConfig(CompressionPipelineConfig config) {
        if (running_.load()) {
            return makeError(ErrorCode::kInvalidState, "Cannot change config while running");
        }
        if (auto result = config.validate(); !result) {
            return result;
        }
        config_ = std::move(config);
        return {};
    }

    void reset() noexcept {
        stats_ = PipelineStats{};
        cancelled_.store(false);
    }

private:
    CompressionPipelineConfig config_;
    PipelineStats stats_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
};

// =============================================================================
// DecompressionPipelineImpl
// =============================================================================

class DecompressionPipelineImpl {
public:
    explicit DecompressionPipelineImpl(DecompressionPipelineConfig config)
        : config_(std::move(config)) {}

    VoidResult run(
        const std::filesystem::path& inputPath,
        const std::filesystem::path& outputPath) {
        // Validate configuration
        if (auto result = config_.validate(); !result) {
            return result;
        }

        running_.store(true);
        cancelled_.store(false);

        auto startTime = std::chrono::steady_clock::now();

        // Reset statistics
        stats_ = PipelineStats{};
        stats_.threadsUsed = config_.effectiveThreads();

        try {
            // Initialize FQC reader
            FQCReaderNodeConfig readerConfig;
            readerConfig.rangeStart = config_.rangeStart;
            readerConfig.rangeEnd = config_.rangeEnd;
            readerConfig.verifyChecksums = config_.verifyChecksums;
            
            FQCReaderNode reader(readerConfig);
            auto openResult = reader.open(inputPath);
            if (!openResult) {
                running_.store(false);
                return openResult;
            }

            // Initialize decompressor
            DecompressorNodeConfig decompressorConfig;
            decompressorConfig.skipCorrupted = config_.skipCorrupted;
            decompressorConfig.placeholderQual = config_.placeholderQual;
            
            DecompressorNode decompressor(decompressorConfig);

            // Initialize FASTQ writer
            FASTQWriterNodeConfig writerConfig;
            writerConfig.bufferSize = config_.outputBufferSize;
            
            FASTQWriterNode writer(writerConfig);
            auto writerOpenResult = writer.open(outputPath);
            if (!writerOpenResult) {
                running_.store(false);
                return writerOpenResult;
            }

            // Load reorder map if needed for original order output
            if (config_.originalOrder && reader.globalHeader().flags & format::flags::kHasReorderMap) {
                // TODO: Load and use reorder map
            }

            // Progress tracking
            auto lastProgressTime = std::chrono::steady_clock::now();

            // Sequential decompression pipeline
            while (!cancelled_.load()) {
                auto blockResult = reader.readBlock();
                if (!blockResult) {
                    running_.store(false);
                    return blockResult.error();
                }
                
                if (!blockResult->has_value()) {
                    // EOF
                    break;
                }
                
                auto& compressedBlock = blockResult->value();
                
                // Decompress
                auto decompressResult = decompressor.decompress(
                    std::move(compressedBlock), reader.globalHeader());
                if (!decompressResult) {
                    if (config_.skipCorrupted) {
                        LOG_WARNING("Skipping corrupted block");
                        continue;
                    }
                    running_.store(false);
                    return decompressResult.error();
                }
                
                // Write
                auto writeResult = writer.writeChunk(std::move(*decompressResult));
                if (!writeResult) {
                    running_.store(false);
                    return writeResult;
                }
                
                // Update statistics
                stats_.totalReads += decompressResult->reads.size();
                ++stats_.totalBlocks;
                
                // Report progress
                if (config_.progressCallback) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - lastProgressTime).count();
                    
                    if (elapsed >= config_.progressIntervalMs) {
                        ProgressInfo info;
                        info.readsProcessed = stats_.totalReads;
                        info.totalReads = reader.globalHeader().totalReadCount;
                        info.currentBlock = stats_.totalBlocks;
                        info.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - startTime).count();
                        
                        if (!config_.progressCallback(info)) {
                            cancelled_.store(true);
                            break;
                        }
                        
                        lastProgressTime = now;
                    }
                }
            }
            
            // Flush output
            if (!cancelled_.load()) {
                auto flushResult = writer.flush();
                if (!flushResult) {
                    running_.store(false);
                    return flushResult;
                }
            }
            
            stats_.outputBytes = writer.totalBytesWritten();
            
        } catch (const FQCException& e) {
            running_.store(false);
            return makeError(e.code(), e.what());
        } catch (const std::exception& e) {
            running_.store(false);
            return makeError(ErrorCode::kInternalError, "Pipeline error: {}", e.what());
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();

        running_.store(false);

        if (cancelled_.load()) {
            return makeError(ErrorCode::kCancelled, "Pipeline cancelled");
        }

        LOG_INFO("Decompression complete: {} reads, {} blocks, {:.1f} MB/s",
                 stats_.totalReads, stats_.totalBlocks, stats_.throughputMBps());

        return {};
    }

    VoidResult runPaired(
        const std::filesystem::path& inputPath,
        const std::filesystem::path& output1Path,
        const std::filesystem::path& output2Path) {
        // Validate configuration
        if (auto result = config_.validate(); !result) {
            return result;
        }

        running_.store(true);
        cancelled_.store(false);

        auto startTime = std::chrono::steady_clock::now();

        // Reset statistics
        stats_ = PipelineStats{};
        stats_.threadsUsed = config_.effectiveThreads();

        try {
            // Initialize FQC reader
            FQCReaderNodeConfig readerConfig;
            readerConfig.rangeStart = config_.rangeStart;
            readerConfig.rangeEnd = config_.rangeEnd;
            readerConfig.verifyChecksums = config_.verifyChecksums;
            
            FQCReaderNode reader(readerConfig);
            auto openResult = reader.open(inputPath);
            if (!openResult) {
                running_.store(false);
                return openResult;
            }

            // Check if input is paired-end
            if (!format::isPaired(reader.globalHeader().flags)) {
                running_.store(false);
                return makeError(ErrorCode::kInvalidFormat, 
                                 "Input file is not paired-end data");
            }

            // Initialize decompressor
            DecompressorNodeConfig decompressorConfig;
            decompressorConfig.skipCorrupted = config_.skipCorrupted;
            decompressorConfig.placeholderQual = config_.placeholderQual;
            
            DecompressorNode decompressor(decompressorConfig);

            // Initialize FASTQ writers for R1 and R2
            FASTQWriterNodeConfig writerConfig;
            writerConfig.bufferSize = config_.outputBufferSize;
            
            FASTQWriterNode writer(writerConfig);
            auto writerOpenResult = writer.openPaired(output1Path, output2Path);
            if (!writerOpenResult) {
                running_.store(false);
                return writerOpenResult;
            }

            // Sequential decompression pipeline
            while (!cancelled_.load()) {
                auto blockResult = reader.readBlock();
                if (!blockResult) {
                    running_.store(false);
                    return blockResult.error();
                }
                
                if (!blockResult->has_value()) {
                    break;
                }
                
                auto& compressedBlock = blockResult->value();
                
                auto decompressResult = decompressor.decompress(
                    std::move(compressedBlock), reader.globalHeader());
                if (!decompressResult) {
                    if (config_.skipCorrupted) {
                        continue;
                    }
                    running_.store(false);
                    return decompressResult.error();
                }
                
                auto writeResult = writer.writeChunk(std::move(*decompressResult));
                if (!writeResult) {
                    running_.store(false);
                    return writeResult;
                }
                
                stats_.totalReads += decompressResult->reads.size();
                ++stats_.totalBlocks;
            }
            
            if (!cancelled_.load()) {
                auto flushResult = writer.flush();
                if (!flushResult) {
                    running_.store(false);
                    return flushResult;
                }
            }
            
            stats_.outputBytes = writer.totalBytesWritten();
            
        } catch (const FQCException& e) {
            running_.store(false);
            return makeError(e.code(), e.what());
        } catch (const std::exception& e) {
            running_.store(false);
            return makeError(ErrorCode::kInternalError, "Pipeline error: {}", e.what());
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();

        running_.store(false);

        if (cancelled_.load()) {
            return makeError(ErrorCode::kCancelled, "Pipeline cancelled");
        }

        return {};
    }

    void cancel() noexcept {
        cancelled_.store(true);
    }

    bool isRunning() const noexcept {
        return running_.load();
    }

    bool isCancelled() const noexcept {
        return cancelled_.load();
    }

    const PipelineStats& stats() const noexcept {
        return stats_;
    }

    const DecompressionPipelineConfig& config() const noexcept {
        return config_;
    }

    VoidResult setConfig(DecompressionPipelineConfig config) {
        if (running_.load()) {
            return makeError(ErrorCode::kInvalidState, "Cannot change config while running");
        }
        if (auto result = config.validate(); !result) {
            return result;
        }
        config_ = std::move(config);
        return {};
    }

    void reset() noexcept {
        stats_ = PipelineStats{};
        cancelled_.store(false);
    }

private:
    DecompressionPipelineConfig config_;
    PipelineStats stats_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
};

// =============================================================================
// CompressionPipeline Implementation
// =============================================================================

CompressionPipeline::CompressionPipeline(CompressionPipelineConfig config)
    : impl_(std::make_unique<CompressionPipelineImpl>(std::move(config))) {}

CompressionPipeline::~CompressionPipeline() = default;

CompressionPipeline::CompressionPipeline(CompressionPipeline&&) noexcept = default;
CompressionPipeline& CompressionPipeline::operator=(CompressionPipeline&&) noexcept = default;

VoidResult CompressionPipeline::run(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputPath) {
    return impl_->run(inputPath, outputPath);
}

VoidResult CompressionPipeline::runPaired(
    const std::filesystem::path& input1Path,
    const std::filesystem::path& input2Path,
    const std::filesystem::path& outputPath) {
    return impl_->runPaired(input1Path, input2Path, outputPath);
}

void CompressionPipeline::cancel() noexcept {
    impl_->cancel();
}

bool CompressionPipeline::isRunning() const noexcept {
    return impl_->isRunning();
}

bool CompressionPipeline::isCancelled() const noexcept {
    return impl_->isCancelled();
}

const PipelineStats& CompressionPipeline::stats() const noexcept {
    return impl_->stats();
}

const CompressionPipelineConfig& CompressionPipeline::config() const noexcept {
    return impl_->config();
}

VoidResult CompressionPipeline::setConfig(CompressionPipelineConfig config) {
    return impl_->setConfig(std::move(config));
}

void CompressionPipeline::reset() noexcept {
    impl_->reset();
}

// =============================================================================
// DecompressionPipeline Implementation
// =============================================================================

DecompressionPipeline::DecompressionPipeline(DecompressionPipelineConfig config)
    : impl_(std::make_unique<DecompressionPipelineImpl>(std::move(config))) {}

DecompressionPipeline::~DecompressionPipeline() = default;

DecompressionPipeline::DecompressionPipeline(DecompressionPipeline&&) noexcept = default;
DecompressionPipeline& DecompressionPipeline::operator=(DecompressionPipeline&&) noexcept = default;

VoidResult DecompressionPipeline::run(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputPath) {
    return impl_->run(inputPath, outputPath);
}

VoidResult DecompressionPipeline::runPaired(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& output1Path,
    const std::filesystem::path& output2Path) {
    return impl_->runPaired(inputPath, output1Path, output2Path);
}

void DecompressionPipeline::cancel() noexcept {
    impl_->cancel();
}

bool DecompressionPipeline::isRunning() const noexcept {
    return impl_->isRunning();
}

bool DecompressionPipeline::isCancelled() const noexcept {
    return impl_->isCancelled();
}

const PipelineStats& DecompressionPipeline::stats() const noexcept {
    return impl_->stats();
}

const DecompressionPipelineConfig& DecompressionPipeline::config() const noexcept {
    return impl_->config();
}

VoidResult DecompressionPipeline::setConfig(DecompressionPipelineConfig config) {
    return impl_->setConfig(std::move(config));
}

void DecompressionPipeline::reset() noexcept {
    impl_->reset();
}

}  // namespace fqc::pipeline
