// =============================================================================
// fq-compressor - Pipeline Node Shared Implementation
// =============================================================================
// Contains configuration validation and BackpressureController.
// Individual node implementations are in separate files:
//   reader_node.cpp, compressor_node.cpp, writer_node.cpp,
//   fqc_reader_node.cpp, decompressor_node.cpp, fastq_writer_node.cpp
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/pipeline/pipeline_node.h"

#include <fmt/format.h>

namespace fqc::pipeline {

// =============================================================================
// Configuration Validation
// =============================================================================

VoidResult ReaderNodeConfig::validate() const {
    if (blockSize < kMinBlockSize || blockSize > kMaxBlockSize) {
        return makeVoidError(
            ErrorCode::kInvalidArgument,
            fmt::format("Block size must be between {} and {}",
            kMinBlockSize, kMaxBlockSize));
    }
    if (bufferSize == 0) {
        return makeVoidError(ErrorCode::kInvalidArgument, "Buffer size must be > 0");
    }
    return {};
}

VoidResult CompressorNodeConfig::validate() const {
    if (compressionLevel < kMinCompressionLevel || compressionLevel > kMaxCompressionLevel) {
        return makeVoidError(
            ErrorCode::kInvalidArgument,
            fmt::format("Compression level must be between {} and {}",
            kMinCompressionLevel, kMaxCompressionLevel));
    }
    return {};
}

VoidResult WriterNodeConfig::validate() const {
    if (bufferSize == 0) {
        return makeVoidError(ErrorCode::kInvalidArgument, "Buffer size must be > 0");
    }
    return {};
}

VoidResult FQCReaderNodeConfig::validate() const {
    if (rangeStart > 0 && rangeEnd > 0 && rangeStart > rangeEnd) {
        return makeVoidError(
            ErrorCode::kInvalidArgument,
            "Range start must be <= range end");
    }
    return {};
}


VoidResult DecompressorNodeConfig::validate() const {
    // Placeholder quality must be valid ASCII
    if (placeholderQual < 33 || placeholderQual > 126) {
        return makeVoidError(
            ErrorCode::kInvalidArgument,
            "Placeholder quality must be ASCII 33-126");
    }
    return {};
}

VoidResult FASTQWriterNodeConfig::validate() const {
    if (bufferSize == 0) {
        return makeVoidError(ErrorCode::kInvalidArgument, "Buffer size must be > 0");
    }
    return {};
}

// =============================================================================
// BackpressureController Implementation
// =============================================================================

BackpressureController::BackpressureController(std::size_t maxInFlight)
    : maxInFlight_(maxInFlight) {}

void BackpressureController::acquire() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return inFlight_.load() < maxInFlight_; });
    ++inFlight_;
}

bool BackpressureController::tryAcquire() {
    std::size_t current = inFlight_.load();
    while (current < maxInFlight_) {
        if (inFlight_.compare_exchange_weak(current, current + 1)) {
            return true;
        }
    }
    return false;
}

void BackpressureController::release() {
    --inFlight_;
    cv_.notify_one();
}

std::size_t BackpressureController::inFlight() const noexcept {
    return inFlight_.load();
}

std::size_t BackpressureController::maxInFlight() const noexcept {
    return maxInFlight_;
}

void BackpressureController::reset() noexcept {
    inFlight_.store(0);
}

}  // namespace fqc::pipeline
// END OF FILE — Node implementations moved to individual files.
#if 0
// Dead code placeholder to suppress remaining lines in this translation unit.
// TODO: Delete everything below after verifying the split files compile.
class DeadCode_ {
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
            
            // Create parsers for both files
            io::ParserOptions parserOpts;
            parserOpts.bufferSize = config_.bufferSize;
            parserOpts.collectStats = true;
            
            parser_ = std::make_unique<io::FastqParser>(path1, parserOpts);
            parser2_ = std::make_unique<io::FastqParser>(path2, parserOpts);
            
            parser_->open();
            parser2_->open();
            
