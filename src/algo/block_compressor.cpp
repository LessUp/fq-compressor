// =============================================================================
// fq-compressor - Block Compressor Implementation
// =============================================================================
// Implements Phase 2 of the two-phase compression strategy.
//
// Requirements: 1.1.2, 2.1
// =============================================================================

#include "fqc/algo/block_compressor.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <numeric>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <xxhash.h>
#include <zstd.h>

#include "fqc/algo/quality_compressor.h"
#include "fqc/common/logger.h"

namespace fqc::algo {

// =============================================================================
// Constants for DNA Encoding
// =============================================================================

namespace {

/// @brief DNA base to index lookup table (A=0, C=1, G=2, T=3)
constexpr std::uint8_t kBaseToIndex[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-15
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 16-31
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 32-47
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 48-63
    0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0,  // 64-79  (A=65, C=67, G=71)
    0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 80-95  (T=84)
    0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0,  // 96-111 (a=97, c=99, g=103)
    0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 112-127 (t=116)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 128-255
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/// @brief Index to DNA base lookup table
constexpr char kIndexToBase[4] = {'A', 'C', 'G', 'T'};

/// @brief Complement lookup table
constexpr char kComplement[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   'T', 0,   'G', 0,   0,   0,   'C', 0,   0,   0,   0,   0,   0,   'N', 0,
    0,   0,   0,   0,   'A', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   't', 0,   'g', 0,   0,   0,   'c', 0,   0,   0,   0,   0,   0,   'n', 0,
    0,   0,   0,   0,   'a', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};


/// @brief Noise encoding table: enc_noise[ref_base][read_base]
/// Based on Minoche et al. substitution statistics
/// Returns '0'-'3' for the encoded substitution
constexpr char kNoiseEncode[128][128] = {
    // Initialize all to 0
};

/// @brief Get noise encoding for a substitution
char getNoiseEncode(char refBase, char readBase) noexcept {
    // Based on Spring's noise encoding (Minoche et al. substitution statistics)
    switch (refBase) {
        case 'A': case 'a':
            switch (readBase) {
                case 'C': case 'c': return '0';
                case 'G': case 'g': return '1';
                case 'T': case 't': return '2';
                case 'N': case 'n': return '3';
                default: return '0';
            }
        case 'C': case 'c':
            switch (readBase) {
                case 'A': case 'a': return '0';
                case 'G': case 'g': return '1';
                case 'T': case 't': return '2';
                case 'N': case 'n': return '3';
                default: return '0';
            }
        case 'G': case 'g':
            switch (readBase) {
                case 'T': case 't': return '0';
                case 'A': case 'a': return '1';
                case 'C': case 'c': return '2';
                case 'N': case 'n': return '3';
                default: return '0';
            }
        case 'T': case 't':
            switch (readBase) {
                case 'G': case 'g': return '0';
                case 'C': case 'c': return '1';
                case 'A': case 'a': return '2';
                case 'N': case 'n': return '3';
                default: return '0';
            }
        case 'N': case 'n':
            switch (readBase) {
                case 'A': case 'a': return '0';
                case 'G': case 'g': return '1';
                case 'C': case 'c': return '2';
                case 'T': case 't': return '3';
                default: return '0';
            }
        default:
            return '0';
    }
}

/// @brief Decode noise character back to read base
char getNoiseDecode(char refBase, char noiseChar) noexcept {
    int idx = noiseChar - '0';
    if (idx < 0 || idx > 3) idx = 0;

    switch (refBase) {
        case 'A': case 'a': {
            constexpr char decode[] = {'C', 'G', 'T', 'N'};
            return decode[idx];
        }
        case 'C': case 'c': {
            constexpr char decode[] = {'A', 'G', 'T', 'N'};
            return decode[idx];
        }
        case 'G': case 'g': {
            constexpr char decode[] = {'T', 'A', 'C', 'N'};
            return decode[idx];
        }
        case 'T': case 't': {
            constexpr char decode[] = {'G', 'C', 'A', 'N'};
            return decode[idx];
        }
        case 'N': case 'n': {
            constexpr char decode[] = {'A', 'G', 'C', 'T'};
            return decode[idx];
        }
        default:
            return 'N';
    }
}

}  // namespace


// =============================================================================
// Utility Functions Implementation
// =============================================================================

inline std::string reverseComplement(std::string_view sequence) {
    std::string result;
    result.reserve(sequence.length());
    for (auto it = sequence.rbegin(); it != sequence.rend(); ++it) {
        char c = kComplement[static_cast<unsigned char>(*it)];
        result.push_back(c ? c : 'N');
    }
    return result;
}

char encodeNoise(char refBase, char readBase) noexcept {
    return getNoiseEncode(refBase, readBase);
}

char decodeNoise(char refBase, char noiseChar) noexcept {
    return getNoiseDecode(refBase, noiseChar);
}

std::size_t hammingDistance(std::string_view seq1, std::string_view seq2,
                            std::size_t maxDistance) {
    std::size_t minLen = std::min(seq1.length(), seq2.length());
    std::size_t distance = 0;

    for (std::size_t i = 0; i < minLen; ++i) {
        if (seq1[i] != seq2[i]) {
            ++distance;
            if (distance > maxDistance) {
                return maxDistance + 1;
            }
        }
    }

    // Add length difference to distance
    distance += std::max(seq1.length(), seq2.length()) - minLen;
    return distance;
}

std::optional<std::pair<int, bool>> findBestAlignment(
    std::string_view read,
    std::string_view reference,
    std::size_t maxShift,
    std::size_t hammingThreshold) {

    std::size_t bestDistance = SIZE_MAX;
    int bestShift = 0;
    bool bestIsRC = false;

    // Try forward orientation
    for (int shift = -static_cast<int>(maxShift);
         shift <= static_cast<int>(maxShift); ++shift) {

        std::size_t refStart = (shift >= 0) ? static_cast<std::size_t>(shift) : 0;
        std::size_t readStart = (shift < 0) ? static_cast<std::size_t>(-shift) : 0;

        if (refStart >= reference.length() || readStart >= read.length()) {
            continue;
        }

        std::size_t compareLen = std::min(reference.length() - refStart,
                                          read.length() - readStart);

        std::size_t dist = hammingDistance(
            reference.substr(refStart, compareLen),
            read.substr(readStart, compareLen),
            hammingThreshold);

        if (dist < bestDistance) {
            bestDistance = dist;
            bestShift = shift;
            bestIsRC = false;
        }
    }

    // Try reverse complement orientation
    std::string rcRead = reverseComplement(read);
    for (int shift = -static_cast<int>(maxShift);
         shift <= static_cast<int>(maxShift); ++shift) {

        std::size_t refStart = (shift >= 0) ? static_cast<std::size_t>(shift) : 0;
        std::size_t readStart = (shift < 0) ? static_cast<std::size_t>(-shift) : 0;

        if (refStart >= reference.length() || readStart >= rcRead.length()) {
            continue;
        }

        std::size_t compareLen = std::min(reference.length() - refStart,
                                          rcRead.length() - readStart);

        std::size_t dist = hammingDistance(
            reference.substr(refStart, compareLen),
            rcRead.substr(readStart, compareLen),
            hammingThreshold);

        if (dist < bestDistance) {
            bestDistance = dist;
            bestShift = shift;
            bestIsRC = true;
        }
    }

    if (bestDistance <= hammingThreshold) {
        return std::make_pair(bestShift, bestIsRC);
    }

    return std::nullopt;
}


// =============================================================================
// ConsensusSequence Implementation
// =============================================================================

void ConsensusSequence::initFromRead(std::string_view read) {
    sequence = std::string(read);
    baseCounts.resize(read.length());

    for (std::size_t i = 0; i < read.length(); ++i) {
        baseCounts[i] = {0, 0, 0, 0};
        std::uint8_t idx = kBaseToIndex[static_cast<unsigned char>(read[i])];
        baseCounts[i][idx] = 1;
    }

    contributingReads = 1;
}

void ConsensusSequence::addRead(std::string_view read, int shift, bool isReverseComplement) {
    std::string alignedRead;
    if (isReverseComplement) {
        alignedRead = reverseComplement(read);
    } else {
        alignedRead = std::string(read);
    }

    // Calculate new consensus length
    std::size_t newStart = (shift < 0) ? static_cast<std::size_t>(-shift) : 0;
    std::size_t readStart = (shift >= 0) ? static_cast<std::size_t>(shift) : 0;
    std::size_t newLen = std::max(baseCounts.size(), readStart + alignedRead.length());

    // Resize if needed
    if (newLen > baseCounts.size()) {
        std::size_t oldSize = baseCounts.size();
        baseCounts.resize(newLen);
        for (std::size_t i = oldSize; i < newLen; ++i) {
            baseCounts[i] = {0, 0, 0, 0};
        }
    }

    // Shift existing counts if needed
    if (shift < 0) {
        // Shift counts to the right
        for (std::size_t i = baseCounts.size() - 1; i >= newStart; --i) {
            if (i >= newStart) {
                baseCounts[i] = baseCounts[i - newStart];
            }
        }
        for (std::size_t i = 0; i < newStart; ++i) {
            baseCounts[i] = {0, 0, 0, 0};
        }
    }

    // Add read bases to counts
    for (std::size_t i = 0; i < alignedRead.length(); ++i) {
        std::size_t pos = readStart + i;
        if (pos < baseCounts.size()) {
            std::uint8_t idx = kBaseToIndex[static_cast<unsigned char>(alignedRead[i])];
            baseCounts[pos][idx]++;
        }
    }

    contributingReads++;
    recomputeConsensus();
}

void ConsensusSequence::recomputeConsensus() {
    sequence.resize(baseCounts.size());

    for (std::size_t i = 0; i < baseCounts.size(); ++i) {
        std::uint16_t maxCount = 0;
        std::uint8_t maxIdx = 0;

        for (std::uint8_t j = 0; j < 4; ++j) {
            if (baseCounts[i][j] > maxCount) {
                maxCount = baseCounts[i][j];
                maxIdx = j;
            }
        }

        sequence[i] = kIndexToBase[maxIdx];
    }
}


// =============================================================================
// Delta Encoding Implementation
// =============================================================================

DeltaEncodedRead computeDelta(std::string_view read, std::string_view consensus,
                               int shift, bool isRC) {
    DeltaEncodedRead delta;
    delta.positionOffset = static_cast<std::int16_t>(shift);
    delta.isReverseComplement = isRC;
    delta.readLength = static_cast<std::uint16_t>(read.length());

    std::string alignedRead;
    if (isRC) {
        alignedRead = reverseComplement(read);
    } else {
        alignedRead = std::string(read);
    }

    // Calculate alignment positions
    std::size_t consStart = (shift >= 0) ? static_cast<std::size_t>(shift) : 0;
    std::size_t readStart = (shift < 0) ? static_cast<std::size_t>(-shift) : 0;

    // Find mismatches
    for (std::size_t i = 0; i < alignedRead.length(); ++i) {
        std::size_t consPos = consStart + i - readStart;

        if (consPos >= consensus.length()) {
            // Read extends beyond consensus - treat as mismatch
            delta.mismatchPositions.push_back(static_cast<std::uint16_t>(i));
            delta.mismatchChars.push_back(alignedRead[i]);
        } else if (i >= readStart && alignedRead[i] != consensus[consPos]) {
            delta.mismatchPositions.push_back(static_cast<std::uint16_t>(i));
            delta.mismatchChars.push_back(
                getNoiseEncode(consensus[consPos], alignedRead[i]));
        }
    }

    return delta;
}

std::string reconstructFromDelta(const DeltaEncodedRead& delta,
                                  std::string_view consensus) {
    int shift = delta.positionOffset;

    // Calculate alignment positions
    std::size_t consStart = (shift >= 0) ? static_cast<std::size_t>(shift) : 0;
    std::size_t readStart = (shift < 0) ? static_cast<std::size_t>(-shift) : 0;

    // Start with consensus-aligned portion
    std::string result;
    result.resize(delta.readLength);

    for (std::size_t i = 0; i < delta.readLength; ++i) {
        std::size_t consPos = consStart + i - readStart;
        if (consPos < consensus.length() && i >= readStart) {
            result[i] = consensus[consPos];
        } else {
            result[i] = 'N';  // Default for positions outside consensus
        }
    }

    // Apply mismatches
    for (std::size_t j = 0; j < delta.mismatchPositions.size(); ++j) {
        std::uint16_t pos = delta.mismatchPositions[j];
        if (pos < result.length()) {
            std::size_t consPos = consStart + pos - readStart;
            if (consPos < consensus.length()) {
                result[pos] = getNoiseDecode(consensus[consPos], delta.mismatchChars[j]);
            } else {
                result[pos] = delta.mismatchChars[j];  // Direct character
            }
        }
    }

    // Reverse complement if needed
    if (delta.isReverseComplement) {
        return reverseComplement(result);
    }

    return result;
}


// =============================================================================
// BlockCompressorConfig Implementation
// =============================================================================

VoidResult BlockCompressorConfig::validate() const {
    if (compressionLevel < kMinCompressionLevel ||
        compressionLevel > kMaxCompressionLevel) {
        return makeVoidError(ErrorCode::kUsageError,
                             "compressionLevel must be in range [1, 9]");
    }

    if (zstdLevel < 1 || zstdLevel > 22) {
        return makeVoidError(ErrorCode::kUsageError,
                             "zstdLevel must be in range [1, 22]");
    }

    if (consensusMinReads == 0) {
        return makeVoidError(ErrorCode::kUsageError,
                             "consensusMinReads must be > 0");
    }

    return makeVoidSuccess();
}

std::uint8_t BlockCompressorConfig::getSequenceCodec() const noexcept {
    using namespace format;

    switch (readLengthClass) {
        case ReadLengthClass::kShort:
            // Use ABC for short reads
            return encodeCodec(CodecFamily::kAbcV1, 0);
        case ReadLengthClass::kMedium:
        case ReadLengthClass::kLong:
            // Use Zstd for medium/long reads
            return encodeCodec(CodecFamily::kZstdPlain, 0);
    }
    return encodeCodec(CodecFamily::kZstdPlain, 0);
}

std::uint8_t BlockCompressorConfig::getQualityCodec() const noexcept {
    using namespace format;

    if (qualityMode == QualityMode::kDiscard) {
        return encodeCodec(CodecFamily::kRaw, 0);
    }

    switch (readLengthClass) {
        case ReadLengthClass::kShort:
        case ReadLengthClass::kMedium:
            // Use SCM Order-2 for short/medium reads
            return encodeCodec(CodecFamily::kScmV1, 0);
        case ReadLengthClass::kLong:
            // Use SCM Order-1 for long reads (lower memory)
            return encodeCodec(CodecFamily::kScmOrder1, 0);
    }
    return encodeCodec(CodecFamily::kScmV1, 0);
}

std::uint8_t BlockCompressorConfig::getIdCodec() const noexcept {
    using namespace format;

    if (idMode == IDMode::kDiscard) {
        return encodeCodec(CodecFamily::kRaw, 0);
    }

    // Use Delta + Zstd for IDs
    return encodeCodec(CodecFamily::kDeltaZstd, 0);
}

std::uint8_t BlockCompressorConfig::getAuxCodec() const noexcept {
    using namespace format;
    return encodeCodec(CodecFamily::kDeltaVarint, 0);
}


// =============================================================================
// BlockCompressorImpl - Implementation Class
// =============================================================================

class BlockCompressorImpl {
public:
    explicit BlockCompressorImpl(BlockCompressorConfig config)
        : config_(std::move(config)) {}

