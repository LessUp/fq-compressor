// =============================================================================
// fq-compressor - Block Compressor Module (Phase 2)
// =============================================================================
// Implements Phase 2 of the two-phase compression strategy:
// - Block-wise compression with complete state isolation
// - Consensus building for similar reads (ABC algorithm for short reads)
// - Delta encoding against consensus
// - Codec integration (ABC for short reads, Zstd for medium/long reads)
//
// Each block can be independently compressed and decompressed, enabling
// random access support.
//
// Requirements: 1.1.2, 2.1
// =============================================================================

#ifndef FQC_ALGO_BLOCK_COMPRESSOR_H
#define FQC_ALGO_BLOCK_COMPRESSOR_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fqc/common/error.h"
#include "fqc/common/types.h"
#include "fqc/format/fqc_format.h"

namespace fqc::algo {

// =============================================================================
// Forward Declarations
// =============================================================================

class BlockCompressorImpl;

// =============================================================================
// Constants
// =============================================================================

/// @brief Memory per read for Phase 2 (bytes)
/// Includes: Read data (~50 bytes) + encoding buffers
inline constexpr std::size_t kMemoryPerReadPhase2 = 50;

/// @brief Default consensus threshold (minimum reads to build consensus)
inline constexpr std::size_t kDefaultConsensusMinReads = 2;

/// @brief Maximum shift for read alignment in consensus building
inline constexpr std::size_t kDefaultMaxShift = 15;

/// @brief Hamming distance threshold for consensus grouping
inline constexpr std::size_t kDefaultConsensusHammingThreshold = 8;

/// @brief Default Zstd compression level
inline constexpr int kDefaultZstdLevel = 3;

// =============================================================================
// Compressed Block Data
// =============================================================================

/// @brief Compressed data for a single block
struct CompressedBlockData {
    /// @brief Block ID (globally continuous)
    BlockId blockId = 0;

    /// @brief Number of reads in this block
    std::uint32_t readCount = 0;

    /// @brief Uniform read length (0 = variable, use auxStream)
    std::uint32_t uniformReadLength = 0;

    /// @brief Compressed ID stream
    std::vector<std::uint8_t> idStream;

    /// @brief Compressed sequence stream
    std::vector<std::uint8_t> seqStream;

    /// @brief Compressed quality stream
    std::vector<std::uint8_t> qualStream;

    /// @brief Compressed auxiliary stream (read lengths if variable)
    std::vector<std::uint8_t> auxStream;

    /// @brief xxHash64 of uncompressed logical streams
    /// Computed over: ID || Seq || Qual || Aux (uncompressed)
    std::uint64_t blockChecksum = 0;

    /// @brief Codec used for ID stream
    std::uint8_t codecIds = 0;

    /// @brief Codec used for sequence stream
    std::uint8_t codecSeq = 0;

    /// @brief Codec used for quality stream
    std::uint8_t codecQual = 0;

    /// @brief Codec used for auxiliary stream
    std::uint8_t codecAux = 0;

    /// @brief Get total compressed size
    [[nodiscard]] std::size_t totalCompressedSize() const noexcept {
        return idStream.size() + seqStream.size() + qualStream.size() + auxStream.size();
    }

    /// @brief Check if block has uniform read length
    [[nodiscard]] bool hasUniformLength() const noexcept {
        return uniformReadLength > 0 && auxStream.empty();
    }

    /// @brief Check if quality was discarded
    [[nodiscard]] bool isQualityDiscarded() const noexcept {
        return qualStream.empty() &&
               format::decodeCodecFamily(codecQual) == CodecFamily::kRaw;
    }

    /// @brief Clear all data
    void clear() noexcept {
        blockId = 0;
        readCount = 0;
        uniformReadLength = 0;
        idStream.clear();
        seqStream.clear();
        qualStream.clear();
        auxStream.clear();
        blockChecksum = 0;
        codecIds = 0;
        codecSeq = 0;
        codecQual = 0;
        codecAux = 0;
    }
};

// =============================================================================
// Decompressed Block Data
// =============================================================================

/// @brief Decompressed data for a single block
struct DecompressedBlockData {
    /// @brief Block ID
    BlockId blockId = 0;

    /// @brief Read records in this block
    std::vector<ReadRecord> reads;

    /// @brief Clear all data
    void clear() noexcept {
        blockId = 0;
        reads.clear();
    }
};

// =============================================================================
// Block Compressor Configuration
// =============================================================================

/// @brief Configuration for block compression
struct BlockCompressorConfig {
    /// @brief Read length class (determines compression strategy)
    ReadLengthClass readLengthClass = ReadLengthClass::kShort;

    /// @brief Quality compression mode
    QualityMode qualityMode = QualityMode::kLossless;

    /// @brief ID handling mode
    IDMode idMode = IDMode::kExact;

