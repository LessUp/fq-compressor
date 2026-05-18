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
#include "fqc/algo/block_compressor.h"
#include "fqc/algo/id_compressor.h"
#include "fqc/algo/quality_compressor.h"
#include "fqc/common/logger.h"
#include "fqc/format/fqc_format.h"
#include "fqc/pipeline/decompressor_node.h"
#include "fqc/pipeline/fastq_writer_node.h"
#include "fqc/pipeline/fqc_reader_node.h"
#include "fqc/pipeline/reader_node.h"
#include "fqc/pipeline/writer_node.h"

#include <xxhash.h>
#include <zstd.h>

#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_pipeline.h>
#include <tbb/task_arena.h>

namespace fqc::pipeline {

// =============================================================================
// Block Compression State (Thread-Local)
// =============================================================================
// Each thread has its own compression state to avoid data races.
// This replaces the previous CompressorNode class with inline logic.

/// @brief Thread-local state for block compression
struct BlockCompressionState {
    algo::BlockCompressorConfig config;
    std::unique_ptr<algo::IDCompressor> idCompressor;
    std::unique_ptr<algo::QualityCompressor> qualityCompressor;
    std::unique_ptr<algo::BlockCompressor> blockCompressor;
    bool initialized = false;

    /// @brief Initialize compressors lazily on first use
    void initialize(const algo::BlockCompressorConfig& cfg) {
        if (initialized) {
            return;
        }
        config = cfg;

        algo::IDCompressorConfig idConfig;
        idConfig.idMode = config.idMode;
        idConfig.compressionLevel = config.compressionLevel;
        idConfig.useZstd = true;
        idConfig.zstdLevel = config.zstdLevel;
        idCompressor = std::make_unique<algo::IDCompressor>(idConfig);

        algo::QualityCompressorConfig qualConfig;
        qualConfig.qualityMode = config.qualityMode;
        qualConfig.contextOrder = (config.readLengthClass == ReadLengthClass::kLong)
            ? algo::QualityContextOrder::kOrder1
            : algo::QualityContextOrder::kOrder2;
        qualConfig.usePositionContext = true;
        qualityCompressor = std::make_unique<algo::QualityCompressor>(qualConfig);

        algo::BlockCompressorConfig blockConfig;
        blockConfig.readLengthClass = config.readLengthClass;
        blockConfig.qualityMode = QualityMode::kDiscard;
        blockConfig.idMode = IDMode::kDiscard;
        blockConfig.compressionLevel = config.compressionLevel;
        blockConfig.zstdLevel = config.zstdLevel;
        blockCompressor = std::make_unique<algo::BlockCompressor>(blockConfig);

        initialized = true;
    }

    /// @brief Compress a read chunk into a compressed block
    [[nodiscard]] Result<CompressedBlock> compress(ReadChunk chunk) {
        CompressedBlock block;
        block.blockId = chunk.chunkId;
        block.readCount = static_cast<std::uint32_t>(chunk.reads.size());
        block.startReadId = chunk.startReadId;
        block.isLast = chunk.isLast;

        if (chunk.reads.empty()) {
            return block;
        }

        const auto marshaled = marshalReadChunk(chunk);
        block.uniformReadLength = marshaled.uniformReadLength;

        // Compress ID stream
        auto idResult = compressIds(marshaled.ids, marshaled.comments);
        if (!idResult) {
            return std::unexpected(idResult.error());
        }
        block.idStream = std::move(*idResult);
        block.codecIds = getIdCodec();

        // Compress sequence stream
        auto seqResult = compressSequences(marshaled);
        if (!seqResult) {
            return std::unexpected(seqResult.error());
        }
        block.seqStream = std::move(*seqResult);
        block.codecSeq = getSequenceCodec();

        // Compress quality stream
        auto qualResult = compressQualities(marshaled.qualities, marshaled.sequences);
        if (!qualResult) {
            return std::unexpected(qualResult.error());
        }
        block.qualStream = std::move(qualResult->data);
        block.codecQual = getQualityCodec();

        // Compress auxiliary stream (lengths) if variable
        if (!marshaled.lengths.empty()) {
            auto auxResult = compressLengths(marshaled.lengths);
            if (!auxResult) {
                return std::unexpected(auxResult.error());
            }
            block.auxStream = std::move(*auxResult);
        }
        block.codecAux = format::encodeCodec(CodecFamily::kDeltaVarint, 0);

        // Calculate block checksum
        block.checksum = calculateBlockChecksum(marshaled.ids,
                                                marshaled.comments,
                                                marshaled.sequences,
                                                marshaled.qualities,
                                                marshaled.lengths);

        return block;
    }