            // Sample from first file for estimation
            if (parser_->canSeek()) {
                auto sampleStats = parser_->sampleRecords(1000);
                estimatedTotalReads_ = estimateTotalReads(path1, sampleStats) * 2;  // PE = 2x
                detectedLengthClass_ = io::detectReadLengthClass(sampleStats);
                effectiveBlockSize_ = recommendedBlockSize(detectedLengthClass_);
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
            
            FQC_LOG_DEBUG("ReaderNode opened (paired): path1={}, path2={}, estimated_reads={}",
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
            chunk.reads.reserve(effectiveBlockSize_);
            
            std::size_t totalBases = 0;
            const std::size_t maxBlockBases = config_.maxBlockBases;
            
            if (isPaired_) {
                // Read interleaved from both files
                while (chunk.reads.size() < effectiveBlockSize_ && 
                       totalBases < maxBlockBases) {
                    auto record1 = parser_->readRecord();
                    auto record2 = parser2_->readRecord();
                    
                    if (!record1 || !record2) {
                        // Check for mismatched file lengths
                        if (record1.has_value() != record2.has_value()) {
                            FQC_LOG_WARNING("Paired-end files have different lengths");
                        }
                        break;
                    }
                    
                    // Add R1
                    ReadRecord r1;
                    r1.id = std::move(record1->id);
                    r1.sequence = std::move(record1->sequence);
                    r1.quality = std::move(record1->quality);
                    totalBases += r1.sequence.size();
                    totalBytesRead_ += r1.id.size() + r1.sequence.size() * 2 + 4;  // Approx FASTQ size
                    chunk.reads.push_back(std::move(r1));
                    
                    // Add R2
                    ReadRecord r2;
                    r2.id = std::move(record2->id);
                    r2.sequence = std::move(record2->sequence);
                    r2.quality = std::move(record2->quality);
                    totalBases += r2.sequence.size();
                    totalBytesRead_ += r2.id.size() + r2.sequence.size() * 2 + 4;
                    chunk.reads.push_back(std::move(r2));
                }
            } else {
                // Single-end reading
                while (chunk.reads.size() < effectiveBlockSize_ && 
                       totalBases < maxBlockBases) {
                    auto record = parser_->readRecord();
                    if (!record) {
                        break;
                    }
                    
                    ReadRecord r;
                    r.id = std::move(record->id);
                    r.sequence = std::move(record->sequence);
                    r.quality = std::move(record->quality);
                    totalBases += r.sequence.size();
                    totalBytesRead_ += r.id.size() + r.sequence.size() * 2 + 4;
                    chunk.reads.push_back(std::move(r));
                }
            }
            
            if (chunk.reads.empty()) {
                // EOF reached
                state_ = NodeState::kFinished;
                FQC_LOG_DEBUG("ReaderNode finished: total_reads={}, total_bytes={}",
                          totalReadsRead_, totalBytesRead_);
                return std::nullopt;
            }
            
            // Update counters
            totalReadsRead_ += chunk.reads.size();
            nextReadId_ += chunk.reads.size();
            ++chunkId_;
            
            // Check if this is the last chunk
            chunk.isLast = parser_->eof() && (!isPaired_ || parser2_->eof());
            
            if (chunk.isLast) {
                state_ = NodeState::kFinished;
            }
            
            FQC_LOG_DEBUG("ReaderNode read chunk: id={}, reads={}, is_last={}",
                      chunk.chunkId, chunk.reads.size(), chunk.isLast);
            
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
// CompressorNodeImpl
// =============================================================================

class CompressorNodeImpl {
public:
    explicit CompressorNodeImpl(CompressorNodeConfig config)
        : config_(std::move(config)) {
        initializeCompressors();
    }

    Result<CompressedBlock> compress(ReadChunk chunk) {
        state_ = NodeState::kRunning;

        try {
            CompressedBlock block;
            block.blockId = chunk.chunkId;
            block.readCount = static_cast<std::uint32_t>(chunk.reads.size());
            block.startReadId = chunk.startReadId;
            block.isLast = chunk.isLast;

            if (chunk.reads.empty()) {
                state_ = NodeState::kIdle;
                return block;
            }

            // Determine uniform read length
            bool uniformLength = true;
            std::uint32_t firstLength = static_cast<std::uint32_t>(chunk.reads[0].sequence.size());
            for (const auto& read : chunk.reads) {
                if (read.sequence.size() != firstLength) {
                    uniformLength = false;
                    break;
                }
            }
            block.uniformReadLength = uniformLength ? firstLength : 0;

            // Prepare data for compression
            std::vector<std::string_view> ids;
            std::vector<std::string_view> sequences;
            std::vector<std::string_view> qualities;
            std::vector<std::uint32_t> lengths;
            
            ids.reserve(chunk.reads.size());
            sequences.reserve(chunk.reads.size());
            qualities.reserve(chunk.reads.size());
            if (!uniformLength) {
                lengths.reserve(chunk.reads.size());
            }

            for (const auto& read : chunk.reads) {
                ids.push_back(read.id);
                sequences.push_back(read.sequence);
                qualities.push_back(read.quality);
                if (!uniformLength) {
                    lengths.push_back(static_cast<std::uint32_t>(read.sequence.size()));
                }
            }

            // Compress ID stream
            auto idResult = compressIds(ids);
            if (!idResult) {
                state_ = NodeState::kError;
                return std::unexpected(idResult.error());
            }
            block.idStream = std::move(idResult->data);
            block.codecIds = getIdCodec();

            // Compress sequence stream
            auto seqResult = compressSequences(sequences);
            if (!seqResult) {
                state_ = NodeState::kError;
                return std::unexpected(seqResult.error());
            }
            block.seqStream = std::move(*seqResult);
            block.codecSeq = getSequenceCodec();

            // Compress quality stream
            auto qualResult = compressQualities(qualities, sequences);
            if (!qualResult) {
                state_ = NodeState::kError;
                return std::unexpected(qualResult.error());
            }
            block.qualStream = std::move(qualResult->data);
            block.codecQual = getQualityCodec();

            // Compress auxiliary stream (lengths) if variable
            if (!uniformLength) {
                auto auxResult = compressLengths(lengths);
                if (!auxResult) {
                    state_ = NodeState::kError;
                    return std::unexpected(auxResult.error());
                }
                block.auxStream = std::move(*auxResult);
            }
            block.codecAux = format::encodeCodec(CodecFamily::kDeltaVarint, 0);

            // Calculate block checksum (over uncompressed logical streams)
            block.checksum = calculateBlockChecksum(ids, sequences, qualities, lengths);

            ++totalBlocksCompressed_;
            state_ = NodeState::kIdle;

            FQC_LOG_DEBUG("CompressorNode compressed block: id={}, reads={}, compressed_size={}",
                      block.blockId, block.readCount, block.totalSize());

            return block;

        } catch (const FQCException& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kCompressionFailed, fmt::format("Compression failed: {}", e.what())});
        }
    }

    NodeState state() const noexcept { return state_; }
    std::uint32_t totalBlocksCompressed() const noexcept { return totalBlocksCompressed_; }

    void reset() noexcept {
        state_ = NodeState::kIdle;
        totalBlocksCompressed_ = 0;
        if (idCompressor_) {
            idCompressor_->reset();
        }
        if (qualityCompressor_) {
            qualityCompressor_->reset();
        }
        if (blockCompressor_) {
            blockCompressor_->reset();
        }
    }

    const CompressorNodeConfig& config() const noexcept { return config_; }

private:
    void initializeCompressors() {
        // Initialize ID compressor
        algo::IDCompressorConfig idConfig;
        idConfig.idMode = config_.idMode;
        idConfig.compressionLevel = config_.compressionLevel;
        idConfig.useZstd = true;
        idConfig.zstdLevel = config_.zstdLevel;
        idCompressor_ = std::make_unique<algo::IDCompressor>(idConfig);

        // Initialize quality compressor
        algo::QualityCompressorConfig qualConfig;
        qualConfig.qualityMode = config_.qualityMode;
        // Use Order-1 for long reads (lower memory), Order-2 for short reads
        qualConfig.contextOrder = (config_.readLengthClass == ReadLengthClass::kLong)
                                      ? algo::QualityContextOrder::kOrder1
                                      : algo::QualityContextOrder::kOrder2;
        qualConfig.usePositionContext = true;
        qualityCompressor_ = std::make_unique<algo::QualityCompressor>(qualConfig);

        // Initialize block compressor
        algo::BlockCompressorConfig blockConfig;
        blockConfig.readLengthClass = config_.readLengthClass;
        blockConfig.qualityMode = config_.qualityMode;
        blockConfig.idMode = config_.idMode;
        blockConfig.compressionLevel = config_.compressionLevel;
        blockConfig.zstdLevel = config_.zstdLevel;
        blockCompressor_ = std::make_unique<algo::BlockCompressor>(blockConfig);
    }

    Result<algo::CompressedIDData> compressIds(std::span<const std::string_view> ids) {
        return idCompressor_->compress(ids);
    }

    Result<std::vector<std::uint8_t>> compressSequences(
        std::span<const std::string_view> sequences) {
        // For short reads, use BlockCompressor (ABC algorithm)
        // For medium/long reads, use Zstd directly
        if (config_.readLengthClass == ReadLengthClass::kShort) {
            // Use zero-copy ReadRecordView — only seqStream is extracted.
            // Reuse seq as quality placeholder (same length, no allocation).
            std::vector<ReadRecordView> views;
            views.reserve(sequences.size());
            for (const auto& seq : sequences) {
                views.emplace_back(std::string_view{}, seq, seq);
            }
            
            auto result = blockCompressor_->compress(
                std::span<const ReadRecordView>(views), 0);
            if (!result) {
                return std::unexpected(result.error());
            }
            return std::move(result->seqStream);
        } else {
            // Use Zstd for medium/long reads
            return compressWithZstd(sequences);
        }
    }

    Result<algo::CompressedQualityData> compressQualities(
        std::span<const std::string_view> qualities,
        std::span<const std::string_view> sequences) {
        if (config_.qualityMode == QualityMode::kDiscard) {
            // Return empty data for discard mode
            algo::CompressedQualityData result;
            result.numStrings = static_cast<std::uint32_t>(qualities.size());
            result.qualityMode = QualityMode::kDiscard;
            return result;
        }
        return qualityCompressor_->compress(qualities, sequences);
    }

    Result<std::vector<std::uint8_t>> compressLengths(std::span<const std::uint32_t> lengths) {
        // Delta + Varint encoding for lengths
        std::vector<std::int64_t> deltas;
        deltas.reserve(lengths.size());
        
        std::int64_t prev = 0;
        for (auto len : lengths) {
            deltas.push_back(static_cast<std::int64_t>(len) - prev);
            prev = static_cast<std::int64_t>(len);
        }
        
        return algo::deltaVarintEncode(deltas);
    }

    Result<std::vector<std::uint8_t>> compressWithZstd(
        std::span<const std::string_view> sequences) {
        // Concatenate sequences with length prefixes
        std::vector<std::uint8_t> buffer;
        
        // Estimate size
        std::size_t totalSize = 0;
        for (const auto& seq : sequences) {
            totalSize += seq.size() + 4;  // 4 bytes for length prefix
        }
        buffer.reserve(totalSize);
        
        // Write sequences with length prefixes
        for (const auto& seq : sequences) {
            auto len = static_cast<std::uint32_t>(seq.size());
            buffer.push_back(static_cast<std::uint8_t>(len & 0xFF));
            buffer.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
            buffer.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFF));
            buffer.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFF));
            buffer.insert(buffer.end(), seq.begin(), seq.end());
        }

        // Compress with Zstd
        if (buffer.empty()) {
            return buffer;
        }

        std::size_t const compressBound = ZSTD_compressBound(buffer.size());
        std::vector<std::uint8_t> compressed(compressBound);

        std::size_t const compressedSize = ZSTD_compress(
            compressed.data(), compressed.size(),
            buffer.data(), buffer.size(),
            3  // compression level
        );

        if (ZSTD_isError(compressedSize)) {
            throw std::runtime_error(
                fmt::format("Zstd compression failed: {}", ZSTD_getErrorName(compressedSize))
            );
        }

        compressed.resize(compressedSize);
        return compressed;
    }

    std::uint64_t calculateBlockChecksum(
        std::span<const std::string_view> ids,
        std::span<const std::string_view> sequences,
        std::span<const std::string_view> qualities,
        std::span<const std::uint32_t> lengths) {
        XXH3_state_t state;
        XXH3_64bits_reset(&state);

        for (const auto& id : ids) {
            XXH3_64bits_update(&state, id.data(), id.size());
        }
        for (const auto& seq : sequences) {
            XXH3_64bits_update(&state, seq.data(), seq.size());
        }
        for (const auto& qual : qualities) {
            XXH3_64bits_update(&state, qual.data(), qual.size());
        }
        if (!lengths.empty()) {
            XXH3_64bits_update(&state, lengths.data(),
                               lengths.size() * sizeof(std::uint32_t));
        }

        return XXH3_64bits_digest(&state);
    }

    std::uint8_t getIdCodec() const noexcept {
        return format::encodeCodec(CodecFamily::kDeltaZstd, 0);
    }

    std::uint8_t getSequenceCodec() const noexcept {
        if (config_.readLengthClass == ReadLengthClass::kShort) {
            return format::encodeCodec(CodecFamily::kAbcV1, 0);
        }
        return format::encodeCodec(CodecFamily::kZstdPlain, 0);
    }

    std::uint8_t getQualityCodec() const noexcept {
        if (config_.qualityMode == QualityMode::kDiscard) {
            return format::encodeCodec(CodecFamily::kRaw, 0);
        }
        if (config_.readLengthClass == ReadLengthClass::kLong) {
            return format::encodeCodec(CodecFamily::kScmOrder1, 0);
        }
        return format::encodeCodec(CodecFamily::kScmV1, 0);
    }

    CompressorNodeConfig config_;
    NodeState state_ = NodeState::kIdle;
    std::atomic<std::uint32_t> totalBlocksCompressed_{0};
    
    std::unique_ptr<algo::IDCompressor> idCompressor_;
    std::unique_ptr<algo::QualityCompressor> qualityCompressor_;
    std::unique_ptr<algo::BlockCompressor> blockCompressor_;
};


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
            
            // Create FQC writer with atomic write support
            writer_ = std::make_unique<format::FQCWriter>(path);
            
            // Write global header
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
            // Handle out-of-order blocks using ordered queue
            if (block.blockId != nextExpectedBlockId_) {
                // Queue for later
                pendingBlocks_[block.blockId] = std::move(block);
                return {};
            }
            
            // Write this block and any queued blocks that are now ready
            auto result = writeBlockInternal(block);
            if (!result) {
                return result;
            }
            
            // Check for queued blocks that can now be written
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
            // Check for any remaining pending blocks (shouldn't happen in normal operation)
            if (!pendingBlocks_.empty()) {
                FQC_LOG_WARNING("WriterNode finalize with {} pending blocks", pendingBlocks_.size());
                // Write remaining blocks in order
                while (!pendingBlocks_.empty()) {
                    auto it = pendingBlocks_.begin();
                    auto result = writeBlockInternal(it->second);
                    if (!result) {
                        return result;
                    }
                    pendingBlocks_.erase(it);
                }
            }
            
            // Write reorder map if provided
            if (reorderMap.has_value() && !reorderMap->empty()) {
                const auto& mapData = reorderMap.value();

                // Prepare ReorderMap header
                fqc::format::ReorderMap mapHeader;
                mapHeader.totalReads = static_cast<std::uint32_t>(mapData.size());
                mapHeader.forwardMapSize = 0;  // Will be set by writeReorderMap
                mapHeader.reverseMapSize = 0;  // Will be set by writeReorderMap

                // Create ReadId vector from byte data
                std::vector<fqc::ReadId> ids;
                ids.reserve(mapData.size() / sizeof(fqc::ReadId));
                for (std::size_t i = 0; i + sizeof(fqc::ReadId) <= mapData.size(); i += sizeof(fqc::ReadId)) {
                    fqc::ReadId id;
                    std::memcpy(&id, mapData.data() + i, sizeof(fqc::ReadId));
                    ids.push_back(id);
                }

                // Compress maps (Delta + Varint encoding)
                auto compressedForward = fqc::format::deltaEncode(std::span<const fqc::ReadId>(ids.data(), ids.size()));
                auto compressedReverse = fqc::format::deltaEncode(std::span<const fqc::ReadId>(ids.data(), ids.size()));

                // Write to archive
                writer_->writeReorderMap(mapHeader, compressedForward, compressedReverse);

                FQC_LOG_DEBUG("WriterNode: Reorder map written ({} reads)", mapHeader.totalReads);
            }
            
            // Finalize the archive (writes index and footer, renames temp to final)
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
        // Build block header
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
        
        // Calculate offsets and sizes
        header.offsetIds = 0;
        header.sizeIds = block.idStream.size();
        header.offsetSeq = header.sizeIds;
        header.sizeSeq = block.seqStream.size();
        header.offsetQual = header.offsetSeq + header.sizeSeq;
        header.sizeQual = block.qualStream.size();
        header.offsetAux = header.offsetQual + header.sizeQual;
        header.sizeAux = block.auxStream.size();
        header.compressedSize = block.totalSize();
        
        // Build payload
        format::BlockPayload payload;
        payload.idsData = block.idStream;
        payload.seqData = block.seqStream;
        payload.qualData = block.qualStream;
        payload.auxData = block.auxStream;
        
        // Write block
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
    std::map<BlockId, CompressedBlock> pendingBlocks_;  // For out-of-order handling
};

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
            
            // Copy global header
            globalHeader_ = reader_->globalHeader();
            
            // Load reorder map if present and needed
            if (reader_->hasReorderMap()) {
                reader_->loadReorderMap();
                if (reader_->reorderMap()) {
                    // Store raw reorder map data for later use
                    // Note: The actual raw bytes would need to be stored separately
                    // For now, we just note that it's available
                }
            }
            
            // Determine block range based on config
            totalBlocks_ = static_cast<std::uint32_t>(reader_->blockCount());
            
            if (config_.rangeStart > 0 || config_.rangeEnd > 0) {
                // Find blocks that contain the requested range
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
                        ++endBlockId_;  // Make it exclusive
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
            // Read block data from FQC file
            auto blockData = reader_->readBlock(currentBlockId_);
            
            // Convert to CompressedBlock
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
            
            // Get start read ID from index
            auto indexEntry = reader_->getIndexEntry(currentBlockId_);
            if (indexEntry) {
                block.startReadId = indexEntry->archiveIdStart;
            } else {
                block.startReadId = 1;
            }
            
            block.isLast = (currentBlockId_ + 1 >= endBlockId_);
            
            ++currentBlockId_;
            ++totalBlocksRead_;
            
            // Verify checksum if configured
            if (config_.verifyChecksums) {
                // Note: Checksum verification happens after decompression
                // because the checksum is over uncompressed data
            }
            
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
                // Write to stdout
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
                // Write interleaved reads to separate files
                for (std::size_t i = 0; i < chunk.reads.size(); ++i) {
                    const auto& record = chunk.reads[i];
                    std::string fastqRecord = formatFastqRecord(record);
                    
                    if (i % 2 == 0) {
                        // R1
                        stream1_.write(fastqRecord.data(), 
                                       static_cast<std::streamsize>(fastqRecord.size()));
                    } else {
                        // R2
                        stream2_.write(fastqRecord.data(), 
                                       static_cast<std::streamsize>(fastqRecord.size()));
                    }
                    
                    totalBytesWritten_ += fastqRecord.size();
                }
            } else {
                // Write all reads to single output
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
    /// @brief Format a read record as FASTQ string
    std::string formatFastqRecord(const ReadRecord& record) const {
        std::string result;
        result.reserve(record.id.size() + record.sequence.size() + 
                       record.quality.size() + 10);
        
        result += '@';
        result += record.id;
        result += '\n';
        
        if (config_.lineWidth > 0 && record.sequence.size() > config_.lineWidth) {
            // Wrap sequence
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
            // Wrap quality
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
// BackpressureController Implementation
// =============================================================================

BackpressureController::BackpressureController(std::size_t maxInFlight)
    : maxInFlight_(maxInFlight) {}

void BackpressureController::acquire() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return inFlight_.load() < maxInFlight_; });
    ++inFlight_;
}

