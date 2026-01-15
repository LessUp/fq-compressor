// =============================================================================
// fq-compressor - FQC Format Property Tests
// =============================================================================
// Property-based tests for FQC format round-trip consistency.
//
// **Property 1: FQC 格式往返一致性**
// *For any* 有效的 GlobalHeader 和 BlockData 序列，写入后读取应产生等价数据
//
// **Validates: Requirements 2.1, 5.1, 5.2**
// =============================================================================

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include "fqc/format/fqc_format.h"
#include "fqc/format/fqc_reader.h"
#include "fqc/format/fqc_writer.h"

namespace fqc::format::test {

// =============================================================================
// Test Utilities
// =============================================================================

/// @brief Generate a temporary file path for testing.
[[nodiscard]] std::filesystem::path tempFilePath() {
    static std::atomic<int> counter{0};
    auto path = std::filesystem::temp_directory_path() /
                ("fqc_test_" + std::to_string(counter++) + "_" +
                 std::to_string(std::random_device{}()) + ".fqc");
    return path;
}

/// @brief RAII cleanup for temporary files.
class TempFileGuard {
public:
    explicit TempFileGuard(std::filesystem::path path) : path_(std::move(path)) {}
    ~TempFileGuard() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

// =============================================================================
// RapidCheck Generators
// =============================================================================

namespace gen {

/// @brief Generate valid flags for GlobalHeader.
[[nodiscard]] rc::Gen<std::uint64_t> validFlags() {
    return rc::gen::map(
        rc::gen::tuple(
            rc::gen::element(false, true),  // IS_PAIRED
            rc::gen::element(false, true),  // PRESERVE_ORDER
            rc::gen::inRange(0, 4),         // QUALITY_MODE (0-3)
            rc::gen::inRange(0, 3),         // ID_MODE (0-2)
            rc::gen::element(false, true),  // HAS_REORDER_MAP
            rc::gen::inRange(0, 2),         // PE_LAYOUT (0-1)
            rc::gen::inRange(0, 3),         // READ_LENGTH_CLASS (0-2)
            rc::gen::element(false, true)   // STREAMING_MODE
        ),
        [](const auto& tuple) -> std::uint64_t {
            auto [isPaired, preserveOrder, qualMode, idMode, hasReorderMap, peLayout,
                  lengthClass, streamingMode] = tuple;
            std::uint64_t flags = 0;
            if (isPaired) flags |= (1ULL << 0);
            if (preserveOrder) flags |= (1ULL << 1);
            flags |= (static_cast<std::uint64_t>(qualMode) << 3);
            flags |= (static_cast<std::uint64_t>(idMode) << 5);
            if (hasReorderMap && !preserveOrder) flags |= (1ULL << 7);
            if (isPaired) flags |= (static_cast<std::uint64_t>(peLayout) << 8);
            flags |= (static_cast<std::uint64_t>(lengthClass) << 10);
            if (streamingMode) {
                flags |= (1ULL << 12);
                flags |= (1ULL << 1);   // Force PRESERVE_ORDER
                flags &= ~(1ULL << 7);  // Clear HAS_REORDER_MAP
            }
            return flags;
        });
}

/// @brief Generate valid compression algorithm ID.
[[nodiscard]] rc::Gen<std::uint8_t> validCompressionAlgo() {
    return rc::gen::inRange<std::uint8_t>(0, 8);  // 0-7 are valid families
}

/// @brief Generate valid checksum type.
[[nodiscard]] rc::Gen<std::uint8_t> validChecksumType() {
    return rc::gen::element<std::uint8_t>(0, 1);  // 0=xxhash64, 1=reserved
}

/// @brief Generate valid original filename (UTF-8, reasonable length).
[[nodiscard]] rc::Gen<std::string> validFilename() {
    return rc::gen::map(
        rc::gen::container<std::string>(
            rc::gen::inRange(1, 64),
            rc::gen::oneOf(
                rc::gen::inRange('a', 'z' + 1),
                rc::gen::inRange('A', 'Z' + 1),
                rc::gen::inRange('0', '9' + 1),
                rc::gen::element('_', '-', '.'))),
        [](std::string s) {
            if (s.empty()) s = "test.fastq";
            if (!s.ends_with(".fastq") && !s.ends_with(".fq")) {
                s += ".fastq";
            }
            return s;
        });
}

/// @brief Generate valid GlobalHeader.
[[nodiscard]] rc::Gen<GlobalHeader> validGlobalHeader() {
    return rc::gen::map(
        rc::gen::tuple(
            validFlags(),
            validCompressionAlgo(),
            validChecksumType(),
            rc::gen::inRange<std::uint64_t>(0, 1000000)),  // total_read_count
        [](const auto& tuple) {
            auto [flags, algo, checksum, readCount] = tuple;
            GlobalHeader header{};
            header.headerSize = sizeof(GlobalHeader);
            header.flags = flags;
            header.compressionAlgo = algo;
            header.checksumType = checksum;
            header.reserved = 0;
            header.totalReadCount = readCount;
            header.originalFilenameLen = 0;
            return header;
        });
}

/// @brief Generate valid codec ID (family:4bit, version:4bit).
[[nodiscard]] rc::Gen<std::uint8_t> validCodecId() {
    return rc::gen::map(
        rc::gen::tuple(
            rc::gen::inRange<std::uint8_t>(0, 9),  // family 0-8
            rc::gen::inRange<std::uint8_t>(0, 4)   // version 0-3
        ),
        [](const auto& tuple) -> std::uint8_t {
            auto [family, version] = tuple;
            return static_cast<std::uint8_t>((family << 4) | version);
        });
}

/// @brief Generate valid BlockHeader.
[[nodiscard]] rc::Gen<BlockHeader> validBlockHeader(std::uint32_t blockId) {
    return rc::gen::map(
        rc::gen::tuple(
            validCodecId(),  // codec_ids
            validCodecId(),  // codec_seq
            validCodecId(),  // codec_qual
            validCodecId(),  // codec_aux
            rc::gen::inRange<std::uint32_t>(1, 10000),  // uncompressed_count
            rc::gen::inRange<std::uint32_t>(0, 512)     // uniform_read_length (0=variable)
        ),
        [blockId](const auto& tuple) {
            auto [codecIds, codecSeq, codecQual, codecAux, count, uniformLen] = tuple;
            BlockHeader header{};
            header.headerSize = sizeof(BlockHeader);
            header.blockId = blockId;
            header.checksumType = 0;  // xxhash64
            header.codecIds = codecIds;
            header.codecSeq = codecSeq;
            header.codecQual = codecQual;
            header.codecAux = codecAux;
            header.reserved1 = 0;
            header.reserved2 = 0;
            header.blockXxhash64 = 0;  // Will be computed
            header.uncompressedCount = count;
            header.uniformReadLength = uniformLen;
            header.compressedSize = 0;  // Will be computed
            header.offsetIds = 0;
            header.offsetSeq = 0;
            header.offsetQual = 0;
            header.offsetAux = 0;
            header.sizeIds = 0;
            header.sizeSeq = 0;
            header.sizeQual = 0;
            header.sizeAux = 0;
            return header;
        });
}

/// @brief Generate random byte data for stream simulation.
[[nodiscard]] rc::Gen<std::vector<std::uint8_t>> randomStreamData(std::size_t minSize,
                                                                   std::size_t maxSize) {
    return rc::gen::container<std::vector<std::uint8_t>>(
        rc::gen::inRange(minSize, maxSize + 1),
        rc::gen::arbitrary<std::uint8_t>());
}

}  // namespace gen

// =============================================================================
// Property Tests
// =============================================================================

/// @brief Property 1: GlobalHeader round-trip consistency.
/// **Validates: Requirements 5.1, 5.2**
RC_GTEST_PROP(FQCFormatProperty, GlobalHeaderRoundTrip, ()) {
    auto header = *gen::validGlobalHeader();
    auto filename = *gen::validFilename();
    auto timestamp = *rc::gen::inRange<std::uint64_t>(0, 2000000000);

    auto tempPath = tempFilePath();
    TempFileGuard guard(tempPath);

    // Write
    {
        FQCWriter writer(tempPath);
        writer.open();
        writer.writeGlobalHeader(header, filename, timestamp);
        writer.finalize();
    }

    // Read
    {
        FQCReader reader(tempPath);
        reader.open();

        const auto& readHeader = reader.globalHeader();

        // Verify header fields
        RC_ASSERT(readHeader.flags == header.flags);
        RC_ASSERT(readHeader.compressionAlgo == header.compressionAlgo);
        RC_ASSERT(readHeader.checksumType == header.checksumType);
        RC_ASSERT(readHeader.totalReadCount == header.totalReadCount);

        // Verify metadata
        RC_ASSERT(reader.originalFilename() == filename);
        RC_ASSERT(reader.timestamp() == timestamp);
    }
}

/// @brief Property 1.1: Empty archive round-trip.
/// **Validates: Requirements 2.1, 5.1**
RC_GTEST_PROP(FQCFormatProperty, EmptyArchiveRoundTrip, ()) {
    auto header = *gen::validGlobalHeader();
    header.totalReadCount = 0;  // Empty archive

    auto tempPath = tempFilePath();
    TempFileGuard guard(tempPath);

    // Write empty archive
    {
        FQCWriter writer(tempPath);
        writer.open();
        writer.writeGlobalHeader(header, "empty.fastq", 0);
        writer.finalize();
    }

    // Read and verify
    {
        FQCReader reader(tempPath);
        reader.open();

        RC_ASSERT(reader.blockCount() == 0);
        RC_ASSERT(reader.totalReadCount() == 0);
        RC_ASSERT(reader.verifyQuick());
    }
}

/// @brief Property 1.2: Single block round-trip.
/// **Validates: Requirements 2.1, 5.1, 5.2**
RC_GTEST_PROP(FQCFormatProperty, SingleBlockRoundTrip, ()) {
    auto globalHeader = *gen::validGlobalHeader();
    auto blockHeader = *gen::validBlockHeader(0);

    // Generate stream data
    auto idsData = *gen::randomStreamData(10, 1000);
    auto seqData = *gen::randomStreamData(100, 5000);
    auto qualData = *gen::randomStreamData(100, 5000);
    auto auxData = blockHeader.uniformReadLength == 0
                       ? *gen::randomStreamData(10, 500)
                       : std::vector<std::uint8_t>{};

    globalHeader.totalReadCount = blockHeader.uncompressedCount;

    auto tempPath = tempFilePath();
    TempFileGuard guard(tempPath);

    // Write
    {
        FQCWriter writer(tempPath);
        writer.open();
        writer.writeGlobalHeader(globalHeader, "test.fastq", 12345);
        writer.writeBlock(blockHeader, idsData, seqData, qualData, auxData);
        writer.finalize();
    }

    // Read and verify
    {
        FQCReader reader(tempPath);
        reader.open();

        RC_ASSERT(reader.blockCount() == 1);
        RC_ASSERT(reader.totalReadCount() == globalHeader.totalReadCount);

        auto block = reader.readBlock(0);
        RC_ASSERT(block.header.blockId == 0);
        RC_ASSERT(block.header.uncompressedCount == blockHeader.uncompressedCount);
        RC_ASSERT(block.header.uniformReadLength == blockHeader.uniformReadLength);

        // Verify stream data (compressed, so sizes may differ)
        RC_ASSERT(!block.idsData.empty() || idsData.empty());
        RC_ASSERT(!block.seqData.empty() || seqData.empty());
        RC_ASSERT(!block.qualData.empty() || qualData.empty());
    }
}

/// @brief Property 1.3: Multiple blocks round-trip.
/// **Validates: Requirements 2.1, 5.1, 5.2**
RC_GTEST_PROP(FQCFormatProperty, MultipleBlocksRoundTrip, ()) {
    auto numBlocks = *rc::gen::inRange(2, 10);
    auto globalHeader = *gen::validGlobalHeader();

    std::vector<BlockHeader> blockHeaders;
    std::vector<std::vector<std::uint8_t>> allIdsData;
    std::vector<std::vector<std::uint8_t>> allSeqData;
    std::vector<std::vector<std::uint8_t>> allQualData;
    std::vector<std::vector<std::uint8_t>> allAuxData;

    std::uint64_t totalReads = 0;
    for (int i = 0; i < numBlocks; ++i) {
        auto blockHeader = *gen::validBlockHeader(static_cast<std::uint32_t>(i));
        totalReads += blockHeader.uncompressedCount;
        blockHeaders.push_back(blockHeader);

        allIdsData.push_back(*gen::randomStreamData(10, 500));
        allSeqData.push_back(*gen::randomStreamData(50, 2000));
        allQualData.push_back(*gen::randomStreamData(50, 2000));
        allAuxData.push_back(blockHeader.uniformReadLength == 0
                                 ? *gen::randomStreamData(5, 200)
                                 : std::vector<std::uint8_t>{});
    }

    globalHeader.totalReadCount = totalReads;

    auto tempPath = tempFilePath();
    TempFileGuard guard(tempPath);

    // Write
    {
        FQCWriter writer(tempPath);
        writer.open();
        writer.writeGlobalHeader(globalHeader, "multi.fastq", 67890);

        for (int i = 0; i < numBlocks; ++i) {
            writer.writeBlock(blockHeaders[i], allIdsData[i], allSeqData[i], allQualData[i],
                              allAuxData[i]);
        }

        writer.finalize();
    }

    // Read and verify
    {
        FQCReader reader(tempPath);
        reader.open();

        RC_ASSERT(reader.blockCount() == static_cast<std::size_t>(numBlocks));
        RC_ASSERT(reader.totalReadCount() == totalReads);

        // Verify each block
        for (int i = 0; i < numBlocks; ++i) {
            auto block = reader.readBlock(static_cast<BlockId>(i));
            RC_ASSERT(block.header.blockId == static_cast<std::uint32_t>(i));
            RC_ASSERT(block.header.uncompressedCount == blockHeaders[i].uncompressedCount);
        }

        // Verify block index
        const auto& index = reader.blockIndex();
        RC_ASSERT(index.size() == static_cast<std::size_t>(numBlocks));

        std::uint64_t expectedArchiveId = 1;
        for (int i = 0; i < numBlocks; ++i) {
            RC_ASSERT(index[i].archiveIdStart == expectedArchiveId);
            RC_ASSERT(index[i].readCount == blockHeaders[i].uncompressedCount);
            expectedArchiveId += blockHeaders[i].uncompressedCount;
        }
    }
}

/// @brief Property 1.4: Selective stream reading.
/// **Validates: Requirements 2.2, 2.3**
RC_GTEST_PROP(FQCFormatProperty, SelectiveStreamReading, ()) {
    auto globalHeader = *gen::validGlobalHeader();
    auto blockHeader = *gen::validBlockHeader(0);

    auto idsData = *gen::randomStreamData(10, 500);
    auto seqData = *gen::randomStreamData(100, 2000);
    auto qualData = *gen::randomStreamData(100, 2000);

    globalHeader.totalReadCount = blockHeader.uncompressedCount;

    auto tempPath = tempFilePath();
    TempFileGuard guard(tempPath);

    // Write
    {
        FQCWriter writer(tempPath);
        writer.open();
        writer.writeGlobalHeader(globalHeader, "selective.fastq", 0);
        writer.writeBlock(blockHeader, idsData, seqData, qualData, {});
        writer.finalize();
    }

    // Read with different selections
    {
        FQCReader reader(tempPath);
        reader.open();

        // Read only IDs
        auto blockIds = reader.readBlock(0, StreamSelection::kIds);
        RC_ASSERT(!blockIds.idsData.empty() || idsData.empty());

        // Read only sequence
        auto blockSeq = reader.readBlock(0, StreamSelection::kSequence);
        RC_ASSERT(!blockSeq.seqData.empty() || seqData.empty());

        // Read only quality
        auto blockQual = reader.readBlock(0, StreamSelection::kQuality);
        RC_ASSERT(!blockQual.qualData.empty() || qualData.empty());

        // Read all
        auto blockAll = reader.readBlock(0, StreamSelection::kAll);
        RC_ASSERT(!blockAll.idsData.empty() || idsData.empty());
        RC_ASSERT(!blockAll.seqData.empty() || seqData.empty());
        RC_ASSERT(!blockAll.qualData.empty() || qualData.empty());
    }
}

/// @brief Property 1.5: Block index random access.
/// **Validates: Requirements 2.1, 5.2**
RC_GTEST_PROP(FQCFormatProperty, BlockIndexRandomAccess, ()) {
    auto numBlocks = *rc::gen::inRange(3, 8);
    auto globalHeader = *gen::validGlobalHeader();

    std::vector<BlockHeader> blockHeaders;
    std::uint64_t totalReads = 0;

    for (int i = 0; i < numBlocks; ++i) {
        auto blockHeader = *gen::validBlockHeader(static_cast<std::uint32_t>(i));
        totalReads += blockHeader.uncompressedCount;
        blockHeaders.push_back(blockHeader);
    }

    globalHeader.totalReadCount = totalReads;

    auto tempPath = tempFilePath();
    TempFileGuard guard(tempPath);

    // Write
    {
        FQCWriter writer(tempPath);
        writer.open();
        writer.writeGlobalHeader(globalHeader, "random.fastq", 0);

        for (int i = 0; i < numBlocks; ++i) {
            auto data = *gen::randomStreamData(50, 500);
            writer.writeBlock(blockHeaders[i], data, data, data, {});
        }

        writer.finalize();
    }

    // Test random access
    {
        FQCReader reader(tempPath);
        reader.open();

        // Access blocks in random order
        std::vector<int> order(numBlocks);
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), std::mt19937{std::random_device{}()});