    /// @brief Reset state for reuse
    void reset() {
        if (idCompressor) {
            idCompressor->reset();
        }
        if (qualityCompressor) {
            qualityCompressor->reset();
        }
        if (blockCompressor) {
            blockCompressor->reset();
        }
    }

private:
    /// @brief Compress IDs and comments
    [[nodiscard]] Result<std::vector<std::uint8_t>> compressIds(
        std::span<const std::string_view> ids, std::span<const std::string_view> comments) {
        if (config.idMode == IDMode::kDiscard) {
            return std::vector<std::uint8_t>{};
        }

        // Build buffer in BlockCompressor-compatible format
        std::vector<std::uint8_t> buffer;
        buffer.reserve(ids.size() * 50);

        for (std::size_t i = 0; i < ids.size(); ++i) {
            std::uint32_t idLen = static_cast<std::uint32_t>(ids[i].length());
            buffer.insert(buffer.end(),
                          reinterpret_cast<const std::uint8_t*>(&idLen),
                          reinterpret_cast<const std::uint8_t*>(&idLen) + sizeof(idLen));
            buffer.insert(buffer.end(), ids[i].begin(), ids[i].end());

            std::uint32_t commentLen =
                (i < comments.size()) ? static_cast<std::uint32_t>(comments[i].length()) : 0;
            buffer.insert(buffer.end(),
                          reinterpret_cast<const std::uint8_t*>(&commentLen),
                          reinterpret_cast<const std::uint8_t*>(&commentLen) + sizeof(commentLen));
            if (commentLen > 0) {
                buffer.insert(buffer.end(), comments[i].begin(), comments[i].end());
            }
        }

        // Compress with Zstd
        std::size_t compressBound = ZSTD_compressBound(buffer.size());
        std::vector<std::uint8_t> compressed(compressBound);

        std::size_t compressedSize = ZSTD_compress(compressed.data(),
                                                   compressed.size(),
                                                   buffer.data(),
                                                   buffer.size(),
                                                   config.zstdLevel > 0 ? config.zstdLevel : 3);

        if (ZSTD_isError(compressedSize)) {
            return makeError<std::vector<std::uint8_t>>(
                ErrorCode::kIOError,
                "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
        }

        compressed.resize(compressedSize);
        return compressed;
    }

    /// @brief Compress sequences using appropriate algorithm
    [[nodiscard]] Result<std::vector<std::uint8_t>> compressSequences(
        const MarshaledReadChunk& marshaled) {
        if (config.readLengthClass == ReadLengthClass::kShort) {
            // Use BlockCompressor (ABC algorithm) for short reads
            auto result =
                blockCompressor->compress(std::span<const ReadRecordView>(marshaled.readViews), 0);
            if (!result) {
                return std::unexpected(result.error());
            }
            return std::move(result->seqStream);
        } else {
            // Use Zstd for medium/long reads
            return compressWithZstd(marshaled.sequences);
        }
    }

    /// @brief Compress quality values
    [[nodiscard]] Result<algo::CompressedQualityData> compressQualities(
        std::span<const std::string_view> qualities, std::span<const std::string_view> sequences) {
        if (config.qualityMode == QualityMode::kDiscard) {
            algo::CompressedQualityData result;
            result.numStrings = static_cast<std::uint32_t>(qualities.size());
            result.qualityMode = QualityMode::kDiscard;
            return result;
        }
        return qualityCompressor->compress(qualities, sequences);
    }

    /// @brief Compress read lengths using delta + varint encoding
    [[nodiscard]] Result<std::vector<std::uint8_t>> compressLengths(
        std::span<const std::uint32_t> lengths) {
        if (lengths.empty()) {
            return std::vector<std::uint8_t>{};
        }

        std::vector<std::uint8_t> encoded;
        encoded.reserve(lengths.size() * 4);

        std::int32_t prev = 0;
        for (auto len : lengths) {
            const std::int32_t current = static_cast<std::int32_t>(len);
            std::uint64_t zigzag = algo::zigzagEncode(static_cast<std::int64_t>(current - prev));
            prev = current;

            do {
                std::uint8_t byte = static_cast<std::uint8_t>(zigzag & 0x7F);
                zigzag >>= 7;
                if (zigzag != 0) {
                    byte |= 0x80;
                }
                encoded.push_back(byte);
            } while (zigzag != 0);
        }

        const std::size_t compressBound = ZSTD_compressBound(encoded.size());
        std::vector<std::uint8_t> compressed(compressBound);

        const std::size_t compressedSize =
            ZSTD_compress(compressed.data(),
                          compressed.size(),
                          encoded.data(),
                          encoded.size(),
                          config.zstdLevel > 0 ? config.zstdLevel : 3);

        if (ZSTD_isError(compressedSize)) {
            return makeError<std::vector<std::uint8_t>>(
                ErrorCode::kIOError,
                "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
        }

        compressed.resize(compressedSize);
        return compressed;
    }

    /// @brief Compress with Zstd directly
    [[nodiscard]] Result<std::vector<std::uint8_t>> compressWithZstd(
        std::span<const std::string_view> sequences) {
        std::vector<std::uint8_t> buffer;

        std::size_t totalSize = 0;
        for (const auto& seq : sequences) {
            totalSize += seq.size() + 4;
        }
        buffer.reserve(totalSize);

        for (const auto& seq : sequences) {
            auto len = static_cast<std::uint32_t>(seq.size());
            buffer.push_back(static_cast<std::uint8_t>(len & 0xFF));
            buffer.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
            buffer.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFF));
            buffer.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFF));
            buffer.insert(buffer.end(), seq.begin(), seq.end());
        }