    Result<CompressedBlockData> compress(std::span<const ReadRecord> reads,
                                          BlockId blockId);

    Result<CompressedBlockData> compress(std::span<const ReadRecordView> reads,
                                          BlockId blockId);

    Result<DecompressedBlockData> decompress(const CompressedBlockData& data);

    Result<DecompressedBlockData> decompress(
        const format::BlockHeader& header,
        std::span<const std::uint8_t> idStream,
        std::span<const std::uint8_t> seqStream,
        std::span<const std::uint8_t> qualStream,
        std::span<const std::uint8_t> auxStream);

    const BlockCompressorConfig& config() const noexcept { return config_; }

    VoidResult setConfig(BlockCompressorConfig config) {
        auto result = config.validate();
        if (!result) return result;
        config_ = std::move(config);
        return makeVoidSuccess();
    }

    void reset() noexcept {
        contigs_.clear();
    }

private:
    BlockCompressorConfig config_;
    std::vector<Contig> contigs_;

    // Internal compression methods
    Result<std::vector<std::uint8_t>> compressSequencesABC(
        std::span<const ReadRecord> reads);
    Result<std::vector<std::uint8_t>> compressSequencesZstd(
        std::span<const ReadRecord> reads);
    Result<std::vector<std::uint8_t>> compressQuality(
        std::span<const ReadRecord> reads);
    Result<std::vector<std::uint8_t>> compressIds(
        std::span<const ReadRecord> reads);
    Result<std::vector<std::uint8_t>> compressAux(
        std::span<const ReadRecord> reads,
        std::uint32_t& uniformLength);