        for (int idx : order) {
            auto block = reader.readBlock(static_cast<BlockId>(idx));
            RC_ASSERT(block.header.blockId == static_cast<std::uint32_t>(idx));
        }

        // Test findBlockForRead
        std::uint64_t archiveId = 1;
        for (int i = 0; i < numBlocks; ++i) {
            auto foundBlock = reader.findBlockForRead(archiveId);
            RC_ASSERT(foundBlock == static_cast<BlockId>(i));
            archiveId += blockHeaders[i].uncompressedCount;
        }
    }
}

/// @brief Property 1.6: Checksum verification.
/// **Validates: Requirements 5.1, 5.2, 5.3**
RC_GTEST_PROP(FQCFormatProperty, ChecksumVerification, ()) {
    auto globalHeader = *gen::validGlobalHeader();
    auto blockHeader = *gen::validBlockHeader(0);

    auto idsData = *gen::randomStreamData(10, 200);
    auto seqData = *gen::randomStreamData(50, 500);
    auto qualData = *gen::randomStreamData(50, 500);

    globalHeader.totalReadCount = blockHeader.uncompressedCount;

    auto tempPath = tempFilePath();
    TempFileGuard guard(tempPath);

    // Write
    {
        FQCWriter writer(tempPath);
        writer.open();
        writer.writeGlobalHeader(globalHeader, "checksum.fastq", 0);
        writer.writeBlock(blockHeader, idsData, seqData, qualData, {});
        writer.finalize();
    }

    // Verify checksums
    {
        FQCReader reader(tempPath);
        reader.open();

        // Quick verification should pass
        RC_ASSERT(reader.verifyQuick());

        // Global checksum should pass
        RC_ASSERT(reader.verifyGlobalChecksum());
    }
}

