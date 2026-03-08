// =============================================================================
// fq-compressor - CompressorNode Implementation
// =============================================================================
// Implements the CompressorNode (parallel processing stage) for compression.
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/algo/block_compressor.h"
#include "fqc/algo/id_compressor.h"
#include "fqc/algo/quality_compressor.h"
#include "fqc/common/logger.h"
#include "fqc/format/fqc_format.h"
#include "fqc/pipeline/pipeline_node.h"

#include <algorithm>

#include <xxhash.h>
#include <zstd.h>

#include <fmt/format.h>

namespace fqc::pipeline {

// =============================================================================
// CompressorNodeImpl
// =============================================================================

class CompressorNodeImpl {
public:
    explicit CompressorNodeImpl(CompressorNodeConfig config) : config_(std::move(config)) {
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
            std::vector<std::string_view> comments;
            std::vector<std::string_view> sequences;
            std::vector<std::string_view> qualities;
            std::vector<std::uint32_t> lengths;

            ids.reserve(chunk.reads.size());
            comments.reserve(chunk.reads.size());
            sequences.reserve(chunk.reads.size());
            qualities.reserve(chunk.reads.size());
            if (!uniformLength) {
                lengths.reserve(chunk.reads.size());
            }

            for (const auto& read : chunk.reads) {
                ids.push_back(read.id);
                comments.push_back(read.comment);
                sequences.push_back(read.sequence);
                qualities.push_back(read.quality);
                if (!uniformLength) {
                    lengths.push_back(static_cast<std::uint32_t>(read.sequence.size()));
                }
            }

            // Compress ID stream (with comments, BlockCompressor-compatible format)
            auto idResult = compressIds(ids, comments);
            if (!idResult) {
                state_ = NodeState::kError;
                return std::unexpected(idResult.error());
            }
            block.idStream = std::move(*idResult);
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
            block.checksum = calculateBlockChecksum(ids, comments, sequences, qualities, lengths);

            ++totalBlocksCompressed_;
            state_ = NodeState::kIdle;

            FQC_LOG_DEBUG("CompressorNode compressed block: id={}, reads={}, compressed_size={}",
                          block.blockId,
                          block.readCount,
                          block.totalSize());

            return block;

        } catch (const FQCException& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{e.code(), e.what()});
        } catch (const std::exception& e) {
            state_ = NodeState::kError;
            return std::unexpected(Error{ErrorCode::kCompressionFailed,
                                         fmt::format("Compression failed: {}", e.what())});
        }
    }

    NodeState state() const noexcept {
        return state_;
    }
    std::uint32_t totalBlocksCompressed() const noexcept {
        return totalBlocksCompressed_;
    }

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

    const CompressorNodeConfig& config() const noexcept {
        return config_;
    }

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

    Result<std::vector<std::uint8_t>> compressIds(std::span<const std::string_view> ids,
                                                  std::span<const std::string_view> comments) {
        if (config_.idMode == IDMode::kDiscard) {
            return std::vector<std::uint8_t>{};
        }

        // Build buffer in BlockCompressor-compatible format:
        // [uint32 idLen][id bytes][uint32 commentLen][comment bytes] per read
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
                                                   config_.zstdLevel > 0 ? config_.zstdLevel : 3);

        if (ZSTD_isError(compressedSize)) {
            return makeError<std::vector<std::uint8_t>>(
                ErrorCode::kIOError,
                "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
        }

        compressed.resize(compressedSize);
        return compressed;
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

            auto result = blockCompressor_->compress(std::span<const ReadRecordView>(views), 0);
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
        std::span<const std::string_view> qualities, std::span<const std::string_view> sequences) {
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

        const std::size_t compressBound = ZSTD_compressBound(buffer.size());
        std::vector<std::uint8_t> compressed(compressBound);

        const std::size_t compressedSize = ZSTD_compress(compressed.data(),
                                                         compressed.size(),
                                                         buffer.data(),
                                                         buffer.size(),
                                                         3  // compression level
        );

        if (ZSTD_isError(compressedSize)) {
            throw std::runtime_error(
                fmt::format("Zstd compression failed: {}", ZSTD_getErrorName(compressedSize)));
        }

        compressed.resize(compressedSize);
        return compressed;
    }

    std::uint64_t calculateBlockChecksum(std::span<const std::string_view> ids,
                                         std::span<const std::string_view> comments,
                                         std::span<const std::string_view> sequences,
                                         std::span<const std::string_view> qualities,
                                         std::span<const std::uint32_t> lengths) {
        XXH64_state_t* state = XXH64_createState();
        if (!state) {
            return 0;
        }
        XXH64_reset(state, 0);

        for (const auto& id : ids) {
            XXH64_update(state, id.data(), id.size());
        }
        for (const auto& seq : sequences) {
            XXH64_update(state, seq.data(), seq.size());
        }
        for (const auto& qual : qualities) {
            XXH64_update(state, qual.data(), qual.size());
        }
        if (!lengths.empty()) {
            XXH64_update(state, lengths.data(), lengths.size() * sizeof(std::uint32_t));
        }

        std::uint64_t checksum = XXH64_digest(state);
        XXH64_freeState(state);
        return checksum;
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

}  // namespace fqc::pipeline