    // Internal decompression methods
    Result<std::vector<std::string>> decompressSequencesABC(
        std::span<const std::uint8_t> data,
        std::uint32_t readCount);
    Result<std::vector<std::string>> decompressSequencesZstd(
        std::span<const std::uint8_t> data,
        std::uint32_t readCount,
        std::uint32_t uniformLength,
        std::span<const std::uint32_t> lengths);
    Result<std::vector<std::string>> decompressQuality(
        std::span<const std::uint8_t> data,
        std::uint32_t readCount,
        std::uint32_t uniformLength,
        std::span<const std::uint32_t> lengths);
    Result<std::vector<std::string>> decompressIds(
        std::span<const std::uint8_t> data,
        std::uint32_t readCount);
    Result<std::vector<std::uint32_t>> decompressAux(
        std::span<const std::uint8_t> data,
        std::uint32_t readCount);

    // Consensus building
    void buildContigs(std::span<const ReadRecord> reads);

    // Checksum computation
    std::uint64_t computeBlockChecksum(
        std::span<const ReadRecord> reads) const;

    void reportProgress(double progress) {
        if (config_.progressCallback) {
            config_.progressCallback(progress);
        }
    }
};


// =============================================================================
// Consensus Building (ABC Algorithm)
// =============================================================================

void BlockCompressorImpl::buildContigs(std::span<const ReadRecord> reads) {
    contigs_.clear();

    if (reads.empty()) return;

    // Track which reads have been assigned to contigs
    std::vector<bool> assigned(reads.size(), false);

    for (std::size_t i = 0; i < reads.size(); ++i) {
        if (assigned[i]) continue;

        // Start a new contig with this read
        Contig contig;
        contig.consensus.initFromRead(reads[i].sequence);

        DeltaEncodedRead delta;
        delta.positionOffset = 0;
        delta.isReverseComplement = false;
        delta.readLength = static_cast<std::uint16_t>(reads[i].sequence.length());
        delta.originalOrder = static_cast<std::uint32_t>(i);
        contig.deltas.push_back(std::move(delta));
        assigned[i] = true;

        // Try to add similar reads to this contig
        for (std::size_t j = i + 1; j < reads.size(); ++j) {
            if (assigned[j]) continue;

            auto alignment = findBestAlignment(
                reads[j].sequence,
                contig.consensus.sequence,
                config_.maxShift,
                config_.consensusHammingThreshold);

            if (alignment.has_value()) {
                auto [shift, isRC] = *alignment;

                // Add read to contig
                contig.consensus.addRead(reads[j].sequence, shift, isRC);

                DeltaEncodedRead readDelta = computeDelta(
                    reads[j].sequence,
                    contig.consensus.sequence,
                    shift, isRC);
                readDelta.originalOrder = static_cast<std::uint32_t>(j);
                contig.deltas.push_back(std::move(readDelta));

                assigned[j] = true;
            }
        }

        contigs_.push_back(std::move(contig));
    }

    FQC_LOG_DEBUG("Built {} contigs from {} reads", contigs_.size(), reads.size());
}


// =============================================================================
// Sequence Compression (ABC)
// =============================================================================

Result<std::vector<std::uint8_t>> BlockCompressorImpl::compressSequencesABC(
    std::span<const ReadRecord> reads) {

    // Build contigs for ABC compression
    buildContigs(reads);

    // Serialize contigs to byte stream
    // Format:
    // [num_contigs: uint32]
    // For each contig:
    //   [consensus_len: uint16][consensus: bytes]
    //   [num_deltas: uint32]
    //   For each delta:
    //     [original_order: uint32][position_offset: int16][flags: uint8]
    //     [read_length: uint16][num_mismatches: uint16]
    //     [mismatch_positions: uint16[]][mismatch_chars: char[]]

    std::vector<std::uint8_t> buffer;
    buffer.reserve(reads.size() * 20);  // Estimate

    // Write number of contigs
    std::uint32_t numContigs = static_cast<std::uint32_t>(contigs_.size());
    buffer.insert(buffer.end(),
                  reinterpret_cast<const std::uint8_t*>(&numContigs),
                  reinterpret_cast<const std::uint8_t*>(&numContigs) + sizeof(numContigs));

    for (const auto& contig : contigs_) {
        // Write consensus
        std::uint16_t consLen = static_cast<std::uint16_t>(contig.consensus.sequence.length());
        buffer.insert(buffer.end(),
                      reinterpret_cast<const std::uint8_t*>(&consLen),
                      reinterpret_cast<const std::uint8_t*>(&consLen) + sizeof(consLen));
        buffer.insert(buffer.end(),
                      contig.consensus.sequence.begin(),
                      contig.consensus.sequence.end());

        // Write deltas
        std::uint32_t numDeltas = static_cast<std::uint32_t>(contig.deltas.size());
        buffer.insert(buffer.end(),
                      reinterpret_cast<const std::uint8_t*>(&numDeltas),
                      reinterpret_cast<const std::uint8_t*>(&numDeltas) + sizeof(numDeltas));

        for (const auto& delta : contig.deltas) {
            // Original order
            buffer.insert(buffer.end(),
                          reinterpret_cast<const std::uint8_t*>(&delta.originalOrder),
                          reinterpret_cast<const std::uint8_t*>(&delta.originalOrder) +
                              sizeof(delta.originalOrder));

            // Position offset
            buffer.insert(buffer.end(),
                          reinterpret_cast<const std::uint8_t*>(&delta.positionOffset),
                          reinterpret_cast<const std::uint8_t*>(&delta.positionOffset) +
                              sizeof(delta.positionOffset));

            // Flags (bit 0 = isReverseComplement)
            std::uint8_t flags = delta.isReverseComplement ? 1 : 0;
            buffer.push_back(flags);

            // Read length
            buffer.insert(buffer.end(),
                          reinterpret_cast<const std::uint8_t*>(&delta.readLength),
                          reinterpret_cast<const std::uint8_t*>(&delta.readLength) +
                              sizeof(delta.readLength));

            // Number of mismatches
            std::uint16_t numMismatches =
                static_cast<std::uint16_t>(delta.mismatchPositions.size());
            buffer.insert(buffer.end(),
                          reinterpret_cast<const std::uint8_t*>(&numMismatches),
                          reinterpret_cast<const std::uint8_t*>(&numMismatches) +
                              sizeof(numMismatches));

            // Mismatch positions
            for (auto pos : delta.mismatchPositions) {
                buffer.insert(buffer.end(),
                              reinterpret_cast<const std::uint8_t*>(&pos),
                              reinterpret_cast<const std::uint8_t*>(&pos) + sizeof(pos));
            }

            // Mismatch characters
            buffer.insert(buffer.end(),
                          delta.mismatchChars.begin(),
                          delta.mismatchChars.end());
        }
    }

    // Compress with Zstd
    std::size_t compressBound = ZSTD_compressBound(buffer.size());
    std::vector<std::uint8_t> compressed(compressBound);

    std::size_t compressedSize = ZSTD_compress(
        compressed.data(), compressed.size(),
        buffer.data(), buffer.size(),
        config_.zstdLevel);

    if (ZSTD_isError(compressedSize)) {
        return makeError<std::vector<std::uint8_t>>(
            ErrorCode::kIOError,
            "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
    }

    compressed.resize(compressedSize);
    return compressed;
}


// =============================================================================
// Sequence Compression (Zstd - for medium/long reads)
// =============================================================================

Result<std::vector<std::uint8_t>> BlockCompressorImpl::compressSequencesZstd(
    std::span<const ReadRecord> reads) {

    // Concatenate all sequences with length prefixes
    std::vector<std::uint8_t> buffer;
    buffer.reserve(reads.size() * 200);  // Estimate for medium reads

    for (const auto& read : reads) {
        // Write length as uint32
        std::uint32_t len = static_cast<std::uint32_t>(read.sequence.length());
        buffer.insert(buffer.end(),
                      reinterpret_cast<const std::uint8_t*>(&len),
                      reinterpret_cast<const std::uint8_t*>(&len) + sizeof(len));

        // Write sequence
        buffer.insert(buffer.end(),
                      read.sequence.begin(),
                      read.sequence.end());
    }

    // Compress with Zstd
    std::size_t compressBound = ZSTD_compressBound(buffer.size());
    std::vector<std::uint8_t> compressed(compressBound);

    std::size_t compressedSize = ZSTD_compress(
        compressed.data(), compressed.size(),
        buffer.data(), buffer.size(),
        config_.zstdLevel);

    if (ZSTD_isError(compressedSize)) {
        return makeError<std::vector<std::uint8_t>>(
            ErrorCode::kIOError,
            "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
    }

    compressed.resize(compressedSize);
    return compressed;
}

// =============================================================================
// Quality Compression
// =============================================================================

Result<std::vector<std::uint8_t>> BlockCompressorImpl::compressQuality(
    std::span<const ReadRecord> reads) {

    if (config_.qualityMode == QualityMode::kDiscard) {
        return std::vector<std::uint8_t>{};
    }

    // Use SCM-based quality compression
    QualityCompressorConfig qualConfig;
    qualConfig.qualityMode = config_.qualityMode;

    // Use Order-1 for long reads (lower memory), Order-2 for short/medium
    if (config_.readLengthClass == ReadLengthClass::kLong) {
        qualConfig.contextOrder = QualityContextOrder::kOrder1;
    } else {
        qualConfig.contextOrder = QualityContextOrder::kOrder2;
    }

    qualConfig.usePositionContext = true;
    qualConfig.numPositionBins = kNumPositionBins;

    QualityCompressor compressor(qualConfig);

    // Convert reads to quality string views
    std::vector<std::string_view> qualities;
    qualities.reserve(reads.size());
    for (const auto& read : reads) {
        qualities.emplace_back(read.quality);
    }

    auto result = compressor.compress(qualities);
    if (!result) {
        return makeError<std::vector<std::uint8_t>>(result.error());
    }

    return std::move(result->data);
}


// =============================================================================
// ID Compression
// =============================================================================

Result<std::vector<std::uint8_t>> BlockCompressorImpl::compressIds(
    std::span<const ReadRecord> reads) {

    if (config_.idMode == IDMode::kDiscard) {
        return std::vector<std::uint8_t>{};
    }

    // Concatenate all IDs with length prefixes
    std::vector<std::uint8_t> buffer;
    buffer.reserve(reads.size() * 50);

    for (const auto& read : reads) {
        // Write length as uint16
        std::uint16_t len = static_cast<std::uint16_t>(read.id.length());
        buffer.insert(buffer.end(),
                      reinterpret_cast<const std::uint8_t*>(&len),
                      reinterpret_cast<const std::uint8_t*>(&len) + sizeof(len));

        // Write ID
        buffer.insert(buffer.end(),
                      read.id.begin(),
                      read.id.end());
    }

    // Compress with Zstd
    std::size_t compressBound = ZSTD_compressBound(buffer.size());
    std::vector<std::uint8_t> compressed(compressBound);

    std::size_t compressedSize = ZSTD_compress(
        compressed.data(), compressed.size(),
        buffer.data(), buffer.size(),
        config_.zstdLevel);

    if (ZSTD_isError(compressedSize)) {
        return makeError<std::vector<std::uint8_t>>(
            ErrorCode::kIOError,
            "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
    }

    compressed.resize(compressedSize);
    return compressed;
}

// =============================================================================
// Auxiliary Stream Compression (Read Lengths)
// =============================================================================

Result<std::vector<std::uint8_t>> BlockCompressorImpl::compressAux(
    std::span<const ReadRecord> reads,
    std::uint32_t& uniformLength) {

    if (reads.empty()) {
        uniformLength = 0;
        return std::vector<std::uint8_t>{};
    }

    // Check if all reads have uniform length
    std::size_t firstLen = reads[0].sequence.length();
    bool isUniform = true;

    for (const auto& read : reads) {
        if (read.sequence.length() != firstLen) {
            isUniform = false;
            break;
        }
    }

    if (isUniform) {
        uniformLength = static_cast<std::uint32_t>(firstLen);
        return std::vector<std::uint8_t>{};
    }

    uniformLength = 0;

    // Store lengths with delta encoding
    std::vector<std::uint8_t> buffer;
    buffer.reserve(reads.size() * 4);

    std::int32_t prevLen = 0;
    for (const auto& read : reads) {
        std::int32_t len = static_cast<std::int32_t>(read.sequence.length());
        std::int32_t delta = len - prevLen;
        prevLen = len;

        // Varint encoding for delta
        std::uint32_t zigzag = static_cast<std::uint32_t>((delta << 1) ^ (delta >> 31));
        do {
            std::uint8_t byte = zigzag & 0x7F;
            zigzag >>= 7;
            if (zigzag != 0) {
                byte |= 0x80;
            }
            buffer.push_back(byte);
        } while (zigzag != 0);
    }

    // Compress with Zstd
    std::size_t compressBound = ZSTD_compressBound(buffer.size());
    std::vector<std::uint8_t> compressed(compressBound);

    std::size_t compressedSize = ZSTD_compress(
        compressed.data(), compressed.size(),
        buffer.data(), buffer.size(),
        config_.zstdLevel);

    if (ZSTD_isError(compressedSize)) {
        return makeError<std::vector<std::uint8_t>>(
            ErrorCode::kIOError,
            "Zstd compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
    }

    compressed.resize(compressedSize);
    return compressed;
}


// =============================================================================
// Checksum Computation
// =============================================================================

std::uint64_t BlockCompressorImpl::computeBlockChecksum(
    std::span<const ReadRecord> reads) const {

    XXH64_state_t* state = XXH64_createState();
    XXH64_reset(state, 0);

    // Hash IDs
    for (const auto& read : reads) {
        XXH64_update(state, read.id.data(), read.id.size());
    }

    // Hash sequences
    for (const auto& read : reads) {
        XXH64_update(state, read.sequence.data(), read.sequence.size());
    }

    // Hash quality
    for (const auto& read : reads) {
        XXH64_update(state, read.quality.data(), read.quality.size());
    }

    // Hash lengths (aux)
    for (const auto& read : reads) {
        std::uint32_t len = static_cast<std::uint32_t>(read.sequence.length());
        XXH64_update(state, &len, sizeof(len));
    }

    std::uint64_t checksum = XXH64_digest(state);
    XXH64_freeState(state);

    return checksum;
}

// =============================================================================
// Main Compression Entry Point
// =============================================================================

Result<CompressedBlockData> BlockCompressorImpl::compress(
    std::span<const ReadRecord> reads,
    BlockId blockId) {

    CompressedBlockData result;
    result.blockId = blockId;
    result.readCount = static_cast<std::uint32_t>(reads.size());

    if (reads.empty()) {
        result.blockChecksum = 0;
        return result;
    }

    reportProgress(0.0);

    // Compress sequences based on read length class
    Result<std::vector<std::uint8_t>> seqResult;
    if (config_.readLengthClass == ReadLengthClass::kShort) {
        seqResult = compressSequencesABC(reads);
        result.codecSeq = config_.getSequenceCodec();
    } else {
        seqResult = compressSequencesZstd(reads);
        result.codecSeq = config_.getSequenceCodec();
    }

    if (!seqResult) {
        return makeError<CompressedBlockData>(seqResult.error());
    }
    result.seqStream = std::move(*seqResult);

    reportProgress(0.4);

    // Compress quality
    auto qualResult = compressQuality(reads);
    if (!qualResult) {
        return makeError<CompressedBlockData>(qualResult.error());
    }
    result.qualStream = std::move(*qualResult);
    result.codecQual = config_.getQualityCodec();

    reportProgress(0.6);

    // Compress IDs
    auto idResult = compressIds(reads);
    if (!idResult) {
        return makeError<CompressedBlockData>(idResult.error());
    }
    result.idStream = std::move(*idResult);
    result.codecIds = config_.getIdCodec();

    reportProgress(0.8);

    // Compress auxiliary (lengths)
    auto auxResult = compressAux(reads, result.uniformReadLength);
    if (!auxResult) {
        return makeError<CompressedBlockData>(auxResult.error());
    }
    result.auxStream = std::move(*auxResult);
    result.codecAux = config_.getAuxCodec();

    // Compute checksum
    result.blockChecksum = computeBlockChecksum(reads);

    reportProgress(1.0);

    FQC_LOG_DEBUG("Compressed block {}: {} reads, {} bytes -> {} bytes",
                  blockId, reads.size(),
                  reads.size() * 200,  // Estimate
                  result.totalCompressedSize());

    return result;
}

Result<CompressedBlockData> BlockCompressorImpl::compress(
    std::span<const ReadRecordView> reads,
    BlockId blockId) {

    // Convert views to records for compression
    std::vector<ReadRecord> records;
    records.reserve(reads.size());

    for (const auto& view : reads) {
        records.push_back(view.toRecord());
    }

    return compress(std::span<const ReadRecord>(records), blockId);
}


// =============================================================================
// Decompression Methods
// =============================================================================

Result<std::vector<std::string>> BlockCompressorImpl::decompressSequencesABC(
    std::span<const std::uint8_t> data,
    std::uint32_t readCount) {

    if (data.empty()) {
        return std::vector<std::string>(readCount);
    }

    // Decompress with Zstd
    std::size_t decompressedSize = ZSTD_getFrameContentSize(data.data(), data.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
        decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kFormatError, "Invalid Zstd frame");
    }

    std::vector<std::uint8_t> buffer(decompressedSize);
    std::size_t actualSize = ZSTD_decompress(
        buffer.data(), buffer.size(),
        data.data(), data.size());

    if (ZSTD_isError(actualSize)) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kIOError,
            "Zstd decompression failed: " + std::string(ZSTD_getErrorName(actualSize)));
    }

    // Parse contigs and reconstruct reads
    std::vector<std::string> sequences(readCount);
    const std::uint8_t* ptr = buffer.data();
    const std::uint8_t* end = buffer.data() + actualSize;

    // Read number of contigs
    if (ptr + sizeof(std::uint32_t) > end) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kFormatError, "Truncated ABC data");
    }
    std::uint32_t numContigs;
    std::memcpy(&numContigs, ptr, sizeof(numContigs));
    ptr += sizeof(numContigs);