    /// @brief Compression level (1-9)
    CompressionLevel compressionLevel = kDefaultCompressionLevel;

    /// @brief Zstd compression level (for medium/long reads)
    int zstdLevel = kDefaultZstdLevel;

    /// @brief Number of threads (0 = auto-detect)
    std::size_t numThreads = 0;

    /// @brief Minimum reads to build consensus
    std::size_t consensusMinReads = kDefaultConsensusMinReads;

    /// @brief Maximum shift for read alignment
    std::size_t maxShift = kDefaultMaxShift;

    /// @brief Hamming distance threshold for consensus grouping
    std::size_t consensusHammingThreshold = kDefaultConsensusHammingThreshold;

    /// @brief Progress callback (called with progress 0.0-1.0)
    std::function<void(double)> progressCallback;

    /// @brief Validate configuration
    /// @return VoidResult indicating success or error
    [[nodiscard]] VoidResult validate() const;

    /// @brief Get the sequence codec for current configuration
    [[nodiscard]] std::uint8_t getSequenceCodec() const noexcept;

    /// @brief Get the quality codec for current configuration
    [[nodiscard]] std::uint8_t getQualityCodec() const noexcept;

    /// @brief Get the ID codec for current configuration
    [[nodiscard]] std::uint8_t getIdCodec() const noexcept;

    /// @brief Get the auxiliary codec for current configuration
    [[nodiscard]] std::uint8_t getAuxCodec() const noexcept;
};

// =============================================================================
// Consensus Sequence
// =============================================================================

/// @brief Represents a consensus sequence built from similar reads
struct ConsensusSequence {
    /// @brief The consensus sequence (majority base at each position)
    std::string sequence;

    /// @brief Base counts at each position [4][length]
    /// Index: A=0, C=1, G=2, T=3
    std::vector<std::array<std::uint16_t, 4>> baseCounts;

    /// @brief Number of reads contributing to this consensus
    std::size_t contributingReads = 0;

    /// @brief Clear the consensus
    void clear() noexcept {
        sequence.clear();
        baseCounts.clear();
        contributingReads = 0;
    }

    /// @brief Initialize consensus from a single read
    /// @param read The initial read sequence
    void initFromRead(std::string_view read);

    /// @brief Update consensus with a new read
    /// @param read The read to add
    /// @param shift Alignment shift (positive = read shifted right)
    /// @param isReverseComplement Whether read is reverse complemented
    void addRead(std::string_view read, int shift, bool isReverseComplement);

    /// @brief Recompute consensus sequence from base counts
    void recomputeConsensus();
};

// =============================================================================
// Delta Encoding
// =============================================================================

/// @brief Delta-encoded read (differences from consensus)
struct DeltaEncodedRead {
    /// @brief Position offset from consensus start
    std::int16_t positionOffset = 0;

    /// @brief Whether this read is reverse complemented
    bool isReverseComplement = false;

    /// @brief Read length
    std::uint16_t readLength = 0;

    /// @brief Positions of mismatches (relative to aligned position)
    std::vector<std::uint16_t> mismatchPositions;

    /// @brief Encoded mismatch characters
    /// Uses noise encoding: enc_noise[ref_base][read_base]
    std::vector<char> mismatchChars;

    /// @brief Original read order (for reconstruction)
    std::uint32_t originalOrder = 0;

    /// @brief Clear the delta
    void clear() noexcept {
        positionOffset = 0;
        isReverseComplement = false;
        readLength = 0;
        mismatchPositions.clear();
        mismatchChars.clear();
        originalOrder = 0;
    }
};

/// @brief A contig (group of similar reads with shared consensus)
struct Contig {
    /// @brief Consensus sequence for this contig
    ConsensusSequence consensus;

    /// @brief Delta-encoded reads in this contig
    std::vector<DeltaEncodedRead> deltas;

    /// @brief Clear the contig
    void clear() noexcept {
        consensus.clear();
        deltas.clear();
    }
};

// =============================================================================
// Block Compressor Class
// =============================================================================

/// @brief Main class for Phase 2 block compression
///
/// The BlockCompressor performs the following steps for each block:
/// 1. Build consensus sequences from groups of similar reads
/// 2. Encode each read as delta from its consensus
/// 3. Compress the encoded data using appropriate codec
/// 4. Ensure complete state isolation for independent decompression
///
/// For short reads (max <= 511): Uses Spring ABC algorithm
/// For medium/long reads: Uses Zstd compression
///
/// Usage:
/// @code
/// BlockCompressorConfig config;
/// config.readLengthClass = ReadLengthClass::kShort;
/// config.qualityMode = QualityMode::kLossless;
///
/// BlockCompressor compressor(config);
/// auto result = compressor.compress(reads, blockId);
/// if (result) {
///     // Use result->seqStream, result->qualStream, etc.
/// }
/// @endcode
class BlockCompressor {
public:
    /// @brief Construct with configuration
    /// @param config Compression configuration
    explicit BlockCompressor(BlockCompressorConfig config = {});