bool BackpressureController::tryAcquire() {
    std::size_t current = inFlight_.load();
    while (current < maxInFlight_) {
        if (inFlight_.compare_exchange_weak(current, current + 1)) {
            return true;
        }
    }
    return false;
}

void BackpressureController::release() {
    --inFlight_;
    cv_.notify_one();
}

std::size_t BackpressureController::inFlight() const noexcept {
    return inFlight_.load();
}

std::size_t BackpressureController::maxInFlight() const noexcept {
    return maxInFlight_;
}

void BackpressureController::reset() noexcept {
    inFlight_.store(0);
}


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

// =============================================================================
// CompressorNode Public Interface
// =============================================================================

CompressorNode::CompressorNode(CompressorNodeConfig config)
    : impl_(std::make_unique<CompressorNodeImpl>(std::move(config))) {}

CompressorNode::~CompressorNode() = default;

CompressorNode::CompressorNode(CompressorNode&&) noexcept = default;
CompressorNode& CompressorNode::operator=(CompressorNode&&) noexcept = default;

Result<CompressedBlock> CompressorNode::compress(ReadChunk chunk) {
    return impl_->compress(std::move(chunk));
}

NodeState CompressorNode::state() const noexcept {
    return impl_->state();
}

std::uint32_t CompressorNode::totalBlocksCompressed() const noexcept {
    return impl_->totalBlocksCompressed();
}

void CompressorNode::reset() noexcept {
    impl_->reset();
}

const CompressorNodeConfig& CompressorNode::config() const noexcept {
    return impl_->config();
}

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
#endif  // #if 0 dead code block