    for (std::uint32_t c = 0; c < numContigs; ++c) {
        // Read consensus
        if (ptr + sizeof(std::uint16_t) > end) {
            return makeError<std::vector<std::string>>(
                ErrorCode::kFormatError, "Truncated ABC data");
        }
        std::uint16_t consLen;
        std::memcpy(&consLen, ptr, sizeof(consLen));
        ptr += sizeof(consLen);

        if (ptr + consLen > end) {
            return makeError<std::vector<std::string>>(
                ErrorCode::kFormatError, "Truncated ABC data");
        }
        std::string consensus(reinterpret_cast<const char*>(ptr), consLen);
        ptr += consLen;

        // Read deltas
        if (ptr + sizeof(std::uint32_t) > end) {
            return makeError<std::vector<std::string>>(
                ErrorCode::kFormatError, "Truncated ABC data");
        }
        std::uint32_t numDeltas;
        std::memcpy(&numDeltas, ptr, sizeof(numDeltas));
        ptr += sizeof(numDeltas);

        for (std::uint32_t d = 0; d < numDeltas; ++d) {
            DeltaEncodedRead delta;

            // Read original order
            if (ptr + sizeof(std::uint32_t) > end) {
                return makeError<std::vector<std::string>>(
                    ErrorCode::kFormatError, "Truncated ABC data");
            }
            std::memcpy(&delta.originalOrder, ptr, sizeof(delta.originalOrder));
            ptr += sizeof(delta.originalOrder);

            // Read position offset
            if (ptr + sizeof(std::int16_t) > end) {
                return makeError<std::vector<std::string>>(
                    ErrorCode::kFormatError, "Truncated ABC data");
            }
            std::memcpy(&delta.positionOffset, ptr, sizeof(delta.positionOffset));
            ptr += sizeof(delta.positionOffset);

            // Read flags
            if (ptr + 1 > end) {
                return makeError<std::vector<std::string>>(
                    ErrorCode::kFormatError, "Truncated ABC data");
            }
            delta.isReverseComplement = (*ptr & 1) != 0;
            ptr++;

            // Read read length
            if (ptr + sizeof(std::uint16_t) > end) {
                return makeError<std::vector<std::string>>(
                    ErrorCode::kFormatError, "Truncated ABC data");
            }
            std::memcpy(&delta.readLength, ptr, sizeof(delta.readLength));
            ptr += sizeof(delta.readLength);

            // Read number of mismatches
            if (ptr + sizeof(std::uint16_t) > end) {
                return makeError<std::vector<std::string>>(
                    ErrorCode::kFormatError, "Truncated ABC data");
            }
            std::uint16_t numMismatches;
            std::memcpy(&numMismatches, ptr, sizeof(numMismatches));
            ptr += sizeof(numMismatches);

            // Read mismatch positions
            delta.mismatchPositions.resize(numMismatches);
            for (std::uint16_t m = 0; m < numMismatches; ++m) {
                if (ptr + sizeof(std::uint16_t) > end) {
                    return makeError<std::vector<std::string>>(
                        ErrorCode::kFormatError, "Truncated ABC data");
                }
                std::memcpy(&delta.mismatchPositions[m], ptr, sizeof(std::uint16_t));
                ptr += sizeof(std::uint16_t);
            }

            // Read mismatch characters
            if (ptr + numMismatches > end) {
                return makeError<std::vector<std::string>>(
                    ErrorCode::kFormatError, "Truncated ABC data");
            }
            delta.mismatchChars.assign(ptr, ptr + numMismatches);
            ptr += numMismatches;

            // Reconstruct sequence
            if (delta.originalOrder < readCount) {
                sequences[delta.originalOrder] = reconstructFromDelta(delta, consensus);
            }
        }
    }