    /// @brief Destructor
    ~BlockCompressor();

    // Non-copyable, movable
    BlockCompressor(const BlockCompressor&) = delete;
    BlockCompressor& operator=(const BlockCompressor&) = delete;
    BlockCompressor(BlockCompressor&&) noexcept;
    BlockCompressor& operator=(BlockCompressor&&) noexcept;

    /// @brief Compress a block of reads
    /// @param reads Vector of read records to compress
    /// @param blockId Block identifier
    /// @return Compressed block data or error
    [[nodiscard]] Result<CompressedBlockData> compress(
        std::span<const ReadRecord> reads,
        BlockId blockId);

    /// @brief Compress a block of reads (view version)
    /// @param reads Vector of read record views to compress
    /// @param blockId Block identifier
    /// @return Compressed block data or error
    [[nodiscard]] Result<CompressedBlockData> compress(
        std::span<const ReadRecordView> reads,
        BlockId blockId);

    /// @brief Decompress a block
    /// @param compressedData Compressed block data
    /// @return Decompressed block data or error
    [[nodiscard]] Result<DecompressedBlockData> decompress(
        const CompressedBlockData& compressedData);

    /// @brief Decompress a block from raw streams
    /// @param header Block header with codec and size information
    /// @param idStream Compressed ID stream
    /// @param seqStream Compressed sequence stream
    /// @param qualStream Compressed quality stream
    /// @param auxStream Compressed auxiliary stream
    /// @return Decompressed block data or error
    [[nodiscard]] Result<DecompressedBlockData> decompress(
        const format::BlockHeader& header,
        std::span<const std::uint8_t> idStream,
        std::span<const std::uint8_t> seqStream,
        std::span<const std::uint8_t> qualStream,
        std::span<const std::uint8_t> auxStream);

    /// @brief Get current configuration
    [[nodiscard]] const BlockCompressorConfig& config() const noexcept;

    /// @brief Update configuration
    /// @param config New configuration
    /// @return VoidResult indicating success or validation error
    [[nodiscard]] VoidResult setConfig(BlockCompressorConfig config);

    /// @brief Reset compressor state (for reuse)
    void reset() noexcept;

private:
    std::unique_ptr<BlockCompressorImpl> impl_;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Compute delta encoding between a read and consensus
/// @param read The read sequence
/// @param consensus The consensus sequence
/// @param shift Alignment shift
/// @param isRC Whether read is reverse complemented
/// @return Delta-encoded read
[[nodiscard]] DeltaEncodedRead computeDelta(
    std::string_view read,
    std::string_view consensus,
    int shift,
    bool isRC);

/// @brief Reconstruct a read from delta and consensus
/// @param delta Delta-encoded read
/// @param consensus The consensus sequence
/// @return Reconstructed read sequence
[[nodiscard]] std::string reconstructFromDelta(
    const DeltaEncodedRead& delta,
    std::string_view consensus);

/// @brief Compute Hamming distance between two sequences
/// @param seq1 First sequence
/// @param seq2 Second sequence
/// @param maxDistance Maximum distance to compute (early exit)
/// @return Hamming distance, or maxDistance+1 if exceeded
[[nodiscard]] std::size_t hammingDistance(
    std::string_view seq1,
    std::string_view seq2,
    std::size_t maxDistance = SIZE_MAX);

/// @brief Find best alignment between a read and reference
/// @param read The read sequence
/// @param reference The reference sequence
/// @param maxShift Maximum shift to try
/// @param hammingThreshold Maximum Hamming distance for valid alignment
/// @return Pair of (shift, isReverseComplement), or nullopt if no good alignment
[[nodiscard]] std::optional<std::pair<int, bool>> findBestAlignment(
    std::string_view read,
    std::string_view reference,
    std::size_t maxShift,
    std::size_t hammingThreshold);

/// @brief Compute reverse complement of a DNA sequence
/// @param sequence Input sequence
/// @return Reverse complement
[[nodiscard]] std::string reverseComplement(std::string_view sequence);

/// @brief Encode noise character (substitution from ref to read)
/// @param refBase Reference base
/// @param readBase Read base
/// @return Encoded noise character ('0'-'3')
[[nodiscard]] char encodeNoise(char refBase, char readBase) noexcept;

/// @brief Decode noise character back to read base
/// @param refBase Reference base
/// @param noiseChar Encoded noise character
/// @return Decoded read base
[[nodiscard]] char decodeNoise(char refBase, char noiseChar) noexcept;

}  // namespace fqc::algo

#endif  // FQC_ALGO_BLOCK_COMPRESSOR_H