// =============================================================================
// Unit Tests (Non-Property)
// =============================================================================

/// @brief Test magic header validation.
TEST(FQCFormatTest, MagicHeaderValidation) {
    auto tempPath = tempFilePath();
    TempFileGuard guard(tempPath);

    // Create valid archive
    {
        FQCWriter writer(tempPath);
        writer.open();
        GlobalHeader header{};
        header.headerSize = sizeof(GlobalHeader);
        header.flags = 0;
        header.compressionAlgo = 0;
        header.checksumType = 0;
        header.totalReadCount = 0;
        writer.writeGlobalHeader(header, "test.fq", 0);
        writer.finalize();
    }

    // Verify magic
    {
        FQCReader reader(tempPath);
        reader.open();
        EXPECT_TRUE(reader.verifyQuick());
        EXPECT_EQ(reader.version(), 0x10);  // Version 1.0
    }
}

/// @brief Test footer magic validation.
TEST(FQCFormatTest, FooterMagicValidation) {
    auto tempPath = tempFilePath();
    TempFileGuard guard(tempPath);

    // Create valid archive
    {
        FQCWriter writer(tempPath);
        writer.open();
        GlobalHeader header{};
        header.headerSize = sizeof(GlobalHeader);
        writer.writeGlobalHeader(header, "test.fq", 0);
        writer.finalize();
    }

    // Verify footer
    {
        FQCReader reader(tempPath);
        reader.open();
        const auto& footer = reader.footer();
        EXPECT_EQ(std::memcmp(footer.magicEnd, kMagicEnd, sizeof(kMagicEnd)), 0);
    }
}

}  // namespace fqc::format::test