    return sequences;
}


Result<std::vector<std::string>> BlockCompressorImpl::decompressSequencesZstd(
    std::span<const std::uint8_t> data,
    std::uint32_t readCount,
    std::uint32_t uniformLength,
    std::span<const std::uint32_t> lengths) {

    if (data.empty()) {
        return std::vector<std::string>(readCount);
    }

    // Decompress with Zstd
    std::size_t decompressedSize = ZSTD_getFrameContentSize(data.data(), data.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
        decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kFormatError, "Invalid Zstd frame");
    }

    std::vector<std::uint8_t> buffer(decompressedSize);
    std::size_t actualSize = ZSTD_decompress(
        buffer.data(), buffer.size(),
        data.data(), data.size());

    if (ZSTD_isError(actualSize)) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kIOError,
            "Zstd decompression failed: " + std::string(ZSTD_getErrorName(actualSize)));
    }

    // Parse sequences
    std::vector<std::string> sequences;
    sequences.reserve(readCount);

    const std::uint8_t* ptr = buffer.data();
    const std::uint8_t* end = buffer.data() + actualSize;

    for (std::uint32_t i = 0; i < readCount; ++i) {
        // Read length
        if (ptr + sizeof(std::uint32_t) > end) {
            return makeError<std::vector<std::string>>(
                ErrorCode::kFormatError, "Truncated sequence data");
        }
        std::uint32_t len;
        std::memcpy(&len, ptr, sizeof(len));
        ptr += sizeof(len);

        // Read sequence
        if (ptr + len > end) {
            return makeError<std::vector<std::string>>(
                ErrorCode::kFormatError, "Truncated sequence data");
        }
        sequences.emplace_back(reinterpret_cast<const char*>(ptr), len);
        ptr += len;
    }

    return sequences;
}