        if (buffer.empty()) {
            return buffer;
        }

        const std::size_t compressBound = ZSTD_compressBound(buffer.size());
        std::vector<std::uint8_t> compressed(compressBound);

        const std::size_t compressedSize =
            ZSTD_compress(compressed.data(), compressed.size(), buffer.data(), buffer.size(), 3);

        if (ZSTD_isError(compressedSize)) {
            return makeError<std::vector<std::uint8_t>>(
                ErrorCode::kCompressionFailed,
                "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
        }

        compressed.resize(compressedSize);
        return compressed;
    }

    /// @brief Calculate block checksum (xxHash64)
    [[nodiscard]] std::uint64_t calculateBlockChecksum(std::span<const std::string_view> ids,
                                                       std::span<const std::string_view> comments,
                                                       std::span<const std::string_view> sequences,
                                                       std::span<const std::string_view> qualities,
                                                       std::span<const std::uint32_t> lengths) {
        XXH64_state_t* state = XXH64_createState();
        if (!state) {
            throw IOError("Failed to create xxHash64 state for block checksum");
        }

        struct XxHashStateGuard {
            XXH64_state_t* state;
            ~XxHashStateGuard() {
                XXH64_freeState(state);
            }
        } guard{state};

        XXH64_reset(state, 0);

        for (const auto& id : ids) {
            XXH64_update(state, id.data(), id.size());
        }
        for (const auto& comment : comments) {
            if (!comment.empty()) {
                XXH64_update(state, comment.data(), comment.size());
            }
        }
        for (const auto& seq : sequences) {
            XXH64_update(state, seq.data(), seq.size());
        }
        for (const auto& qual : qualities) {
            XXH64_update(state, qual.data(), qual.size());
        }

        if (!lengths.empty()) {
            for (const auto len : lengths) {
                XXH64_update(state, &len, sizeof(len));
            }
        } else {
            for (const auto& seq : sequences) {
                std::uint32_t len = static_cast<std::uint32_t>(seq.size());
                XXH64_update(state, &len, sizeof(len));
            }
        }

        return XXH64_digest(state);
    }

    /// @brief Get ID codec identifier
    [[nodiscard]] std::uint8_t getIdCodec() const noexcept {
        return format::encodeCodec(CodecFamily::kDeltaZstd, 0);
    }

    /// @brief Get sequence codec identifier
    [[nodiscard]] std::uint8_t getSequenceCodec() const noexcept {
        if (config.readLengthClass == ReadLengthClass::kShort) {
            return format::encodeCodec(CodecFamily::kAbcV1, 0);
        }
        return format::encodeCodec(CodecFamily::kZstdPlain, 0);
    }

    /// @brief Get quality codec identifier
    [[nodiscard]] std::uint8_t getQualityCodec() const noexcept {
        if (config.qualityMode == QualityMode::kDiscard) {
            return format::encodeCodec(CodecFamily::kRaw, 0);
        }
        if (config.readLengthClass == ReadLengthClass::kLong) {
            return format::encodeCodec(CodecFamily::kScmOrder1, 0);
        }
        return format::encodeCodec(CodecFamily::kScmV1, 0);
    }
};

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

std::size_t estimateMemoryUsage(const CompressionPipelineConfig& config,
                                std::size_t estimatedReads) {
    // Phase 1 memory: ~24 bytes/read for minimizer index + reorder map
    constexpr std::size_t kPhase1BytesPerRead = 24;

    // Phase 2 memory: ~50 bytes/read per block in flight
    constexpr std::size_t kPhase2BytesPerRead = 50;

    std::size_t phase1Memory = 0;
    if (!config.streamingMode && config.enableReorder) {
        phase1Memory = estimatedReads * kPhase1BytesPerRead;
    }

    std::size_t blockSize = config.blockSize > 0
        ? config.blockSize
        : recommendedBlockSize(config.compressorConfig.readLengthClass);
    std::size_t phase2Memory = blockSize * kPhase2BytesPerRead * config.maxInFlightBlocks;

    // Buffer memory
    std::size_t bufferMemory = config.inputBufferSize + config.outputBufferSize;

    return phase1Memory + phase2Memory + bufferMemory;
}

