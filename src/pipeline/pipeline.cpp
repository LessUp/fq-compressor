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

#include <fmt/format.h>

// TBB headers for parallel pipeline
#include <tbb/parallel_pipeline.h>
#include <tbb/task_arena.h>

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
        return makeVoidError(
            ErrorCode::kInvalidArgument,
            fmt::format("Block size must be between {} and {}",
            kMinBlockSize, kMaxBlockSize));
    }

    if (compressionLevel < kMinCompressionLevel || compressionLevel > kMaxCompressionLevel) {
        return makeVoidError(
            ErrorCode::kInvalidArgument,
            fmt::format("Compression level must be between {} and {}",
            kMinCompressionLevel, kMaxCompressionLevel));
    }

    if (maxInFlightBlocks == 0) {
        return makeVoidError(ErrorCode::kInvalidArgument, "Max in-flight blocks must be > 0");
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
        return makeVoidError(
            ErrorCode::kInvalidArgument,
            fmt::format("Range start ({}) must be <= range end ({})",
            rangeStart, rangeEnd));
    }

    if (maxInFlightBlocks == 0) {
        return makeVoidError(ErrorCode::kInvalidArgument, "Max in-flight blocks must be > 0");
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
            
            // =================================================================
            // TBB Parallel Pipeline Implementation
            // =================================================================
            // Stage 1 (Reader): serial_in_order - reads FASTQ chunks
            // Stage 2 (Compressor): parallel - compresses chunks concurrently
            // Stage 3 (Writer): serial_in_order - writes blocks in order
            // =================================================================
            
            std::atomic<std::uint32_t> blockId{0};
            std::atomic<bool> readerError{false};
            std::atomic<bool> writerError{false};
            std::optional<Error> pipelineError;
            std::mutex errorMutex;
            
            // Thread-local compressor index for load balancing
            std::atomic<std::size_t> compressorIndex{0};
            
            // Create task arena with configured thread count
            tbb::task_arena arena(static_cast<int>(config_.effectiveThreads()));
            
            arena.execute([&] {
                tbb::parallel_pipeline(
                    config_.maxInFlightBlocks,
                    
                    // Stage 1: Reader (serial, in-order)
                    tbb::make_filter<void, std::optional<ReadChunk>>(
                        tbb::filter_mode::serial_in_order,
                        [&](tbb::flow_control& fc) -> std::optional<ReadChunk> {
                            if (cancelled_.load() || readerError.load() || writerError.load()) {
                                fc.stop();
                                return std::nullopt;
                            }
                            
                            auto chunkResult = reader.readChunk();
                            if (!chunkResult) {
                                std::lock_guard<std::mutex> lock(errorMutex);
                                pipelineError = chunkResult.error();
                                readerError.store(true);
                                fc.stop();
                                return std::nullopt;
                            }
                            
                            if (!chunkResult->has_value()) {
                                // EOF reached
                                fc.stop();
                                return std::nullopt;
                            }
                            
                            auto chunk = std::move(chunkResult->value());
                            chunk.chunkId = blockId.fetch_add(1);
                            return chunk;
                        }
                    ) &
                    
                    // Stage 2: Compressor (parallel)
                    tbb::make_filter<std::optional<ReadChunk>, std::optional<CompressedBlock>>(
                        tbb::filter_mode::parallel,
                        [&](std::optional<ReadChunk> chunkOpt) -> std::optional<CompressedBlock> {
                            if (!chunkOpt.has_value()) {
                                return std::nullopt;
                            }
                            
                            auto& chunk = *chunkOpt;
                            std::size_t readCount = chunk.reads.size();
                            
                            // Select compressor using round-robin
                            std::size_t idx = compressorIndex.fetch_add(1) % compressors.size();
                            
                            auto compressResult = compressors[idx]->compress(std::move(chunk));
                            if (!compressResult) {
                                std::lock_guard<std::mutex> lock(errorMutex);
                                pipelineError = compressResult.error();
                                readerError.store(true);  // Signal to stop reader
                                return std::nullopt;
                            }
                            
                            // Update progress atomically
                            readsProcessed.fetch_add(readCount);
                            
                            return std::move(*compressResult);
                        }
                    ) &
                    
                    // Stage 3: Writer (serial, in-order)
                    tbb::make_filter<std::optional<CompressedBlock>, void>(
                        tbb::filter_mode::serial_in_order,
                        [&](std::optional<CompressedBlock> blockOpt) {
                            if (!blockOpt.has_value()) {
                                return;
                            }
                            
                            auto writeResult = writer.writeBlock(std::move(*blockOpt));
                            if (!writeResult) {
                                std::lock_guard<std::mutex> lock(errorMutex);
                                pipelineError = writeResult.error();
                                writerError.store(true);
                                return;
                            }
                            
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
                                    info.elapsedMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - startTime).count());
                                    
                                    if (!config_.progressCallback(info)) {
                                        cancelled_.store(true);
                                    }
                                    
                                    lastProgressTime = now;
                                }
                            }
                        }
                    )
                );
            });
            
            // Check for pipeline errors
            if (readerError.load() || writerError.load()) {
                running_.store(false);
                return std::unexpected(*pipelineError);
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
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            running_.store(false);
            return std::unexpected(Error{ErrorCode::kInternalError, fmt::format("Pipeline error: {}", e.what())});
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count());

        running_.store(false);

        if (cancelled_.load()) {
            return std::unexpected(Error{ErrorCode::kCancelled, "Pipeline cancelled"});
        }

        FQC_LOG_INFO("Compression complete: {} reads, {} blocks, {:.2f}x ratio, {:.1f} MB/s",
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
                    return std::unexpected(chunkResult.error());
                }
                
                if (!chunkResult->has_value()) {
                    break;
                }
                
                auto& chunk = chunkResult->value();
                chunk.chunkId = blockId++;
                
                auto compressResult = compressor.compress(std::move(chunk));
                if (!compressResult) {
                    running_.store(false);
                    return std::unexpected(compressResult.error());
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
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            running_.store(false);
            return std::unexpected(Error{ErrorCode::kInternalError, fmt::format("Pipeline error: {}", e.what())});
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count());

        running_.store(false);

        if (cancelled_.load()) {
            return std::unexpected(Error{ErrorCode::kCancelled, "Pipeline cancelled"});
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
            return makeVoidError(ErrorCode::kInvalidState, "Cannot change config while running");
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
                FQC_LOG_DEBUG("Loading reorder map for original order output...");
                // Note: Reorder map loading is handled by FQCReader internally
                // The reader will use it when lookupOriginalId() is called
                FQC_LOG_INFO("Reorder map will be loaded on demand");
            }

            // Progress tracking
            auto lastProgressTime = std::chrono::steady_clock::now();

            // =================================================================
            // TBB Parallel Decompression Pipeline
            // =================================================================
            // Stage 1 (Reader): serial_in_order - reads FQC blocks
            // Stage 2 (Decompressor): parallel - decompresses blocks concurrently
            // Stage 3 (Writer): serial_in_order - writes FASTQ in order
            // =================================================================
            
            std::atomic<std::uint64_t> readsProcessed{0};
            std::atomic<std::uint32_t> blocksProcessed{0};
            std::atomic<bool> readerError{false};
            std::atomic<bool> writerError{false};
            std::optional<Error> pipelineError;
            std::mutex errorMutex;
            
            // Store global header reference for decompressor
            const auto& globalHeader = reader.globalHeader();
            
            // Create multiple decompressors for parallel processing
            std::vector<std::unique_ptr<DecompressorNode>> decompressors;
            for (std::size_t i = 0; i < config_.effectiveThreads(); ++i) {
                decompressors.push_back(std::make_unique<DecompressorNode>(decompressorConfig));
            }
            std::atomic<std::size_t> decompressorIndex{0};
            
            // Create task arena with configured thread count
            tbb::task_arena arena(static_cast<int>(config_.effectiveThreads()));
            
            arena.execute([&] {
                tbb::parallel_pipeline(
                    config_.maxInFlightBlocks,
                    
                    // Stage 1: FQC Reader (serial, in-order)
                    tbb::make_filter<void, std::optional<CompressedBlock>>(
                        tbb::filter_mode::serial_in_order,
                        [&](tbb::flow_control& fc) -> std::optional<CompressedBlock> {
                            if (cancelled_.load() || readerError.load() || writerError.load()) {
                                fc.stop();
                                return std::nullopt;
                            }
                            
                            auto blockResult = reader.readBlock();
                            if (!blockResult) {
                                std::lock_guard<std::mutex> lock(errorMutex);
                                pipelineError = blockResult.error();
                                readerError.store(true);
                                fc.stop();
                                return std::nullopt;
                            }
                            
                            if (!blockResult->has_value()) {
                                // EOF reached
                                fc.stop();
                                return std::nullopt;
                            }
                            
                            return std::move(blockResult->value());
                        }
                    ) &
                    
                    // Stage 2: Decompressor (parallel)
                    tbb::make_filter<std::optional<CompressedBlock>, std::optional<ReadChunk>>(
                        tbb::filter_mode::parallel,
                        [&](std::optional<CompressedBlock> blockOpt) -> std::optional<ReadChunk> {
                            if (!blockOpt.has_value()) {
                                return std::nullopt;
                            }
                            
                            // Select decompressor using round-robin
                            std::size_t idx = decompressorIndex.fetch_add(1) % decompressors.size();
                            
                            auto decompressResult = decompressors[idx]->decompress(
                                std::move(*blockOpt), globalHeader);
                            
                            if (!decompressResult) {
                                if (config_.skipCorrupted) {
                                    FQC_LOG_WARNING("Skipping corrupted block");
                                    return std::nullopt;
                                }
                                std::lock_guard<std::mutex> lock(errorMutex);
                                pipelineError = decompressResult.error();
                                readerError.store(true);
                                return std::nullopt;
                            }
                            
                            return std::move(*decompressResult);
                        }
                    ) &
                    
                    // Stage 3: FASTQ Writer (serial, in-order)
                    tbb::make_filter<std::optional<ReadChunk>, void>(
                        tbb::filter_mode::serial_in_order,
                        [&](std::optional<ReadChunk> chunkOpt) {
                            if (!chunkOpt.has_value()) {
                                return;
                            }
                            
                            std::size_t readCount = chunkOpt->reads.size();
                            
                            auto writeResult = writer.writeChunk(std::move(*chunkOpt));
                            if (!writeResult) {
                                std::lock_guard<std::mutex> lock(errorMutex);
                                pipelineError = writeResult.error();
                                writerError.store(true);
                                return;
                            }
                            
                            // Update statistics atomically
                            readsProcessed.fetch_add(readCount);
                            blocksProcessed.fetch_add(1);
                            
                            // Report progress
                            if (config_.progressCallback) {
                                auto now = std::chrono::steady_clock::now();
                                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    now - lastProgressTime).count();
                                
                                if (elapsed >= config_.progressIntervalMs) {
                                    ProgressInfo info;
                                    info.readsProcessed = readsProcessed.load();
                                    info.totalReads = globalHeader.totalReadCount;
                                    info.currentBlock = blocksProcessed.load();
                                    info.elapsedMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - startTime).count());
                                    
                                    if (!config_.progressCallback(info)) {
                                        cancelled_.store(true);
                                    }
                                    
                                    lastProgressTime = now;
                                }
                            }
                        }
                    )
                );
            });
            
            // Check for pipeline errors
            if (readerError.load() || writerError.load()) {
                running_.store(false);
                return std::unexpected(*pipelineError);
            }
            
            // Update final statistics
            stats_.totalReads = readsProcessed.load();
            stats_.totalBlocks = blocksProcessed.load();
            
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
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            running_.store(false);
            return std::unexpected(Error{ErrorCode::kInternalError, fmt::format("Pipeline error: {}", e.what())});
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count());

        running_.store(false);

        if (cancelled_.load()) {
            return std::unexpected(Error{ErrorCode::kCancelled, "Pipeline cancelled"});
        }

        FQC_LOG_INFO("Decompression complete: {} reads, {} blocks, {:.1f} MB/s",
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
                return std::unexpected(Error{ErrorCode::kInvalidFormat, 
                                 "Input file is not paired-end data"});
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
                    return std::unexpected(blockResult.error());
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
                    return std::unexpected(decompressResult.error());
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
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            running_.store(false);
            return std::unexpected(Error{ErrorCode::kInternalError, fmt::format("Pipeline error: {}", e.what())});
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count());

        running_.store(false);

        if (cancelled_.load()) {
            return std::unexpected(Error{ErrorCode::kCancelled, "Pipeline cancelled"});
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
            return makeVoidError(ErrorCode::kInvalidState, "Cannot change config while running");
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