Result<std::vector<std::string>> BlockCompressorImpl::decompressQuality(
    std::span<const std::uint8_t> data,
    std::uint32_t readCount,
    std::uint32_t uniformLength,
    std::span<const std::uint32_t> lengths) {

    if (data.empty()) {
        // Quality was discarded - return placeholder quality strings
        std::vector<std::string> qualities;
        qualities.reserve(readCount);
        for (std::uint32_t i = 0; i < readCount; ++i) {
            std::uint32_t len = uniformLength;
            if (len == 0 && i < lengths.size()) {
                len = lengths[i];
            }
            qualities.emplace_back(len, kDefaultPlaceholderQual);
        }
        return qualities;
    }

    // Use SCM-based quality decompression
    QualityCompressorConfig qualConfig;
    qualConfig.qualityMode = config_.qualityMode;

    // Use Order-1 for long reads (lower memory), Order-2 for short/medium
    if (config_.readLengthClass == ReadLengthClass::kLong) {
        qualConfig.contextOrder = QualityContextOrder::kOrder1;
    } else {
        qualConfig.contextOrder = QualityContextOrder::kOrder2;
    }

    qualConfig.usePositionContext = true;
    qualConfig.numPositionBins = kNumPositionBins;

    QualityCompressor compressor(qualConfig);

    // Build lengths vector
    std::vector<std::uint32_t> lengthsVec;
    if (uniformLength > 0) {
        lengthsVec.assign(readCount, uniformLength);
    } else if (!lengths.empty()) {
        lengthsVec.assign(lengths.begin(), lengths.end());
    } else {
        // Fallback: assume uniform length from data
        return makeError<std::vector<std::string>>(
            ErrorCode::kFormatError, "Missing length information for quality decompression");
    }

    return compressor.decompress(data, lengthsVec);
}