bool hasEnoughMemory(const CompressionPipelineConfig& config, std::size_t estimatedReads) {
    if (config.memoryLimitMB == 0) {
        return true;  // No limit
    }

    std::size_t estimated = estimateMemoryUsage(config, estimatedReads);
    std::size_t limitBytes = config.memoryLimitMB * 1024 * 1024;

    return estimated <= limitBytes;
}

MarshaledReadChunk marshalReadChunk(const ReadChunk& chunk) {
    MarshaledReadChunk marshaled;
    if (chunk.reads.empty()) {
        return marshaled;
    }

    marshaled.readViews.reserve(chunk.reads.size());
    marshaled.ids.reserve(chunk.reads.size());
    marshaled.comments.reserve(chunk.reads.size());
    marshaled.sequences.reserve(chunk.reads.size());
    marshaled.qualities.reserve(chunk.reads.size());

    const auto firstLength = static_cast<std::uint32_t>(chunk.reads.front().sequence.size());
    bool uniformLength = true;

    for (const auto& read : chunk.reads) {
        marshaled.ids.push_back(read.id);
        marshaled.comments.push_back(read.comment);
        marshaled.sequences.push_back(read.sequence);
        marshaled.qualities.push_back(read.quality);
        marshaled.readViews.emplace_back(read.id, read.comment, read.sequence, read.quality);

        if (read.sequence.size() != firstLength) {
            uniformLength = false;
        }
    }

    if (uniformLength) {
        marshaled.uniformReadLength = firstLength;
        return marshaled;
    }

    marshaled.lengths.reserve(chunk.reads.size());
    for (const auto& read : chunk.reads) {
        marshaled.lengths.push_back(static_cast<std::uint32_t>(read.sequence.size()));
    }

    return marshaled;
}

void filterChunkToReadRange(ReadChunk& chunk, ReadId rangeStart, ReadId rangeEnd) {
    if (chunk.reads.empty() || (rangeStart == 0 && rangeEnd == 0)) {
        return;
    }

    const ReadId chunkStart = chunk.startReadId;
    const ReadId chunkEnd = chunk.startReadId + static_cast<ReadId>(chunk.reads.size()) - 1;
    const ReadId effectiveStart = rangeStart > 0 ? std::max(rangeStart, chunkStart) : chunkStart;
    const ReadId effectiveEnd = rangeEnd > 0 ? std::min(rangeEnd, chunkEnd) : chunkEnd;

    if (effectiveStart > effectiveEnd) {
        chunk.reads.clear();
        return;
    }

    const auto beginOffset = static_cast<std::size_t>(effectiveStart - chunkStart);
    const auto endOffset = static_cast<std::size_t>(effectiveEnd - chunkStart + 1);

    if (beginOffset == 0 && endOffset == chunk.reads.size()) {
        return;
    }

    std::vector<ReadRecord> filteredReads;
    filteredReads.reserve(endOffset - beginOffset);
    for (std::size_t i = beginOffset; i < endOffset; ++i) {
        filteredReads.push_back(std::move(chunk.reads[i]));
    }

    chunk.reads = std::move(filteredReads);
    chunk.startReadId = effectiveStart;
}

// =============================================================================
// CompressionPipelineConfig Implementation
// =============================================================================

