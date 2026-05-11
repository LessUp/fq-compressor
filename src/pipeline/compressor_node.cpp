// =============================================================================
// fq-compressor - CompressorNode Implementation
// =============================================================================
// Implements the CompressorNode (parallel processing stage) for compression.
//
// Requirements: 4.1 (Parallel processing)
// =============================================================================

#include "fqc/pipeline/compressor_node.h"

#include "fqc/algo/block_compressor.h"
#include "fqc/algo/id_compressor.h"
#include "fqc/algo/quality_compressor.h"
#include "fqc/common/logger.h"
#include "fqc/format/fqc_format.h"

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
    explicit CompressorNodeImpl(algo::BlockCompressorConfig config) : config_(std::move(config)) {}

    Result<CompressedBlock> compress(ReadChunk chunk) {
        state_ = NodeState::kRunning;

        try {
            initializeCompressors();

            CompressedBlock block;
            block.blockId = chunk.chunkId;
            block.readCount = static_cast<std::uint32_t>(chunk.reads.size());
            block.startReadId = chunk.startReadId;
            block.isLast = chunk.isLast;

            if (chunk.reads.empty()) {
                state_ = NodeState::kIdle;
                return block;
            }

            const auto marshaled = marshalReadChunk(chunk);
            block.uniformReadLength = marshaled.uniformReadLength;

            // Compress ID stream (with comments, BlockCompressor-compatible format)
            auto idResult = compressIds(marshaled.ids, marshaled.comments);
            if (!idResult) {
                state_ = NodeState::kError;
                return std::unexpected(idResult.error());
            }
            block.idStream = std::move(*idResult);
            block.codecIds = getIdCodec();

            // Compress sequence stream
            auto seqResult = compressSequences(marshaled);
            if (!seqResult) {
                state_ = NodeState::kError;
                return std::unexpected(seqResult.error());
            }
            block.seqStream = std::move(*seqResult);
            block.codecSeq = getSequenceCodec();

            // Compress quality stream
            auto qualResult = compressQualities(marshaled.qualities, marshaled.sequences);
            if (!qualResult) {
                state_ = NodeState::kError;
                return std::unexpected(qualResult.error());
            }
            block.qualStream = std::move(qualResult->data);
            block.codecQual = getQualityCodec();

            // Compress auxiliary stream (lengths) if variable
            if (!marshaled.lengths.empty()) {
                auto auxResult = compressLengths(marshaled.lengths);
                if (!auxResult) {
                    state_ = NodeState::kError;
                    return std::unexpected(auxResult.error());
                }
                block.auxStream = std::move(*auxResult);
            }
            block.codecAux = format::encodeCodec(CodecFamily::kDeltaVarint, 0);

            // Calculate block checksum (over uncompressed logical streams)
            // 校验对象：ID || Comments || Seq || Qual || Aux (未压缩的逻辑数据流)
            // 用于验证解压后数据的完整性，而非压缩数据的完整性
            // 注意：CompressorNode 的校验和计算方式需要与 BlockCompressorImpl 保持一致
            block.checksum = calculateBlockChecksum(marshaled.ids,
                                                    marshaled.comments,
                                                    marshaled.sequences,
                                                    marshaled.qualities,
                                                    marshaled.lengths);

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

    const algo::BlockCompressorConfig& config() const noexcept {
        return config_;
    }

private:
    void initializeCompressors() {
        if (idCompressor_ && qualityCompressor_ && blockCompressor_) {
            return;
        }

        algo::IDCompressorConfig idConfig;
        idConfig.idMode = config_.idMode;
        idConfig.compressionLevel = config_.compressionLevel;
        idConfig.useZstd = true;
        idConfig.zstdLevel = config_.zstdLevel;
        idCompressor_ = std::make_unique<algo::IDCompressor>(idConfig);

        algo::QualityCompressorConfig qualConfig;
        qualConfig.qualityMode = config_.qualityMode;
        qualConfig.contextOrder = (config_.readLengthClass == ReadLengthClass::kLong)
            ? algo::QualityContextOrder::kOrder1
            : algo::QualityContextOrder::kOrder2;
        qualConfig.usePositionContext = true;
        qualityCompressor_ = std::make_unique<algo::QualityCompressor>(qualConfig);

        algo::BlockCompressorConfig blockConfig;
        blockConfig.readLengthClass = config_.readLengthClass;
        blockConfig.qualityMode = QualityMode::kDiscard;
        blockConfig.idMode = IDMode::kDiscard;
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

    Result<std::vector<std::uint8_t>> compressSequences(const MarshaledReadChunk& marshaled) {
        // For short reads, use BlockCompressor (ABC algorithm)
        // For medium/long reads, use Zstd directly
        if (config_.readLengthClass == ReadLengthClass::kShort) {
            auto result =
                blockCompressor_->compress(std::span<const ReadRecordView>(marshaled.readViews), 0);
            if (!result) {
                return std::unexpected(result.error());
            }
            return std::move(result->seqStream);
        } else {
            // Use Zstd for medium/long reads
            return compressWithZstd(marshaled.sequences);
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
        if (lengths.empty()) {
            return std::vector<std::uint8_t>{};
        }

        // Delta + Varint encoding for lengths
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
                          config_.zstdLevel > 0 ? config_.zstdLevel : 3);

        if (ZSTD_isError(compressedSize)) {
            return makeError<std::vector<std::uint8_t>>(
                ErrorCode::kIOError,
                "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
        }

        compressed.resize(compressedSize);
        return compressed;
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
            throw FQCException(
                ErrorCode::kCompressionFailed,
                fmt::format("Zstd compression failed: {}", ZSTD_getErrorName(compressedSize)));
        }

        compressed.resize(compressedSize);
        return compressed;
    }

    /// @brief 计算块校验和 (xxHash64)
    ///
    /// 校验对象：逻辑未压缩数据流，按以下顺序计算：
    ///   1. ID 流 - 所有 read 的 ID 顺序拼接
    ///   2. Comment 流 - 所有 read 的注释顺序拼接（非空时）
    ///   3. Sequence 流 - 所有 read 的序列顺序拼接
    ///   4. Quality 流 - 所有 read 的质量值顺序拼接
    ///   5. Auxiliary 流 - 每个 read 的长度信息（逐个 uint32_t 添加，总是计算）
    ///
    /// 与 BlockCompressorImpl::computeBlockChecksum 保持一致
    std::uint64_t calculateBlockChecksum(std::span<const std::string_view> ids,
                                         std::span<const std::string_view> comments,
                                         std::span<const std::string_view> sequences,
                                         std::span<const std::string_view> qualities,
                                         std::span<const std::uint32_t> lengths) {
        XXH64_state_t* state = XXH64_createState();
        if (!state) {
            throw IOError("Failed to create xxHash64 state for block checksum");
        }

        // Use RAII guard to ensure state is freed
        struct XxHashStateGuard {
            XXH64_state_t* state;
            ~XxHashStateGuard() {
                XXH64_freeState(state);
            }
        } guard{state};

        XXH64_reset(state, 0);

        // Hash IDs
        for (const auto& id : ids) {
            XXH64_update(state, id.data(), id.size());
        }

        // Hash comments
        for (const auto& comment : comments) {
            if (!comment.empty()) {
                XXH64_update(state, comment.data(), comment.size());
            }
        }

        // Hash sequences
        for (const auto& seq : sequences) {
            XXH64_update(state, seq.data(), seq.size());
        }

        // Hash qualities
        for (const auto& qual : qualities) {
            XXH64_update(state, qual.data(), qual.size());
        }

        // Hash lengths (aux) - 与 BlockCompressorImpl 一致，总是计算所有长度
        // 注意：lengths 可能为空（均匀长度时），此时使用 sequences 的长度
        if (!lengths.empty()) {
            for (const auto len : lengths) {
                XXH64_update(state, &len, sizeof(len));
            }
        } else {
            // 均匀长度时，从 sequences 推断每个 read 的长度
            for (const auto& seq : sequences) {
                std::uint32_t len = static_cast<std::uint32_t>(seq.size());
                XXH64_update(state, &len, sizeof(len));
            }
        }

        return XXH64_digest(state);
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

    algo::BlockCompressorConfig config_;
    NodeState state_ = NodeState::kIdle;
    std::atomic<std::uint32_t> totalBlocksCompressed_{0};

    std::unique_ptr<algo::IDCompressor> idCompressor_;
    std::unique_ptr<algo::QualityCompressor> qualityCompressor_;
    std::unique_ptr<algo::BlockCompressor> blockCompressor_;
};

// =============================================================================
// CompressorNode Public Interface
// =============================================================================

CompressorNode::CompressorNode(algo::BlockCompressorConfig config)
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

const algo::BlockCompressorConfig& CompressorNode::config() const noexcept {
    return impl_->config();
}

}  // namespace fqc::pipeline