Result<std::vector<std::string>> BlockCompressorImpl::decompressIds(
    std::span<const std::uint8_t> data,
    std::uint32_t readCount) {

    if (data.empty()) {
        // IDs were discarded - return empty strings
        return std::vector<std::string>(readCount);
    }

    // Decompress with Zstd
    std::size_t decompressedSize = ZSTD_getFrameContentSize(data.data(), data.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
        decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kFormatError, "Invalid Zstd frame");
    }

    std::vector<std::uint8_t> buffer(decompressedSize);
    std::size_t actualSize = ZSTD_decompress(
        buffer.data(), buffer.size(),
        data.data(), data.size());

    if (ZSTD_isError(actualSize)) {
        return makeError<std::vector<std::string>>(
            ErrorCode::kIOError,
            "Zstd decompression failed: " + std::string(ZSTD_getErrorName(actualSize)));
    }

    // Parse IDs
    std::vector<std::string> ids;
    ids.reserve(readCount);

    const std::uint8_t* ptr = buffer.data();
    const std::uint8_t* end = buffer.data() + actualSize;

    for (std::uint32_t i = 0; i < readCount; ++i) {
        // Read length
        if (ptr + sizeof(std::uint16_t) > end) {
            return makeError<std::vector<std::string>>(
                ErrorCode::kFormatError, "Truncated ID data");
        }
        std::uint16_t len;
        std::memcpy(&len, ptr, sizeof(len));
        ptr += sizeof(len);

        // Read ID
        if (ptr + len > end) {
            return makeError<std::vector<std::string>>(
                ErrorCode::kFormatError, "Truncated ID data");
        }
        ids.emplace_back(reinterpret_cast<const char*>(ptr), len);
        ptr += len;
    }

    return ids;
}