VoidResult CompressionPipelineConfig::validate() const {
    if (blockSize > 0 && (blockSize < kMinBlockSize || blockSize > kMaxBlockSize)) {
        return makeVoidError(
            ErrorCode::kInvalidArgument,
            fmt::format("Block size must be between {} and {}", kMinBlockSize, kMaxBlockSize));
    }

    if (auto result = compressorConfig.validate(); !result) {
        return result;
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
    return blockSize > 0 ? blockSize : recommendedBlockSize(compressorConfig.readLengthClass);
}

// =============================================================================
// DecompressionPipelineConfig Implementation
// =============================================================================

VoidResult DecompressionPipelineConfig::validate() const {
    if (rangeStart > 0 && rangeEnd > 0 && rangeStart > rangeEnd) {
        return makeVoidError(
            ErrorCode::kInvalidArgument,
            fmt::format("Range start ({}) must be <= range end ({})", rangeStart, rangeEnd));
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

    VoidResult run(const std::filesystem::path& inputPath,
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
            ReaderNodeConfig readerConfig;
            readerConfig.blockSize = config_.effectiveBlockSize();
            readerConfig.bufferSize = config_.inputBufferSize;
            readerConfig.readLengthClass = config_.compressorConfig.readLengthClass;
            readerConfig.maxBlockBases = config_.maxBlockBases;

            ReaderNode reader(readerConfig);
            auto openResult = reader.open(inputPath);
            if (!openResult) {
                running_.store(false);
                return openResult;
            }

            WriterNodeConfig writerConfig;
            writerConfig.bufferSize = config_.outputBufferSize;
            writerConfig.atomicWrite = true;

            WriterNode writer(writerConfig);

            format::GlobalHeader globalHeader;
            globalHeader.headerSize = format::GlobalHeader::kMinSize;
            globalHeader.flags = format::buildFlags(false,  // isPaired
                                                    config_.archivePreservesOrder,
                                                    config_.compressorConfig.qualityMode,
                                                    config_.compressorConfig.idMode,
                                                    config_.archiveHasReorderMap,
                                                    PELayout::kInterleaved,
                                                    config_.compressorConfig.readLengthClass,
                                                    config_.streamingMode);
            globalHeader.compressionAlgo = static_cast<std::uint8_t>(
                config_.compressorConfig.readLengthClass == ReadLengthClass::kShort
                    ? CodecFamily::kAbcV1
                    : CodecFamily::kZstdPlain);
            globalHeader.checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);
            globalHeader.totalReadCount = reader.estimatedTotalReads();

            auto writerOpenResult =
                writer.open(outputPath, globalHeader, inputPath.filename().string());
            if (!writerOpenResult) {
                running_.store(false);
                return writerOpenResult;
            }

            // Thread-local compression state (one per TBB worker thread)
            tbb::enumerable_thread_specific<BlockCompressionState> threadLocalState;
            const auto& compressorConfig = config_.compressorConfig;

            // Progress tracking
            std::atomic<std::uint64_t> readsProcessed{0};
            std::atomic<std::uint64_t> basesProcessed{0};
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
                        }) &

                        // Stage 2: Compressor (parallel)
                        tbb::make_filter<std::optional<ReadChunk>, std::optional<CompressedBlock>>(
                            tbb::filter_mode::parallel,
                            [&](std::optional<ReadChunk> chunkOpt)
                                -> std::optional<CompressedBlock> {
                                if (!chunkOpt.has_value()) {
                                    return std::nullopt;
                                }

                                auto& chunk = *chunkOpt;
                                std::size_t readCount = chunk.reads.size();
                                std::uint64_t baseCount = 0;
                                for (const auto& read : chunk.reads) {
                                    baseCount += read.sequence.size();
                                }

                                // Get thread-local compression state
                                auto& state = threadLocalState.local();
                                state.initialize(compressorConfig);

                                auto compressResult = state.compress(std::move(chunk));
                                if (!compressResult) {
                                    std::lock_guard<std::mutex> lock(errorMutex);
                                    pipelineError = compressResult.error();
                                    readerError.store(true);  // Signal to stop reader
                                    return std::nullopt;
                                }

                                // Update progress atomically
                                readsProcessed.fetch_add(readCount);
                                basesProcessed.fetch_add(baseCount);

                                return std::move(*compressResult);
                            }) &

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
                                    auto elapsed =
                                        std::chrono::duration_cast<std::chrono::milliseconds>(
                                            now - lastProgressTime)
                                            .count();

                                    if (elapsed >= config_.progressIntervalMs) {
                                        ProgressInfo info;
                                        info.readsProcessed = readsProcessed.load();
                                        info.totalReads = reader.estimatedTotalReads();
                                        info.bytesProcessed = reader.totalBytesRead();
                                        info.currentBlock = blocksProcessed.load();
                                        info.elapsedMs = static_cast<uint64_t>(
                                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                                now - startTime)
                                                .count());

                                        if (!config_.progressCallback(info)) {
                                            cancelled_.store(true);
                                        }

                                        lastProgressTime = now;
                                    }
                                }
                            }));
            });

            // Check for pipeline errors
            if (readerError.load() || writerError.load()) {
                running_.store(false);
                return std::unexpected(*pipelineError);
            }

            // Finalize
            if (!cancelled_.load()) {
                auto updateHeaderResult = writer.updateTotalReadCount(readsProcessed.load());
                if (!updateHeaderResult) {
                    running_.store(false);
                    return updateHeaderResult;
                }
                auto finalizeResult = writer.finalize(std::nullopt);
                if (!finalizeResult) {
                    running_.store(false);
                    return finalizeResult;
                }
            }

            // Update statistics
            stats_.totalReads = readsProcessed.load();
            stats_.totalBases = basesProcessed.load();
            stats_.totalBlocks = blocksProcessed.load();
            stats_.inputBytes =
                inputPath == "-" ? reader.totalBytesRead() : std::filesystem::file_size(inputPath);
            stats_.outputBytes = writer.totalBytesWritten();

        } catch (const FQCException& e) {
            running_.store(false);
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            running_.store(false);
            return std::unexpected(
                Error{ErrorCode::kInternalError, fmt::format("Pipeline error: {}", e.what())});
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

        running_.store(false);

        if (cancelled_.load()) {
            return std::unexpected(Error{ErrorCode::kCancelled, "Pipeline cancelled"});
        }

        FQC_LOG_INFO("Compression complete: {} reads, {} blocks, {:.2f}x ratio, {:.1f} MB/s",
                     stats_.totalReads,
                     stats_.totalBlocks,
                     stats_.compressionRatio(),
                     stats_.throughputMBps());

        return {};
    }

    VoidResult runPaired(const std::filesystem::path& input1Path,
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
            ReaderNodeConfig readerConfig;
            readerConfig.blockSize = config_.effectiveBlockSize();
            readerConfig.bufferSize = config_.inputBufferSize;
            readerConfig.readLengthClass = config_.compressorConfig.readLengthClass;

            ReaderNode reader(readerConfig);
            auto openResult = reader.openPaired(input1Path, input2Path);
            if (!openResult) {
                running_.store(false);
                return openResult;
            }

            WriterNodeConfig writerConfig;
            writerConfig.bufferSize = config_.outputBufferSize;
            writerConfig.atomicWrite = true;

            WriterNode writer(writerConfig);

            format::GlobalHeader globalHeader;
            globalHeader.headerSize = format::GlobalHeader::kMinSize;
            globalHeader.flags = format::buildFlags(true,  // isPaired
                                                    config_.archivePreservesOrder,
                                                    config_.compressorConfig.qualityMode,
                                                    config_.compressorConfig.idMode,
                                                    config_.archiveHasReorderMap,
                                                    PELayout::kInterleaved,
                                                    config_.compressorConfig.readLengthClass,
                                                    config_.streamingMode);
            globalHeader.compressionAlgo = static_cast<std::uint8_t>(
                config_.compressorConfig.readLengthClass == ReadLengthClass::kShort
                    ? CodecFamily::kAbcV1
                    : CodecFamily::kZstdPlain);
            globalHeader.checksumType = static_cast<std::uint8_t>(ChecksumType::kXxHash64);
            globalHeader.totalReadCount = reader.estimatedTotalReads();

            auto writerOpenResult =
                writer.open(outputPath, globalHeader, input1Path.filename().string());
            if (!writerOpenResult) {
                running_.store(false);
                return writerOpenResult;
            }

            // Thread-local compression state (one per TBB worker thread)
            tbb::enumerable_thread_specific<BlockCompressionState> threadLocalState;
            const auto& compressorConfig = config_.compressorConfig;

            // Progress tracking
            std::atomic<std::uint64_t> readsProcessed{0};
            std::atomic<std::uint64_t> basesProcessed{0};
            std::atomic<std::uint32_t> blocksProcessed{0};
            auto lastProgressTime = std::chrono::steady_clock::now();

            // =================================================================
            // TBB Parallel Pipeline for Paired-End Compression
            // =================================================================
            std::atomic<std::uint32_t> blockId{0};
            std::atomic<bool> readerError{false};
            std::atomic<bool> writerError{false};
            std::optional<Error> pipelineError;
            std::mutex errorMutex;

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
                                fc.stop();
                                return std::nullopt;
                            }

                            auto chunk = std::move(chunkResult->value());
                            chunk.chunkId = blockId.fetch_add(1);
                            return chunk;
                        }) &

                        // Stage 2: Compressor (parallel)
                        tbb::make_filter<std::optional<ReadChunk>, std::optional<CompressedBlock>>(
                            tbb::filter_mode::parallel,
                            [&](std::optional<ReadChunk> chunkOpt)
                                -> std::optional<CompressedBlock> {
                                if (!chunkOpt.has_value()) {
                                    return std::nullopt;
                                }

                                auto& chunk = *chunkOpt;
                                std::size_t readCount = chunk.reads.size();
                                std::uint64_t baseCount = 0;
                                for (const auto& read : chunk.reads) {
                                    baseCount += read.sequence.size();
                                }

                                // Get thread-local compression state
                                auto& state = threadLocalState.local();
                                state.initialize(compressorConfig);

                                auto compressResult = state.compress(std::move(chunk));
                                if (!compressResult) {
                                    std::lock_guard<std::mutex> lock(errorMutex);
                                    pipelineError = compressResult.error();
                                    readerError.store(true);
                                    return std::nullopt;
                                }

                                readsProcessed.fetch_add(readCount);
                                basesProcessed.fetch_add(baseCount);

                                return std::move(*compressResult);
                            }) &

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
                                    auto elapsed =
                                        std::chrono::duration_cast<std::chrono::milliseconds>(
                                            now - lastProgressTime)
                                            .count();

                                    if (elapsed >= config_.progressIntervalMs) {
                                        ProgressInfo info;
                                        info.readsProcessed = readsProcessed.load();
                                        info.totalReads = reader.estimatedTotalReads();
                                        info.bytesProcessed = reader.totalBytesRead();
                                        info.currentBlock = blocksProcessed.load();
                                        info.elapsedMs = static_cast<uint64_t>(
                                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                                now - startTime)
                                                .count());

                                        if (!config_.progressCallback(info)) {
                                            cancelled_.store(true);
                                        }

                                        lastProgressTime = now;
                                    }
                                }
                            }));
            });

            // Check for pipeline errors
            if (readerError.load() || writerError.load()) {
                running_.store(false);
                return std::unexpected(*pipelineError);
            }

            // Finalize
            if (!cancelled_.load()) {
                auto updateHeaderResult = writer.updateTotalReadCount(readsProcessed.load());
                if (!updateHeaderResult) {
                    running_.store(false);
                    return updateHeaderResult;
                }
                auto finalizeResult = writer.finalize(std::nullopt);
                if (!finalizeResult) {
                    running_.store(false);
                    return finalizeResult;
                }
            }

            // Update statistics
            stats_.totalReads = readsProcessed.load();
            stats_.totalBases = basesProcessed.load();
            stats_.totalBlocks = blocksProcessed.load();
            if (input1Path == "-" || input2Path == "-") {
                stats_.inputBytes = reader.totalBytesRead();
            } else {
                stats_.inputBytes =
                    std::filesystem::file_size(input1Path) + std::filesystem::file_size(input2Path);
            }
            stats_.outputBytes = writer.totalBytesWritten();

        } catch (const FQCException& e) {
            running_.store(false);
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            running_.store(false);
            return std::unexpected(
                Error{ErrorCode::kInternalError, fmt::format("Pipeline error: {}", e.what())});
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

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

    VoidResult run(const std::filesystem::path& inputPath,
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
            decompressorConfig.verifyChecksums = config_.verifyChecksums;
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
            if (config_.originalOrder &&
                reader.globalHeader().flags & format::flags::kHasReorderMap) {
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
            std::atomic<std::uint64_t> basesProcessed{0};
            std::atomic<std::uint32_t> blocksProcessed{0};
            std::atomic<bool> readerError{false};
            std::atomic<bool> writerError{false};
            std::optional<Error> pipelineError;
            std::mutex errorMutex;

            // Store global header reference for decompressor
            const auto& globalHeader = reader.globalHeader();

            // Create one decompressor per in-flight slot to prevent data races.
            std::vector<std::unique_ptr<DecompressorNode>> decompressors;
            for (std::size_t i = 0; i < config_.maxInFlightBlocks; ++i) {
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
                        }) &

                        // Stage 2: Decompressor (parallel)
                        tbb::make_filter<std::optional<CompressedBlock>, std::optional<ReadChunk>>(
                            tbb::filter_mode::parallel,
                            [&](std::optional<CompressedBlock> blockOpt)
                                -> std::optional<ReadChunk> {
                                if (!blockOpt.has_value()) {
                                    return std::nullopt;
                                }

                                // Select decompressor using round-robin
                                std::size_t idx =
                                    decompressorIndex.fetch_add(1) % decompressors.size();

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

                                filterChunkToReadRange(
                                    *decompressResult, config_.rangeStart, config_.rangeEnd);
                                if (decompressResult->reads.empty()) {
                                    return std::nullopt;
                                }

                                return std::move(*decompressResult);
                            }) &

                        // Stage 3: FASTQ Writer (serial, in-order)
                        tbb::make_filter<std::optional<ReadChunk>, void>(
                            tbb::filter_mode::serial_in_order,
                            [&](std::optional<ReadChunk> chunkOpt) {
                                if (!chunkOpt.has_value()) {
                                    return;
                                }

                                std::size_t readCount = chunkOpt->reads.size();
                                std::uint64_t baseCount = 0;
                                for (const auto& read : chunkOpt->reads) {
                                    baseCount += read.sequence.size();
                                }

                                auto writeResult = writer.writeChunk(std::move(*chunkOpt));
                                if (!writeResult) {
                                    std::lock_guard<std::mutex> lock(errorMutex);
                                    pipelineError = writeResult.error();
                                    writerError.store(true);
                                    return;
                                }

                                // Update statistics atomically
                                readsProcessed.fetch_add(readCount);
                                basesProcessed.fetch_add(baseCount);
                                blocksProcessed.fetch_add(1);

                                // Report progress
                                if (config_.progressCallback) {
                                    auto now = std::chrono::steady_clock::now();
                                    auto elapsed =
                                        std::chrono::duration_cast<std::chrono::milliseconds>(
                                            now - lastProgressTime)
                                            .count();

                                    if (elapsed >= config_.progressIntervalMs) {
                                        ProgressInfo info;
                                        info.readsProcessed = readsProcessed.load();
                                        info.totalReads = globalHeader.totalReadCount;
                                        info.currentBlock = blocksProcessed.load();
                                        info.elapsedMs = static_cast<uint64_t>(
                                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                                now - startTime)
                                                .count());

                                        if (!config_.progressCallback(info)) {
                                            cancelled_.store(true);
                                        }

                                        lastProgressTime = now;
                                    }
                                }
                            }));
            });

            // Check for pipeline errors
            if (readerError.load() || writerError.load()) {
                running_.store(false);
                return std::unexpected(*pipelineError);
            }

            // Update final statistics
            stats_.totalReads = readsProcessed.load();
            stats_.totalBases = basesProcessed.load();
            stats_.totalBlocks = blocksProcessed.load();
            stats_.inputBytes = std::filesystem::file_size(inputPath);

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
            return std::unexpected(
                Error{ErrorCode::kInternalError, fmt::format("Pipeline error: {}", e.what())});
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

        running_.store(false);

        if (cancelled_.load()) {
            return std::unexpected(Error{ErrorCode::kCancelled, "Pipeline cancelled"});
        }

        FQC_LOG_INFO("Decompression complete: {} reads, {} blocks, {:.1f} MB/s",
                     stats_.totalReads,
                     stats_.totalBlocks,
                     stats_.throughputMBps());

        return {};
    }

    VoidResult runPaired(const std::filesystem::path& inputPath,
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
                return std::unexpected(
                    Error{ErrorCode::kInvalidFormat, "Input file is not paired-end data"});
            }

            // Initialize decompressor
            DecompressorNodeConfig decompressorConfig;
            decompressorConfig.skipCorrupted = config_.skipCorrupted;
            decompressorConfig.verifyChecksums = config_.verifyChecksums;
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

                auto decompressResult =
                    decompressor.decompress(std::move(compressedBlock), reader.globalHeader());
                if (!decompressResult) {
                    if (config_.skipCorrupted) {
                        continue;
                    }
                    running_.store(false);
                    return std::unexpected(decompressResult.error());
                }

                filterChunkToReadRange(*decompressResult, config_.rangeStart, config_.rangeEnd);
                if (decompressResult->reads.empty()) {
                    continue;
                }

                const auto readCount = decompressResult->reads.size();
                std::uint64_t baseCount = 0;
                for (const auto& read : decompressResult->reads) {
                    baseCount += read.sequence.size();
                }

                auto writeResult = writer.writeChunk(std::move(*decompressResult));
                if (!writeResult) {
                    running_.store(false);
                    return writeResult;
                }

                stats_.totalReads += readCount;
                stats_.totalBases += baseCount;
                ++stats_.totalBlocks;
            }

            if (!cancelled_.load()) {
                auto flushResult = writer.flush();
                if (!flushResult) {
                    running_.store(false);
                    return flushResult;
                }
            }

            stats_.inputBytes = std::filesystem::file_size(inputPath);
            stats_.outputBytes = writer.totalBytesWritten();

        } catch (const FQCException& e) {
            running_.store(false);
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            running_.store(false);
            return std::unexpected(
                Error{ErrorCode::kInternalError, fmt::format("Pipeline error: {}", e.what())});
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.processingTimeMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

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

VoidResult CompressionPipeline::run(const std::filesystem::path& inputPath,
                                    const std::filesystem::path& outputPath) {
    return impl_->run(inputPath, outputPath);
}

VoidResult CompressionPipeline::runPaired(const std::filesystem::path& input1Path,
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

VoidResult DecompressionPipeline::run(const std::filesystem::path& inputPath,
                                      const std::filesystem::path& outputPath) {
    return impl_->run(inputPath, outputPath);
}

VoidResult DecompressionPipeline::runPaired(const std::filesystem::path& inputPath,
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