Result<std::vector<std::uint32_t>> BlockCompressorImpl::decompressAux(
    std::span<const std::uint8_t> data,
    std::uint32_t readCount) {

    if (data.empty()) {
        return std::vector<std::uint32_t>{};
    }

    // Decompress with Zstd
    std::size_t decompressedSize = ZSTD_getFrameContentSize(data.data(), data.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
        decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        return makeError<std::vector<std::uint32_t>>(
            ErrorCode::kFormatError, "Invalid Zstd frame");
    }

    std::vector<std::uint8_t> buffer(decompressedSize);
    std::size_t actualSize = ZSTD_decompress(
        buffer.data(), buffer.size(),
        data.data(), data.size());

    if (ZSTD_isError(actualSize)) {
        return makeError<std::vector<std::uint32_t>>(
            ErrorCode::kIOError,
            "Zstd decompression failed: " + std::string(ZSTD_getErrorName(actualSize)));
    }

    // Parse varint-encoded deltas
    std::vector<std::uint32_t> lengths;
    lengths.reserve(readCount);

    const std::uint8_t* ptr = buffer.data();
    const std::uint8_t* end = buffer.data() + actualSize;

    std::int32_t prevLen = 0;
    for (std::uint32_t i = 0; i < readCount && ptr < end; ++i) {
        // Decode varint
        std::uint32_t zigzag = 0;
        std::uint32_t shift = 0;

        while (ptr < end) {
            std::uint8_t byte = *ptr++;
            zigzag |= static_cast<std::uint32_t>(byte & 0x7F) << shift;
            shift += 7;
            if ((byte & 0x80) == 0) break;
        }

        // Decode zigzag
        std::int32_t delta = static_cast<std::int32_t>((zigzag >> 1) ^ -(zigzag & 1));
        std::int32_t len = prevLen + delta;
        prevLen = len;

        lengths.push_back(static_cast<std::uint32_t>(len));
    }

    return lengths;
}

// =============================================================================
// Main Decompression Entry Point
// =============================================================================

Result<DecompressedBlockData> BlockCompressorImpl::decompress(
    const CompressedBlockData& data) {

    DecompressedBlockData result;
    result.blockId = data.blockId;
    result.reads.resize(data.readCount);

    if (data.readCount == 0) {
        return result;
    }

    // Decompress auxiliary (lengths) first
    std::vector<std::uint32_t> lengths;
    if (!data.auxStream.empty()) {
        auto auxResult = decompressAux(data.auxStream, data.readCount);
        if (!auxResult) {
            return makeError<DecompressedBlockData>(auxResult.error());
        }
        lengths = std::move(*auxResult);
    }

    // Decompress sequences
    Result<std::vector<std::string>> seqResult;
    auto seqCodec = format::decodeCodecFamily(data.codecSeq);

    if (seqCodec == CodecFamily::kAbcV1) {
        seqResult = decompressSequencesABC(data.seqStream, data.readCount);
    } else {
        seqResult = decompressSequencesZstd(
            data.seqStream, data.readCount, data.uniformReadLength, lengths);
    }

    if (!seqResult) {
        return makeError<DecompressedBlockData>(seqResult.error());
    }

    // Decompress quality
    auto qualResult = decompressQuality(
        data.qualStream, data.readCount, data.uniformReadLength, lengths);
    if (!qualResult) {
        return makeError<DecompressedBlockData>(qualResult.error());
    }

    // Decompress IDs
    auto idResult = decompressIds(data.idStream, data.readCount);
    if (!idResult) {
        return makeError<DecompressedBlockData>(idResult.error());
    }

    // Assemble reads
    for (std::uint32_t i = 0; i < data.readCount; ++i) {
        result.reads[i].id = std::move((*idResult)[i]);
        result.reads[i].sequence = std::move((*seqResult)[i]);
        result.reads[i].quality = std::move((*qualResult)[i]);
    }

    return result;
}

Result<DecompressedBlockData> BlockCompressorImpl::decompress(
    const format::BlockHeader& header,
    std::span<const std::uint8_t> idStream,
    std::span<const std::uint8_t> seqStream,
    std::span<const std::uint8_t> qualStream,
    std::span<const std::uint8_t> auxStream) {

    CompressedBlockData data;
    data.blockId = header.blockId;
    data.readCount = header.uncompressedCount;
    data.uniformReadLength = header.uniformReadLength;
    data.codecIds = header.codecIds;
    data.codecSeq = header.codecSeq;
    data.codecQual = header.codecQual;
    data.codecAux = header.codecAux;

    data.idStream.assign(idStream.begin(), idStream.end());
    data.seqStream.assign(seqStream.begin(), seqStream.end());
    data.qualStream.assign(qualStream.begin(), qualStream.end());
    data.auxStream.assign(auxStream.begin(), auxStream.end());

    return decompress(data);
}


// =============================================================================
// BlockCompressor - Public Interface Implementation
// =============================================================================

BlockCompressor::BlockCompressor(BlockCompressorConfig config)
    : impl_(std::make_unique<BlockCompressorImpl>(std::move(config))) {}

BlockCompressor::~BlockCompressor() = default;

BlockCompressor::BlockCompressor(BlockCompressor&&) noexcept = default;
BlockCompressor& BlockCompressor::operator=(BlockCompressor&&) noexcept = default;

Result<CompressedBlockData> BlockCompressor::compress(
    std::span<const ReadRecord> reads,
    BlockId blockId) {
    return impl_->compress(reads, blockId);
}

Result<CompressedBlockData> BlockCompressor::compress(
    std::span<const ReadRecordView> reads,
    BlockId blockId) {
    return impl_->compress(reads, blockId);
}

Result<DecompressedBlockData> BlockCompressor::decompress(
    const CompressedBlockData& compressedData) {
    return impl_->decompress(compressedData);
}

Result<DecompressedBlockData> BlockCompressor::decompress(
    const format::BlockHeader& header,
    std::span<const std::uint8_t> idStream,
    std::span<const std::uint8_t> seqStream,
    std::span<const std::uint8_t> qualStream,
    std::span<const std::uint8_t> auxStream) {
    return impl_->decompress(header, idStream, seqStream, qualStream, auxStream);
}

const BlockCompressorConfig& BlockCompressor::config() const noexcept {
    return impl_->config();
}

VoidResult BlockCompressor::setConfig(BlockCompressorConfig config) {
    return impl_->setConfig(std::move(config));
}

void BlockCompressor::reset() noexcept {
    impl_->reset();
}

}  // namespace fqc::algo
